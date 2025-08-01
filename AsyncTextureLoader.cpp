#include "AsyncTextureLoader.h"
#include <stb_image.h>
#include <filesystem>
#include <DirectXTex.h>
#include <DirectXPackedVector.h>
#include <algorithm>
#include <cmath>
#include <string>

#include "CommandListPool.h"
#include "DescriptorAllocator.h"
#include "PipelineStateManager.h"
#include "Utils/Logger.h"
#include "Utils/Utils.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;

namespace Lunar
{

TextureLoadJob::~TextureLoadJob()
{
	delete[] imageData;
	delete[] hdrImageData;
	for (auto& faceData : cubemapFaceData) 
	{
		delete[] faceData;
	}
}

AsyncTextureLoader::AsyncTextureLoader() = default;

AsyncTextureLoader::~AsyncTextureLoader() { Shutdown(); }

void AsyncTextureLoader::Initialize(size_t numWorkerThreads, ID3D12Device* device, 
                                   CommandListPool* uploadPool, DescriptorAllocator* descriptorAllocator,
                                   ID3D12CommandQueue* commandQueue, ID3D12Fence* fence,
                                   PipelineStateManager* pipelineStateManager) 
{
	LOG_FUNCTION_ENTRY();
	
	if (!device || !uploadPool || !descriptorAllocator || !commandQueue || !fence || !pipelineStateManager) 
	{
		LOG_ERROR("Invalid parameters for GPU upload initialization");
		return;
	}
	
	m_device = device;
	m_uploadPool = uploadPool;
	m_descriptorAllocator = descriptorAllocator;
	m_commandQueue = commandQueue;
	m_fence = fence;
	m_pipelineStateManager = pipelineStateManager;
	m_gpuUploadEnabled = true;
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	
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
    if (!m_device || !m_uploadPool || !m_descriptorAllocator || !m_commandQueue || !m_pipelineStateManager) 
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
		if (job->textureInfo.fileType == LunarConstants::FileType::HDR)
		{
			LOG_DEBUG("Processing HDR texture: ", job->textureInfo.name);
			bool success = ProcessHDRTexture(job.get());
			
			job->loadEndTime = chrono::high_resolution_clock::now();
			auto totalLoadDuration = chrono::duration_cast<chrono::milliseconds>(job->loadEndTime - job->loadStartTime);
			
			job->completion.set_value(success);
			
			if (success) {
				m_loadedTextures.fetch_add(1);
				LOG_DEBUG("HDR texture processing completed: ", job->textureInfo.name, " (", totalLoadDuration.count(), "ms)");
				lock_guard<mutex> lock(m_statsMutex);
			} else {
				LOG_ERROR("Failed to process HDR texture: ", job->textureInfo.name);
				lock_guard<mutex> lock(m_statsMutex);
				m_stats.failedJobs++;
			}
			return;
		}
		
		if (!filesystem::exists(job->textureInfo.path)) 
		{
			LOG_ERROR("Texture file not found: ", job->textureInfo.path);
			job->completion.set_value(false);
			lock_guard<mutex> lock(m_statsMutex);
			m_stats.failedJobs++;
			return;
		}
		
		bool success = UploadTextureToGPU(job.get());
		
		job->loadEndTime = chrono::high_resolution_clock::now();
		auto totalLoadDuration = chrono::duration_cast<chrono::milliseconds>(job->loadEndTime - job->loadStartTime);
		
		job->completion.set_value(success);
		
		if (success) 
		{
			m_loadedTextures.fetch_add(1);
			LOG_DEBUG("Texture processing completed: ", job->textureInfo.name, " (", totalLoadDuration.count(), "ms)");
			lock_guard<mutex> lock(m_statsMutex);
		} 
		else 
		{
			LOG_ERROR("Failed to process texture: ", job->textureInfo.name);
			lock_guard<mutex> lock(m_statsMutex);
			m_stats.failedJobs++;
		}
	}
	catch (const exception& e)
	{
		LOG_ERROR("Exception during texture processing: ", e.what(), " for texture: ", job->textureInfo.name);
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
	wstring wfilename(filename.begin(), filename.end());
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
	job->hdrImageData = data;
	
	return true;
}

bool AsyncTextureLoader::UploadTextureToGPU(TextureLoadJob* job) 
{
    if (!IsInitialized())
		return false;

	try 
	{
		ScopedCommandList cmdList(m_uploadPool);
		
		if (!cmdList.IsValid()) 
		{
			LOG_ERROR("Failed to get command list from pool");
			return false;
		}
		
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
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		
		job->gpuResource = CreateTextureResource(job, textureDesc, job->uploadBuffer, cmdList.Get());
		if (!job->gpuResource) 
		{
			LOG_ERROR("Failed to create GPU texture resource");
			return false;
		}
		
		SetupSRVDescription(job);
		
		m_descriptorAllocator->CreateSRVAtRangeIndex(job->textureInfo.rangeType,
		                                            job->textureInfo.registerIndex,
		                                            job->textureInfo.name,
		                                            job->gpuResource.Get(),
		                                            &job->srvDesc);
		
		queue<unique_ptr<TextureLoadJob>> completedTextures;
	
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

		const UINT64 currentFenceValue = cmdList.GetFenceValue();
		m_commandQueue->Signal(m_fence, currentFenceValue);

		// wait for completing current frame
		if (m_fence->GetCompletedValue() < currentFenceValue)
		{
			THROW_IF_FAILED(m_fence->SetEventOnCompletion(currentFenceValue, m_fenceEvent))
			WaitForSingleObject(m_fenceEvent, INFINITE);
		}
		
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

ComPtr<ID3D12Resource> AsyncTextureLoader::CreateTextureResource(TextureLoadJob* job, D3D12_RESOURCE_DESC textureDesc, ComPtr<ID3D12Resource>& uploadBuffer, ID3D12GraphicsCommandList* commandList) 
{
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

	// Set up copy locations
	D3D12_TEXTURE_COPY_LOCATION destLocation = {};
	destLocation.pResource = texture.Get();
	destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	destLocation.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
	srcLocation.pResource = uploadBuffer.Get();
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcLocation.PlacedFootprint = layouts;

	commandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);

	// Transition texture to shader resource state
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(1, &barrier);

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
bool AsyncTextureLoader::IsTextureReady(const string& textureName) const 
{
	lock_guard<mutex> lock(m_loadedTexturesMutex);
	return m_textures.find(textureName) != m_textures.end();
}

ID3D12Resource* AsyncTextureLoader::GetTexture(const string& textureName) const 
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

bool AsyncTextureLoader::ProcessHDRTexture(TextureLoadJob* job) 
{
	LOG_DEBUG("Processing HDR texture (TextureManager style): ", job->textureInfo.name);
	
	if (!IsInitialized())
    {
		LOG_ERROR("AsyncTextureLoader not properly initialized for HDR processing");
		return false;
	}

    // 1. Load HDR Image and save as a cubemap
	// 2. Create an empty cubemap resource for irradiance map
	// 3. Calculate the irradiance map and copy to the resource
    // 4. Create an empty cubemap resource for prefiltered map
    // 5. Calculate the prefiltered map and copy to the resource
    // 6. Create an empty 2d resource for BRDF LUT
    // 7. Calculate the BRDF LUT and copy to the resource

	try 
    {
		ScopedCommandList cmdList(m_uploadPool);
		if (!cmdList.IsValid()) 
        {
			LOG_ERROR("Failed to get command list from pool");
			return false;
		}
		
		string filename = job->textureInfo.path;
		
		if (stbi_is_hdr(filename.c_str()) == 0)
		{
			LOG_ERROR("File is not a valid HDR texture: ", filename);
			return false;
		}

		int width, height, channels;
		float* data = stbi_loadf(filename.c_str(), &width, &height, &channels, 3);
		if (!data) 
		{
			LOG_ERROR("Failed to load HDR texture: ", filename);
			return false;
		}
		
		LOG_DEBUG("HDR texture loaded: ", filename, " (", width, "x", height, ", ", channels, " channels)");
		UINT cubemapSize = max(width / 4, height / 2);
		
		job->hdrImageData = data;
		job->width = width;
		job->height = height;
		job->gpuResource = CreateSkyboxCubemap(job, job->uploadBuffer, cmdList.Get());
		if (!job->gpuResource)  
        {
			LOG_ERROR("Failed to create skybox cubemap");
			return false;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = static_cast<D3D12_SRV_DIMENSION>(D3D12_SRV_DIMENSION_TEXTURECUBE);
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		
		m_descriptorAllocator->CreateSRVAtRangeIndex(job->textureInfo.rangeType, job->textureInfo.registerIndex, job->textureInfo.name, job->gpuResource.Get(), &srvDesc);

		auto irradianceResource = GenerateIrradianceMap(job->gpuResource.Get(), cubemapSize, job->textureInfo.name, cmdList.Get());
		auto prefilteredResource = GeneratePrefilteredMap(cubemapSize, job->textureInfo.name, cmdList.Get());
		auto brdfLutResource = GenerateBRDFLUT(job->textureInfo.name, cmdList.Get());

		BindHDRDerivedTextures(job->textureInfo.name, irradianceResource.Get(), prefilteredResource.Get(), brdfLutResource.Get());
		
		string baseName = job->textureInfo.name;
		m_textures[baseName] = job->gpuResource;
		m_textures[baseName + "_irradiance"] = irradianceResource;
		m_textures[baseName + "_prefiltered"] = prefilteredResource;
		m_textures[baseName + "_brdf_lut"] = brdfLutResource;

		if (job->hdrImageData)
		{
			stbi_image_free(job->hdrImageData); 
			job->hdrImageData = nullptr;
		}

		cmdList->Close();
		ID3D12CommandList* commandLists[] = { cmdList.Get() };
		m_commandQueue->ExecuteCommandLists(1, commandLists);

		const UINT64 fenceValue = cmdList.GetFenceValue();
		m_commandQueue->Signal(m_fence, fenceValue);

		// wait for completing current frame
		if (m_fence->GetCompletedValue() < fenceValue)
		{
			THROW_IF_FAILED(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent))
			WaitForSingleObject(m_fenceEvent, INFINITE);
		}

		LOG_DEBUG("HDR texture processing completed: ", job->textureInfo.name);
		return true;
	}
	catch (const exception& e)
	{
		LOG_ERROR("Exception during HDR processing: ", e.what());
		return false;
	}
}

ComPtr<ID3D12Resource> AsyncTextureLoader::CreateSkyboxCubemap(TextureLoadJob* job, ComPtr<ID3D12Resource>& uploadBuffer, ID3D12GraphicsCommandList* commandList) 
{
	vector<vector<float>> faceData = EquirectangularToCubemap(job->hdrImageData, job->width, job->height);

	UINT cubemapSize = max(static_cast<UINT>(job->width / 4), static_cast<UINT>(job->height / 2));
	
	vector<vector<uint16_t>> faceDataRGBA16(6);
	for (int face = 0; face < 6; ++face) 
    {
		faceDataRGBA16[face].resize(cubemapSize * cubemapSize * 4);
		for (size_t i = 0; i < cubemapSize * cubemapSize; ++i) 
        {
			faceDataRGBA16[face][i * 4 + 0] = DirectX::PackedVector::XMConvertFloatToHalf(faceData[face][i * 3 + 0]); // R
			faceDataRGBA16[face][i * 4 + 1] = DirectX::PackedVector::XMConvertFloatToHalf(faceData[face][i * 3 + 1]); // G
			faceDataRGBA16[face][i * 4 + 2] = DirectX::PackedVector::XMConvertFloatToHalf(faceData[face][i * 3 + 2]); // B
			faceDataRGBA16[face][i * 4 + 3] = DirectX::PackedVector::XMConvertFloatToHalf(1.0f); // A
		}
	}

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Width = cubemapSize;
	textureDesc.Height = cubemapSize;
	textureDesc.DepthOrArraySize = 6;
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// return CreateTextureResource(job, textureDesc, uploadBuffer, commandList);

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

	vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(6);
	vector<UINT> numRows(6);
	vector<UINT64> rowPitch(6);
	UINT64 totalUploadBufferSize = 0;

	for (int face = 0; face < 6; ++face)
	{
		UINT64 faceUploadSize = 0;
		// Get GPU memory layout requirements for each face
		m_device->GetCopyableFootprints(
			&textureDesc,           // Input: texture description
			face,                   // Current subresource index (face)
			1,                      // Number of subresources
			totalUploadBufferSize,          // Current offset in upload buffer
			&layouts[face],         // Output: memory layout with alignment
			&numRows[face],         // Output: number of rows in the texture
			&rowPitch[face],        // Output: actual bytes per row (without padding)
			&faceUploadSize         // Output: bytes needed for this face
		);
		totalUploadBufferSize += faceUploadSize;
	}

	D3D12_HEAP_PROPERTIES uploadHeapProperties = defaultHeapProperties;
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC uploadBufferDesc = {};
	uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadBufferDesc.Width = totalUploadBufferSize;
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
	
	UINT64 rowSizeInBytes = static_cast<UINT64>(cubemapSize) * 4 * sizeof(uint16_t);

	for (int face = 0; face < 6; ++face)
	{
		BYTE* destSliceStart = reinterpret_cast<BYTE*>(mappedData) + layouts[face].Offset;
		for (UINT i = 0; i < numRows[face]; i++) 
		{
			memcpy(
				destSliceStart + layouts[face].Footprint.RowPitch * i, 
				faceData[face].data() + rowSizeInBytes * i, 
				rowSizeInBytes
			);
		}
	}

	uploadBuffer->Unmap(0, nullptr);

	for (int face = 0; face < 6; ++face)
	{
		// Set up copy locations
		D3D12_TEXTURE_COPY_LOCATION destLocation = {};
		destLocation.pResource = texture.Get();
		destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		destLocation.SubresourceIndex = face;

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.pResource = uploadBuffer.Get();
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcLocation.PlacedFootprint = layouts[face];

		commandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);
	}

	// Transition texture to shader resource state
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(1, &barrier);

	return texture;	
}

ComPtr<ID3D12Resource> AsyncTextureLoader::CreateEmptyMapResource(UINT mapSize, UINT depthOrArraySize, DXGI_FORMAT format, UINT mipLevels) 
{
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = mapSize;
	resourceDesc.Height = mapSize;
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(depthOrArraySize);
	resourceDesc.MipLevels = static_cast<UINT16>(mipLevels);
	resourceDesc.Format = format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES defaultHeapProperties = {};
	defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	defaultHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	defaultHeapProperties.CreationNodeMask = 1;
	defaultHeapProperties.VisibleNodeMask = 1;

	ComPtr<ID3D12Resource> resource;
	THROW_IF_FAILED(m_device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&resource)));

