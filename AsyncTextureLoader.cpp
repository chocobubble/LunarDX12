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
    
    m_workerThreads.reserve(numWorkerThreads);
    for (size_t i = 0; i < numWorkerThreads; ++i) {
        m_workerThreads.emplace_back(&AsyncTextureLoader::WorkerThreadFunction, this);
        LOG_DEBUG("Created worker thread ", i, " for texture loading");
    }
    
    LOG_DEBUG("AsyncTextureLoader initialized with ", numWorkerThreads, " worker threads");
}

void AsyncTextureLoader::Shutdown() {
    LOG_FUNCTION_ENTRY();
    
    {
        lock_guard<mutex> lock(m_queueMutex);
        m_shouldStop = true;
    }
    m_cv.notify_all();
    
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_workerThreads.clear();
    
    while (!m_jobQueue.empty()) {
        auto job = move(m_jobQueue.front());
        m_jobQueue.pop();
        if (job->imageData) {
            stbi_image_free(job->imageData);
        }
    }
    
    while (!m_completedJobs.empty()) {
        auto job = move(m_completedJobs.front());
        m_completedJobs.pop();
        if (job->imageData) {
            stbi_image_free(job->imageData);
        }
    }
    
    LOG_DEBUG("AsyncTextureLoader shutdown completed");
}

future<bool> AsyncTextureLoader::LoadTextureAsync(const LunarConstants::TextureInfo& textureInfo) {
    auto job = make_unique<TextureLoadJob>();
    job->textureInfo = textureInfo;
    job->textureInfo.path = textureInfo.path;
    
    auto future = job->completion.get_future();
    
    {
        lock_guard<mutex> lock(m_queueMutex);
        m_jobQueue.push(move(job));
    }
    m_cv.notify_one();
    
    LOG_DEBUG("Queued texture loading job: ", textureInfo.name);
    return future;
}

vector<unique_ptr<TextureLoadJob>> AsyncTextureLoader::GetCompletedJobs() {
    vector<unique_ptr<TextureLoadJob>> completedJobs;
    
    lock_guard<mutex> lock(m_completedMutex);
    while (!m_completedJobs.empty()) {
        completedJobs.push_back(move(m_completedJobs.front()));
        m_completedJobs.pop();
    }
    
    return completedJobs;
}

void AsyncTextureLoader::WorkerThreadFunction() {
    LOG_DEBUG("Worker thread started, ID: ", this_thread::get_id());
    
    while (true) {
        unique_ptr<TextureLoadJob> job;
        
        {
            unique_lock<mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this] { return m_shouldStop || !m_jobQueue.empty(); });
            
            if (m_shouldStop && m_jobQueue.empty()) {
                break;
            }
            
            if (!m_jobQueue.empty()) {
                job = move(m_jobQueue.front());
                m_jobQueue.pop();
            }
        }
        
        if (job) {
            ProcessLoadJob(move(job));
        }
    }
    
    LOG_DEBUG("Worker thread terminated, ID: ", this_thread::get_id());
}

void AsyncTextureLoader::ProcessLoadJob(unique_ptr<TextureLoadJob> job) {
    auto startTime = chrono::high_resolution_clock::now();
    
    try {
        if (!filesystem::exists(job->textureInfo.path)) {
            LOG_ERROR("Texture file not found: ", job->textureInfo.path);
            job->completion.set_value(false);
            return;
        }
        
        bool success = false;
        
        if (job->textureInfo.fileType == LunarConstants::FileType::HDR) {
            success = LoadHDRTexture(job.get());
        } else {
            return; // TODO: Implement other texture types
        }
        
        auto endTime = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        
        if (success) {
            LOG_DEBUG("Texture loaded successfully: ", job->textureInfo.name, 
                     " (", job->width, "x", job->height, ") in ", duration.count(), "ms");
            
            {
                lock_guard<mutex> lock(m_completedMutex);
                m_completedJobs.push(move(job));
            }
        } else {
            LOG_ERROR("Failed to load texture: ", job->textureInfo.name);
        }
        
        job->completion.set_value(success);
        
    } catch (const exception& e) {
        LOG_ERROR("Exception while loading texture ", job->textureInfo.name, ": ", e.what());
        job->completion.set_value(false);
    }
}

bool AsyncTextureLoader::Load2DTexture(TextureLoadJob* job) {
    return false;
}

bool AsyncTextureLoader::LoadCubemapTexture(TextureLoadJob* job) {
    return false;
}

bool AsyncTextureLoader::LoadHDRTexture(TextureLoadJob* job) {
    LOG_DEBUG("Loading HDR texture: ", job->textureInfo.path);
    
    int width, height, channels;
    float* data = stbi_loadf(job->textureInfo.path.c_str(), &width, &height, &channels, 3); // loadf : float
    if (!data) {
        LOG_ERROR("Failed to load HDR texture: ", job->textureInfo.path);
        return false;
    }
    
    LOG_DEBUG("HDR texture loaded: ", job->textureInfo.path, " (", width, "x", height, ", ", channels, " channels)");
    
    job->width = width;
    job->height = height;
    job->imageData = reinterpret_cast<uint8_t*>(data);
}

} // namespace Lunar
