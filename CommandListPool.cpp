#include "CommandListPool.h"
#include "Utils/Logger.h"
#include <algorithm>

using namespace Microsoft::WRL;

namespace Lunar
{

CommandListPool::CommandListPool() = default;

CommandListPool::~CommandListPool() {
    Shutdown();
}

void CommandListPool::Initialize(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, size_t poolSize) {
    LOG_FUNCTION_ENTRY();
    
    if (m_initialized) {
        LOG_WARNING("CommandListPool already initialized");
        return;
    }
    
    m_device = device;
    m_commandListType = type;
    
    m_contexts.reserve(poolSize);
    for (size_t i = 0; i < poolSize; ++i) {
        CreateCommandListContext(device, type);
        m_availableIndices.push(i);
    }
    
    m_initialized = true;
    LOG_DEBUG("CommandListPool initialized with ", poolSize, " contexts");
}

void CommandListPool::Shutdown() {
    if (!m_initialized) return;
    
    LOG_FUNCTION_ENTRY();
    
    std::lock_guard<std::mutex> lock(m_poolMutex);
    
    for (auto& context : m_contexts) {
        if (context->inUse) {
            LOG_WARNING("Command list context still in use during shutdown");
        }
    }
    
    m_contexts.clear();
    while (!m_availableIndices.empty()) {
        m_availableIndices.pop();
    }
    
    m_initialized = false;
    LOG_DEBUG("CommandListPool shutdown completed");
}

void CommandListPool::CreateCommandListContext(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) {
    auto context = std::make_unique<CommandListContext>();
    
    HRESULT hr = device->CreateCommandAllocator(
        type,
        IID_PPV_ARGS(&context->allocator)
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create command allocator: ", std::hex, hr);
        throw std::runtime_error("Failed to create command allocator");
    }
    
    hr = device->CreateCommandList(
        0,
        type,
        context->allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&context->commandList)
    );
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create command list: ", std::hex, hr);
        throw std::runtime_error("Failed to create command list");
    }
    
    context->commandList->Close();
    
    size_t index = m_contexts.size();
    m_contexts.push_back(std::move(context));
    
    LOG_DEBUG("Created command list context ", index);
}

CommandListContext* CommandListPool::GetAvailableCommandList(UINT64 completedFenceValue) {
    std::lock_guard<std::mutex> lock(m_poolMutex);
    
    if (!m_initialized) {
        LOG_ERROR("CommandListPool not initialized");
        return nullptr;
    }
    
    for (size_t i = 0; i < m_contexts.size(); ++i) {
        auto& context = m_contexts[i];
        if (context->inUse && context->fenceValue <= completedFenceValue) {
            context->inUse = false;
            m_availableIndices.push(i);
            
            context->endTime = std::chrono::high_resolution_clock::now();
            UpdateStats(context.get());
            
            // LOG_DEBUG("Command list context ", i, " returned to pool");
        }
    }
    
    if (!m_availableIndices.empty()) {
        size_t index = m_availableIndices.front();
        m_availableIndices.pop();
        
        auto* context = m_contexts[index].get();
        context->inUse = true;
        context->fenceValue = 0; 
        context->startTime = std::chrono::high_resolution_clock::now();
        
        HRESULT hr = context->allocator->Reset();
        if (FAILED(hr)) {
            LOG_ERROR("Failed to reset command allocator: ", std::hex, hr);
            context->inUse = false;
            m_availableIndices.push(index);
            return nullptr;
        }
        
        hr = context->commandList->Reset(context->allocator.Get(), nullptr);
        if (FAILED(hr)) {
            LOG_ERROR("Failed to reset command list: ", std::hex, hr);
            context->inUse = false;
            m_availableIndices.push(index);
            return nullptr;
        }
        
        // LOG_DEBUG("Allocated command list context ", index);
        return context;
    }
    
    LOG_WARNING("No available command list contexts");
    return nullptr;
}

void CommandListPool::ReturnCommandList(CommandListContext* context) {
    if (!context) return;
    
    std::lock_guard<std::mutex> lock(m_poolMutex);
    
    auto it = std::find_if(m_contexts.begin(), m_contexts.end(),
        [context](const std::unique_ptr<CommandListContext>& ctx) {
            return ctx.get() == context;
        });
    
    if (it != m_contexts.end()) {
        size_t index = std::distance(m_contexts.begin(), it);
        
        context->commandList->Close();
        
        // LOG_DEBUG("Command list context ", index, " marked for return");
    } else {
        LOG_ERROR("Invalid command list context returned");
    }
}

CommandListPool::PoolStats CommandListPool::GetStats() const {
    std::lock_guard<std::mutex> poolLock(m_poolMutex);
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    
    PoolStats stats;
    stats.totalContexts = m_contexts.size();
    stats.availableContexts = m_availableIndices.size();
    stats.inUseContexts = stats.totalContexts - stats.availableContexts;
    stats.totalExecutions = m_totalExecutions.load();
    
    if (stats.totalExecutions > 0) {
        stats.averageExecutionTime = m_totalExecutionTime.load() / stats.totalExecutions;
    }
    
    return stats;
}

void CommandListPool::UpdateStats(const CommandListContext* context) {
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        context->endTime - context->startTime
    );
    
    float executionTimeMs = duration.count() / 1000.0f;
    
    m_totalExecutions++;
    // m_totalExecutionTime += executionTimeMs;
}

void CommandListPool::LogPoolStatus() const {
    auto stats = GetStats();
    LOG_DEBUG("CommandListPool Status:");
    LOG_DEBUG("  Total Contexts: ", stats.totalContexts);
    LOG_DEBUG("  Available: ", stats.availableContexts);
    LOG_DEBUG("  In Use: ", stats.inUseContexts);
    LOG_DEBUG("  Total Executions: ", stats.totalExecutions);
    LOG_DEBUG("  Average Execution Time: ", stats.averageExecutionTime, "ms");
}

ScopedCommandList::ScopedCommandList(CommandListPool* pool, UINT64 completedFenceValue)
    : m_pool(pool), m_context(nullptr) {
    if (m_pool) {
        m_context = m_pool->GetAvailableCommandList(completedFenceValue);
    }
}

ScopedCommandList::~ScopedCommandList() {
    if (m_pool && m_context) {
        m_pool->ReturnCommandList(m_context);
    }
}

ScopedCommandList::ScopedCommandList(ScopedCommandList&& other) noexcept
    : m_pool(other.m_pool), m_context(other.m_context) {
    other.m_pool = nullptr;
    other.m_context = nullptr;
}

ScopedCommandList& ScopedCommandList::operator=(ScopedCommandList&& other) noexcept {
    if (this != &other) {
        if (m_pool && m_context) {
            m_pool->ReturnCommandList(m_context);
        }
        
        m_pool = other.m_pool;
        m_context = other.m_context;
        
        other.m_pool = nullptr;
        other.m_context = nullptr;
    }
    return *this;
}

} // namespace Lunar