	return resource;
}

ComPtr<ID3D12Resource> AsyncTextureLoader::GenerateIrradianceMap(ID3D12Resource* skyboxResource, UINT cubemapSize, const string& baseName, ID3D12GraphicsCommandList* commandList) 
{
	UINT irradianceMapSize = cubemapSize / 2;
	auto irradianceResource = CreateEmptyMapResource(irradianceMapSize, 6, DXGI_FORMAT_R16G16B16A16_FLOAT);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Texture2DArray.MipSlice = 0;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.ArraySize = 6;

	string uavName = baseName + "_irradiance_uav";
	m_descriptorAllocator->CreateUAV(LunarConstants::RangeType::DYNAMIC_UAV, uavName, irradianceResource.Get(), &uavDesc);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = skyboxResource;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	barrier.Transition.pResource = irradianceResource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	commandList->ResourceBarrier(1, &barrier);

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_descriptorAllocator->GetHeap() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	ID3D12RootSignature* computeRootSignature = m_pipelineStateManager->GetComputeRootSignature(); 
	commandList->SetComputeRootSignature(computeRootSignature);
	
	commandList->SetComputeRootDescriptorTable(LunarConstants::COMPUTE_INPUT_SRV_INDEX, m_descriptorAllocator->GetGPUHandle(baseName));
	commandList->SetComputeRootDescriptorTable(LunarConstants::COMPUTE_OUTPUT_UAV_INDEX, m_descriptorAllocator->GetGPUHandle(uavName));
	commandList->SetPipelineState(m_pipelineStateManager->GetPSO("irradiance"));
	commandList->Dispatch((irradianceMapSize + 7) / 8, (irradianceMapSize + 7) / 8, 6);

	barrier.Transition.pResource = irradianceResource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &barrier);

	barrier.Transition.pResource = skyboxResource;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &barrier);

	LOG_DEBUG("Irradiance map generated successfully for: ", baseName);
	return irradianceResource;
}

