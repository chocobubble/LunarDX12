#include "CommandListPool.h"
#include "Utils/Logger.h"
#include <algorithm>

#include "Utils/Utils.h"

using namespace Microsoft::WRL;
using namespace std;

namespace Lunar
{

CommandListPool::CommandListPool() = default;

CommandListPool::~CommandListPool() 
{
    Shutdown();
}

void CommandListPool::Initialize(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, size_t poolSize, ID3D12Fence* fence) 
{
    LOG_FUNCTION_ENTRY();
    
    if (m_initialized) 
    {
        LOG_WARNING("CommandListPool already initialized");
        return;
    }
    
    if (!device || !fence) 
    {
        LOG_ERROR("Invalid device or fence for CommandListPool initialization");
        return;
    }
    
    m_device = device;
    m_commandListType = type;
    m_fence = fence;
    
    m_contexts.reserve(poolSize);
    for (size_t i = 0; i < poolSize; ++i) 
    {
        CreateCommandListContext(device, type);
        m_availableIndices.push(i);
    }
    
    m_initialized = true;
    LOG_DEBUG("CommandListPool initialized with ", poolSize, " contexts and fence management");
}

void CommandListPool::Shutdown() 
{
    if (!m_initialized) return;
    
    LOG_FUNCTION_ENTRY();
    
    std::lock_guard<std::mutex> lock(m_poolMutex);
    
    for (auto& context : m_contexts) 
    {
        if (context->inUse) 
        {
            LOG_WARNING("Command list context still in use during shutdown");
        }
    }
    
    m_contexts.clear();
    while (!m_availableIndices.empty()) 
    {
        m_availableIndices.pop();
    }
    
    m_initialized = false;
    LOG_DEBUG("CommandListPool shutdown completed");
}

void CommandListPool::CreateCommandListContext(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) 
{
    auto context = std::make_unique<CommandListContext>();
    
    THROW_IF_FAILED(device->CreateCommandAllocator(type, IID_PPV_ARGS(&context->allocator)))

    THROW_IF_FAILED(device->CreateCommandList(
        0,
        type,
        context->allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&context->commandList)
    ))
    
    context->commandList->Close();
    
    size_t index = m_contexts.size();
    m_contexts.push_back(std::move(context));
    
    LOG_DEBUG("Created command list context ", index);
}

CommandListContext* CommandListPool::GetAvailableCommandList()
{
    std::lock_guard<std::mutex> lock(m_poolMutex);
    
    if (!m_initialized) 
    {
        LOG_ERROR("CommandListPool not initialized");
        return nullptr;
    }
    
    // Get current completed fence value from GPU
    UINT64 completedFenceValue = m_fence->GetCompletedValue();
    
    // Mark completed contexts as available
    for (size_t i = 0; i < m_contexts.size(); ++i) 
    {
        auto& context = m_contexts[i];
        if (context->inUse && context->fenceValue <= completedFenceValue) 
        {
            context->inUse = false;
            m_availableIndices.push(i);
            
            context->endTime = chrono::high_resolution_clock::now();
            UpdateStats(context.get());
            
            // LOG_DEBUG("Command list context ", i, " returned to pool");
        }
    }
    
    if (!m_availableIndices.empty()) 
    {
        size_t index = m_availableIndices.front();
        m_availableIndices.pop();
        
        auto* context = m_contexts[index].get();
        context->inUse = true;
        context->startTime = chrono::high_resolution_clock::now();
        
        HRESULT hr = context->allocator->Reset();
        if (FAILED(hr)) 
        {
            LOG_ERROR("Failed to reset command allocator: ", std::hex, hr);
            context->inUse = false;
            m_availableIndices.push(index);
            return nullptr;
        }
        
        hr = context->commandList->Reset(context->allocator.Get(), nullptr);
        if (FAILED(hr)) 
        {
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

void CommandListPool::ReturnCommandList(CommandListContext* context) 
{

}

UINT64 CommandListPool::GetNextFenceValue()
{
	m_nextFenceValue.fetch_add(1);
	return m_nextFenceValue.load();
}

CommandListPool::PoolStats CommandListPool::GetStats() const 
{
    std::lock_guard<std::mutex> poolLock(m_poolMutex);
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    
    PoolStats stats;
    stats.totalContexts = m_contexts.size();
    stats.availableContexts = m_availableIndices.size();
    stats.inUseContexts = stats.totalContexts - stats.availableContexts;
    stats.totalExecutions = m_totalExecutions.load();
    
    if (stats.totalExecutions > 0) 
    {
        stats.averageExecutionTime = m_totalExecutionTime.load() / stats.totalExecutions;
    }
    
    return stats;
}

void CommandListPool::UpdateStats(const CommandListContext* context) 
{
    auto duration = chrono::duration_cast<chrono::microseconds>(
        context->endTime - context->startTime
    );
    
    float executionTimeMs = duration.count() / 1000.0f;
    
    m_totalExecutions++;
    // m_totalExecutionTime += executionTimeMs;
}

void CommandListPool::LogPoolStatus() const 
{
    auto stats = GetStats();
    LOG_DEBUG("CommandListPool Status:");
    LOG_DEBUG("  Total Contexts: ", stats.totalContexts);
    LOG_DEBUG("  Available: ", stats.availableContexts);
    LOG_DEBUG("  In Use: ", stats.inUseContexts);
    LOG_DEBUG("  Total Executions: ", stats.totalExecutions);
    LOG_DEBUG("  Average Execution Time: ", stats.averageExecutionTime, "ms");
}

ScopedCommandList::ScopedCommandList(CommandListPool* pool)
    : m_pool(pool), m_context(nullptr) 
{
    if (m_pool) 
    {
        m_context = m_pool->GetAvailableCommandList();
    }
}

ScopedCommandList::~ScopedCommandList() 
{
	if (m_pool && m_context) 
    {
        m_pool->ReturnCommandList(m_context);
    }
}

ScopedCommandList::ScopedCommandList(ScopedCommandList&& other) noexcept
    : m_pool(other.m_pool), m_context(other.m_context) 
{
    other.m_pool = nullptr;
    other.m_context = nullptr;
}

ScopedCommandList& ScopedCommandList::operator=(ScopedCommandList&& other) noexcept 
{
    if (this != &other) 
    {
        if (m_pool && m_context) 
        {
            m_pool->ReturnCommandList(m_context);
        }
        
        m_pool = other.m_pool;
        m_context = other.m_context;
        
        other.m_pool = nullptr;
        other.m_context = nullptr;
    }
    return *this;
}

UINT64 ScopedCommandList::GetFenceValue() const
{
	m_context->fenceValue = m_pool->GetNextFenceValue();
	return m_context->fenceValue;
}

} // namespace Lunar
