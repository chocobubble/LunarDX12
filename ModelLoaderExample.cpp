// AsyncModelLoader 사용 예제 - FBX 애니메이션 중심
// 이 파일은 참고용이며, 실제 프로젝트에는 포함되지 않습니다.

#include "AsyncModelLoader.h"
#include "CommandListPool.h"
#include "DescriptorAllocator.h"
#include "PipelineStateManager.h"

using namespace Lunar;

class ModelLoaderExample 
{
private:
    std::unique_ptr<AsyncModelLoader> m_modelLoader;
    
public:
    void Initialize(ID3D12Device* device, CommandListPool* commandPool, 
                   DescriptorAllocator* descriptorAllocator, 
                   ID3D12CommandQueue* commandQueue, ID3D12Fence* fence,
                   PipelineStateManager* pipelineStateManager)
    {
        // AsyncModelLoader 초기화
        m_modelLoader = std::make_unique<AsyncModelLoader>();
        m_modelLoader->Initialize(
            4,  // 워커 스레드 수
            device,
            commandPool,
            descriptorAllocator,
            commandQueue,
            fence,
            pipelineStateManager
        );
        
        // 모델들을 비동기로 로드
        LoadModelsAsync();
    }
    
    void LoadModelsAsync()
    {
        // LunarConstants에 정의된 모델들을 로드
        for (const auto& modelInfo : LunarConstants::MODEL_INFO) {
            auto future = m_modelLoader->LoadModelAsync(modelInfo);
            
            // 필요하다면 future를 저장해서 나중에 완료 여부 확인 가능
            // m_loadingFutures.push_back(std::move(future));
        }
    }
    
    void Update()
    {
        // 로딩 진행률 확인
        float progress = m_modelLoader->GetLoadingProgress();
        if (progress < 1.0f) {
            // 아직 로딩 중
            LOG_INFO("Model loading progress: ", progress * 100.0f, "%");
        }
        
        // 정적 모델 (OBJ) 사용
        if (m_modelLoader->IsModelReady("cube")) {
            LoadedModel* cubeModel = m_modelLoader->GetModel("cube");
            if (cubeModel) {
                RenderStaticModel(cubeModel);
            }
        }
        
        // 애니메이션 모델 (FBX) 사용
        if (m_modelLoader->IsModelReady("character")) {
            LoadedModel* characterModel = m_modelLoader->GetModel("character");
            if (characterModel && characterModel->hasAnimation) {
                RenderAnimatedModel(characterModel);
            }
        }
        
        // 건물 모델 (FBX, 정적)
        if (m_modelLoader->IsModelReady("building")) {
            LoadedModel* buildingModel = m_modelLoader->GetModel("building");
            if (buildingModel) {
                RenderStaticModel(buildingModel);
            }
        }
    }
    
    void RenderStaticModel(LoadedModel* model)
    {
        // 정적 모델 렌더링 (OBJ, 정적 FBX)
        for (const auto& mesh : model->meshes) {
            // 버텍스 버퍼 설정
            // commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
            
            // 인덱스 버퍼 설정
            // commandList->IASetIndexBuffer(&mesh.indexBufferView);
            
            // 머티리얼 설정
            if (mesh.materialIndex < model->materials.size()) {
                const auto& material = model->materials[mesh.materialIndex];
                // 머티리얼 상수 버퍼 업데이트
                // UpdateMaterialConstants(material);
            }
            
            // 드로우 콜
            // commandList->DrawIndexedInstanced(
            //     static_cast<UINT>(mesh.indices.size()), 1, 0, 0, 0);
        }
    }
    
    void RenderAnimatedModel(LoadedModel* model)
    {
        // 애니메이션 모델 렌더링 (FBX)
        
        // 1. 애니메이션 업데이트 (향후 구현)
        // UpdateAnimation(model, currentTime);
        
        // 2. 본 변환 행렬 계산 (향후 구현)
        // CalculateBoneTransforms(model);
        
        // 3. 스키닝된 메시 렌더링
        for (const auto& mesh : model->meshes) {
            // 버텍스 버퍼 설정 (본 가중치 포함)
            // commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
            
            // 인덱스 버퍼 설정
            // commandList->IASetIndexBuffer(&mesh.indexBufferView);
            
            // 본 변환 행렬 상수 버퍼 설정
            // commandList->SetGraphicsRoot32BitConstants(BONE_MATRICES_ROOT_INDEX, 
            //     sizeof(XMMATRIX) * model->bones.size() / 4, 
            //     boneMatrices, 0);
            
            // 머티리얼 설정
            if (mesh.materialIndex < model->materials.size()) {
                const auto& material = model->materials[mesh.materialIndex];
                // UpdateMaterialConstants(material);
            }
            
            // 스키닝 셰이더로 드로우
            // commandList->DrawIndexedInstanced(
            //     static_cast<UINT>(mesh.indices.size()), 1, 0, 0, 0);
        }
    }
    
    void Shutdown()
    {
        if (m_modelLoader) {
            m_modelLoader->Shutdown();
            m_modelLoader.reset();
        }
    }
    
    // 통계 정보 출력
    void LogStats()
    {
        if (m_modelLoader) {
            m_modelLoader->LogStatus();
            
            // FBX 모델 특별 정보 출력
            if (m_modelLoader->IsModelReady("character")) {
                LoadedModel* character = m_modelLoader->GetModel("character");
                LOG_INFO("Character Model Info:");
                LOG_INFO("  Has Animation: ", character->hasAnimation);
                LOG_INFO("  Bone Count: ", character->bones.size());
                LOG_INFO("  Animation Count: ", character->animations.size());
            }
        }
    }
};

// FBX 중심 사용 방법:
/*
1. AsyncModelLoader 초기화
2. LoadModelAsync()로 FBX/OBJ 모델들을 비동기 로드
3. Update()에서 로딩 진행률 확인
4. IsModelReady()로 특정 모델 준비 상태 확인
5. GetModel()로 로드된 모델 데이터 가져오기
6. 정적 모델: RenderStaticModel() 사용
7. 애니메이션 모델: RenderAnimatedModel() 사용 (향후 애니메이션 시스템 구현 시)

FBX 모델의 장점:
- 애니메이션 완전 지원
- 복잡한 머티리얼 지원
- 본 계층구조 지원
- Maya/3ds Max 표준 포맷
*/
