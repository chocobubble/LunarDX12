#include "ParallelRenderingSystem.h"
#include "AsyncTextureLoader.h"
#include "Utils/Logger.h"
#include <algorithm>

using namespace Microsoft::WRL;

namespace Lunar
{

ParallelRenderingSystem::ParallelRenderingSystem() = default;

ParallelRenderingSystem::~ParallelRenderingSystem() {
    Shutdown();
}

void ParallelRenderingSystem::Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, 
                                       CommandListPool* commandListPool, size_t numWorkerThreads) {
    LOG_FUNCTION_ENTRY();
    
    if (m_initialized) {
        LOG_WARNING("ParallelRenderingSystem already initialized");
        return;
    }
    
    m_device = device;
    m_commandQueue = commandQueue;
    m_commandListPool = commandListPool;
    
    // 워커 스레드 생성
    m_workerThreads.reserve(numWorkerThreads);
    for (size_t i = 0; i < numWorkerThreads; ++i) {
        m_workerThreads.emplace_back(&ParallelRenderingSystem::WorkerThreadFunction, this);
        LOG_DEBUG("Created rendering worker thread ", i);
    }
    
    // 기본 렌더링 패스들 생성
    CreateDefaultRenderPasses();
    
    m_initialized = true;
    LOG_INFO("ParallelRenderingSystem initialized with ", numWorkerThreads, " worker threads");
}

void ParallelRenderingSystem::Shutdown() {
    if (!m_initialized) return;
    
    LOG_FUNCTION_ENTRY();
    
    // 모든 워커 스레드 종료
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_shouldStop = true;
    }
    m_cv.notify_all();
    
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_workerThreads.clear();
    
    // 렌더링 패스 정리
    m_renderPasses.clear();
    
    m_initialized = false;
    LOG_INFO("ParallelRenderingSystem shutdown completed");
}

void ParallelRenderingSystem::CreateDefaultRenderPasses() {
    // 🎯 Shadow Pass - 병렬 실행 가능
    auto shadowPass = std::make_unique<RenderPass>();
    shadowPass->name = "ShadowPass";
    shadowPass->canRunInParallel = true;
    m_renderPasses["ShadowPass"] = std::move(shadowPass);
    
    // 🎯 Main Pass - 일부 병렬 실행 가능
    auto mainPass = std::make_unique<RenderPass>();
    mainPass->name = "MainPass";
    mainPass->canRunInParallel = true;
    m_renderPasses["MainPass"] = std::move(mainPass);
    
    // 🎯 Post Process Pass - 순차 실행 필요
    auto postPass = std::make_unique<RenderPass>();
    postPass->name = "PostProcessPass";
    postPass->canRunInParallel = false;
    m_renderPasses["PostProcessPass"] = std::move(postPass);
}

void ParallelRenderingSystem::RegisterRenderJob(RenderJobType type, 
                                               std::function<void(ID3D12GraphicsCommandList*)> renderFunc,
                                               const std::string& name, int priority) {
    auto job = std::make_unique<RenderJob>();
    job->type = type;
    job->renderFunction = renderFunc;
    job->name = name;
    job->priority = priority;
    job->completed = false;
    
    // 의존성 설정
    switch (type) {
        case RenderJobType::Particles:
            job->dependencies.push_back(RenderJobType::ShadowMap);
            break;
        case RenderJobType::PostProcess:
            job->dependencies.push_back(RenderJobType::Skybox);
            job->dependencies.push_back(RenderJobType::Particles);
            break;
        case RenderJobType::UI:
            job->dependencies.push_back(RenderJobType::PostProcess);
            break;
    }
    
    // 적절한 패스에 추가
    std::string passName;
    switch (type) {
        case RenderJobType::ShadowMap:
            passName = "ShadowPass";
            break;
        case RenderJobType::Skybox:
        case RenderJobType::Particles:
            passName = "MainPass";
            break;
        case RenderJobType::PostProcess:
        case RenderJobType::UI:
            passName = "PostProcessPass";
            break;
    }
    
    if (m_renderPasses.find(passName) != m_renderPasses.end()) {
        m_renderPasses[passName]->jobs.push_back(std::move(job));
        LOG_DEBUG("Registered render job: ", name, " in pass: ", passName);
    }
}

void ParallelRenderingSystem::ExecuteParallelRenderPass(const std::string& passName) {
    auto it = m_renderPasses.find(passName);
    if (it == m_renderPasses.end()) {
        LOG_ERROR("Render pass not found: ", passName);
        return;
    }
    
    auto& renderPass = it->second;
    if (!renderPass->canRunInParallel) {
        ExecuteRenderPass(passName);
        return;
    }
    
    LOG_DEBUG("Executing parallel render pass: ", passName);
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 🚀 병렬 실행: 모든 작업을 큐에 추가
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        for (auto& job : renderPass->jobs) {
            job->completed = false;
            m_jobQueue.push(job.get());
        }
    }
    m_cv.notify_all();
    
    // 모든 작업 완료 대기
    while (!renderPass->IsCompleted()) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    // 🎮 모든 커맨드 리스트를 GPU에 제출
    SubmitAllCommandLists();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.parallelRenderTime += duration.count();
        m_stats.completedJobs += renderPass->jobs.size();
    }
    
    LOG_DEBUG("Parallel render pass completed: ", passName, " in ", duration.count(), "ms");
}

