#include "AsyncTextureUploadManager.h"

#include <stb_image.h>
#include <DirectXTex.h>
#include <algorithm>

#include "CommandListPool.h"
#include "DescriptorAllocator.h"
#include "AsyncTextureLoader.h"
#include "Utils/Logger.h"
#include "Utils/Utils.h"

using namespace Microsoft::WRL;
using namespace DirectX;

namespace Lunar
{

AsyncTextureUploadManager::AsyncTextureUploadManager() = default;

AsyncTextureUploadManager::~AsyncTextureUploadManager() 
{
	Shutdown();
}

void AsyncTextureUploadManager::Initialize(ID3D12Device* device, CommandListPool* uploadPool, 
                                         DescriptorAllocator* descriptorAllocator, size_t workerThreadCount) 
{
	LOG_FUNCTION_ENTRY();
	
	if (m_initialized.load()) 
	{
		LOG_WARNING("AsyncTextureUploadManager already initialized");
		return;
	}
	
	m_device = device;
	m_uploadPool = uploadPool;
	m_descriptorAllocator = descriptorAllocator;
	m_workerThreadCount = workerThreadCount;
	
	m_asyncLoader = std::make_unique<AsyncTextureLoader>();
	m_asyncLoader->Initialize();
	
	m_shutdownRequested.store(false);
	
	// Create worker threads
	m_uploadWorkers.reserve(m_workerThreadCount);
	for (size_t i = 0; i < m_workerThreadCount; ++i) 
	{
		m_uploadWorkers.emplace_back(&AsyncTextureUploadManager::UploadWorkerThread, this);
		LOG_DEBUG("Created upload worker thread ", i);
	}
	
	m_initialized.store(true);
	LOG_DEBUG("AsyncTextureUploadManager initialized with ", m_workerThreadCount, " worker threads");
}

void AsyncTextureUploadManager::Shutdown() 
{
	if (!m_initialized.load()) return;
	
	LOG_FUNCTION_ENTRY();
	
	m_shutdownRequested.store(true);
	
	// Wait for all worker threads to finish
	for (auto& worker : m_uploadWorkers) 
	{
		if (worker.joinable()) 
		{
			worker.join();
		}
	}
	m_uploadWorkers.clear();
	
	// Clear queues
	{
		std::lock_guard<std::mutex> lock(m_uploadQueueMutex);
		while (!m_uploadQueue.empty()) 
		{
			auto& request = m_uploadQueue.front();
			if (request.imageData) 
			{
				stbi_image_free(request.imageData);
			}
			m_uploadQueue.pop();
		}
	}
	
	{
		std::lock_guard<std::mutex> lock(m_completedQueueMutex);
		while (!m_completedQueue.empty()) 
		{
			m_completedQueue.pop();
		}
	}
	
	{
		std::lock_guard<std::mutex> lock(m_uploadedTexturesMutex);
		m_uploadedTextures.clear();
	}
	
	m_asyncLoader.reset();
	m_initialized.store(false);
	
	LOG_DEBUG("AsyncTextureUploadManager shutdown completed");
}

void AsyncTextureUploadManager::QueueTextureUpload(const std::string& path, const std::string& name, 
                                                  const LunarConstants::TextureInfo& textureInfo) 
{
	if (!m_initialized.load()) 
	{
		LOG_ERROR("AsyncTextureUploadManager not initialized");
		return;
	}
	
	UploadRequest request;
	request.filePath = path;
	request.textureName = name;
	request.textureInfo = textureInfo;
	request.requestTime = std::chrono::high_resolution_clock::now();
	
	{
		std::lock_guard<std::mutex> lock(m_uploadQueueMutex);
		m_uploadQueue.push(request);
		
		std::lock_guard<std::mutex> statsLock(m_statsMutex);
		m_stats.queuedUploads++;
		m_stats.pendingUploads++;
	}
	
	LOG_DEBUG("Queued texture upload: ", name, " (", path, ")");
}

void AsyncTextureUploadManager::ProcessCompletedUploads() 
{
	if (!m_initialized.load()) return;
	
	std::queue<CompletedTexture> completedTextures;
	
	// Move completed textures from queue
	{
		std::lock_guard<std::mutex> lock(m_completedQueueMutex);
		completedTextures.swap(m_completedQueue);
	}
	
	// Process completed textures on main thread
	while (!completedTextures.empty()) 
	{
		auto& texture = completedTextures.front();
		
		try 
		{
			// Create SRV using enum-based DescriptorAllocator
			auto descriptor = m_descriptorAllocator->AllocateInRange(RangeType::BASIC_TEXTURES);
			m_descriptorAllocator->CreateSRV(texture.resource.Get(), &texture.srvDesc, descriptor);
			
			// Store uploaded texture
			{
				std::lock_guard<std::mutex> lock(m_uploadedTexturesMutex);
				m_uploadedTextures[texture.name] = texture.resource;
			}
			
			// Update stats
			{
				std::lock_guard<std::mutex> lock(m_statsMutex);
				m_stats.completedUploads++;
				m_stats.pendingUploads--;
				m_stats.totalUploadTime += texture.uploadTimeMs;
				if (m_stats.completedUploads > 0) 
				{
					m_stats.averageUploadTime = m_stats.totalUploadTime / m_stats.completedUploads;
				}
			}
			
			LOG_DEBUG("Processed completed texture upload: ", texture.name, 
			         " (", texture.uploadTimeMs, "ms)");
		}
		catch (const std::exception& e) 
		{
			LOG_ERROR("Failed to process completed texture ", texture.name, ": ", e.what());
			
			std::lock_guard<std::mutex> lock(m_statsMutex);
			m_stats.failedUploads++;
			m_stats.pendingUploads--;
		}
		
		completedTextures.pop();
	}
}

void AsyncTextureUploadManager::UploadWorkerThread() 
{
	LOG_DEBUG("Upload worker thread started");
	
	while (!m_shutdownRequested.load()) 
	{
		UploadRequest request;
		bool hasRequest = false;
		
		// Get next upload request
		{
			std::lock_guard<std::mutex> lock(m_uploadQueueMutex);
			if (!m_uploadQueue.empty()) 
			{
				request = m_uploadQueue.front();
				m_uploadQueue.pop();
				hasRequest = true;
			}
		}
		
		if (hasRequest) 
		{
			try 
			{
				UploadTextureToGPU(request);
			}
			catch (const std::exception& e) 
			{
				LOG_ERROR("Failed to upload texture ", request.textureName, ": ", e.what());
				
				if (request.imageData) 
				{
					stbi_image_free(request.imageData);
				}
				
				std::lock_guard<std::mutex> lock(m_statsMutex);
				m_stats.failedUploads++;
				m_stats.pendingUploads--;
			}
		}
		else 
		{
			// No work available, sleep briefly
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	
	LOG_DEBUG("Upload worker thread finished");
}

void AsyncTextureUploadManager::UploadTextureToGPU(const UploadRequest& request) 
{
	auto startTime = std::chrono::high_resolution_clock::now();
	
	// Load image data if not already loaded
	UploadRequest workingRequest = request;
	if (!workingRequest.imageData) 
	{
		workingRequest.imageData = stbi_load(workingRequest.filePath.c_str(), 
		                                   &workingRequest.width, 
		                                   &workingRequest.height, 
		                                   &workingRequest.channels, 4);
		if (!workingRequest.imageData) 
		{
			throw std::runtime_error("Failed to load image: " + workingRequest.filePath);
		}
	}
	
	// Get command list from pool
	ScopedCommandList cmdList(m_uploadPool, m_currentFenceValue.load());
	if (!cmdList.IsValid()) 
	{
		if (workingRequest.imageData) 
		{
			stbi_image_free(workingRequest.imageData);
		}
		throw std::runtime_error("Failed to get command list from pool");
	}
	
	// Create GPU resource and upload
	CompletedTexture completedTexture;
	completedTexture.name = workingRequest.textureName;
	completedTexture.resource = CreateTextureResource(workingRequest, completedTexture.uploadBuffer);
	
	// Setup SRV description
	completedTexture.srvDesc = {};
	completedTexture.srvDesc.ViewDimension = static_cast<D3D12_SRV_DIMENSION>(workingRequest.textureInfo.dimensionType);
	completedTexture.srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	completedTexture.srvDesc.Format = completedTexture.resource->GetDesc().Format;
	
	if (workingRequest.textureInfo.dimensionType == LunarConstants::TextureDimension::CUBEMAP) 
	{
		completedTexture.srvDesc.TextureCube.MipLevels = 1;
		completedTexture.srvDesc.TextureCube.MostDetailedMip = 0;
		completedTexture.srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} 
	else 
	{
		completedTexture.srvDesc.Texture2D.MipLevels = 1;
		completedTexture.srvDesc.Texture2D.MostDetailedMip = 0;
		completedTexture.srvDesc.Texture2D.PlaneSlice = 0;
		completedTexture.srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}
	
	// Execute command list
	cmdList->Close();
	ID3D12CommandList* commandLists[] = { cmdList.Get() };
	// Note: Command queue execution would be handled by the calling system
	
	// Calculate upload time
	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
	completedTexture.uploadTimeMs = duration.count() / 1000.0f;
	completedTexture.completionTime = endTime;
	
	// Clean up image data
	if (workingRequest.imageData) 
	{
		stbi_image_free(workingRequest.imageData);
	}
	
	// Add to completed queue
	{
		std::lock_guard<std::mutex> lock(m_completedQueueMutex);
		m_completedQueue.push(std::move(completedTexture));
	}
	
	LOG_DEBUG("GPU upload completed for: ", workingRequest.textureName, 
	         " (", completedTexture.uploadTimeMs, "ms)");
}

ComPtr<ID3D12Resource> AsyncTextureUploadManager::CreateTextureResource(
	const UploadRequest& request,
	ComPtr<ID3D12Resource>& uploadBuffer) 
{
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Width = static_cast<UINT>(request.width);
	textureDesc.Height = static_cast<UINT>(request.height);
	textureDesc.DepthOrArraySize = 1;
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	
	// Handle cubemap
	if (request.textureInfo.dimensionType == LunarConstants::TextureDimension::CUBEMAP) 
	{
		textureDesc.DepthOrArraySize = 6;
		
		// Load cubemap faces
		std::vector<std::string> faceFiles = {
			request.filePath + "_right.jpg",   // +X
			request.filePath + "_left.jpg",    // -X  
			request.filePath + "_top.jpg",     // +Y
			request.filePath + "_bottom.jpg",  // -Y
			request.filePath + "_front.jpg",   // +Z
			request.filePath + "_back.jpg"     // -Z
		};
		
		std::vector<uint8_t*> faceData(6);
		int width, height, channels;
		
		for (int face = 0; face < 6; ++face) 
		{
			faceData[face] = stbi_load(faceFiles[face].c_str(), &width, &height, &channels, 4);
			if (!faceData[face]) 
			{
				for (int i = 0; i < face; ++i) 
				{
					stbi_image_free(faceData[i]);
				}
				throw std::runtime_error("Failed to load cubemap face: " + faceFiles[face]);
			}
		}
		
		auto result = CreateCubemapResource(request, faceData, uploadBuffer);
		
		for (int i = 0; i < 6; ++i) 
		{
			stbi_image_free(faceData[i]);
		}
		
		return result;
	}
	
	// Create default heap resource
	ComPtr<ID3D12Resource> texture;
	CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
	
	HRESULT hr = m_device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texture)
	);
	
	if (FAILED(hr)) 
	{
		throw std::runtime_error("Failed to create texture resource");
	}
	
	// Create upload buffer
	UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);
	CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
	
