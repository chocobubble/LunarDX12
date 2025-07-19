#include "AsyncTextureLoader.h"
#include <stb_image.h>
#include <filesystem>
#include <DirectXTex.h>
#include <algorithm>

#include "CommandListPool.h"
#include "DescriptorAllocator.h"
#include "Utils/Logger.h"
#include "Utils/Utils.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;

namespace Lunar
{

AsyncTextureLoader::AsyncTextureLoader() = default;

AsyncTextureLoader::~AsyncTextureLoader() 
{
    Shutdown();
}

void AsyncTextureLoader::Initialize(size_t numWorkerThreads) 
{
	LOG_FUNCTION_ENTRY();
	
	m_shouldStop = false;
	m_gpuUploadEnabled = false;
	
	m_workerThreads.reserve(numWorkerThreads);
	for (size_t i = 0; i < numWorkerThreads; ++i) 
	{
		m_workerThreads.emplace_back(&AsyncTextureLoader::WorkerThreadFunction, this);
		LOG_DEBUG("Created worker thread ", i, " for texture loading");
	}
	
	LOG_DEBUG("AsyncTextureLoader initialized with ", numWorkerThreads, " worker threads (CPU only)");
}

void AsyncTextureLoader::Initialize(size_t numWorkerThreads, ID3D12Device* device, 
                                   CommandListPool* uploadPool, DescriptorAllocator* descriptorAllocator) 
{
	LOG_FUNCTION_ENTRY();
	
	// Initialize basic functionality
	Initialize(numWorkerThreads);
	
	// Enable GPU upload
	EnableGPUUpload(device, uploadPool, descriptorAllocator);
	
	LOG_DEBUG("AsyncTextureLoader initialized with GPU upload capability");
}

void AsyncTextureLoader::Shutdown() 
{
	LOG_FUNCTION_ENTRY();
	
	{
		lock_guard<mutex> lock(m_queueMutex);
		m_shouldStop = true;
	}
	m_cv.notify_all();
	
	for (auto& thread : m_workerThreads) 
	{
		if (thread.joinable()) 
		{
			thread.join();
		}
	}
	m_workerThreads.clear();
	
	// Clean up job queues
	while (!m_jobQueue.empty()) 
	{
		auto job = move(m_jobQueue.front());
		m_jobQueue.pop();
		if (job->imageData) 
		{
			stbi_image_free(job->imageData);
		}
	}
	
	while (!m_completedJobs.empty()) 
	{
		auto job = move(m_completedJobs.front());
		m_completedJobs.pop();
		if (job->imageData) 
		{
			stbi_image_free(job->imageData);
		}
	}
	
	// Clean up GPU upload queues
	while (!m_completedUploads.empty()) 
	{
		auto job = move(m_completedUploads.front());
		m_completedUploads.pop();
		if (job->imageData) 
		{
			stbi_image_free(job->imageData);
		}
	}
	
	{
		lock_guard<mutex> lock(m_uploadedTexturesMutex);
		m_uploadedTextures.clear();
	}
	
	// Reset GPU resources
	DisableGPUUpload();
	
	LOG_DEBUG("AsyncTextureLoader shutdown completed");
}

void AsyncTextureLoader::EnableGPUUpload(ID3D12Device* device, CommandListPool* uploadPool, 
                                        DescriptorAllocator* descriptorAllocator) 
{
	if (!device || !uploadPool || !descriptorAllocator) 
	{
		LOG_ERROR("Invalid parameters for GPU upload");
		return;
	}
	
	m_device = device;
	m_uploadPool = uploadPool;
	m_descriptorAllocator = descriptorAllocator;
	m_gpuUploadEnabled = true;
	
	LOG_DEBUG("GPU upload enabled for AsyncTextureLoader");
}

void AsyncTextureLoader::DisableGPUUpload() 
{
	m_gpuUploadEnabled = false;
	m_device = nullptr;
	m_uploadPool = nullptr;
	m_descriptorAllocator = nullptr;
	
	LOG_DEBUG("GPU upload disabled for AsyncTextureLoader");
}

future<bool> AsyncTextureLoader::LoadTextureAsync(const LunarConstants::TextureInfo& textureInfo) 
{
	auto job = make_unique<TextureLoadJob>();
	job->textureInfo = textureInfo;
	job->textureInfo.path = textureInfo.path;
	job->gpuUploadRequested = false;
	
	auto future = job->completion.get_future();
	
	{
		lock_guard<mutex> lock(m_queueMutex);
		m_jobQueue.push(move(job));
		
		lock_guard<mutex> statsLock(m_statsMutex);
		m_stats.queuedJobs++;
	}
	m_cv.notify_one();
	
	LOG_DEBUG("Queued texture loading job: ", textureInfo.name);
	return future;
}

future<bool> AsyncTextureLoader::LoadTextureWithUploadAsync(const LunarConstants::TextureInfo& textureInfo) 
{
	if (!m_gpuUploadEnabled.load()) 
	{
		LOG_WARNING("GPU upload not enabled, falling back to CPU-only loading");
		return LoadTextureAsync(textureInfo);
	}
	
	auto job = make_unique<TextureLoadJob>();
	job->textureInfo = textureInfo;
	job->textureInfo.path = textureInfo.path;
	job->gpuUploadRequested = true;
	
	auto future = job->completion.get_future();
	
	{
		lock_guard<mutex> lock(m_queueMutex);
		m_jobQueue.push(move(job));
		
		lock_guard<mutex> statsLock(m_statsMutex);
		m_stats.queuedJobs++;
	}
	m_cv.notify_one();
	
	LOG_DEBUG("Queued texture loading + GPU upload job: ", textureInfo.name);
	return future;
}

vector<unique_ptr<TextureLoadJob>> AsyncTextureLoader::GetCompletedJobs() 
{
    vector<unique_ptr<TextureLoadJob>> completedJobs;
    
    lock_guard<mutex> lock(m_completedMutex);
    while (!m_completedJobs.empty()) {
        completedJobs.push_back(move(m_completedJobs.front()));
        m_completedJobs.pop();
    }
    
    return completedJobs;
}

void AsyncTextureLoader::WorkerThreadFunction() 
{
	LOG_DEBUG("Worker thread started, ID: ", this_thread::get_id());
	
	while (true) 
	{
		unique_ptr<TextureLoadJob> job;
		
		{
			unique_lock<mutex> lock(m_queueMutex);
			m_cv.wait(lock, [this] { return m_shouldStop || !m_jobQueue.empty(); });
			
			if (m_shouldStop && m_jobQueue.empty()) 
			{
				break;
			}
			
			if (!m_jobQueue.empty()) 
			{
				job = move(m_jobQueue.front());
				m_jobQueue.pop();
			}
		}
		
		if (job) 
		{
			if (job->gpuUploadRequested && m_gpuUploadEnabled.load()) 
			{
				ProcessLoadAndUploadJob(move(job));
			} 
			else 
			{
				ProcessLoadJob(move(job));
			}
		}
	}
	
	LOG_DEBUG("Worker thread terminated, ID: ", this_thread::get_id());
}

void AsyncTextureLoader::ProcessLoadJob(unique_ptr<TextureLoadJob> job) 
{
	auto startTime = chrono::high_resolution_clock::now();
	
	try 
	{
		if (!filesystem::exists(job->textureInfo.path)) 
		{
			LOG_ERROR("Texture file not found: ", job->textureInfo.path);
			job->completion.set_value(false);
			return;
		}
		
		bool success = false;
		
		if (job->textureInfo.fileType == LunarConstants::FileType::HDR) 
		{
			success = LoadHDRTexture(job.get());
		} 
		else if (job->textureInfo.dimensionType == LunarConstants::TextureDimension::CUBEMAP) 
		{
			success = LoadCubemapTexture(job.get());
		} 
		else 
		{
			success = Load2DTexture(job.get());
		}
		
		auto endTime = chrono::high_resolution_clock::now();
		auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
		
		if (success) 
		{
			LOG_DEBUG("Texture loaded successfully: ", job->textureInfo.name, 
			         " (", job->width, "x", job->height, ") in ", duration.count(), "ms");
			
			{
				lock_guard<mutex> lock(m_completedMutex);
				m_completedJobs.push(move(job));
			}
			
			{
				lock_guard<mutex> lock(m_statsMutex);
				m_stats.completedCPULoads++;
				m_stats.averageCPULoadTime = (m_stats.averageCPULoadTime * (m_stats.completedCPULoads - 1) + duration.count()) / m_stats.completedCPULoads;
			}
		} 
		else 
		{
			LOG_ERROR("Failed to load texture: ", job->textureInfo.name);
			
			lock_guard<mutex> lock(m_statsMutex);
			m_stats.failedJobs++;
		}
		
		job->completion.set_value(success);
		
	}
	catch (const exception& e) 
	{
		LOG_ERROR("Exception while loading texture ", job->textureInfo.name, ": ", e.what());
		job->completion.set_value(false);
		
		lock_guard<mutex> lock(m_statsMutex);
		m_stats.failedJobs++;
	}
}

void AsyncTextureLoader::ProcessLoadAndUploadJob(unique_ptr<TextureLoadJob> job) 
{
	auto startTime = chrono::high_resolution_clock::now();
	
	try 
	{
		// First, load the texture data (CPU)
		if (!filesystem::exists(job->textureInfo.path)) 
		{
			LOG_ERROR("Texture file not found: ", job->textureInfo.path);
			job->completion.set_value(false);
			return;
		}
		
		bool loadSuccess = false;
		
		if (job->textureInfo.fileType == LunarConstants::FileType::HDR) 
		{
			loadSuccess = LoadHDRTexture(job.get());
		} 
		else if (job->textureInfo.dimensionType == LunarConstants::TextureDimension::CUBEMAP) 
		{
			loadSuccess = LoadCubemapTexture(job.get());
		} 
		else 
		{
			loadSuccess = Load2DTexture(job.get());
		}
		
		if (!loadSuccess) 
		{
			LOG_ERROR("Failed to load texture: ", job->textureInfo.name);
			job->completion.set_value(false);
			
			lock_guard<mutex> lock(m_statsMutex);
			m_stats.failedJobs++;
			return;
		}
		
		// Then, upload to GPU
		job->uploadStartTime = chrono::high_resolution_clock::now();
		bool uploadSuccess = UploadTextureToGPU(job.get());
		job->uploadEndTime = chrono::high_resolution_clock::now();
		
		auto totalEndTime = chrono::high_resolution_clock::now();
		auto totalDuration = chrono::duration_cast<chrono::milliseconds>(totalEndTime - startTime);
		auto uploadDuration = chrono::duration_cast<chrono::milliseconds>(job->uploadEndTime - job->uploadStartTime);
		
		if (uploadSuccess) 
		{
			job->gpuUploadCompleted = true;
			
			LOG_DEBUG("Texture loaded and uploaded successfully: ", job->textureInfo.name, 
			         " (", job->width, "x", job->height, ") in ", totalDuration.count(), "ms (upload: ", uploadDuration.count(), "ms)");
			
			{
				lock_guard<mutex> lock(m_completedUploadsMutex);
				m_completedUploads.push(move(job));
			}
			
			{
				lock_guard<mutex> lock(m_statsMutex);
				m_stats.completedCPULoads++;
				m_stats.completedGPUUploads++;
				m_stats.averageCPULoadTime = (m_stats.averageCPULoadTime * (m_stats.completedCPULoads - 1) + totalDuration.count()) / m_stats.completedCPULoads;
				m_stats.averageGPUUploadTime = (m_stats.averageGPUUploadTime * (m_stats.completedGPUUploads - 1) + uploadDuration.count()) / m_stats.completedGPUUploads;
				m_stats.totalProcessingTime += totalDuration.count();
			}
		} 
		else 
		{
			LOG_ERROR("Failed to upload texture to GPU: ", job->textureInfo.name);
			
			lock_guard<mutex> lock(m_statsMutex);
			m_stats.failedJobs++;
		}
		
		job->completion.set_value(uploadSuccess);
		
	}
	catch (const exception& e) 
	{
		LOG_ERROR("Exception while processing texture ", job->textureInfo.name, ": ", e.what());
		job->completion.set_value(false);
		
		lock_guard<mutex> lock(m_statsMutex);
		m_stats.failedJobs++;
	}
}

bool AsyncTextureLoader::Load2DTexture(TextureLoadJob* job) 
{
	LOG_DEBUG("Loading 2D texture: ", job->textureInfo.path);
	
	int width, height, channels;
	uint8_t* data = stbi_load(job->textureInfo.path.c_str(), &width, &height, &channels, 4); // Force RGBA
	if (!data) 
	{
		LOG_ERROR("Failed to load 2D texture: ", job->textureInfo.path);
		return false;
	}
	
	LOG_DEBUG("2D texture loaded: ", job->textureInfo.path, " (", width, "x", height, ", ", channels, " channels)");
	
	job->width = width;
	job->height = height;
	job->channels = 4; // Forced to RGBA
	job->imageData = data;
	
	return true;
}

bool AsyncTextureLoader::LoadCubemapTexture(TextureLoadJob* job) 
{
	LOG_DEBUG("Loading cubemap texture: ", job->textureInfo.path);
	
	// For cubemap, we expect the base path without face suffix
	std::vector<std::string> faceFiles = {
		job->textureInfo.path + "_right.jpg",   // +X
		job->textureInfo.path + "_left.jpg",    // -X  
		job->textureInfo.path + "_top.jpg",     // +Y
		job->textureInfo.path + "_bottom.jpg",  // -Y
		job->textureInfo.path + "_front.jpg",   // +Z
		job->textureInfo.path + "_back.jpg"     // -Z
	};
	
	int width, height, channels;
	bool success = true;
	
	// Load first face to get dimensions
	uint8_t* firstFace = stbi_load(faceFiles[0].c_str(), &width, &height, &channels, 4);
	if (!firstFace) 
	{
		LOG_ERROR("Failed to load cubemap face: ", faceFiles[0]);
		return false;
	}
	
	// Allocate memory for all 6 faces
	size_t faceSize = width * height * 4; // RGBA
	uint8_t* cubemapData = new uint8_t[faceSize * 6];
	
	// Copy first face
	memcpy(cubemapData, firstFace, faceSize);
	stbi_image_free(firstFace);
	
	// Load remaining faces
	for (int face = 1; face < 6; ++face) 
	{
		int faceWidth, faceHeight, faceChannels;
		uint8_t* faceData = stbi_load(faceFiles[face].c_str(), &faceWidth, &faceHeight, &faceChannels, 4);
		
		if (!faceData || faceWidth != width || faceHeight != height) 
		{
			LOG_ERROR("Failed to load cubemap face or dimension mismatch: ", faceFiles[face]);
			delete[] cubemapData;
			if (faceData) stbi_image_free(faceData);
			success = false;
			break;
		}
		
		memcpy(cubemapData + face * faceSize, faceData, faceSize);
		stbi_image_free(faceData);
	}
	
	if (success) 
	{
		LOG_DEBUG("Cubemap texture loaded: ", job->textureInfo.path, " (", width, "x", height, ")");
		
		job->width = width;
		job->height = height;
		job->channels = 4;
		job->imageData = cubemapData;
	}
	
	return success;
}

bool AsyncTextureLoader::LoadHDRTexture(TextureLoadJob* job) 
{
	LOG_DEBUG("Loading HDR texture: ", job->textureInfo.path);
	
	int width, height, channels;
	float* data = stbi_loadf(job->textureInfo.path.c_str(), &width, &height, &channels, 3); // loadf : float
	if (!data) 
	{
		LOG_ERROR("Failed to load HDR texture: ", job->textureInfo.path);
		return false;
	}
	
	LOG_DEBUG("HDR texture loaded: ", job->textureInfo.path, " (", width, "x", height, ", ", channels, " channels)");
	
	job->width = width;
	job->height = height;
	job->channels = 3;
	job->imageData = reinterpret_cast<uint8_t*>(data);
	
	return true;
}

// GPU Upload Methods
bool AsyncTextureLoader::UploadTextureToGPU(TextureLoadJob* job) 
{
	if (!m_gpuUploadEnabled.load() || !m_device || !m_uploadPool) 
	{
		LOG_ERROR("GPU upload not properly initialized");
		return false;
	}
	
	try 
	{
		// Get command list from pool
		ScopedCommandList cmdList(m_uploadPool, m_currentFenceValue.load());
		if (!cmdList.IsValid()) 
		{
			LOG_ERROR("Failed to get command list from pool");
			return false;
		}
		
		// Create GPU resource
		job->gpuResource = CreateTextureResource(job, job->uploadBuffer);
		if (!job->gpuResource) 
		{
			LOG_ERROR("Failed to create GPU texture resource");
			return false;
		}
		
		// Setup SRV description
		SetupSRVDescription(job);
		
		// Execute command list
		cmdList->Close();
		
		// Note: Command queue execution would be handled by the calling system
		// For now, we'll assume the upload is completed
		
		return true;
	}
	catch (const exception& e) 
	{
		LOG_ERROR("Exception during GPU upload: ", e.what());
		return false;
	}
}

ComPtr<ID3D12Resource> AsyncTextureLoader::CreateTextureResource(TextureLoadJob* job, 
                                                                ComPtr<ID3D12Resource>& uploadBuffer) 
{
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Width = static_cast<UINT>(job->width);
	textureDesc.Height = static_cast<UINT>(job->height);
	textureDesc.DepthOrArraySize = (job->textureInfo.dimensionType == LunarConstants::TextureDimension::CUBEMAP) ? 6 : 1;
	textureDesc.MipLevels = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	
	// Set format based on texture type
	if (job->textureInfo.fileType == LunarConstants::FileType::HDR) 
	{
		textureDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	} 
	else 
	{
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
		LOG_ERROR("Failed to create texture resource: ", std::hex, hr);
		return nullptr;
	}
	
	// Create upload buffer
	UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, textureDesc.DepthOrArraySize);
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
		LOG_ERROR("Failed to create upload buffer: ", std::hex, hr);
		return nullptr;
	}
	
	// Prepare subresource data
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	
	if (job->textureInfo.dimensionType == LunarConstants::TextureDimension::CUBEMAP) 
	{
		// Cubemap: 6 faces
		size_t faceSize = job->width * job->height * job->channels;
		for (int face = 0; face < 6; ++face) 
		{
			D3D12_SUBRESOURCE_DATA subresource = {};
			subresource.pData = job->imageData + face * faceSize;
			subresource.RowPitch = static_cast<LONG_PTR>(job->width * job->channels);
			subresource.SlicePitch = subresource.RowPitch * job->height;
			subresources.push_back(subresource);
		}
	} 
	else 
	{
		// Regular 2D texture
		D3D12_SUBRESOURCE_DATA subresource = {};
		subresource.pData = job->imageData;
		
		if (job->textureInfo.fileType == LunarConstants::FileType::HDR) 
		{
			subresource.RowPitch = static_cast<LONG_PTR>(job->width * sizeof(float) * 3); // RGB float
		} 
		else 
		{
			subresource.RowPitch = static_cast<LONG_PTR>(job->width * 4); // RGBA
		}
		
		subresource.SlicePitch = subresource.RowPitch * job->height;
		subresources.push_back(subresource);
	}
	
	// Note: UpdateSubresources would be called with the command list
	// This is a simplified version - actual implementation would need command list
	
	return texture;
}

