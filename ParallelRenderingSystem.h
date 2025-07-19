#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <thread>
#include <future>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <chrono>

#include "CommandListPool.h"

namespace Lunar
{

class SceneRenderer;
class AsyncTextureLoader;

// 렌더링 작업 타입
enum class RenderJobType {
    ShadowMap,
    Particles,
    PostProcess,
    UI,
    Skybox
};

// 개별 렌더링 작업
struct RenderJob {
    RenderJobType type;
    std::function<void(ID3D12GraphicsCommandList*)> renderFunction;
    std::string name;
    int priority = 0; // 높을수록 우선순위
    
    // 의존성 관리
    std::vector<RenderJobType> dependencies;
    std::atomic<bool> completed{false};
};

// 렌더링 패스 (여러 작업들의 그룹)
struct RenderPass {
    std::string name;
    std::vector<std::unique_ptr<RenderJob>> jobs;
    bool canRunInParallel = true;
    
    // 패스 완료 확인
    bool IsCompleted() const {
        return std::all_of(jobs.begin(), jobs.end(), 
            [](const auto& job) { return job->completed.load(); });
    }
};

class ParallelRenderingSystem {
public:
    ParallelRenderingSystem();
    ~ParallelRenderingSystem();
    
    void Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, 
                   CommandListPool* commandListPool, size_t numWorkerThreads = 4);
    void Shutdown();
    
    // 렌더링 작업 등록
    void RegisterRenderJob(RenderJobType type, 
                          std::function<void(ID3D12GraphicsCommandList*)> renderFunc,
                          const std::string& name, int priority = 0);
    
    // 렌더링 패스 실행
    void ExecuteRenderPass(const std::string& passName);
    void ExecuteParallelRenderPass(const std::string& passName);
    
    // 비동기 텍스처 로딩과 연동
    void SetAsyncTextureLoader(AsyncTextureLoader* loader) { m_asyncTextureLoader = loader; }
    
    // 성능 통계
    struct RenderingStats {
        float totalFrameTime = 0.0f;
        float parallelRenderTime = 0.0f;
        float sequentialRenderTime = 0.0f;
        size_t completedJobs = 0;
        size_t activeThreads = 0;
        float speedupRatio = 1.0f; // 병렬화 성능 향상 비율
    };
    RenderingStats GetStats() const { return m_stats; }
    
    // 디버깅
    void LogRenderingStats() const;
    void EnableDebugMode(bool enable) { m_debugMode = enable; }
    
private:
    void WorkerThreadFunction();
    void ExecuteRenderJob(RenderJob* job, CommandListContext* context);
    bool CheckDependencies(const RenderJob* job) const;
    void UpdateStats();
    
    // 렌더링 패스 관리
    std::unordered_map<std::string, std::unique_ptr<RenderPass>> m_renderPasses;
    
    // 스레드 풀
    std::vector<std::thread> m_workerThreads;
    std::queue<RenderJob*> m_jobQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_shouldStop{false};
    
    // DirectX 12 리소스
    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    CommandListPool* m_commandListPool = nullptr;
    
    // 비동기 텍스처 로더 연동
    AsyncTextureLoader* m_asyncTextureLoader = nullptr;
    
    // 성능 추적
    mutable std::mutex m_statsMutex;
    RenderingStats m_stats;
    std::chrono::high_resolution_clock::time_point m_frameStartTime;
    
    // 디버깅
    bool m_debugMode = false;
    std::atomic<size_t> m_activeWorkers{0};
    
    bool m_initialized = false;
};

} // namespace Lunar