ComPtr<ID3D12Resource> AsyncTextureLoader::GeneratePrefilteredMap(UINT cubemapSize, const string& baseName, ID3D12GraphicsCommandList* commandList) 
{
	if (!m_pipelineStateManager) 
    {
		LOG_ERROR("PipelineStateManager not available for prefiltered map generation");
		return nullptr;
	}

	UINT prefilteredMapSize = cubemapSize / 2;
	UINT maxMipLevels = static_cast<UINT>(std::floor(std::log2(prefilteredMapSize))) + 1;
	
	auto prefilteredResource = CreateEmptyMapResource(prefilteredMapSize, 6, DXGI_FORMAT_R16G16B16A16_FLOAT, maxMipLevels);

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_descriptorAllocator->GetHeap() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	commandList->SetComputeRootSignature(m_pipelineStateManager->GetComputeRootSignature());

	commandList->SetComputeRootDescriptorTable(LunarConstants::COMPUTE_INPUT_SRV_INDEX, m_descriptorAllocator->GetGPUHandle(baseName));

	// Generate each mip level
	for (UINT mip = 0; mip < maxMipLevels; ++mip)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.Subresource = mip;
		barrier.Transition.pResource = prefilteredResource.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		commandList->ResourceBarrier(1, &barrier);

		UINT mipSize = prefilteredMapSize >> mip;
		if (mipSize == 0) mipSize = 1;

		float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);

		// Set constants for this mip level
		UINT constants[4] = {
			*reinterpret_cast<UINT*>(&roughness), // float as uint32
			mip,                                   // mip level
			0,                                     // padding
			0                                      // padding
		};

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = mip;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = 6;

		string uavName = baseName + "_prefiltered_uav_mip" + to_string(mip);
		m_descriptorAllocator->CreateUAV(LunarConstants::RangeType::DYNAMIC_UAV, uavName, prefilteredResource.Get(), &uavDesc);

		commandList->SetComputeRoot32BitConstants(LunarConstants::COMPUTE_CONSTANTS_INDEX, 4, constants, 0);
		commandList->SetComputeRootDescriptorTable(LunarConstants::COMPUTE_OUTPUT_UAV_INDEX, m_descriptorAllocator->GetGPUHandle(uavName));
		commandList->SetPipelineState(m_pipelineStateManager->GetPSO("prefiltered"));

		commandList->Dispatch((mipSize + 7) / 8, (mipSize + 7) / 8, 6);

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		commandList->ResourceBarrier(1, &barrier);
	}

	LOG_DEBUG("Prefiltered map generated successfully with ", maxMipLevels, " mip levels");
	return prefilteredResource;
}

