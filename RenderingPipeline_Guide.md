# LunarDX12 렌더링 파이프라인 완전 가이드

## 📋 목차
1. [파이프라인 개요](#파이프라인-개요)
2. [1단계: Particle Compute Shader](#1단계-particle-compute-shader)
3. [2단계: Scene Render Target](#2단계-scene-render-target)
4. [3단계: Post-Processing](#3단계-post-processing)
5. [4단계: Present](#4단계-present)
6. [리소스 상태 관리](#리소스-상태-관리)
7. [성능 최적화](#성능-최적화)
8. [디버깅 가이드](#디버깅-가이드)

---

## 파이프라인 개요

```
┌─────────────────────────────────────────────────────────────────┐
│                    LunarDX12 렌더링 파이프라인                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  [Particle Compute] → [Scene RT] → [Post-Processing] → [Present] │
│                                                                 │
│  ┌─────────────┐   ┌─────────────┐   ┌─────────────┐   ┌──────┐ │
│  │   GPU       │   │  Graphics   │   │   Compute   │   │ Swap │ │
│  │ Simulation  │   │ Rendering   │   │ Shaders     │   │Chain │ │
│  │             │   │             │   │             │   │      │ │
│  │ • Physics   │   │ • Geometry  │   │ • Tone Map  │   │ • UI │ │
│  │ • Collision │   │ • Lighting  │   │ • Bloom     │   │ •Show│ │
│  │ • Forces    │   │ • Shadows   │   │ • Effects   │   │      │ │
│  └─────────────┘   └─────────────┘   └─────────────┘   └──────┘ │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 1단계: Particle Compute Shader

### 🎯 목적
- GPU에서 파티클 물리 시뮬레이션 수행
- CPU-GPU 동기화 최소화
- 대량의 파티클 병렬 처리

### 📊 데이터 구조

```cpp
// ParticleSystem.h
struct Particle {
    float position[3];  // 위치
    float velocity[3];  // 속도  
    float force[3];     // 힘 (중력 등)
    float color[4];     // 색상
    float lifetime;     // 수명
    float age;          // 현재 나이
};

// 더블 버퍼링
Microsoft::WRL::ComPtr<ID3D12Resource> m_particleBuffers[2];
int m_currentBuffer = 0;
```

### 🔄 실행 과정

#### **1.1 Resource State 준비**
```cpp
// ParticleSystem::Update()
void ParticleSystem::Update(float deltaTime, ID3D12GraphicsCommandList* cmdList)
{
    int inputBufferIndex = m_currentBuffer;
    int outputBufferIndex = 1 - m_currentBuffer;
    
    // Input Buffer: GENERIC_READ 상태 확인
    if (m_bufferStates[inputBufferIndex] != D3D12_RESOURCE_STATE_GENERIC_READ) {
        TransitionResource(cmdList, m_particleBuffers[inputBufferIndex].Get(),
                          m_bufferStates[inputBufferIndex], 
                          D3D12_RESOURCE_STATE_GENERIC_READ);
    }
    
    // Output Buffer: UAV 상태로 전환
    if (m_bufferStates[outputBufferIndex] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        TransitionResource(cmdList, m_particleBuffers[outputBufferIndex].Get(),
                          m_bufferStates[outputBufferIndex], 
                          D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
}
```

#### **1.2 Compute Shader 바인딩**
```cpp
// MainApp::Render()
{
    PROFILE_SCOPE(m_performanceProfiler.get(), "Particle Update");
    
    // Compute Root Signature 설정
    m_commandList->SetComputeRootSignature(m_pipelineStateManager->GetRootSignature());
    
    // Compute PSO 설정
    m_commandList->SetPipelineState(m_pipelineStateManager->GetPSO("particlesUpdate"));
    
    // 파티클 시스템 업데이트
    m_sceneRenderer->UpdateParticleSystem(deltaTime, m_commandList.Get());
}
```

#### **1.3 Descriptor 바인딩**
```cpp
// ParticleSystem::Update() 계속
// SRV 바인딩 (Input Buffer - t0, space2)
cmdList->SetComputeRootShaderResourceView(
    LunarConstants::PARTICLE_SRV_ROOT_PARAMETER_INDEX, 
    m_particleBuffers[inputBufferIndex]->GetGPUVirtualAddress()
);

// UAV 바인딩 (Output Buffer - u0)
cmdList->SetComputeRootUnorderedAccessView(
    LunarConstants::PARTICLE_UAV_ROOT_PARAMETER_INDEX, 
    m_particleBuffers[outputBufferIndex]->GetGPUVirtualAddress()
);
```

#### **1.4 Compute Shader 실행**
```hlsl
// ParticlesComputeShader.hlsl
struct Particle {
    float3 position;
    float3 velocity;
    float3 force;
    float4 color;
    float lifetime;
    float age;    
};

StructuredBuffer<Particle> particlesInput : register(t0, space2);
RWStructuredBuffer<Particle> particlesOutput : register(u0);

[numthreads(64, 1, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
    if (index >= 512) return;
    
    Particle particle = particlesInput[index];
    
    float deltaTime = 0.016f;
    particle.age += deltaTime;
    
    if (particle.age < particle.lifetime) {
        // 물리 시뮬레이션
        particle.position += particle.velocity * deltaTime;
        particle.velocity += particle.force * deltaTime;
    }
    
    particlesOutput[index] = particle;
}
```

#### **1.5 GPU 동기화**
```cpp
// Thread Group 계산 및 Dispatch
int threadGroups = (particles.size() + 63) / 64;  // 512 particles = 8 groups
cmdList->Dispatch(threadGroups, 1, 1);

// UAV Barrier (Compute 완료 보장)
D3D12_RESOURCE_BARRIER uavBarrier = {};
uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
uavBarrier.UAV.pResource = m_particleBuffers[outputBufferIndex].Get();
cmdList->ResourceBarrier(1, &uavBarrier);

// Output Buffer를 다음 프레임 SRV로 전환
TransitionResource(cmdList, m_particleBuffers[outputBufferIndex].Get(),
                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                  D3D12_RESOURCE_STATE_GENERIC_READ);

// 버퍼 교체 (Ping-Pong)
m_currentBuffer = outputBufferIndex;
```

---

## 2단계: Scene Render Target

### 🎯 목적
- 메인 씬을 별도 렌더 타겟에 렌더링
- Post-processing을 위한 HDR 데이터 생성
- Back buffer 직접 렌더링 방지

### 📊 Scene RT 구조

```cpp
// PostProcessManager.h
struct ComputeTexture {
    ComPtr<ID3D12Resource> texture;
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE uavHandle;
    DXGI_FORMAT format;
    UINT width, height;
};

ComputeTexture m_sceneTexture;  // Scene Render Target
ComputeTexture m_hdrBuffer[2];  // Post-processing buffers
ComputeTexture m_ldrBuffer;     // Final output
```

### 🔄 실행 과정

#### **2.1 Scene RT 준비**
```cpp
// MainApp::Render()
{
    PROFILE_SCOPE(m_performanceProfiler.get(), "Scene Setup");
    m_postProcessManager->BeginScene(m_commandList.Get());
}
```

#### **2.2 BeginScene 구현**
```cpp
// PostProcessManager::BeginScene()
void PostProcessManager::BeginScene(ID3D12GraphicsCommandList* cmdList)
{
    // Scene texture를 Render Target으로 전환
    TransitionResource(cmdList, m_sceneTexture.texture.Get(), 
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
                      D3D12_RESOURCE_STATE_RENDER_TARGET);
    
    // Depth buffer 준비
    TransitionResource(cmdList, m_depthBuffer.Get(),
                      D3D12_RESOURCE_STATE_DEPTH_READ,
                      D3D12_RESOURCE_STATE_DEPTH_WRITE);
    
    // Render Target 설정
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetSceneRTV();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetSceneDSV();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    
    // Clear
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, 
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
}
```

#### **2.3 Scene 렌더링**
```cpp
// MainApp::Render() 계속
{
    PROFILE_SCOPE(m_performanceProfiler.get(), "Scene Rendering");
    
    // Graphics Root Signature 설정
    m_commandList->SetGraphicsRootSignature(m_pipelineStateManager->GetRootSignature());
    
    // Descriptor Heap 바인딩
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_sceneRenderer->GetSRVHeap() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    
    // 텍스처 바인딩
    D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle = 
        m_sceneRenderer->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();
    m_commandList->SetGraphicsRootDescriptorTable(
        LunarConstants::TEXTURE_SR_ROOT_PARAMETER_INDEX, descriptorHandle);
    
    // 지오메트리 렌더링
    m_sceneRenderer->RenderScene(m_commandList.Get());
}
```

#### **2.4 Particle 렌더링**
```cpp
{
    PROFILE_SCOPE(m_performanceProfiler.get(), "Particle Rendering");
    
    // Particle Graphics PSO 설정
    m_commandList->SetPipelineState(m_pipelineStateManager->GetPSO("particles"));
    
    // 파티클 렌더링 (현재 업데이트된 버퍼 사용)
    m_sceneRenderer->RenderParticles(m_commandList.Get());
}
```

#### **2.5 Scene RT 마무리**
```cpp
// MainApp::Render() 계속
{
    PROFILE_SCOPE(m_performanceProfiler.get(), "Scene Finalize");
    m_postProcessManager->EndScene(m_commandList.Get());
}
```

#### **2.6 EndScene 구현**
```cpp
// PostProcessManager::EndScene()
void PostProcessManager::EndScene(ID3D12GraphicsCommandList* cmdList)
{
    // Scene texture를 SRV로 전환 (Post-processing에서 읽기 위해)
    TransitionResource(cmdList, m_sceneTexture.texture.Get(), 
                      D3D12_RESOURCE_STATE_RENDER_TARGET, 
                      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    
    // Depth buffer를 SRV로 전환
    TransitionResource(cmdList, m_depthBuffer.Get(),
                      D3D12_RESOURCE_STATE_DEPTH_WRITE,
                      D3D12_RESOURCE_STATE_DEPTH_READ);
}
```

---

## 3단계: Post-Processing

### 🎯 목적
- HDR 톤 매핑
- Bloom, Blur 등 시각 효과
- NPR (Non-Photorealistic Rendering) 효과
- 최종 LDR 이미지 생성

### 📊 Post-Processing 구조

```cpp
// ConstantBuffers.h - 모든 상수 버퍼 구조체 통합 관리
struct PostProcessConstants {
    float exposure = 1.0f;
    float gamma = 2.2f;
    float bloomThreshold = 1.0f;
    float bloomIntensity = 0.8f;
    uint32_t colorLevels = 4;
    float edgeThreshold = 0.1f;
    // ... 기타 파라미터들
};

// PostProcessManager.h
enum class PostProcessEffect {
    None,
    ToneMapping,
    Bloom,
    ToonShading,
    EdgeDetection,
    Halftone,
    Watercolor,
    Count
};
```

### 🔄 실행 과정

#### **3.1 Post-Processing 시작**
```cpp
// MainApp::Render()
{
    PROFILE_SCOPE(m_performanceProfiler.get(), "Post Processing");
    m_postProcessManager->ApplyPostEffects(m_commandList.Get());
}
```

#### **3.2 ApplyPostEffects 구현**
```cpp
// PostProcessManager::ApplyPostEffects()
void PostProcessManager::ApplyPostEffects(ID3D12GraphicsCommandList* cmdList)
{
    if (m_activeEffect == PostProcessEffect::None) {
        // 효과 없음 - 직접 복사
        CopySceneToBackBuffer(cmdList);
        return;
    }
    
    // Constant Buffer 업데이트
    UpdateConstantBuffer();
    
    // Compute Descriptor Heap 설정
    ID3D12DescriptorHeap* heaps[] = { m_srvUavHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    
    // Compute Root Signature 설정
    cmdList->SetComputeRootSignature(m_computeRootSignature.Get());
    
    // 1. Scene → HDR Buffer 복사
    CopySceneToHDR(cmdList);
    
    // 2. 선택된 효과 적용
    switch (m_activeEffect) {
        case PostProcessEffect::ToneMapping:
            ApplyToneMapping(cmdList);
            break;
        case PostProcessEffect::Bloom:
            ApplyBloom(cmdList);
            break;
        // ... 기타 효과들
    }
    
    // 3. 최종 결과를 Back Buffer로 복사
    CopyToBackBuffer(cmdList);
}
```

#### **3.3 Scene → HDR 복사**
```cpp
void PostProcessManager::CopySceneToHDR(ID3D12GraphicsCommandList* cmdList)
{
    // HDR Buffer 1을 UAV로 전환
    TransitionResource(cmdList, m_hdrBuffer[0].texture.Get(),
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    
    // Copy Compute Shader PSO 설정
    cmdList->SetPipelineState(m_computePSOs["Copy"].Get());
    
    // Scene texture SRV 바인딩 (t0)
    D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV = GetSceneTextureSRV();
    cmdList->SetComputeRootDescriptorTable(1, sceneSRV);
    
    // HDR Buffer UAV 바인딩 (u0)
    D3D12_GPU_DESCRIPTOR_HANDLE hdrUAV = m_hdrBuffer[0].uavGpuHandle;
    cmdList->SetComputeRootDescriptorTable(2, hdrUAV);
    
    // Dispatch
    UINT dispatchX = GetDispatchSize(m_width, 8);
    UINT dispatchY = GetDispatchSize(m_height, 8);
    cmdList->Dispatch(dispatchX, dispatchY, 1);
    
    // UAV Barrier
    UAVBarrier(cmdList, m_hdrBuffer[0].texture.Get());
}
```

#### **3.4 Tone Mapping 적용**
```cpp
void PostProcessManager::ApplyToneMapping(ID3D12GraphicsCommandList* cmdList)
{
    // Tone Mapping PSO 설정
    cmdList->SetPipelineState(m_computePSOs["ToneMapping"].Get());
    
    // Constant Buffer 바인딩
    cmdList->SetComputeRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());
    
    // Input: HDR Buffer 1 (SRV)
    cmdList->SetComputeRootDescriptorTable(1, m_hdrBuffer[0].srvGpuHandle);
    
    // Output: LDR Buffer (UAV)
    TransitionResource(cmdList, m_ldrBuffer.texture.Get(),
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmdList->SetComputeRootDescriptorTable(2, m_ldrBuffer.uavGpuHandle);
    
    // Dispatch
    UINT dispatchX = GetDispatchSize(m_width, 8);
    UINT dispatchY = GetDispatchSize(m_height, 8);
    cmdList->Dispatch(dispatchX, dispatchY, 1);
    
    // UAV Barrier
    UAVBarrier(cmdList, m_ldrBuffer.texture.Get());
}
```

#### **3.5 Tone Mapping Compute Shader**
```hlsl
// ToneMapping.hlsl
cbuffer PostProcessConstants : register(b0) {
    float exposure;
    float gamma;
    // ... 기타 상수들
};

Texture2D<float4> inputTexture : register(t0);
RWTexture2D<float4> outputTexture : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= textureWidth || id.y >= textureHeight) return;
    
    // HDR 색상 읽기
    float4 hdrColor = inputTexture[id.xy];
    
    // Exposure 적용
    hdrColor.rgb *= exposure;
    
    // ACES Filmic Tone Mapping
    float3 color = hdrColor.rgb;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    
    color = (color * (a * color + b)) / (color * (c * color + d) + e);
    
    // Gamma Correction
    color = pow(color, 1.0f / gamma);
    
    // LDR 출력
    outputTexture[id.xy] = float4(color, hdrColor.a);
}
```

#### **3.6 Bloom 효과 (다중 패스)**
```cpp
void PostProcessManager::ApplyBloom(ID3D12GraphicsCommandList* cmdList)
{
    // Pass 1: Bright Pass (밝은 부분 추출)
    ApplyPass(cmdList, "BloomBrightPass");
    SwapBuffers();
    
    // Pass 2: Horizontal Blur
    SetBlurDirection(1.0f, 0.0f);
    ApplyPass(cmdList, "GaussianBlur");
    SwapBuffers();
    
    // Pass 3: Vertical Blur
    SetBlurDirection(0.0f, 1.0f);
    ApplyPass(cmdList, "GaussianBlur");
    SwapBuffers();
    
    // Pass 4: Combine with original
    ApplyPass(cmdList, "BloomCombine");
}
```

---

## 4단계: Present

### 🎯 목적
- 최종 이미지를 Back Buffer로 복사
- UI 렌더링
- 화면에 출력

### 🔄 실행 과정

#### **4.1 Back Buffer로 복사**
```cpp
void PostProcessManager::CopyToBackBuffer(ID3D12GraphicsCommandList* cmdList)
{
    // Back Buffer 상태 확인 및 전환
    // (일반적으로 Present 상태에서 Render Target으로)
    
    // Copy 또는 Fullscreen Quad 렌더링
    // LDR Buffer → Back Buffer
    
    // 구현 방법 1: Compute Shader Copy
    // 구현 방법 2: Fullscreen Triangle 렌더링
}
```

#### **4.2 UI 렌더링**
```cpp
// MainApp::Render()
{
    PROFILE_SCOPE(m_performanceProfiler.get(), "Back Buffer Setup");
    
    // Back Buffer를 Render Target으로 설정
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = 
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    renderTargetViewHandle.ptr += m_frameIndex * m_renderTargetViewDescriptorSize;
    
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = 
        m_sceneRenderer->GetDSVHeap()->GetCPUDescriptorHandleForHeapStart();
    
    m_commandList->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, &depthStencilViewHandle);
}

{
    PROFILE_SCOPE(m_performanceProfiler.get(), "GUI Rendering");
    
    // ImGui 렌더링
    m_gui->Render(deltaTime);
    
    // ImGui Descriptor Heap 설정
    ID3D12DescriptorHeap* heaps[] = { m_imGuiDescriptorHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    
    // ImGui 렌더 데이터 출력
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
}
```

#### **4.3 Command List 실행**
```cpp
// MainApp::Render() 마지막
// Command List 종료
THROW_IF_FAILED(m_commandList->Close());

// Command Queue에 제출
ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

// Present (화면 출력)
THROW_IF_FAILED(m_swapChain->Present(1, 0));
```

#### **4.4 GPU 동기화**
```cpp
// 다음 프레임을 위한 동기화
const UINT64 currentFenceValue = m_fenceValue;
m_commandQueue->Signal(m_fence.Get(), currentFenceValue);
m_fenceValue++;

// GPU 완료 대기
if (m_fence->GetCompletedValue() < currentFenceValue) {
    THROW_IF_FAILED(m_fence->SetEventOnCompletion(currentFenceValue, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
}
```

---

## 리소스 상태 관리

### 📊 주요 리소스 상태 전환

```cpp
// 파티클 버퍼
GENERIC_READ → UNORDERED_ACCESS → GENERIC_READ

// Scene Render Target  
UNORDERED_ACCESS → RENDER_TARGET → NON_PIXEL_SHADER_RESOURCE

// HDR/LDR 버퍼
UNORDERED_ACCESS ↔ NON_PIXEL_SHADER_RESOURCE

// Depth Buffer
DEPTH_READ → DEPTH_WRITE → DEPTH_READ

// Back Buffer
PRESENT → RENDER_TARGET → PRESENT
```

### 🔧 Resource Barrier 패턴

```cpp
void TransitionResource(ID3D12GraphicsCommandList* cmdList,
                       ID3D12Resource* resource,
                       D3D12_RESOURCE_STATES before,
                       D3D12_RESOURCE_STATES after)
{
    if (before == after) return;
    
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    cmdList->ResourceBarrier(1, &barrier);
}
```

---

## 성능 최적화

### 🚀 최적화 포인트

1. **Ping-Pong 버퍼링**
   - 메모리 사용량 최소화
   - GPU 대역폭 효율성

2. **Compute Shader Thread Group**
   - 8×8×1 = 64 threads (권장)
   - Warp/Wavefront 효율성

3. **Resource State 추적**
   - 불필요한 Barrier 방지
   - State 캐싱

4. **Descriptor 관리**
   - 통합 Descriptor Heap
   - 동적 바인딩

### 📈 성능 측정

```cpp
// PerformanceProfiler 사용
PROFILE_SCOPE(m_performanceProfiler.get(), "Particle Update");
PROFILE_SCOPE(m_performanceProfiler.get(), "Scene Rendering");
PROFILE_SCOPE(m_performanceProfiler.get(), "Post Processing");
```

---

## 디버깅 가이드

### 🔍 RenderDoc 사용법

1. **Event Browser**에서 각 단계 확인
2. **Pipeline State**에서 바인딩 상태 확인
3. **Resource Inspector**에서 텍스처 내용 확인
4. **Errors/Warnings** 탭에서 D3D12 오류 확인

### 🐛 일반적인 문제들

1. **Resource State 불일치**
   - 해결: State 추적 배열 사용

2. **Descriptor 바인딩 오류**
   - 해결: GPU Handle 검증

3. **UAV Barrier 누락**
   - 해결: Compute Shader 후 명시적 Barrier

4. **Thread Group 계산 오류**
   - 해결: `(size + groupSize - 1) / groupSize`

---

## 📝 요약

이 파이프라인은 현대적인 GPU 렌더링의 핵심 개념들을 모두 포함합니다:

- **Compute Shader**: GPU 병렬 처리
- **Render Target**: 오프스크린 렌더링
- **Post-Processing**: 이미지 효과
- **Resource Management**: 효율적인 메모리 사용

각 단계는 독립적이면서도 서로 연결되어 있어, 하나의 문제가 전체 파이프라인에 영향을 줄 수 있습니다. 따라서 **단계별 검증**과 **성능 프로파일링**이 중요합니다.

---

*이 문서는 LunarDX12 프로젝트의 렌더링 파이프라인을 완전히 이해하고 디버깅하는 데 도움이 되도록 작성되었습니다.*
