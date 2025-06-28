#pragma once
#include <memory>

namespace Lunar
{

class LunarGui;
class PerformanceProfiler;
struct GraphData;
struct TableData;

/*
 * Performance Monitor UI Layout:
 *
 *   ┌─────────────────────────────┐
 *   │ Performance Monitor         │
 *   ├─────────────────────────────┤
 *   │ Current FPS: 60.5           │
 *   │ Average FPS: 58.2           │
 *   │ Frame Time: 16.7 ms         │
 *   │ Avg Frame Time: 17.2 ms     │
 *   │                             │
 *   │ [FPS Graph ████▆▇█▅▄▃]      │
 *   │ [Frame Time ▃▄▅█▇▆████]     │
 *   │                             │
 *   │ Section Timings:            │
 *   │ ┌─────────┬────────┬──────┐ │
 *   │ │ Section │ Time   │ %    │ │
 *   │ └─────────┴────────┴──────┘ │
 *   └─────────────────────────────┘
 */

class PerformanceViewModel 
{
public:
    PerformanceViewModel() = default;
    ~PerformanceViewModel() = default;

    void Initialize(LunarGui* gui, PerformanceProfiler* profiler);

private:
    bool m_showPerformanceWindow = true;
    
    std::unique_ptr<GraphData> m_fpsGraphData;
    std::unique_ptr<GraphData> m_frameTimeGraphData;
    std::unique_ptr<TableData> m_sectionTableData;
    
};

} // namespace Lunar