ComPtr<ID3D12Resource> AsyncTextureLoader::GenerateBRDFLUT(const string& baseName, ID3D12GraphicsCommandList* commandList) 
{
	const UINT brdfLutSize = 512;
	auto brdfLutResource = CreateEmptyMapResource(brdfLutSize, 1, DXGI_FORMAT_R16G16_FLOAT);

	D3D12_UNORDERED_ACCESS_VIEW_DESC brdfLutUavDesc = {};
	brdfLutUavDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	brdfLutUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	brdfLutUavDesc.Texture2D.MipSlice = 0;
	brdfLutUavDesc.Texture2D.PlaneSlice = 0;

	string uavName = baseName + "_brdf_lut_uav";
	m_descriptorAllocator->CreateUAV(LunarConstants::RangeType::DYNAMIC_UAV, uavName, brdfLutResource.Get(), &brdfLutUavDesc);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = brdfLutResource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_descriptorAllocator->GetHeap() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	commandList->SetComputeRootSignature(m_pipelineStateManager->GetComputeRootSignature());
	commandList->SetComputeRootDescriptorTable(LunarConstants::COMPUTE_OUTPUT_UAV_INDEX, m_descriptorAllocator->GetGPUHandle(uavName));
	commandList->SetPipelineState(m_pipelineStateManager->GetPSO("brdfLut"));
	commandList->Dispatch((brdfLutSize + 7) / 8, (brdfLutSize + 7) / 8, 1);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &barrier);

	LOG_DEBUG("BRDF LUT generated successfully for: ", baseName, " (", brdfLutSize, "x", brdfLutSize, ")");
	return brdfLutResource;
}