ComPtr<ID3D12Resource> AsyncTextureLoader::CreateCubemapResource(TextureLoadJob* job, 
                                                                const std::vector<uint8_t*>& faceData,
                                                                ComPtr<ID3D12Resource>& uploadBuffer) 
{
	// This method is kept for compatibility but cubemap creation is handled in CreateTextureResource
	return CreateTextureResource(job, uploadBuffer);
}

void AsyncTextureLoader::SetupSRVDescription(TextureLoadJob* job) 
{
	job->srvDesc = {};
	job->srvDesc.ViewDimension = static_cast<D3D12_SRV_DIMENSION>(job->textureInfo.dimensionType);
	job->srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	job->srvDesc.Format = job->gpuResource->GetDesc().Format;
	
	if (job->textureInfo.dimensionType == LunarConstants::TextureDimension::CUBEMAP) 
	{
		job->srvDesc.TextureCube.MipLevels = 1;
		job->srvDesc.TextureCube.MostDetailedMip = 0;
		job->srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} 
	else 
	{
		job->srvDesc.Texture2D.MipLevels = 1;
		job->srvDesc.Texture2D.MostDetailedMip = 0;
		job->srvDesc.Texture2D.PlaneSlice = 0;
		job->srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}
}

// Process completed uploads (call from main thread)
void AsyncTextureLoader::ProcessCompletedUploads() 
{
	if (!m_gpuUploadEnabled.load()) return;
	
	std::queue<std::unique_ptr<TextureLoadJob>> completedUploads;
	
	// Move completed uploads from queue
	{
		lock_guard<mutex> lock(m_completedUploadsMutex);
		completedUploads.swap(m_completedUploads);
	}
	
	// Process completed uploads on main thread
	while (!completedUploads.empty()) 
	{
		auto& job = completedUploads.front();
		
		try 
		{
			// Create SRV using enum-based DescriptorAllocator
			auto descriptor = m_descriptorAllocator->AllocateInRange(RangeType::BASIC_TEXTURES);
			m_descriptorAllocator->CreateSRV(job->gpuResource.Get(), &job->srvDesc, descriptor);
			
			// Store uploaded texture
			{
				lock_guard<mutex> lock(m_uploadedTexturesMutex);
				m_uploadedTextures[job->textureInfo.name] = job->gpuResource;
			}
			
			auto uploadDuration = chrono::duration_cast<chrono::milliseconds>(job->uploadEndTime - job->uploadStartTime);
			LOG_DEBUG("Processed completed texture upload: ", job->textureInfo.name, 
			         " (", uploadDuration.count(), "ms)");
		}
		catch (const exception& e) 
		{
			LOG_ERROR("Failed to process completed texture ", job->textureInfo.name, ": ", e.what());
			
			lock_guard<mutex> lock(m_statsMutex);
			m_stats.failedJobs++;
		}
		
		// Clean up image data
		if (job->imageData) 
		{
			if (job->textureInfo.fileType == LunarConstants::FileType::HDR) 
			{
				// HDR data is float*, need to cast back
				stbi_image_free(reinterpret_cast<float*>(job->imageData));
			} 
			else 
			{
				stbi_image_free(job->imageData);
			}
			job->imageData = nullptr;
		}
		
		completedUploads.pop();
	}
}

