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

AsyncTextureLoader::~AsyncTextureLoader() { Shutdown(); }

void AsyncTextureLoader::Initialize(size_t numWorkerThreads, ID3D12Device* device, 
                                   CommandListPool* uploadPool, DescriptorAllocator* descriptorAllocator,
                                   ID3D12CommandQueue* commandQueue, ID3D12Fence* fence) 
{
	LOG_FUNCTION_ENTRY();
	
	if (!device || !uploadPool || !descriptorAllocator || !commandQueue || !fence) 
	{
		LOG_ERROR("Invalid parameters for GPU upload initialization");
		return;
	}
	
	m_device = device;
	m_uploadPool = uploadPool;
	m_descriptorAllocator = descriptorAllocator;
	m_commandQueue = commandQueue;
	m_fence = fence;
	m_gpuUploadEnabled = true;
	
	m_workerThreads.reserve(numWorkerThreads);
	for (size_t i = 0; i < numWorkerThreads; ++i) 
	{
		m_workerThreads.emplace_back(&AsyncTextureLoader::WorkerThreadFunction, this);
		LOG_DEBUG("Created texture loading worker thread ", i);
	}
	
	LOG_DEBUG("AsyncTextureLoader initialized with ", numWorkerThreads, " worker threads and GPU upload capability");
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
	
	while (!m_completedTextures.empty()) 
	{
		auto job = move(m_completedTextures.front());
		m_completedTextures.pop();
		if (job->imageData) 
		{
			stbi_image_free(job->imageData);
		}
	}
	
	{
		lock_guard<mutex> lock(m_loadedTexturesMutex);
		m_textures.clear();
	}
	
	m_device = nullptr;
	m_uploadPool = nullptr;
	m_descriptorAllocator = nullptr;
	m_commandQueue = nullptr;
	
	LOG_DEBUG("AsyncTextureLoader shutdown completed");
}

future<bool> AsyncTextureLoader::LoadTextureAsync(const LunarConstants::TextureInfo& textureInfo) 
{
    // For tracking loading progress
	m_totalTextures.fetch_add(1);
	
	auto job = make_unique<TextureLoadJob>();
	job->textureInfo = textureInfo;
	job->textureInfo.path = textureInfo.path;
	
	auto future = job->completion.get_future();
	
	{
		lock_guard<mutex> lock(m_queueMutex);
		m_jobQueue.push(move(job));
		m_stats.queuedJobs++;
	}
	m_cv.notify_one();
	
	LOG_DEBUG("Queued texture loading job: ", textureInfo.name);
	         
	return future;
}

bool AsyncTextureLoader::IsInitialized() const 
{
    if (!m_device || !m_uploadPool || !m_descriptorAllocator || !m_commandQueue) 
    {
        LOG_ERROR("AsyncTextureLoader is not properly initialized");
        return false;
    }
    return true;
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
                m_stats.queuedJobs--;
			}
		}
		
		if (job) 
		{
            ProcessLoadAndUploadJob(move(job));
		}
	}
	
	LOG_DEBUG("Worker thread terminated, ID: ", this_thread::get_id());
}