void AsyncTextureLoader::BindHDRDerivedTextures(const string& baseName, ID3D12Resource* irradiance, ID3D12Resource* prefiltered, ID3D12Resource* brdfLut) 
{
	D3D12_SHADER_RESOURCE_VIEW_DESC irradianceSrvDesc = {};
	irradianceSrvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	irradianceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	irradianceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	irradianceSrvDesc.TextureCube.MostDetailedMip = 0;
	irradianceSrvDesc.TextureCube.MipLevels = 1;
	irradianceSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	auto prefilteredDesc = prefiltered->GetDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC prefilteredSrvDesc = {};
	prefilteredSrvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	prefilteredSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	prefilteredSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	prefilteredSrvDesc.TextureCube.MostDetailedMip = 0;
	prefilteredSrvDesc.TextureCube.MipLevels = prefilteredDesc.MipLevels;
	prefilteredSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	D3D12_SHADER_RESOURCE_VIEW_DESC brdfLutSrvDesc = {};
	brdfLutSrvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	brdfLutSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	brdfLutSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	brdfLutSrvDesc.Texture2D.MostDetailedMip = 0;
	brdfLutSrvDesc.Texture2D.MipLevels = 1;
	brdfLutSrvDesc.Texture2D.PlaneSlice = 0;
	brdfLutSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	m_descriptorAllocator->CreateSRVAtRangeIndex(
		LunarConstants::RangeType::ENVIRONMENT, 1, 
		baseName + "_irradiance", irradiance, &irradianceSrvDesc);

	m_descriptorAllocator->CreateSRVAtRangeIndex(
		LunarConstants::RangeType::ENVIRONMENT, 2, 
		baseName + "_prefiltered", prefiltered, &prefilteredSrvDesc);

	m_descriptorAllocator->CreateSRVAtRangeIndex(
		LunarConstants::RangeType::ENVIRONMENT, 3, 
		baseName + "_brdf_lut", brdfLut, &brdfLutSrvDesc);

	LOG_DEBUG("HDR derived textures bound for: ", baseName);
}

