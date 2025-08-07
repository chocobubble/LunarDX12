#include "AsyncModelLoader.h"
#include "CommandListPool.h"
#include "DescriptorAllocator.h"
#include "PipelineStateManager.h"
#include "Utils/Logger.h"

#include <algorithm>
#include <filesystem>

using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;

namespace Lunar
{

// ModelLoadJob 소멸자
ModelLoadJob::~ModelLoadJob() = default;

AsyncModelLoader::AsyncModelLoader() = default;

AsyncModelLoader::~AsyncModelLoader()
{
    Shutdown();
}

void AsyncModelLoader::Initialize(size_t numWorkerThreads, ID3D12Device* device, 
                                 CommandListPool* uploadPool, DescriptorAllocator* descriptorAllocator,
                                 ID3D12CommandQueue* commandQueue, ID3D12Fence* fence,
                                 PipelineStateManager* pipelineStateManager)
{
    LOG_FUNCTION_ENTRY();
    
    m_device = device;
    m_uploadPool = uploadPool;
    m_descriptorAllocator = descriptorAllocator;
    m_commandQueue = commandQueue;
    m_fence = fence;
    m_pipelineStateManager = pipelineStateManager;
    
    // 워커 스레드 시작
    m_shouldStop = false;
    m_workerThreads.reserve(numWorkerThreads);
    
    for (size_t i = 0; i < numWorkerThreads; ++i) {
        m_workerThreads.emplace_back(&AsyncModelLoader::WorkerThreadFunction, this);
        LOG_DEBUG("Model loader worker thread started, ID: ", m_workerThreads.back().get_id());
    }
    
    LOG_INFO("AsyncModelLoader initialized with ", numWorkerThreads, " worker threads");
}

void AsyncModelLoader::Shutdown()
{
    LOG_FUNCTION_ENTRY();
    
    if (m_shouldStop.load()) {
        return; // 이미 종료됨
    }
    
    // 모든 워커 스레드에 종료 신호
    m_shouldStop = true;
    m_cv.notify_all();
    
    // 모든 스레드 종료 대기
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_workerThreads.clear();
    
    // 리소스 정리
    {
        lock_guard<mutex> lock(m_loadedModelsMutex);
        m_models.clear();
    }
    
    LOG_INFO("AsyncModelLoader shutdown completed");
}

bool AsyncModelLoader::IsInitialized() const
{
    return m_device != nullptr && 
           m_uploadPool != nullptr && 
           m_descriptorAllocator != nullptr &&
           m_commandQueue != nullptr &&
           m_fence != nullptr;
}

future<bool> AsyncModelLoader::LoadModelAsync(const LunarConstants::ModelInfo& modelInfo)
{
    LOG_DEBUG("Queuing model for async loading: ", modelInfo.name);
    
    auto job = make_unique<ModelLoadJob>();
    job->modelInfo = modelInfo;
    
    auto future = job->completion.get_future();
    
    {
        lock_guard<mutex> lock(m_queueMutex);
        m_jobQueue.push(move(job));
        m_totalModels.fetch_add(1);
        
        lock_guard<mutex> statsLock(m_statsMutex);
        m_stats.queuedJobs++;
    }
    
    m_cv.notify_one();
    
    LOG_DEBUG("Model queued successfully: ", modelInfo.name);
    return future;
}

bool AsyncModelLoader::IsModelReady(const string& modelName) const
{
    lock_guard<mutex> lock(m_loadedModelsMutex);
    return m_models.find(modelName) != m_models.end();
}

LoadedModel* AsyncModelLoader::GetModel(const string& modelName) const
{
    lock_guard<mutex> lock(m_loadedModelsMutex);
    auto it = m_models.find(modelName);
    return (it != m_models.end()) ? it->second.get() : nullptr;
}

bool AsyncModelLoader::AreAllModelsLoaded() const
{
    return m_loadedModels.load() >= m_totalModels.load();
}

float AsyncModelLoader::GetLoadingProgress() const
{
    int total = m_totalModels.load();
    if (total == 0) return 1.0f;
    return static_cast<float>(m_loadedModels.load()) / total;
}

AsyncModelLoader::LoadingStats AsyncModelLoader::GetStats() const
{
    lock_guard<mutex> lock(m_statsMutex);
    return m_stats;
}

void AsyncModelLoader::LogStatus() const
{
    auto stats = GetStats();
    LOG_INFO("AsyncModelLoader Status:");
    LOG_INFO("  Queued: ", stats.queuedJobs);
    LOG_INFO("  Completed: ", stats.completedLoads);
    LOG_INFO("  Failed: ", stats.failedJobs);
    LOG_INFO("  Progress: ", GetLoadingProgress() * 100.0f, "%");
}

void AsyncModelLoader::WorkerThreadFunction()
{
    LOG_DEBUG("Model loader worker thread started, ID: ", this_thread::get_id());
    
    while (!m_shouldStop.load()) {
        unique_ptr<ModelLoadJob> job;
        
        {
            unique_lock<mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this] { return !m_jobQueue.empty() || m_shouldStop.load(); });
            
            if (m_shouldStop.load()) {
                break;
            }
            
            if (!m_jobQueue.empty()) {
                job = move(m_jobQueue.front());
                m_jobQueue.pop();
            }
        }
        
