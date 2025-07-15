#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <vector>
#include <string>
#include <functional>

#include "LunarConstants.h"

namespace Lunar
{

struct TextureLoadJob {
    LunarConstants::TextureInfo textureInfo;
    std::string filePath;
    std::promise<bool> completion;
    
    // CPU에서 로드된 데이터
    uint8_t* imageData = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
};

class AsyncTextureLoader {
public:
    AsyncTextureLoader();
    ~AsyncTextureLoader();
    
    void Initialize(size_t numWorkerThreads = 4);
    void Shutdown();
    
    // 비동기 텍스처 로딩 시작
    std::future<bool> LoadTextureAsync(const LunarConstants::TextureInfo& textureInfo);
    
    // GPU 업로드를 위한 완료된 작업 가져오기
    std::vector<std::unique_ptr<TextureLoadJob>> GetCompletedJobs();
    
    // 통계 정보
    size_t GetPendingJobCount() const { return m_pendingJobs.load(); }
    size_t GetCompletedJobCount() const { return m_completedJobs.size(); }
    
private:
    void WorkerThreadFunction();
    void ProcessLoadJob(std::unique_ptr<TextureLoadJob> job);
    
    // 텍스처 타입별 로딩 메서드
    bool Load2DTexture(TextureLoadJob* job);
    bool LoadCubemapTexture(TextureLoadJob* job);
    bool LoadHDRTexture(TextureLoadJob* job);
    
    // 스레드 관리
    std::vector<std::thread> m_workerThreads;
    std::atomic<bool> m_shouldStop{false};
    
    // 작업 큐
    std::queue<std::unique_ptr<TextureLoadJob>> m_jobQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    
    // 완료된 작업들 (GPU 업로드 대기)
    std::queue<std::unique_ptr<TextureLoadJob>> m_completedJobs;
    std::mutex m_completedMutex;
    
    // 통계
    std::atomic<size_t> m_pendingJobs{0};
    
    // 성능 측정
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_endTime;
};

} // namespace Lunar