void ParallelRenderingSystem::ExecuteRenderPass(const std::string& passName) {
    auto it = m_renderPasses.find(passName);
    if (it == m_renderPasses.end()) {
        LOG_ERROR("Render pass not found: ", passName);
        return;
    }
    
    LOG_DEBUG("Executing sequential render pass: ", passName);
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 🐌 순차 실행
    UINT64 completedFenceValue = GetCompletedFenceValue();
    ScopedCommandList scopedCmdList(m_commandListPool, completedFenceValue);
    
    if (!scopedCmdList.IsValid()) {
        LOG_ERROR("Failed to get command list for sequential rendering");
        return;
    }
    
    auto* cmdList = scopedCmdList.Get();
    auto& renderPass = it->second;
    
    for (auto& job : renderPass->jobs) {
        if (CheckDependencies(job.get())) {
            job->renderFunction(cmdList);
            job->completed = true;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.sequentialRenderTime += duration.count();
        m_stats.completedJobs += renderPass->jobs.size();
    }
    
    LOG_DEBUG("Sequential render pass completed: ", passName, " in ", duration.count(), "ms");
}

void ParallelRenderingSystem::WorkerThreadFunction() {
    LOG_DEBUG("Rendering worker thread started, ID: ", std::this_thread::get_id());
    
    while (true) {
        RenderJob* job = nullptr;
        
        // 작업 가져오기
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this] { return m_shouldStop || !m_jobQueue.empty(); });
            
            if (m_shouldStop && m_jobQueue.empty()) {
                break;
            }
            
            if (!m_jobQueue.empty()) {
                job = m_jobQueue.front();
                m_jobQueue.pop();
            }
        }
        
        if (job && CheckDependencies(job)) {
            m_activeWorkers++;
            
            // 🎮 Command List 할당
            UINT64 completedFenceValue = GetCompletedFenceValue();
            ScopedCommandList scopedCmdList(m_commandListPool, completedFenceValue);
            
            if (scopedCmdList.IsValid()) {
                ExecuteRenderJob(job, scopedCmdList.GetContext());
            } else {
                LOG_WARNING("Failed to get command list for job: ", job->name);
            }
            
            m_activeWorkers--;
        }
    }
    
    LOG_DEBUG("Rendering worker thread terminated, ID: ", std::this_thread::get_id());
}

void ParallelRenderingSystem::ExecuteRenderJob(RenderJob* job, CommandListContext* context) {
    if (!job || !context) return;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (m_debugMode) {
        LOG_DEBUG("Executing render job: ", job->name, " on thread: ", std::this_thread::get_id());
    }
    
    try {
        // 🎮 실제 렌더링 작업 실행
        job->renderFunction(context->commandList.Get());
        job->completed = true;
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        if (m_debugMode) {
            LOG_DEBUG("Render job completed: ", job->name, " in ", duration.count(), "μs");
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in render job ", job->name, ": ", e.what());
        job->completed = false;
    }
}

bool ParallelRenderingSystem::CheckDependencies(const RenderJob* job) const {
    if (!job) return false;
    
    // 의존성이 없으면 실행 가능
    if (job->dependencies.empty()) {
        return true;
    }
    
    // 모든 의존성 작업이 완료되었는지 확인
    for (const auto& passEntry : m_renderPasses) {
        for (const auto& otherJob : passEntry.second->jobs) {
            auto it = std::find(job->dependencies.begin(), job->dependencies.end(), otherJob->type);
            if (it != job->dependencies.end() && !otherJob->completed.load()) {
                return false; // 의존성 작업이 아직 완료되지 않음
            }
        }
    }
    
    return true;
}

UINT64 ParallelRenderingSystem::GetCompletedFenceValue() const {
    // 실제 구현에서는 fence에서 완료된 값을 가져와야 함
    // 여기서는 임시로 0 반환
    return 0;
}

void ParallelRenderingSystem::SubmitAllCommandLists() {
    // 실제 구현에서는 모든 완료된 command list들을 수집해서 GPU에 제출
    // 현재는 각 ScopedCommandList가 개별적으로 제출됨
    LOG_DEBUG("Submitting all command lists to GPU");
}

ParallelRenderingSystem::RenderingStats ParallelRenderingSystem::GetStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    RenderingStats stats = m_stats;
    stats.activeThreads = m_activeWorkers.load();
    stats.totalFrameTime = stats.parallelRenderTime + stats.sequentialRenderTime;
    
    if (stats.sequentialRenderTime > 0) {
        stats.speedupRatio = stats.sequentialRenderTime / stats.parallelRenderTime;
    }
    
    return stats;
}

void ParallelRenderingSystem::LogRenderingStats() const {
    auto stats = GetStats();
    LOG_INFO("=== Parallel Rendering Statistics ===");
    LOG_INFO("Total Frame Time: ", stats.totalFrameTime, "ms");
    LOG_INFO("Parallel Render Time: ", stats.parallelRenderTime, "ms");
    LOG_INFO("Sequential Render Time: ", stats.sequentialRenderTime, "ms");
    LOG_INFO("Speedup Ratio: ", stats.speedupRatio, "x");
    LOG_INFO("Completed Jobs: ", stats.completedJobs);
    LOG_INFO("Active Threads: ", stats.activeThreads);
}

} // namespace Lunar