        if (job) {
            ProcessModelLoadJob(move(job));
        }
    }
    
    LOG_DEBUG("Model loader worker thread terminated, ID: ", this_thread::get_id());
}

void AsyncModelLoader::ProcessModelLoadJob(unique_ptr<ModelLoadJob> job)
{
    job->loadStartTime = chrono::high_resolution_clock::now();
    
    try {
        LOG_DEBUG("Processing model: ", job->modelInfo.name);
        
        job->loadedModel = make_unique<LoadedModel>();
        job->loadedModel->name = job->modelInfo.name;
        job->loadedModel->filePath = job->modelInfo.path;
        job->loadedModel->hasAnimation = job->modelInfo.hasAnimation;
        
        bool success = LoadModelFromFile(job->modelInfo.path, *job->loadedModel);
        
        job->loadEndTime = chrono::high_resolution_clock::now();
        auto totalLoadDuration = chrono::duration_cast<chrono::milliseconds>(job->loadEndTime - job->loadStartTime);
        
        if (success) {
            // 성공적으로 로드된 모델을 저장
            {
                lock_guard<mutex> lock(m_loadedModelsMutex);
                m_models[job->modelInfo.name] = move(job->loadedModel);
            }
            
            m_loadedModels.fetch_add(1);
            LOG_DEBUG("Model loading completed: ", job->modelInfo.name, " (", totalLoadDuration.count(), "ms)");
            
            {
                lock_guard<mutex> lock(m_statsMutex);
                m_stats.completedLoads++;
                m_stats.averageLoadTime = (m_stats.averageLoadTime * (m_stats.completedLoads - 1) + totalLoadDuration.count()) / m_stats.completedLoads;
            }
        } else {
            LOG_ERROR("Failed to load model: ", job->modelInfo.name);
            {
                lock_guard<mutex> lock(m_statsMutex);
                m_stats.failedJobs++;
            }
        }
        
        job->completion.set_value(success);
        
    } catch (const exception& e) {
        LOG_ERROR("Exception during model loading: ", e.what());
        job->completion.set_value(false);
        
        {
            lock_guard<mutex> lock(m_statsMutex);
            m_stats.failedJobs++;
        }
    }
}

