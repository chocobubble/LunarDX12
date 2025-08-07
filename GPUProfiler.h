#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace Lunar
{

class GPUProfiler
{
public:
    struct TimingResult
    {
        std::string name;
        float gpuTimeMs = 0.0f;
        float cpuTimeMs = 0.0f;
        UINT64 triangleCount = 0;
        UINT64 drawCalls = 0;
        UINT queryStartIndex = 0;
        UINT queryEndIndex = 0;
        bool isComplete = false;
    };

    struct FrameStats
    {
        float totalGPUTime = 0.0f;
        float totalCPUTime = 0.0f;
        UINT64 totalTriangles = 0;
        UINT64 totalDrawCalls = 0;
        std::vector<TimingResult> events;
    };

    GPUProfiler();
    ~GPUProfiler();

    void Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, UINT maxEvents = 256);
    void Shutdown();

    // 프레임 시작/끝
    void BeginFrame(ID3D12GraphicsCommandList* cmdList);
    void EndFrame(ID3D12GraphicsCommandList* cmdList);

    // 이벤트 측정
    void BeginEvent(ID3D12GraphicsCommandList* cmdList, const std::string& name);
    void EndEvent(ID3D12GraphicsCommandList* cmdList);
    
    // 추가 통계 정보
    void AddDrawCall(UINT64 triangleCount = 0);
    void SetTriangleCount(const std::string& eventName, UINT64 triangleCount);

    // 결과 조회
    bool IsFrameComplete() const;
    FrameStats GetLastFrameStats();
    void ResolveTimestamps();

    // 디버깅 및 로깅
    void LogFrameStats() const;
    void EnableProfiling(bool enable) { m_enabled = enable; }
    bool IsProfilingEnabled() const { return m_enabled; }

private:
    struct EventData
    {
        std::string name;
        UINT queryStartIndex;
        UINT queryEndIndex;
        UINT64 triangleCount;
        UINT64 drawCalls;
        std::chrono::high_resolution_clock::time_point cpuStartTime;
        std::chrono::high_resolution_clock::time_point cpuEndTime;
        bool isActive;
    };

    void CreateQueryHeap();
    void CreateReadbackBuffer();
    UINT64 GetTimestampFrequency();

    // D3D12 리소스
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_timestampHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_readbackBuffer;

    // 프로파일링 데이터
    std::vector<EventData> m_events;
    std::unordered_map<std::string, size_t> m_activeEvents; // 이벤트 이름 -> 인덱스
    
    UINT m_maxEvents;
    UINT m_currentQueryIndex;
    UINT64 m_timestampFrequency;
    
    // 프레임 데이터
    FrameStats m_lastFrameStats;
    bool m_frameInProgress;
    bool m_enabled;
    
    // 동기화
    mutable std::mutex m_dataMutex;
    
    // 통계
    UINT64 m_currentFrameTriangles;
    UINT64 m_currentFrameDrawCalls;
};

// RAII 스타일 프로파일링 헬퍼
class ScopedGPUEvent
{
public:
    ScopedGPUEvent(GPUProfiler* profiler, ID3D12GraphicsCommandList* cmdList, const std::string& name)
        : m_profiler(profiler), m_cmdList(cmdList)
    {
        if (m_profiler && m_profiler->IsProfilingEnabled())
        {
            m_profiler->BeginEvent(m_cmdList, name);
        }
    }

    ~ScopedGPUEvent()
    {
        if (m_profiler && m_profiler->IsProfilingEnabled())
        {
            m_profiler->EndEvent(m_cmdList);
        }
    }

    ScopedGPUEvent(const ScopedGPUEvent&) = delete;
    ScopedGPUEvent& operator=(const ScopedGPUEvent&) = delete;

private:
    GPUProfiler* m_profiler;
    ID3D12GraphicsCommandList* m_cmdList;
};

// 편의 매크로
#define GPU_PROFILE_SCOPE(profiler, cmdList, name) \
    ScopedGPUEvent _gpuEvent(profiler, cmdList, name)

} // namespace Lunar