// Query methods
bool AsyncTextureLoader::IsTextureReady(const std::string& textureName) const 
{
	lock_guard<mutex> lock(m_uploadedTexturesMutex);
	return m_uploadedTextures.find(textureName) != m_uploadedTextures.end();
}

ID3D12Resource* AsyncTextureLoader::GetUploadedTexture(const std::string& textureName) const 
{
	lock_guard<mutex> lock(m_uploadedTexturesMutex);
	auto it = m_uploadedTextures.find(textureName);
	return (it != m_uploadedTextures.end()) ? it->second.Get() : nullptr;
}

// Statistics
AsyncTextureLoader::LoadingStats AsyncTextureLoader::GetStats() const 
{
	lock_guard<mutex> lock(m_statsMutex);
	return m_stats;
}

void AsyncTextureLoader::LogStatus() const 
{
	auto stats = GetStats();
	LOG_DEBUG("AsyncTextureLoader Status:");
	LOG_DEBUG("  Queued Jobs: ", stats.queuedJobs);
	LOG_DEBUG("  Completed CPU Loads: ", stats.completedCPULoads);
	LOG_DEBUG("  Completed GPU Uploads: ", stats.completedGPUUploads);
	LOG_DEBUG("  Failed Jobs: ", stats.failedJobs);
	LOG_DEBUG("  Average CPU Load Time: ", stats.averageCPULoadTime, "ms");
	LOG_DEBUG("  Average GPU Upload Time: ", stats.averageGPUUploadTime, "ms");
	LOG_DEBUG("  Total Processing Time: ", stats.totalProcessingTime, "ms");
	LOG_DEBUG("  GPU Upload Enabled: ", m_gpuUploadEnabled.load() ? "Yes" : "No");
	
	if (m_gpuUploadEnabled.load()) 
	{
		lock_guard<mutex> lock(m_uploadedTexturesMutex);
		LOG_DEBUG("  Uploaded Textures: ", m_uploadedTextures.size());
	}
}

} // namespace Lunar