bool AsyncModelLoader::LoadModelFromFile(const string& filePath, LoadedModel& outModel)
{
    LOG_DEBUG("Loading model from file: ", filePath);
    
    if (!filesystem::exists(filePath)) {
        LOG_ERROR("Model file does not exist: ", filePath);
        return false;
    }
    
    Assimp::Importer importer;
    
    // 기본 후처리 플래그
    uint32_t flags = aiProcess_Triangulate |           // 모든 면을 삼각형으로
                     aiProcess_GenSmoothNormals |      // 부드러운 법선 생성
                     aiProcess_FlipUVs |              // UV 좌표 뒤집기 (DirectX용)
                     aiProcess_CalcTangentSpace |     // 탄젠트 공간 계산
                     aiProcess_JoinIdenticalVertices | // 중복 버텍스 제거
                     aiProcess_ImproveCacheLocality |  // 캐시 지역성 개선
                     aiProcess_OptimizeMeshes;         // 메시 최적화
    
    // 파일 확장자 확인
    string extension = filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    // FBX 파일이거나 애니메이션 모델인 경우 추가 플래그 적용
    if (extension == ".fbx" || outModel.hasAnimation) {
        flags |= aiProcess_ConvertToLeftHanded |      // DirectX 왼손 좌표계 변환
                 aiProcess_LimitBoneWeights |         // 본 가중치 제한 (4개)
                 aiProcess_SplitByBoneCount;          // 본 개수로 메시 분할
        LOG_DEBUG("FBX/Animation model detected - adding bone processing flags");
    }
    
    // 데이터 검증은 항상 수행
    flags |= aiProcess_ValidateDataStructure;
    
    const aiScene* scene = importer.ReadFile(filePath, flags);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR("Assimp Error: ", importer.GetErrorString());
        return false;
    }
    
    // FBX 파일 특별 처리
    if (extension == ".fbx") {
        LOG_DEBUG("Processing FBX file with special handling");
        
        // FBX는 보통 복잡한 루트 변환을 가짐
        aiMatrix4x4 globalTransform = scene->mRootNode->mTransformation;
        globalTransform.Inverse();
        outModel.globalInverseTransform = ConvertAssimpMatrix(globalTransform);
        
        LOG_DEBUG("FBX Root node transform: ");
        LOG_DEBUG("  [", scene->mRootNode->mTransformation.a1, ", ", scene->mRootNode->mTransformation.a2, ", ", scene->mRootNode->mTransformation.a3, ", ", scene->mRootNode->mTransformation.a4, "]");
        LOG_DEBUG("  [", scene->mRootNode->mTransformation.b1, ", ", scene->mRootNode->mTransformation.b2, ", ", scene->mRootNode->mTransformation.b3, ", ", scene->mRootNode->mTransformation.b4, "]");
        LOG_DEBUG("  [", scene->mRootNode->mTransformation.c1, ", ", scene->mRootNode->mTransformation.c2, ", ", scene->mRootNode->mTransformation.c3, ", ", scene->mRootNode->mTransformation.c4, "]");
        LOG_DEBUG("  [", scene->mRootNode->mTransformation.d1, ", ", scene->mRootNode->mTransformation.d2, ", ", scene->mRootNode->mTransformation.d3, ", ", scene->mRootNode->mTransformation.d4, "]");
    } else {
        // OBJ 등 다른 포맷은 간단한 처리
        aiMatrix4x4 globalTransform = scene->mRootNode->mTransformation;
        globalTransform.Inverse();
        outModel.globalInverseTransform = ConvertAssimpMatrix(globalTransform);
        LOG_DEBUG("Non-FBX model - using standard transform processing");
    }
    
    // 머티리얼 로드
    LoadMaterials(scene, outModel);
    
    // 노드 처리 (재귀적으로 메시 로드)
    ProcessNode(scene->mRootNode, scene, outModel);
    
    // 애니메이션 로드 (FBX에서 특히 중요)
    if (outModel.hasAnimation && scene->mNumAnimations > 0) {
        LoadAnimations(scene, outModel);
        LOG_DEBUG("Loaded ", scene->mNumAnimations, " animations from ", extension, " file");
    }
    
    LOG_INFO("Model loaded successfully: ", filePath);
    LOG_INFO("  Format: ", extension);
    LOG_INFO("  Meshes: ", outModel.meshes.size());
    LOG_INFO("  Materials: ", outModel.materials.size());
    LOG_INFO("  Bones: ", outModel.bones.size());
    LOG_INFO("  Animations: ", outModel.animations.size());
    LOG_INFO("  Used ConvertToLeftHanded: ", (flags & aiProcess_ConvertToLeftHanded) ? "Yes" : "No");
    
    return true;
}

void AsyncModelLoader::ProcessNode(aiNode* node, const aiScene* scene, LoadedModel& model)
{
    // 현재 노드의 모든 메시 처리
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        model.meshes.push_back(ProcessMesh(mesh, scene, model));
    }
    
    // 자식 노드들 재귀 처리
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, model);
    }
}

