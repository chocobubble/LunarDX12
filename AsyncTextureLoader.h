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

struct TextureLoadJob
{
    LunarConstants::TextureInfo textureInfo;
    std::promise<bool> completion;
    
    uint8_t* imageData = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
};

class AsyncTextureLoader
{
public:
    AsyncTextureLoader();
    ~AsyncTextureLoader();
    
    void Initialize(size_t numWorkerThreads = 4);
    void Shutdown();
    
    std::future<bool> LoadTextureAsync(const LunarConstants::TextureInfo& textureInfo);
    
    std::vector<std::unique_ptr<TextureLoadJob>> GetCompletedJobs();
private:
    void WorkerThreadFunction();
    void ProcessLoadJob(std::unique_ptr<TextureLoadJob> job);
    
    bool Load2DTexture(TextureLoadJob* job);
    bool LoadCubemapTexture(TextureLoadJob* job);
    bool LoadHDRTexture(TextureLoadJob* job);
    
    std::vector<std::thread> m_workerThreads;
    std::atomic<bool> m_shouldStop{false};
    
    std::queue<std::unique_ptr<TextureLoadJob>> m_jobQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    
    std::queue<std::unique_ptr<TextureLoadJob>> m_completedJobs;
    std::mutex m_completedMutex;
};

} // namespace Lunar
