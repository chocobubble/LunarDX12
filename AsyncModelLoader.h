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

// Assimp includes
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "LunarConstants.h"

namespace Lunar
{

class CommandListPool;
class DescriptorAllocator;
class PipelineStateManager;

struct ModelVertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 texCoord;
    DirectX::XMFLOAT3 tangent;
    DirectX::XMFLOAT3 bitangent;
    // 애니메이션용
    DirectX::XMUINT4 boneIDs = {0, 0, 0, 0};
    DirectX::XMFLOAT4 boneWeights = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct ModelMesh {
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t materialIndex;
    std::string name;
    
    // DirectX 12 리소스
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexUploadBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexUploadBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
};

struct ModelMaterial {
    std::string name;
    
    // 기본 색상 정보
    DirectX::XMFLOAT3 diffuse = {1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 specular = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 ambient = {0.1f, 0.1f, 0.1f};
    DirectX::XMFLOAT3 emissive = {0.0f, 0.0f, 0.0f};
    
    // PBR 속성
    float metallic = 0.0f;
    float roughness = 0.5f;
    float shininess = 32.0f;
    float opacity = 1.0f;
    float reflectivity = 0.0f;
    
    // 텍스처 맵 경로들
    std::string diffuseTexture;
    std::string normalTexture;
    std::string specularTexture;
    std::string emissiveTexture;
    std::string metallicTexture;
    std::string roughnessTexture;
    std::string aoTexture;
    std::string opacityTexture;
};

struct ModelBone {
    std::string name;
    DirectX::XMMATRIX offsetMatrix;
    int32_t parentIndex = -1;
};

struct ModelAnimation {
    std::string name;
    float duration;
    float ticksPerSecond;
    
    struct NodeAnimation {
        std::string nodeName;
        
        struct VectorKey {
            float time;
            DirectX::XMFLOAT3 value;
        };
        
        struct QuatKey {
            float time;
            DirectX::XMFLOAT4 value;
        };
        
        std::vector<VectorKey> positionKeys;
        std::vector<QuatKey> rotationKeys;
        std::vector<VectorKey> scalingKeys;
    };
    
    std::vector<NodeAnimation> nodeAnimations;
};

struct LoadedModel {
    std::vector<ModelMesh> meshes;
    std::vector<ModelMaterial> materials;
    std::vector<ModelBone> bones;
    std::vector<ModelAnimation> animations;
    DirectX::XMMATRIX globalInverseTransform;
    
    // 본 매핑
    std::unordered_map<std::string, uint32_t> boneMapping;
    
    // 모델 정보
    std::string name;
    std::string filePath;
    bool hasAnimation = false;
};

struct ModelLoadJob {
    ~ModelLoadJob();
    LunarConstants::ModelInfo modelInfo;
    std::promise<bool> completion;
    
    std::unique_ptr<LoadedModel> loadedModel;
    std::chrono::high_resolution_clock::time_point loadStartTime;
    std::chrono::high_resolution_clock::time_point loadEndTime;
};

class AsyncModelLoader
{
public:
    AsyncModelLoader();
    ~AsyncModelLoader();
    
    void Initialize(size_t numWorkerThreads, ID3D12Device* device, 
                   CommandListPool* uploadPool, DescriptorAllocator* descriptorAllocator,
                   ID3D12CommandQueue* commandQueue, ID3D12Fence* fence,
                   PipelineStateManager* pipelineStateManager);
    
    void Shutdown();
    
    std::future<bool> LoadModelAsync(const LunarConstants::ModelInfo& modelInfo);
    
    bool IsModelReady(const std::string& modelName) const;
    LoadedModel* GetModel(const std::string& modelName) const;
    
    bool AreAllModelsLoaded() const;
    float GetLoadingProgress() const;
    int GetTotalModels() const { return m_totalModels.load(); }
    int GetLoadedModels() const { return m_loadedModels.load(); }
    
    struct LoadingStats {
        size_t queuedJobs = 0;
        size_t completedLoads = 0;
        size_t failedJobs = 0;
        float averageLoadTime = 0.0f;
        float totalProcessingTime = 0.0f;
    };
    LoadingStats GetStats() const;
    
    void LogStatus() const;
    
private:
    bool IsInitialized() const;
    void WorkerThreadFunction();
    void ProcessModelLoadJob(std::unique_ptr<ModelLoadJob> job);
    
    // Assimp 로딩 함수들
    bool LoadModelFromFile(const std::string& filePath, LoadedModel& outModel);
    void ProcessNode(aiNode* node, const aiScene* scene, LoadedModel& model);
    ModelMesh ProcessMesh(aiMesh* mesh, const aiScene* scene, LoadedModel& model);
    void LoadMaterials(const aiScene* scene, LoadedModel& model);
    void LoadBones(aiMesh* mesh, LoadedModel& model, std::vector<ModelVertex>& vertices);
    void LoadAnimations(const aiScene* scene, LoadedModel& model);
    
    // DirectX 12 리소스 생성
    bool CreateVertexBuffer(ModelMesh& mesh);
    bool CreateIndexBuffer(ModelMesh& mesh);
    
    // 유틸리티 함수들
    DirectX::XMMATRIX ConvertAssimpMatrix(const aiMatrix4x4& mat);
    DirectX::XMFLOAT3 ConvertAssimpVector3(const aiVector3D& vec);
    DirectX::XMFLOAT4 ConvertAssimpQuaternion(const aiQuaternion& quat);
    
    std::vector<std::thread> m_workerThreads;
    std::atomic<bool> m_shouldStop{false};
    
    std::queue<std::unique_ptr<ModelLoadJob>> m_jobQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    
    std::unordered_map<std::string, std::unique_ptr<LoadedModel>> m_models;
    mutable std::mutex m_loadedModelsMutex;
    
    // GPU 리소스
    ID3D12Device* m_device = nullptr;
    CommandListPool* m_uploadPool = nullptr;
    DescriptorAllocator* m_descriptorAllocator = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    ID3D12Fence* m_fence = nullptr;
    PipelineStateManager* m_pipelineStateManager = nullptr;
    
    // 통계
    mutable std::mutex m_statsMutex;
    LoadingStats m_stats;
    
    // 완료 추적
    std::atomic<int> m_totalModels{0};
    std::atomic<int> m_loadedModels{0};
};

} // namespace Lunar
