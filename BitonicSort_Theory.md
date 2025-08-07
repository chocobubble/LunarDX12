# Bitonic Sort 이론 및 GPU 구현 가이드

## 목차
1. [Bitonic Sort란?](#bitonic-sort란)
2. [Bitonic Sequence의 이해](#bitonic-sequence의-이해)
3. [알고리즘 동작 원리](#알고리즘-동작-원리)
4. [GPU 병렬화 전략](#gpu-병렬화-전략)
5. [성능 분석](#성능-분석)
6. [실제 구현 예시](#실제-구현-예시)

---

## Bitonic Sort란?

**Bitonic Sort**는 병렬 처리에 최적화된 정렬 알고리즘으로, GPU의 SIMD(Single Instruction, Multiple Data) 아키텍처에 매우 적합합니다.

### 주요 특징
- **시간 복잡도**: O(log²n)
- **공간 복잡도**: O(1) - in-place 정렬
- **병렬성**: 완벽한 병렬 처리 가능
- **제약사항**: 배열 크기가 2의 거듭제곱이어야 함

---

## Bitonic Sequence의 이해

### Bitonic Sequence 정의
**Bitonic Sequence**는 다음 중 하나의 형태를 가지는 수열입니다:
1. 먼저 증가하다가 감소하는 수열
2. 먼저 감소하다가 증가하는 수열
3. 위 두 형태를 순환 이동한 수열

### 예시
```
증가 후 감소: [1, 3, 7, 9, 6, 4, 2]
감소 후 증가: [9, 6, 4, 2, 1, 3, 7]
단조 증가:   [1, 2, 3, 4, 5, 6, 7] (특수한 경우)
단조 감소:   [7, 6, 5, 4, 3, 2, 1] (특수한 경우)
```

### Bitonic Split의 핵심 성질
**중요한 성질**: Bitonic sequence를 반으로 나누어 각 절반의 대응되는 요소들을 비교하여 교환하면, 두 개의 Bitonic sequence가 생성되며, 한쪽은 다른 쪽보다 모든 요소가 작습니다.

```
원본 Bitonic: [1, 4, 7, 9, 6, 3, 2, 0]
절반으로 분할: [1, 4, 7, 9] | [6, 3, 2, 0]
비교 후:      [1, 3, 2, 0] | [6, 4, 7, 9]
결과: 왼쪽 모든 요소 ≤ 오른쪽 모든 요소
```

---

## 알고리즘 동작 원리

### 1단계: 작은 Bitonic Sequence 생성
```
초기 배열: [3, 7, 4, 8, 6, 2, 1, 5]

크기 2의 bitonic sequence 생성:
[3, 7] → [3, 7] (오름차순)
[4, 8] → [8, 4] (내림차순)
[6, 2] → [2, 6] (오름차순)
[1, 5] → [5, 1] (내림차순)

결과: [3, 7, 8, 4, 2, 6, 5, 1]
```

### 2단계: 더 큰 Bitonic Sequence로 병합
```
크기 4의 bitonic sequence 생성:
[3, 7, 8, 4] → bitonic split → [3, 4, 7, 8] (정렬됨)
[2, 6, 5, 1] → bitonic split → [6, 5, 2, 1] (역정렬됨)

결과: [3, 4, 7, 8, 6, 5, 2, 1] (크기 8의 bitonic sequence)
```

### 3단계: 최종 정렬
```
전체 배열을 bitonic split:
[3, 4, 7, 8, 6, 5, 2, 1] → [1, 2, 3, 4, 5, 6, 7, 8]
```

### 비트 연산을 통한 인덱스 계산
```cpp
// level = 현재 처리 중인 bitonic sequence 크기
// levelMask = 현재 비교 거리

for (uint level = 2; level <= arraySize; level *= 2) {
    for (uint levelMask = level/2; levelMask > 0; levelMask /= 2) {
        uint compareIndex = index ^ levelMask;
        
        // 정렬 방향 결정
        bool ascending = ((index & level) == 0);
        
        // 비교 및 교환
        if (shouldSwap(data[index], data[compareIndex], ascending)) {
            swap(data[index], data[compareIndex]);
        }
    }
}
```

---

## GPU 병렬화 전략

### 병렬성의 핵심
1. **동시 비교**: 같은 레벨에서 모든 비교가 독립적으로 수행 가능
2. **메모리 접근 패턴**: 규칙적인 stride로 메모리 접근
3. **동기화**: 각 단계마다 전역 동기화 필요

### 메모리 접근 패턴 최적화

#### 기본 구현 (글로벌 메모리)
```hlsl
[numthreads(256, 1, 1)]
void CSBitonicSort(uint3 id : SV_DispatchThreadID) {
    uint index = id.x;
    uint compareIndex = index ^ g_levelMask;
    
    // 글로벌 메모리에서 직접 읽기/쓰기
    SortElement element1 = g_sortData[index];
    SortElement element2 = g_sortData[compareIndex];
    
    // 비교 및 교환
    if (shouldSwap(element1, element2)) {
        g_sortData[index] = element2;
        g_sortData[compareIndex] = element1;
    }
}
```

#### 최적화된 구현 (공유 메모리)
```hlsl
groupshared SortElement g_sharedData[512];

[numthreads(256, 1, 1)]
void CSBitonicSortOptimized(uint3 id : SV_DispatchThreadID,
                           uint groupIndex : SV_GroupIndex) {
    // 공유 메모리로 데이터 로드
    g_sharedData[groupIndex] = g_sortData[id.x];
    g_sharedData[groupIndex + 256] = g_sortData[id.x + 256];
    
    GroupMemoryBarrierWithGroupSync();
    
    // 로컬에서 bitonic sort 수행
    for (uint level = 2; level <= 512; level *= 2) {
        for (uint levelMask = level/2; levelMask > 0; levelMask /= 2) {
            uint compareIndex = groupIndex ^ levelMask;
            
            if (compareIndex > groupIndex) {
                // 공유 메모리에서 비교 및 교환
                if (shouldSwap(g_sharedData[groupIndex], 
                              g_sharedData[compareIndex])) {
                    swap(g_sharedData[groupIndex], 
                         g_sharedData[compareIndex]);
                }
            }
            
            GroupMemoryBarrierWithGroupSync();
        }
    }
    
    // 결과를 글로벌 메모리에 저장
    g_sortData[id.x] = g_sharedData[groupIndex];
    g_sortData[id.x + 256] = g_sharedData[groupIndex + 256];
}
```

---

## 성능 분석

### 시간 복잡도 분석
- **단계 수**: log₂(n) 개의 레벨
- **각 레벨의 패스 수**: 평균 log₂(n)/2
- **총 시간 복잡도**: O(log²n)

### GPU 성능 특성
```
배열 크기    CPU 정렬 시간    GPU Bitonic Sort    성능 향상
1,024       0.05ms          0.02ms             2.5x
4,096       0.25ms          0.05ms             5.0x
16,384      1.2ms           0.15ms             8.0x
65,536      5.8ms           0.4ms              14.5x
262,144     28ms            1.2ms              23.3x
```

### 메모리 대역폭 활용
```cpp
// 이론적 메모리 대역폭 계산
float theoreticalBandwidth = 
    (numElements * sizeof(SortElement) * numPasses) / executionTime;

// 실제 GPU 메모리 대역폭과 비교
float efficiency = theoreticalBandwidth / gpuMemoryBandwidth;
```

---

## 실제 구현 예시

### CPU 제어 코드
```cpp
class BitonicSorter {
private:
    ComPtr<ID3D12PipelineState> m_generateKeysPSO;
    ComPtr<ID3D12PipelineState> m_bitonicSortPSO;
    ComPtr<ID3D12PipelineState> m_generateIndicesPSO;
    
public:
    void SortParticles(ID3D12GraphicsCommandList* cmdList,
                      uint32_t numParticles,
                      const XMFLOAT3& cameraPos) {
        
        // 1. 2의 거듭제곱으로 크기 조정
        uint32_t sortSize = NextPowerOfTwo(numParticles);
        
        // 2. 정렬 키 생성
        GenerateSortKeys(cmdList, numParticles, cameraPos);
        
        // 3. Bitonic Sort 실행
        ExecuteBitonicSort(cmdList, sortSize);
        
        // 4. 정렬된 인덱스 생성
        GenerateSortedIndices(cmdList, numParticles);
    }
    
private:
    void ExecuteBitonicSort(ID3D12GraphicsCommandList* cmdList, 
                           uint32_t sortSize) {
        
        cmdList->SetPipelineState(m_bitonicSortPSO.Get());
        
        // Bitonic Sort의 이중 루프
        for (uint32_t level = 2; level <= sortSize; level *= 2) {
            for (uint32_t levelMask = level/2; levelMask > 0; levelMask /= 2) {
                
                // 상수 버퍼 업데이트
                SortConstants constants = {
                    .g_numElements = sortSize,
                    .g_level = level,
                    .g_levelMask = levelMask,
                    .g_width = sortSize
                };
                
                UpdateConstantBuffer(constants);
                
                // 컴퓨트 셰이더 디스패치
                uint32_t dispatchSize = (sortSize + 255) / 256;
                cmdList->Dispatch(dispatchSize, 1, 1);
                
                // UAV 배리어로 동기화
                InsertUAVBarrier(cmdList);
            }
        }
    }
};
```

### 디버깅 및 검증
```cpp
void ValidateSortResult() {
    // GPU에서 CPU로 결과 복사 (디버그용)
    std::vector<SortElement> sortedData = ReadbackSortData();
    
    // 정렬 검증
    bool isValid = true;
    for (size_t i = 1; i < sortedData.size(); ++i) {
        if (sortedData[i-1].key < sortedData[i].key) {
            isValid = false;
            break;
        }
    }
    
    if (!isValid) {
        OutputDebugString(L"Bitonic Sort 결과가 올바르지 않습니다!\n");
    }
}
```

---

## 최적화 팁

### 1. 워프 레벨 최적화
```hlsl
// 32개 요소 이하는 워프 내에서 처리
if (g_levelMask <= 16) {
    // 워프 내 shuffle 명령어 사용 가능
    // 공유 메모리 없이도 빠른 교환 가능
}
```

### 2. 메모리 접근 패턴 최적화
```hlsl
// 연속된 메모리 접근을 위한 인덱스 재배치
uint coalescedIndex = (groupIndex / 32) * 64 + (groupIndex % 32);
```

### 3. 조기 종료 최적화
```cpp
// 이미 정렬된 구간은 건너뛰기
if (IsAlreadySorted(startIndex, endIndex)) {
    continue;
}
```

---

## 결론

Bitonic Sort는 GPU의 병렬 처리 능력을 최대한 활용할 수 있는 정렬 알고리즘입니다. 파티클 시스템에서 깊이 정렬이 필요한 경우, CPU-GPU 간 데이터 전송 없이 순수하게 GPU에서만 정렬을 수행할 수 있어 매우 효율적입니다.

특히 실시간 렌더링에서는 정렬의 완벽성보다는 성능이 더 중요한 경우가 많으므로, Bitonic Sort의 O(log²n) 복잡도와 완벽한 병렬성은 매우 유용한 특성입니다.
