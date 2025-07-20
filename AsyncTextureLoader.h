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
#include <unordered_map>
#include <chrono>

#include "LunarConstants.h"

namespace Lunar
{

class CommandListPool;
class DescriptorAllocator;

struct TextureLoadJob
{
	LunarConstants::TextureInfo textureInfo;
	std::promise<bool> completion;
	
	uint8_t* imageData = nullptr;
	int width = 0;
	int height = 0;
	int channels = 0;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> gpuResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	std::chrono::high_resolution_clock::time_point loadStartTime;
	std::chrono::high_resolution_clock::time_point loadEndTime;
};

class AsyncTextureLoader
{
public:
	AsyncTextureLoader();
	~AsyncTextureLoader();
	
	void Initialize(size_t numWorkerThreads, ID3D12Device* device, 
	               CommandListPool* uploadPool, DescriptorAllocator* descriptorAllocator,
	               ID3D12CommandQueue* commandQueue, ID3D12Fence* fence);
	
	void Shutdown();
	
	std::future<bool> LoadTextureAsync(const LunarConstants::TextureInfo& textureInfo);
	
	bool IsTextureReady(const std::string& textureName) const;
	ID3D12Resource* GetTexture(const std::string& textureName) const;
	
	bool AreAllTexturesLoaded() const;
	float GetLoadingProgress() const;
	int GetTotalTextures() const { return m_totalTextures.load(); }
	int GetLoadedTextures() const { return m_loadedTextures.load(); }
	
	struct LoadingStats 
	{
		size_t queuedJobs = 0;
		size_t completedCPULoads = 0;
		size_t completedGPUUploads = 0;
		size_t failedJobs = 0;
		float averageCPULoadTime = 0.0f;
		float averageGPUUploadTime = 0.0f;
		float totalProcessingTime = 0.0f;
	};
	LoadingStats GetStats() const;
	
	void LogStatus() const;
	
private:
    bool IsInitialized() const;
	void WorkerThreadFunction();
	void ProcessLoadAndUploadJob(std::unique_ptr<TextureLoadJob> job);
	
	bool Load2DTexture(TextureLoadJob* job);
	bool LoadCubemapTexture(TextureLoadJob* job);
	bool LoadHDRTexture(TextureLoadJob* job);
	
	// GPU upload functions
	bool UploadTextureToGPU(TextureLoadJob* job);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(TextureLoadJob* job, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
	void SetupSRVDescription(TextureLoadJob* job);
	
	std::vector<std::thread> m_workerThreads;
	std::atomic<bool> m_shouldStop{false};
	std::atomic<bool> m_gpuUploadEnabled{false};
	
	std::queue<std::unique_ptr<TextureLoadJob>> m_jobQueue;
	std::mutex m_queueMutex;
	std::condition_variable m_cv;
	
	// Completed textures queue (GPU upload completed)
	std::queue<std::unique_ptr<TextureLoadJob>> m_completedTextures;
	std::mutex m_completedTexturesMutex;
	
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> m_textures;
	mutable std::mutex m_loadedTexturesMutex;
	
	// GPU resources
	ID3D12Device* m_device = nullptr;
	CommandListPool* m_uploadPool = nullptr;
	DescriptorAllocator* m_descriptorAllocator = nullptr;
	ID3D12CommandQueue* m_commandQueue = nullptr;
	ID3D12Fence* m_fence = nullptr;
	
	// Statistics
	mutable std::mutex m_statsMutex;
	LoadingStats m_stats;
	
	// Simple completion tracking
	std::atomic<int> m_totalTextures{0};      // Total texture requests
	std::atomic<int> m_loadedTextures{0};     // Completed textures (GPU uploaded)
};

} // namespace Lunar
