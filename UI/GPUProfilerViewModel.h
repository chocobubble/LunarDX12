#pragma once
#include <vector>
#include <string>

namespace Lunar
{

class GPUProfiler;

class GPUProfilerViewModel
{
public:
    GPUProfilerViewModel();
    ~GPUProfilerViewModel() = default;

    void Initialize(GPUProfiler* profiler);
    void Update();
    void Render();

private:
    void RenderFrameStats();
    void RenderEventList();
    void RenderTimingGraph();
    void RenderMemoryStats();

    GPUProfiler* m_gpuProfiler = nullptr;
    
    // UI 상태
    bool m_showFrameStats = true;
    bool m_showEventList = true;
    bool m_showTimingGraph = true;
    bool m_showMemoryStats = false;
    bool m_enableProfiling = true;
    
    // 그래프 데이터
    std::vector<float> m_gpuTimeHistory;
    std::vector<float> m_cpuTimeHistory;
    std::vector<float> m_triangleHistory;
    std::vector<float> m_drawCallHistory;
    
    static constexpr int MAX_HISTORY_SIZE = 120; // 2초 @ 60fps
    
    // 필터링
    std::string m_eventFilter;
    float m_minTimeThreshold = 0.0f; // ms
    
    // 정렬
    enum class SortMode
    {
        Name,
        GPUTime,
        CPUTime,
        DrawCalls,
        Triangles
    };
    SortMode m_sortMode = SortMode::GPUTime;
    bool m_sortDescending = true;
};

} // namespace Lunar
