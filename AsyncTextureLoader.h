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
	
	// GPU upload related
	Microsoft::WRL::ComPtr<ID3D12Resource> gpuResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	std::chrono::high_resolution_clock::time_point uploadStartTime;
	std::chrono::high_resolution_clock::time_point uploadEndTime;
	bool gpuUploadRequested = false;
	bool gpuUploadCompleted = false;
};

class AsyncTextureLoader
{
public:
	AsyncTextureLoader();
	~AsyncTextureLoader();
	
	// Basic initialization (CPU loading only)
	void Initialize(size_t numWorkerThreads = 4);
	
	// Extended initialization with GPU upload capability
	void Initialize(size_t numWorkerThreads, ID3D12Device* device, 
	               CommandListPool* uploadPool, DescriptorAllocator* descriptorAllocator);
	
	void Shutdown();
	
	// Enable/disable GPU upload functionality
	void EnableGPUUpload(ID3D12Device* device, CommandListPool* uploadPool, 
	                    DescriptorAllocator* descriptorAllocator);
	void DisableGPUUpload();
	
	// Load texture (CPU only)
	std::future<bool> LoadTextureAsync(const LunarConstants::TextureInfo& textureInfo);
	
	// Load texture with GPU upload
	std::future<bool> LoadTextureWithUploadAsync(const LunarConstants::TextureInfo& textureInfo);
	
	// Get completed jobs (CPU loading only)
	std::vector<std::unique_ptr<TextureLoadJob>> GetCompletedJobs();
	
	// Process completed GPU uploads (call from main thread)
	void ProcessCompletedUploads();
	
	// Query functions
	bool IsTextureReady(const std::string& textureName) const;
	ID3D12Resource* GetUploadedTexture(const std::string& textureName) const;
	
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
	void WorkerThreadFunction();
	void ProcessLoadJob(std::unique_ptr<TextureLoadJob> job);
	void ProcessLoadAndUploadJob(std::unique_ptr<TextureLoadJob> job);
	
	bool Load2DTexture(TextureLoadJob* job);
	bool LoadCubemapTexture(TextureLoadJob* job);
	bool LoadHDRTexture(TextureLoadJob* job);
	
	// GPU upload functions
	bool UploadTextureToGPU(TextureLoadJob* job);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(TextureLoadJob* job, 
	                                                             Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateCubemapResource(TextureLoadJob* job, 
	                                                             const std::vector<uint8_t*>& faceData,
	                                                             Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
	void SetupSRVDescription(TextureLoadJob* job);
	
	std::vector<std::thread> m_workerThreads;
	std::atomic<bool> m_shouldStop{false};
	std::atomic<bool> m_gpuUploadEnabled{false};
	
	std::queue<std::unique_ptr<TextureLoadJob>> m_jobQueue;
	std::mutex m_queueMutex;
	std::condition_variable m_cv;
	
	std::queue<std::unique_ptr<TextureLoadJob>> m_completedJobs;
	std::mutex m_completedMutex;
	
	// GPU upload related
	std::queue<std::unique_ptr<TextureLoadJob>> m_completedUploads;
	std::mutex m_completedUploadsMutex;
	
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> m_uploadedTextures;
	mutable std::mutex m_uploadedTexturesMutex;
	
	// GPU resources
	ID3D12Device* m_device = nullptr;
	CommandListPool* m_uploadPool = nullptr;
	DescriptorAllocator* m_descriptorAllocator = nullptr;
	
	// Statistics
	mutable std::mutex m_statsMutex;
	LoadingStats m_stats;
	std::atomic<UINT64> m_currentFenceValue{0};
};

} // namespace Lunar