vector<vector<float>> AsyncTextureLoader::EquirectangularToCubemap(float* imageData, UINT width, UINT height) 
{
	UINT cubemapSize = max(width / 4, height / 2);

	vector<vector<float>> faceData(6);

	for (int face = 0; face < 6; ++face)
	{
		LOG_DEBUG("Processing cubemap face: ", face);
		faceData[face].resize(cubemapSize * cubemapSize * 3); // 3 channels RGB

		for (UINT y = 0; y < cubemapSize; ++y)
		{
			for (UINT x = 0; x < cubemapSize; ++x)
			{
				// Convert cubemap coordinates to direction 
				float u = ((x + 0.5f) * 2.0f) / cubemapSize - 1.0f; // [-1, 1]
				float v = ((y + 0.5f) * 2.0f) / cubemapSize - 1.0f; 

				float direction[3];
				switch (face) {
					case 0: // +X
						direction[0] = 1.0f;
						direction[1] = -v;
						direction[2] = -u;
						break;
					case 1: // -X
						direction[0] = -1.0f;
						direction[1] = -v;
						direction[2] = u;
						break;
					case 2: // +Y
						direction[0] = u;
						direction[1] = 1.0f;
						direction[2] = v;
						break;
					case 3: // -Y
						direction[0] = u;
						direction[1] = -1.0f;
						direction[2] = -v;
						break;
					case 4: // +Z
						direction[0] = u;
						direction[1] = -v;
						direction[2] = 1.0f;
						break;
					case 5: // -Z
						direction[0] = -u;
						direction[1] = -v;
						direction[2] = -1.0f;
						break;
					default:
						LOG_ERROR("Invalid cubemap face index: ", face);
						continue;
				}
				
				// Normalize direction vector
				float length = sqrtf(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
				if (length == 0.0f)
				{
					LOG_ERROR("Zero length direction vector for cubemap face: ", face);
					continue;
				}
				direction[0] /= length;
				direction[1] /= length;
				direction[2] /= length;

				const float M_PI = 3.141592f;
				float theta = atan2f(direction[2], direction[0]); // azimuth
				float phi = asinf(direction[1]); // elevation
				float equiU = (theta + M_PI) / (2.0f * M_PI); 
				float equiV = 0.5f - phi / M_PI; 

				int equiX = static_cast<int>(equiU * width);
				int equiY = static_cast<int>(equiV * height);
				equiX = clamp(equiX, 0, static_cast<int>(width - 1));
				equiY = clamp(equiY, 0, static_cast<int>(height - 1));

				// data sampling
				int srcIndex = (equiY * width + equiX) * 3; // 3 : channels RGB
				int dstIndex = (y * cubemapSize + x) * 3; // 3 : channels RGB

				faceData[face][dstIndex] = imageData[srcIndex];     // R
				faceData[face][dstIndex + 1] = imageData[srcIndex + 1]; // G
				faceData[face][dstIndex + 2] = imageData[srcIndex + 2]; // B
			}
		}
	}
	
	return faceData;
}

} // namespace Lunar
