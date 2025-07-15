#include "AsyncTextureLoader.h"
#include <stb_image.h>
#include <filesystem>
#include "Utils/Logger.h"

using namespace std;

namespace Lunar
{

AsyncTextureLoader::AsyncTextureLoader() = default;

AsyncTextureLoader::~AsyncTextureLoader() {
    Shutdown();
}

void AsyncTextureLoader::Initialize(size_t numWorkerThreads) {
    LOG_FUNCTION_ENTRY();
    
    m_shouldStop = false;
    m_startTime = std::chrono::high_resolution_clock::now();
    
    // 워커 스레드 생성
    m_workerThreads.reserve(numWorkerThreads);
    for (size_t i = 0; i < numWorkerThreads; ++i) {
        m_workerThreads.emplace_back(&AsyncTextureLoader::WorkerThreadFunction, this);
        LOG_DEBUG("Created worker thread ", i, " for texture loading");
    }
    
    LOG_INFO("AsyncTextureLoader initialized with ", numWorkerThreads, " worker threads");
}

void AsyncTextureLoader::Shutdown() {
    LOG_FUNCTION_ENTRY();
    
    // 모든 스레드에 종료 신호
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_shouldStop = true;
    }
    m_cv.notify_all();
    
    // 모든 워커 스레드 종료 대기
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_workerThreads.clear();
    
    // 남은 작업들 정리
    while (!m_jobQueue.empty()) {
        auto job = std::move(m_jobQueue.front());
        m_jobQueue.pop();
        if (job->imageData) {
            stbi_image_free(job->imageData);
        }
    }
    
    while (!m_completedJobs.empty()) {
        auto job = std::move(m_completedJobs.front());
        m_completedJobs.pop();
        if (job->imageData) {
            stbi_image_free(job->imageData);
        }
    }
    
    LOG_INFO("AsyncTextureLoader shutdown completed");
}

std::future<bool> AsyncTextureLoader::LoadTextureAsync(const LunarConstants::TextureInfo& textureInfo) {
    auto job = std::make_unique<TextureLoadJob>();
    job->textureInfo = textureInfo;
    job->filePath = textureInfo.path;
    
    auto future = job->completion.get_future();
    
    // 작업 큐에 추가
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_jobQueue.push(std::move(job));
        m_pendingJobs++;
    }
    m_cv.notify_one();
    
    LOG_DEBUG("Queued texture loading job: ", textureInfo.name);
    return future;
}

std::vector<std::unique_ptr<TextureLoadJob>> AsyncTextureLoader::GetCompletedJobs() {
    std::vector<std::unique_ptr<TextureLoadJob>> completedJobs;
    
    std::lock_guard<std::mutex> lock(m_completedMutex);
    while (!m_completedJobs.empty()) {
        completedJobs.push_back(std::move(m_completedJobs.front()));
        m_completedJobs.pop();
    }
    
    return completedJobs;
}

void AsyncTextureLoader::WorkerThreadFunction() {
    LOG_DEBUG("Worker thread started, ID: ", std::this_thread::get_id());
    
    while (true) {
        std::unique_ptr<TextureLoadJob> job;
        
        // 작업 가져오기
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this] { return m_shouldStop || !m_jobQueue.empty(); });
            
            if (m_shouldStop && m_jobQueue.empty()) {
                break;
            }
            
            if (!m_jobQueue.empty()) {
                job = std::move(m_jobQueue.front());
                m_jobQueue.pop();
            }
        }
        
        if (job) {
            ProcessLoadJob(std::move(job));
        }
    }
    
    LOG_DEBUG("Worker thread terminated, ID: ", std::this_thread::get_id());
}

void AsyncTextureLoader::ProcessLoadJob(std::unique_ptr<TextureLoadJob> job) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        // 파일 존재 확인
        if (!std::filesystem::exists(job->filePath)) {
            LOG_ERROR("Texture file not found: ", job->filePath);
            job->completion.set_value(false);
            m_pendingJobs--;
            return;
        }
        
        // 텍스처 타입별 로딩
        bool success = false;
        
        if (job->textureInfo.fileType == LunarConstants::FileType::DEFAULT) {
            if (job->textureInfo.dimensionType == LunarConstants::TextureDimension::CUBEMAP) {
                // 큐브맵 로딩 (6개 면)
                success = LoadCubemapTexture(job.get());
            } else {
                // 일반 2D 텍스처 로딩
                success = Load2DTexture(job.get());
            }
        } else if (job->textureInfo.fileType == LunarConstants::FileType::HDR) {
            // HDR 텍스처 로딩
            success = LoadHDRTexture(job.get());
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        if (success) {
            LOG_DEBUG("Texture loaded successfully: ", job->textureInfo.name, 
                     " (", job->width, "x", job->height, ") in ", duration.count(), "ms");
            
            // 완료된 작업 큐에 추가
            {
                std::lock_guard<std::mutex> lock(m_completedMutex);
                m_completedJobs.push(std::move(job));
            }
        } else {
            LOG_ERROR("Failed to load texture: ", job->textureInfo.name);
        }
        
        job->completion.set_value(success);
        m_pendingJobs--;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while loading texture ", job->textureInfo.name, ": ", e.what());
        job->completion.set_value(false);
        m_pendingJobs--;
    }
}

bool AsyncTextureLoader::Load2DTexture(TextureLoadJob* job) {
    job->imageData = stbi_load(job->filePath.c_str(), &job->width, &job->height, &job->channels, 4);
    return job->imageData != nullptr;
}

bool AsyncTextureLoader::LoadCubemapTexture(TextureLoadJob* job) {
    // 큐브맵 6면 로딩 구현
    // 현재는 단순화를 위해 false 반환
    LOG_WARNING("Cubemap async loading not yet implemented");
    return false;
}

bool AsyncTextureLoader::LoadHDRTexture(TextureLoadJob* job) {
    // HDR 텍스처 로딩 구현
    // 현재는 단순화를 위해 false 반환
    LOG_WARNING("HDR async loading not yet implemented");
    return false;
}

} // namespace Lunar
