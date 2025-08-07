struct Particle
{
    float3 position;
    float life;
    float3 velocity;
    float maxLife;
    float4 color;
    float size;
    uint active;
    uint2 padding;
};

struct SortElement
{
    float key;          
    uint originalIndex; 
};

cbuffer SortConstants : register(b0)
{
    uint g_numElements;     
    uint g_level;           
    uint g_levelMask;       
    uint g_width;           
    float3 g_cameraPos;     
    float g_padding;
};

StructuredBuffer<Particle> g_inputParticles : register(t0);
RWStructuredBuffer<SortElement> g_sortData : register(u0);
RWStructuredBuffer<uint> g_sortedIndices : register(u1);

[numthreads(256, 1, 1)]
void CSGenerateSortKeys(uint3 id : SV_DispatchThreadID)
{
    uint index = id.x;
    if (index >= g_numElements) return;
    
    Particle particle = g_inputParticles[index];
    
    // 비활성 파티클은 맨 뒤로 보내기
    if (particle.active == 0)
    {
        g_sortData[index].key = 999999.0f;
        g_sortData[index].originalIndex = index;
        return;
    }
    
    // 카메라로부터의 거리 계산 (제곱 거리 사용으로 성능 최적화)
    float3 toCamera = particle.position - g_cameraPos;
    float distanceSquared = dot(toCamera, toCamera);
    
    // 뒤에서 앞으로 정렬하기 위해 거리를 키로 사용
    g_sortData[index].key = distanceSquared;
    g_sortData[index].originalIndex = index;
}

//=============================================================================
// 2단계: Bitonic Sort 메인 알고리즘
//=============================================================================

[numthreads(256, 1, 1)]
void CSBitonicSort(uint3 id : SV_DispatchThreadID)
{
    uint index = id.x;
    if (index >= g_width) return;
    
    // Bitonic Sort의 핵심: XOR 연산으로 비교 대상 찾기
    uint compareIndex = index ^ g_levelMask;
    
    // 중복 비교 방지: 자신보다 큰 인덱스와만 비교
    if (compareIndex <= index) return;
    if (compareIndex >= g_width) return;
    
    // 두 요소 로드
    SortElement element1 = g_sortData[index];
    SortElement element2 = g_sortData[compareIndex];
    
    // 정렬 방향 결정
    // g_level 비트에 따라 오름차순(0) 또는 내림차순(1) 결정
    bool ascending = ((index & g_level) == 0);
    
    // 교환 조건 확인
    bool shouldSwap = (element1.key > element2.key) == ascending;
    
    if (shouldSwap)
    {
        // 원자적 교환
        g_sortData[index] = element2;
        g_sortData[compareIndex] = element1;
    }
}

//=============================================================================
// 3단계: 최적화된 Bitonic Sort (공유 메모리 사용)
//=============================================================================

#define BITONIC_BLOCK_SIZE 512
#define HALF_BLOCK_SIZE 256

groupshared SortElement g_sharedData[BITONIC_BLOCK_SIZE];

[numthreads(HALF_BLOCK_SIZE, 1, 1)]
void CSBitonicSortOptimized(uint3 id : SV_DispatchThreadID,
                           uint3 groupID : SV_GroupID,
                           uint groupIndex : SV_GroupIndex)
{
    uint globalIndex = id.x;
    uint localIndex = groupIndex;
    
    // 공유 메모리로 데이터 로드 (각 스레드가 2개 요소 처리)
    uint index1 = globalIndex;
    uint index2 = globalIndex + HALF_BLOCK_SIZE;
    
    if (index1 < g_width)
        g_sharedData[localIndex] = g_sortData[index1];
    else
        g_sharedData[localIndex] = { 999999.0f, 0 };
        
    if (index2 < g_width)
        g_sharedData[localIndex + HALF_BLOCK_SIZE] = g_sortData[index2];
    else
        g_sharedData[localIndex + HALF_BLOCK_SIZE] = { 999999.0f, 0 };
    
    GroupMemoryBarrierWithGroupSync();
    
    // 로컬 Bitonic Sort 수행
    for (uint level = 2; level <= BITONIC_BLOCK_SIZE; level *= 2)
    {
        for (uint levelMask = level / 2; levelMask > 0; levelMask /= 2)
        {
            uint compareIndex = localIndex ^ levelMask;
            
            if (compareIndex > localIndex)
            {
                SortElement element1 = g_sharedData[localIndex];
                SortElement element2 = g_sharedData[compareIndex];
                
                bool ascending = ((localIndex & level) == 0);
                bool shouldSwap = (element1.key > element2.key) == ascending;
                
                if (shouldSwap)
                {
                    g_sharedData[localIndex] = element2;
                    g_sharedData[compareIndex] = element1;
                }
            }
            
            GroupMemoryBarrierWithGroupSync();
        }
    }
    
    // 결과를 글로벌 메모리에 저장
    if (index1 < g_width)
        g_sortData[index1] = g_sharedData[localIndex];
    if (index2 < g_width)
        g_sortData[index2] = g_sharedData[localIndex + HALF_BLOCK_SIZE];
}

//=============================================================================
// 4단계: 정렬된 인덱스 생성
//=============================================================================

[numthreads(256, 1, 1)]
void CSGenerateSortedIndices(uint3 id : SV_DispatchThreadID)
{
    uint index = id.x;
    if (index >= g_numElements) return;
    
    // 정렬된 순서대로 원본 인덱스를 저장
    g_sortedIndices[index] = g_sortData[index].originalIndex;
}

//=============================================================================
// 5단계: 디버그용 정렬 상태 확인
//=============================================================================

RWStructuredBuffer<uint> g_debugOutput : register(u2);

[numthreads(256, 1, 1)]
void CSDebugSortState(uint3 id : SV_DispatchThreadID)
{
    uint index = id.x;
    if (index >= g_numElements) return;
    
    // 정렬이 올바르게 되었는지 확인
    if (index > 0)
    {
        float currentKey = g_sortData[index].key;
        float prevKey = g_sortData[index - 1].key;
        
        // 내림차순 정렬이므로 이전 키가 현재 키보다 크거나 같아야 함
        if (prevKey < currentKey)
        {
            // 정렬 오류 발견
            InterlockedAdd(g_debugOutput[0], 1);
        }
    }
    
    // 활성 파티클 수 카운트
    uint originalIndex = g_sortData[index].originalIndex;
    Particle particle = g_inputParticles[originalIndex];
    
    if (particle.active != 0)
    {
        InterlockedAdd(g_debugOutput[1], 1);
    }
}

//=============================================================================
// 6단계: 거리 기반 LOD (Level of Detail) 계산
//=============================================================================

RWStructuredBuffer<float> g_lodFactors : register(u3);

[numthreads(256, 1, 1)]
void CSCalculateLOD(uint3 id : SV_DispatchThreadID)
{
    uint index = id.x;
    if (index >= g_numElements) return;
    
    uint originalIndex = g_sortedIndices[index];
    Particle particle = g_inputParticles[originalIndex];
    
    if (particle.active == 0)
    {
        g_lodFactors[index] = 0.0f;
        return;
    }
    
    // 거리에 따른 LOD 계산
    float distance = sqrt(g_sortData[index].key);
    
    // 거리별 LOD 레벨
    float lodFactor = 1.0f;
    if (distance > 100.0f)
        lodFactor = 0.5f;  // 멀리 있는 파티클은 크기 감소
    else if (distance > 50.0f)
        lodFactor = 0.75f; // 중간 거리
    
    g_lodFactors[index] = lodFactor;
}
