#pragma once
#include <string>
#include <vector>
#include <memory>
#include <future>
#include <unordered_map>
#include <DirectXMath.h>

// Assimp includes
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Lunar
{

// 전방 선언
class TextureManager;
class AsyncTextureLoader;

// 기본 구조체들
struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 texCoord;
    DirectX::XMFLOAT3 tangent;
    DirectX::XMFLOAT3 bitangent;
    
    // 스켈레탈 애니메이션용
    DirectX::XMINT4 boneIDs = {-1, -1, -1, -1};
    DirectX::XMFLOAT4 boneWeights = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct Material
{
    std::string name;
    
    // PBR 머티리얼 속성
    DirectX::XMFLOAT3 albedo = {1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    DirectX::XMFLOAT3 emission = {0.0f, 0.0f, 0.0f};
    
    // 텍스처 경로들
    std::string albedoTexture;
    std::string normalTexture;
    std::string metallicTexture;
    std::string roughnessTexture;
    std::string aoTexture;
    std::string emissionTexture;
    std::string heightTexture;
    
    // 추가 속성들
    float opacity = 1.0f;
    bool doubleSided = false;
    bool alphaTest = false;
    float alphaTestThreshold = 0.5f;
};

struct Mesh
{
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<UINT> indices;
    UINT materialIndex = 0;
    
    // 바운딩 박스
    DirectX::XMFLOAT3 minBounds;
    DirectX::XMFLOAT3 maxBounds;
    
    // GPU 리소스 (나중에 생성)
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
};

// 스켈레탈 애니메이션 구조체들
struct Bone
{
    std::string name;
    int id = -1;
    DirectX::XMFLOAT4X4 offsetMatrix;  // 본 공간 -> 메시 공간 변환
    int parentIndex = -1;
    std::vector<int> childIndices;
};

struct Skeleton
{
    std::vector<Bone> bones;
    std::unordered_map<std::string, int> boneNameToIndex;
    DirectX::XMFLOAT4X4 globalInverseTransform;
    int rootBoneIndex = -1;
};

// 애니메이션 키프레임
struct PositionKey
{
    float time;
    DirectX::XMFLOAT3 position;
};

struct RotationKey
{
    float time;
    DirectX::XMFLOAT4 rotation; // 쿼터니언
};

struct ScaleKey
{
    float time;
    DirectX::XMFLOAT3 scale;
};

struct BoneAnimation
{
    std::string boneName;
    std::vector<PositionKey> positionKeys;
    std::vector<RotationKey> rotationKeys;
    std::vector<ScaleKey> scaleKeys;
    
    DirectX::XMFLOAT3 InterpolatePosition(float time) const;
    DirectX::XMFLOAT4 InterpolateRotation(float time) const;
    DirectX::XMFLOAT3 InterpolateScale(float time) const;
};

struct AnimationClip
{
    std::string name;
    float duration;              // 애니메이션 길이 (초)
    float ticksPerSecond;        // 초당 틱 수
    std::vector<BoneAnimation> boneAnimations;
    std::unordered_map<std::string, int> boneAnimationMap; // 본 이름 -> 인덱스
};

// 모델 데이터 전체
struct ModelData
{
    std::string name;
    std::string filePath;
    
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::optional<Skeleton> skeleton;
    std::vector<AnimationClip> animations;
    
    // 전체 바운딩 박스
    DirectX::XMFLOAT3 minBounds;
    DirectX::XMFLOAT3 maxBounds;
    
    // 로딩 통계
    struct LoadingStats
    {
        float loadTimeMs = 0.0f;
        UINT vertexCount = 0;
        UINT triangleCount = 0;
        UINT textureCount = 0;
        UINT boneCount = 0;
        UINT animationCount = 0;
    } stats;
};

class ModelLoader
{
public:
    struct LoadingSettings
    {
        bool generateNormals = true;
        bool generateTangents = true;
        bool optimizeMeshes = true;
        bool flipUVs = false;
        bool pretransformVertices = false;
        bool limitBoneWeights = true;
        UINT maxBoneWeights = 4;
        bool validateDataStructure = true;
        float smoothingAngle = 80.0f; // 도 단위
    };

    ModelLoader();
    ~ModelLoader();

    void Initialize(TextureManager* textureManager, AsyncTextureLoader* asyncTextureLoader);
    void Shutdown();

    // 동기 로딩
    bool LoadModel(const std::string& filePath, ModelData& outData, const LoadingSettings& settings = {});
    
    // 비동기 로딩
    std::future<std::unique_ptr<ModelData>> LoadModelAsync(const std::string& filePath, const LoadingSettings& settings = {});
    
    // GPU 리소스 생성 (별도 호출 필요)
    bool CreateGPUResources(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ModelData& modelData);
    
    // 유틸리티 함수들
    static bool IsAnimatedModel(const std::string& filePath);
    static std::vector<std::string> GetSupportedFormats();
    
    // 디버깅
    void LogModelInfo(const ModelData& modelData) const;

private:
    // Assimp 처리 함수들
    bool ProcessScene(const aiScene* scene, const std::string& directory, ModelData& modelData, const LoadingSettings& settings);
    void ProcessNode(aiNode* node, const aiScene* scene, const std::string& directory, ModelData& modelData, const LoadingSettings& settings);
    
    Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene, const LoadingSettings& settings);
    Material ProcessMaterial(aiMaterial* mat, const std::string& directory);
    
    void ProcessSkeleton(const aiScene* scene, ModelData& modelData);
    void ProcessAnimations(const aiScene* scene, ModelData& modelData);
    
    // 헬퍼 함수들
    std::string LoadTextureFromMaterial(aiMaterial* mat, aiTextureType type, const std::string& directory);
    DirectX::XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& matrix);
    DirectX::XMFLOAT3 ConvertVector3(const aiVector3D& vector);
    DirectX::XMFLOAT4 ConvertQuaternion(const aiQuaternion& quat);
    
    void CalculateBoundingBox(const std::vector<Vertex>& vertices, DirectX::XMFLOAT3& minBounds, DirectX::XMFLOAT3& maxBounds);
    void OptimizeMesh(Mesh& mesh);
    void ValidateModel(const ModelData& modelData) const;
    
    // 본 처리
    void ExtractBoneWeights(aiMesh* mesh, std::vector<Vertex>& vertices, const std::unordered_map<std::string, int>& boneMap);
    void BuildBoneHierarchy(aiNode* node, const aiScene* scene, Skeleton& skeleton, int parentIndex = -1);
    
    // 리소스
    Assimp::Importer m_importer;
    TextureManager* m_textureManager = nullptr;
    AsyncTextureLoader* m_asyncTextureLoader = nullptr;
    
    // 설정
    LoadingSettings m_defaultSettings;
    
    // 캐시 (같은 모델 중복 로딩 방지)
    std::unordered_map<std::string, std::weak_ptr<ModelData>> m_modelCache;
    
    // 통계
    struct LoaderStats
    {
        UINT totalModelsLoaded = 0;
        UINT totalVerticesLoaded = 0;
        UINT totalTrianglesLoaded = 0;
        float totalLoadTime = 0.0f;
    } m_stats;
};

} // namespace Lunar