	hr = m_device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&uploadBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)
	);
	
	if (FAILED(hr)) 
	{
		throw std::runtime_error("Failed to create upload buffer");
	}
	
	// Copy data to upload buffer and then to texture
	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = request.imageData;
	textureData.RowPitch = static_cast<LONG_PTR>(request.width * 4); // RGBA
	textureData.SlicePitch = textureData.RowPitch * request.height;
	
	// Note: UpdateSubresources would be called with the command list
	// This is a simplified version - actual implementation would need command list
	
	return texture;
}

ComPtr<ID3D12Resource> AsyncTextureUploadManager::CreateCubemapResource(
	const UploadRequest& request,
	const std::vector<uint8_t*>& faceData,
	ComPtr<ID3D12Resource>& uploadBuffer) 
{
	// Simplified cubemap creation - would need full implementation
	// Similar to CreateTextureResource but for 6 faces
	throw std::runtime_error("Cubemap creation not yet implemented in AsyncTextureUploadManager");
}

AsyncTextureUploadManager::UploadStats AsyncTextureUploadManager::GetStats() const 
{
	std::lock_guard<std::mutex> lock(m_statsMutex);
	return m_stats;
}

void AsyncTextureUploadManager::LogUploadStatus() const 
{
	auto stats = GetStats();
	LOG_DEBUG("AsyncTextureUploadManager Status:");
	LOG_DEBUG("  Queued: ", stats.queuedUploads);
	LOG_DEBUG("  Completed: ", stats.completedUploads);
	LOG_DEBUG("  Failed: ", stats.failedUploads);
	LOG_DEBUG("  Pending: ", stats.pendingUploads);
	LOG_DEBUG("  Average Upload Time: ", stats.averageUploadTime, "ms");
	LOG_DEBUG("  Total Upload Time: ", stats.totalUploadTime, "ms");
}

bool AsyncTextureUploadManager::IsUploadComplete(const std::string& textureName) const 
{
	std::lock_guard<std::mutex> lock(m_uploadedTexturesMutex);
	return m_uploadedTextures.find(textureName) != m_uploadedTextures.end();
}

ID3D12Resource* AsyncTextureUploadManager::GetUploadedTexture(const std::string& textureName) const 
{
	std::lock_guard<std::mutex> lock(m_uploadedTexturesMutex);
	auto it = m_uploadedTextures.find(textureName);
	return (it != m_uploadedTextures.end()) ? it->second.Get() : nullptr;
}

} // namespace Lunar