ModelMesh AsyncModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene, LoadedModel& model)
{
    ModelMesh newMesh;
    newMesh.name = mesh->mName.C_Str();
    newMesh.vertices.reserve(mesh->mNumVertices);
    
    // 버텍스 데이터 추출
    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        ModelVertex vertex;
        
        // Position
        vertex.position = {
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        };
        
        // Normal
        if (mesh->HasNormals()) {
            vertex.normal = {
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            };
        }
        
        // Texture Coordinates
        if (mesh->mTextureCoords[0]) {
            vertex.texCoord = {
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            };
        } else {
            vertex.texCoord = {0.0f, 0.0f};
        }
        
        // Tangent and Bitangent
        if (mesh->HasTangentsAndBitangents()) {
            vertex.tangent = {
                mesh->mTangents[i].x,
                mesh->mTangents[i].y,
                mesh->mTangents[i].z
            };
            
            vertex.bitangent = {
                mesh->mBitangents[i].x,
                mesh->mBitangents[i].y,
                mesh->mBitangents[i].z
            };
        }
        
        newMesh.vertices.push_back(vertex);
    }
    
    // 인덱스 데이터 추출
    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; j++) {
            newMesh.indices.push_back(face.mIndices[j]);
        }
    }
    
    // 본 데이터 로드 (애니메이션이 있는 경우)
    if (mesh->HasBones() && model.hasAnimation) {
        LoadBones(mesh, model, newMesh.vertices);
    }
    
    // 머티리얼 인덱스
    newMesh.materialIndex = mesh->mMaterialIndex;
    
    // DirectX 12 버퍼 생성
    if (!CreateVertexBuffer(newMesh) || !CreateIndexBuffer(newMesh)) {
        LOG_ERROR("Failed to create GPU buffers for mesh: ", newMesh.name);
    }
    
    return newMesh;
}