void AsyncTextureLoader::ProcessLoadAndUploadJob(unique_ptr<TextureLoadJob> job) 
{
	
	job->loadStartTime = chrono::high_resolution_clock::now();

	try 
	{
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
		else if (job->textureInfo.fileType == LunarConstants::FileType::DDS) 
		{
			loadSuccess = LoadDDSTexture(job.get());
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
		
		bool uploadSuccess = UploadTextureToGPU(job.get());

	    auto uploadStartTime = chrono::high_resolution_clock::now();
		auto uploadEndTime = chrono::high_resolution_clock::now();
		auto uploadDuration = chrono::duration_cast<chrono::milliseconds>(uploadEndTime - uploadStartTime);

		job->loadEndTime = chrono::high_resolution_clock::now();
		auto totalLoadDuration = chrono::duration_cast<chrono::milliseconds>(job->loadEndTime - job->loadStartTime);

		if (uploadSuccess) 
		{
			m_loadedTextures.fetch_add(1);
			LOG_DEBUG("Texture loaded and uploaded successfully: ", job->textureInfo.name, 
			         " (", job->width, "x", job->height, ") in ", totalLoadDuration.count(), "ms (upload: ", uploadDuration.count(), "ms)");
			
			{
				lock_guard<mutex> lock(m_statsMutex);
				m_stats.completedCPULoads++;
				m_stats.completedGPUUploads++;
				m_stats.averageCPULoadTime = (m_stats.averageCPULoadTime * (m_stats.completedCPULoads - 1) + totalLoadDuration.count()) / m_stats.completedCPULoads;
				m_stats.averageGPUUploadTime = (m_stats.averageGPUUploadTime * (m_stats.completedGPUUploads - 1) + uploadDuration.count()) / m_stats.completedGPUUploads;
				m_stats.totalProcessingTime += totalLoadDuration.count();
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
	uint8_t* data = stbi_load(job->textureInfo.path, &width, &height, &channels, 4); // Force RGBA
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

	string path = job->textureInfo.path;
	vector<string> faceFiles = {
		path + "_right.jpg",   // +X
		path + "_left.jpg",    // -X  
		path + "_top.jpg",     // +Y
		path + "_bottom.jpg",  // -Y
		path + "_front.jpg",   // +Z
		path + "_back.jpg"     // -Z
	};
	
	int width, height, channels;
	bool success = true;
	
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

bool AsyncTextureLoader::LoadDDSTexture(TextureLoadJob* job) 
{
	LOG_DEBUG("Loading DDS texture: ", job->textureInfo.path);

	string filename = job->textureInfo.path;
	std::wstring wfilename(filename.begin(), filename.end());
	HRESULT hr = DirectX::LoadFromDDSFile(wfilename.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, job->ddsImage);
	if (FAILED(hr))
	{
		LOG_ERROR("Failed to load DDS texture: ", job->textureInfo.path);
		return false;
	}
	
	const DirectX::Image* img = job->ddsImage.GetImage(0, 0, 0);
	if (!img)
	{
		LOG_ERROR("Failed to get DDS image data: ", job->textureInfo.path);
		return false;
	}
	
	LOG_DEBUG("DDS texture loaded: ", job->textureInfo.path, " (", img->width, "x", img->height, ")");
	
	job->width = static_cast<int>(img->width);
	job->height = static_cast<int>(img->height);
	job->channels = 4;
	job->format = img->format;
	job->mipLevels = static_cast<int>(job->ddsImage.GetMetadata().mipLevels);
	job->rowPitch = img->rowPitch;
	
	job->imageData = img->pixels;
	
	return true;
}

bool AsyncTextureLoader::LoadHDRTexture(TextureLoadJob* job) 
{
	LOG_DEBUG("Loading HDR texture: ", job->textureInfo.path);
	
	int width, height, channels;
	float* data = stbi_loadf(job->textureInfo.path, &width, &height, &channels, 3); // loadf : float
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

bool AsyncTextureLoader::UploadTextureToGPU(TextureLoadJob* job) 
{
    if (!IsInitialized())
		return false;

	try 
	{
		// Get command list from pool (fence management handled internally)
		ScopedCommandList cmdList(m_uploadPool);
		
		if (!cmdList.IsValid()) 
		{
			LOG_ERROR("Failed to get command list from pool");
			return false;
		}
		
		job->gpuResource = CreateTextureResource(job, job->uploadBuffer);
		if (!job->gpuResource) 
		{
			LOG_ERROR("Failed to create GPU texture resource");
			return false;
		}
		
		SetupSRVDescription(job);
		m_descriptorAllocator->CreateSRV(LunarConstants::RangeType::BASIC_TEXTURES, job->textureInfo.name, job->gpuResource.Get(), &job->srvDesc);
		
		std::queue<std::unique_ptr<TextureLoadJob>> completedTextures;
	
		{
			lock_guard<mutex> lock(m_completedTexturesMutex);
			completedTextures.swap(m_completedTextures);
		}
	
		m_textures[job->textureInfo.name] = job->gpuResource;

		// Clean up image data
		if (job->imageData) 
		{
			if (job->textureInfo.fileType == LunarConstants::FileType::HDR) 
			{
				stbi_image_free(reinterpret_cast<float*>(job->imageData));
			} 
			else if (job->textureInfo.fileType != LunarConstants::FileType::DDS) 
			{
				stbi_image_free(job->imageData);
			}
			// DDS memory is managed by ScratchImage, no manual cleanup needed
			job->imageData = nullptr;
		}
	
		cmdList->Close();
		
		ID3D12CommandList* commandLists[] = { cmdList.Get() };
		m_commandQueue->ExecuteCommandLists(1, commandLists);
		m_commandQueue->Signal(m_fence, cmdList.GetContext()->fenceValue);
		
		LOG_DEBUG("GPU upload executed for texture: ", job->textureInfo.name, 
		         " (fence value: ", cmdList.GetContext()->fenceValue, ")");
		
		return true;
	}
	catch (const exception& e) 
	{
		LOG_ERROR("Exception during GPU upload: ", e.what());
		return false;
	}
}

ComPtr<ID3D12Resource> AsyncTextureLoader::CreateTextureResource(TextureLoadJob* job, ComPtr<ID3D12Resource>& uploadBuffer) 
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
	else if (job->textureInfo.fileType == LunarConstants::FileType::DDS) 
	{
		textureDesc.Format = job->format;
	} 
	else 
	{
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	
	D3D12_HEAP_PROPERTIES defaultHeapProperties = {};
	defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	defaultHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	defaultHeapProperties.CreationNodeMask = 1;
	defaultHeapProperties.VisibleNodeMask = 1;

	ComPtr<ID3D12Resource> texture;

    THROW_IF_FAILED(m_device->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&texture)))

	// Calculate the size of the upload buffer
	UINT64 uploadBufferSize;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts;
	UINT numRows;
	UINT64 rowPitch;
	
	// Get GPU memory layout requirements for the texture
	m_device->GetCopyableFootprints(
		&textureDesc,           // Input: texture description
		0,                      // First subresource index
		1,                      // Number of subresources
		0,                      // Base offset in upload buffer
		&layouts,               // Output: memory layout with alignment
		&numRows,               // Output: number of rows in the texture
		&rowPitch,              // Output: actual bytes per row (without padding)
		&uploadBufferSize       // Output: total bytes needed for upload buffer
	);

	D3D12_HEAP_PROPERTIES uploadHeapProperties = defaultHeapProperties;
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC uploadBufferDesc = {};
	uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadBufferDesc.Width = uploadBufferSize;
	uploadBufferDesc.Height = 1;
	uploadBufferDesc.DepthOrArraySize = 1;
	uploadBufferDesc.MipLevels = 1;
	uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadBufferDesc.SampleDesc.Count = 1;
	uploadBufferDesc.SampleDesc.Quality = 0;
	uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	THROW_IF_FAILED(m_device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&uploadBuffer)))

	// Copy data to upload buffer
	void* mappedData;
	uploadBuffer->Map(0, nullptr, &mappedData);
	
	UINT64 rowSizeInBytes;
	if (job->textureInfo.fileType == LunarConstants::FileType::HDR) 
	{
		rowSizeInBytes = static_cast<UINT64>(job->width) * sizeof(float) * 3; // RGB float
	} 
	else if (job->textureInfo.fileType == LunarConstants::FileType::DDS) 
	{
		rowSizeInBytes = job->rowPitch; 
	} 
	else 
	{
		rowSizeInBytes = static_cast<UINT64>(job->width) * 4; // RGBA
	}
	
	BYTE* destSliceStart = reinterpret_cast<BYTE*>(mappedData) + layouts.Offset;
	for (UINT i = 0; i < numRows; i++) 
	{
		memcpy(
			destSliceStart + layouts.Footprint.RowPitch * i, 
			job->imageData + rowSizeInBytes * i, 
			rowSizeInBytes
		);
	}
	uploadBuffer->Unmap(0, nullptr);

	// Get command list and copy texture (fence management handled by CommandListPool)
	ScopedCommandList cmdList(m_uploadPool);
	if (!cmdList.IsValid()) 
	{
		LOG_ERROR("Failed to get command list for texture copy");
		return nullptr;
	}

	// Set up copy locations
	D3D12_TEXTURE_COPY_LOCATION destLocation = {};
	destLocation.pResource = texture.Get();
	destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	destLocation.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
	srcLocation.pResource = uploadBuffer.Get();
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcLocation.PlacedFootprint = layouts;

	cmdList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);

	// Transition texture to shader resource state
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	cmdList->ResourceBarrier(1, &barrier);

	return texture;
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

// Query methods
bool AsyncTextureLoader::IsTextureReady(const std::string& textureName) const 
{
	lock_guard<mutex> lock(m_loadedTexturesMutex);
	return m_textures.find(textureName) != m_textures.end();
}

ID3D12Resource* AsyncTextureLoader::GetTexture(const std::string& textureName) const 
{
	lock_guard<mutex> lock(m_loadedTexturesMutex);
	auto it = m_textures.find(textureName);
	return (it != m_textures.end()) ? it->second.Get() : nullptr;
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
	
	{
		lock_guard<mutex> lock(m_loadedTexturesMutex);
		LOG_DEBUG("  Loaded Textures: ", m_textures.size());
	}
}

// Simple completion tracking methods
bool AsyncTextureLoader::AreAllTexturesLoaded() const 
{
	return m_loadedTextures.load() >= m_totalTextures.load();
}

float AsyncTextureLoader::GetLoadingProgress() const 
{
	int total = m_totalTextures.load();
	if (total == 0) return 1.0f;
	return static_cast<float>(m_loadedTextures.load()) / total;
}

} // namespace Lunar
