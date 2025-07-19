#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <chrono>

#include "LunarConstants.h"

namespace Lunar
{

class CommandListPool;
class DescriptorAllocator;
class AsyncTextureLoader;

struct UploadRequest 
{
	std::string filePath;
	std::string textureName;
	LunarConstants::TextureInfo textureInfo;
	uint8_t* imageData = nullptr;
	int width = 0;
	int height = 0;
	int channels = 0;
	std::chrono::high_resolution_clock::time_point requestTime;
};

struct CompletedTexture 
{
	std::string name;
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	std::chrono::high_resolution_clock::time_point completionTime;
	float uploadTimeMs = 0.0f;
};

class AsyncTextureUploadManager 
{
public:
	AsyncTextureUploadManager();
	~AsyncTextureUploadManager();
	
	void Initialize(ID3D12Device* device, CommandListPool* uploadPool, 
	               DescriptorAllocator* descriptorAllocator, size_t workerThreadCount = 2);
	void Shutdown();
	
	void QueueTextureUpload(const std::string& path, const std::string& name, 
	                       const LunarConstants::TextureInfo& textureInfo);
	void ProcessCompletedUploads();
	
	struct UploadStats 
	{
		size_t queuedUploads = 0;
		size_t completedUploads = 0;
		size_t failedUploads = 0;
		size_t pendingUploads = 0;
		float averageUploadTime = 0.0f;
		float totalUploadTime = 0.0f;
	};
	UploadStats GetStats() const;
	
	void LogUploadStatus() const;
	
	bool IsUploadComplete(const std::string& textureName) const;
	ID3D12Resource* GetUploadedTexture(const std::string& textureName) const;
	
private:
	void UploadWorkerThread();
	void UploadTextureToGPU(const UploadRequest& request);
	
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
		const UploadRequest& request,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
	
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateCubemapResource(
		const UploadRequest& request,
		const std::vector<uint8_t*>& faceData,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
	
	void CreateShaderResourceView(const CompletedTexture& texture);
	
	std::vector<std::thread> m_uploadWorkers;
	std::atomic<bool> m_shutdownRequested{false};
	std::atomic<bool> m_initialized{false};
	
	std::queue<UploadRequest> m_uploadQueue;
	std::queue<CompletedTexture> m_completedQueue;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> m_uploadedTextures;
	
	mutable std::mutex m_uploadQueueMutex;
	mutable std::mutex m_completedQueueMutex;
	mutable std::mutex m_uploadedTexturesMutex;
	mutable std::mutex m_statsMutex;
	
	ID3D12Device* m_device = nullptr;
	CommandListPool* m_uploadPool = nullptr;
	DescriptorAllocator* m_descriptorAllocator = nullptr;
	std::unique_ptr<AsyncTextureLoader> m_asyncLoader;
	
	UploadStats m_stats;
	std::atomic<UINT64> m_currentFenceValue{0};
	
	size_t m_workerThreadCount = 2;
};

} // namespace Lunar