void AsyncModelLoader::LoadMaterials(const aiScene* scene, LoadedModel& model)
{
    model.materials.reserve(scene->mNumMaterials);
    
    for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* mat = scene->mMaterials[i];
        ModelMaterial material;
        
        // 머티리얼 이름
        aiString name;
        if (mat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
            material.name = name.C_Str();
        } else {
            material.name = "Material_" + std::to_string(i);
        }
        
        LOG_DEBUG("Loading material: ", material.name);
        
        // 기본 색상 정보
        aiColor3D color;
        if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            material.diffuse = {color.r, color.g, color.b};
        }
        
        if (mat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
            material.specular = {color.r, color.g, color.b};
        }
        
        if (mat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
            material.ambient = {color.r, color.g, color.b};
        }
        
        if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) {
            material.emissive = {color.r, color.g, color.b};
        }
        
        // 물리적 속성
        float value;
        if (mat->Get(AI_MATKEY_SHININESS, value) == AI_SUCCESS) {
            material.shininess = value;
        }
        
        if (mat->Get(AI_MATKEY_OPACITY, value) == AI_SUCCESS) {
            material.opacity = value;
        }
        
        if (mat->Get(AI_MATKEY_REFLECTIVITY, value) == AI_SUCCESS) {
            material.reflectivity = value;
        }
        
        // PBR 속성 (FBX에서 지원하는 경우)
        if (mat->Get(AI_MATKEY_METALLIC_FACTOR, value) == AI_SUCCESS) {
            material.metallic = value;
            LOG_DEBUG("  Metallic: ", value);
        } else {
            // 기본값 또는 추정값 사용
            material.metallic = 0.0f;
        }
        
        if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, value) == AI_SUCCESS) {
            material.roughness = value;
            LOG_DEBUG("  Roughness: ", value);
        } else {
            // Shininess에서 Roughness 추정
            material.roughness = 1.0f - (material.shininess / 128.0f);
            material.roughness = std::clamp(material.roughness, 0.0f, 1.0f);
            LOG_DEBUG("  Roughness (estimated from shininess): ", material.roughness);
        }
        
        // 텍스처 경로들
        aiString texPath;
        
        // Diffuse/Albedo 텍스처
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
            material.diffuseTexture = texPath.C_Str();
            LOG_DEBUG("  Diffuse texture: ", material.diffuseTexture);
        }
        
        // Normal 텍스처
        if (mat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS) {
            material.normalTexture = texPath.C_Str();
            LOG_DEBUG("  Normal texture: ", material.normalTexture);
        } else if (mat->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS) {
            // Height map을 Normal map으로 사용하는 경우
            material.normalTexture = texPath.C_Str();
            LOG_DEBUG("  Normal texture (from height): ", material.normalTexture);
        }
        
        // Specular 텍스처
        if (mat->GetTexture(aiTextureType_SPECULAR, 0, &texPath) == AI_SUCCESS) {
            material.specularTexture = texPath.C_Str();
            LOG_DEBUG("  Specular texture: ", material.specularTexture);
        }
        
        // Emissive 텍스처
        if (mat->GetTexture(aiTextureType_EMISSIVE, 0, &texPath) == AI_SUCCESS) {
            material.emissiveTexture = texPath.C_Str();
            LOG_DEBUG("  Emissive texture: ", material.emissiveTexture);
        }
        
        // PBR 텍스처들
        if (mat->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS) {
            material.metallicTexture = texPath.C_Str();
            LOG_DEBUG("  Metallic texture: ", material.metallicTexture);
        }
        
        if (mat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texPath) == AI_SUCCESS) {
            material.roughnessTexture = texPath.C_Str();
            LOG_DEBUG("  Roughness texture: ", material.roughnessTexture);
        }
        
        // AO 텍스처
        if (mat->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &texPath) == AI_SUCCESS) {
            material.aoTexture = texPath.C_Str();
            LOG_DEBUG("  AO texture: ", material.aoTexture);
        }
        
        // Opacity 텍스처
        if (mat->GetTexture(aiTextureType_OPACITY, 0, &texPath) == AI_SUCCESS) {
            material.opacityTexture = texPath.C_Str();
            LOG_DEBUG("  Opacity texture: ", material.opacityTexture);
        }
        
        // 디버그 정보 출력
        LOG_DEBUG("Material loaded: ", material.name);
        LOG_DEBUG("  Diffuse: (", material.diffuse.x, ", ", material.diffuse.y, ", ", material.diffuse.z, ")");
        LOG_DEBUG("  Metallic: ", material.metallic, ", Roughness: ", material.roughness);
        LOG_DEBUG("  Shininess: ", material.shininess, ", Opacity: ", material.opacity);
        
        model.materials.push_back(material);
    }
    
    LOG_INFO("Loaded ", model.materials.size(), " materials from FBX");
    
    // 머티리얼이 없는 경우 기본 머티리얼 생성
    if (model.materials.empty()) {
        ModelMaterial defaultMaterial;
        defaultMaterial.name = "DefaultMaterial";
        defaultMaterial.diffuse = {0.7f, 0.7f, 0.7f};
        defaultMaterial.metallic = 0.0f;
        defaultMaterial.roughness = 0.5f;
        defaultMaterial.shininess = 32.0f;
        defaultMaterial.opacity = 1.0f;
        
        model.materials.push_back(defaultMaterial);
        LOG_INFO("Created default material");
    }
}

void AsyncModelLoader::LoadBones(aiMesh* mesh, LoadedModel& model, vector<ModelVertex>& vertices)
{
    // 본 로딩 구현 (애니메이션 지원 시 필요)
    // 현재는 기본 구현만 제공
    LOG_DEBUG("Loading bones for mesh (not implemented yet)");
}

void AsyncModelLoader::LoadAnimations(const aiScene* scene, LoadedModel& model)
{
    // 애니메이션 로딩 구현 (애니메이션 지원 시 필요)
    // 현재는 기본 구현만 제공
    LOG_DEBUG("Loading animations (not implemented yet)");
}

bool AsyncModelLoader::CreateVertexBuffer(ModelMesh& mesh)
{
    if (!IsInitialized() || mesh.vertices.empty()) {
        return false;
    }
    
    const UINT vertexBufferSize = static_cast<UINT>(mesh.vertices.size() * sizeof(ModelVertex));
    
    // 기본 힙에 버텍스 버퍼 생성
    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    defaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    defaultHeapProps.CreationNodeMask = 1;
    defaultHeapProps.VisibleNodeMask = 1;
    
    D3D12_RESOURCE_DESC vertexBufferDesc = {};
    vertexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexBufferDesc.Width = vertexBufferSize;
    vertexBufferDesc.Height = 1;
    vertexBufferDesc.DepthOrArraySize = 1;
    vertexBufferDesc.MipLevels = 1;
    vertexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    vertexBufferDesc.SampleDesc.Count = 1;
    vertexBufferDesc.SampleDesc.Quality = 0;
    vertexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    vertexBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    HRESULT hr = m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mesh.vertexBuffer)
    );
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create vertex buffer");
        return false;
    }
    
    // 업로드 힙에 임시 버퍼 생성
    D3D12_HEAP_PROPERTIES uploadHeapProps = defaultHeapProps;
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mesh.vertexUploadBuffer)
    );
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create vertex upload buffer");
        return false;
    }
    
    // 데이터 복사
    void* mappedData;
    hr = mesh.vertexUploadBuffer->Map(0, nullptr, &mappedData);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to map vertex upload buffer");
        return false;
    }
    
    memcpy(mappedData, mesh.vertices.data(), vertexBufferSize);
    mesh.vertexUploadBuffer->Unmap(0, nullptr);
    
    // 버텍스 버퍼 뷰 설정
    mesh.vertexBufferView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
    mesh.vertexBufferView.StrideInBytes = sizeof(ModelVertex);
    mesh.vertexBufferView.SizeInBytes = vertexBufferSize;
    
    return true;
}

bool AsyncModelLoader::CreateIndexBuffer(ModelMesh& mesh)
{
    if (!IsInitialized() || mesh.indices.empty()) {
        return false;
    }
    
    const UINT indexBufferSize = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));
    
    // 기본 힙에 인덱스 버퍼 생성
    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    defaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    defaultHeapProps.CreationNodeMask = 1;
    defaultHeapProps.VisibleNodeMask = 1;
    
    D3D12_RESOURCE_DESC indexBufferDesc = {};
    indexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    indexBufferDesc.Width = indexBufferSize;
    indexBufferDesc.Height = 1;
    indexBufferDesc.DepthOrArraySize = 1;
    indexBufferDesc.MipLevels = 1;
    indexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    indexBufferDesc.SampleDesc.Count = 1;
    indexBufferDesc.SampleDesc.Quality = 0;
    indexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    indexBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    HRESULT hr = m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mesh.indexBuffer)
    );
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create index buffer");
        return false;
    }
    
    // 업로드 힙에 임시 버퍼 생성
    D3D12_HEAP_PROPERTIES uploadHeapProps = defaultHeapProps;
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mesh.indexUploadBuffer)
    );
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create index upload buffer");
        return false;
    }
    
    // 데이터 복사
    void* mappedData;
    hr = mesh.indexUploadBuffer->Map(0, nullptr, &mappedData);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to map index upload buffer");
        return false;
    }
    
    memcpy(mappedData, mesh.indices.data(), indexBufferSize);
    mesh.indexUploadBuffer->Unmap(0, nullptr);
    
    // 인덱스 버퍼 뷰 설정
    mesh.indexBufferView.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();
    mesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    mesh.indexBufferView.SizeInBytes = indexBufferSize;
    
    return true;
}

// 유틸리티 함수들
XMMATRIX AsyncModelLoader::ConvertAssimpMatrix(const aiMatrix4x4& mat)
{
    return XMMATRIX(
        mat.a1, mat.b1, mat.c1, mat.d1,
        mat.a2, mat.b2, mat.c2, mat.d2,
        mat.a3, mat.b3, mat.c3, mat.d3,
        mat.a4, mat.b4, mat.c4, mat.d4
    );
}

XMFLOAT3 AsyncModelLoader::ConvertAssimpVector3(const aiVector3D& vec)
{
    return XMFLOAT3(vec.x, vec.y, vec.z);
}

XMFLOAT4 AsyncModelLoader::ConvertAssimpQuaternion(const aiQuaternion& quat)
{
    return XMFLOAT4(quat.x, quat.y, quat.z, quat.w);
}

} // namespace Lunar
