#include "PerformanceViewModel.h"
#include "LunarGUI.h"
#include "../PerformanceProfiler.h"
#include "../Utils/Logger.h"

using namespace std;

namespace Lunar
{

void PerformanceViewModel::Initialize(LunarGui* gui, PerformanceProfiler* profiler)
{
    if (!gui || !profiler)
    {
        LOG_ERROR("PerformanceViewModel: Invalid gui or profiler pointer");
        return;
    }

    gui->BindCheckbox("Show Performance Window", &m_showPerformanceWindow);
    
    gui->BindReadOnlyFloat("Current FPS", profiler->GetCurrentFPSPtr(), "Current FPS", "%.1f");
    gui->BindReadOnlyFloat("Average FPS", profiler->GetAverageFPSPtr(), "Average FPS", "%.1f");
    gui->BindReadOnlyFloat("Frame Time", profiler->GetCurrentFrameTimePtr(), "Frame Time", "%.3f ms");
    gui->BindReadOnlyFloat("Avg Frame Time", profiler->GetAverageFrameTimePtr(), "Avg Frame Time", "%.3f ms");
    
    m_fpsGraphData = make_unique<GraphData>();
    m_fpsGraphData->label = "FPS History";
    m_fpsGraphData->minValue = 0.0f;
    m_fpsGraphData->maxValue = 200.0f;
    m_fpsGraphData->updateCallback = [this, profiler]() {
	    m_fpsGraphData->values = profiler->GetFPSHistory();
    };
    gui->BindGraph("FPS Graph", m_fpsGraphData.get());
    
    m_frameTimeGraphData = make_unique<GraphData>();
    m_frameTimeGraphData->label = "Frame Time History (ms)";
    m_frameTimeGraphData->minValue = 0.0f;
    m_frameTimeGraphData->maxValue = 50.0f;
	m_frameTimeGraphData->updateCallback = [this, profiler]() {
		m_frameTimeGraphData->values = profiler->GetFrameTimeHistory();
	};
    gui->BindGraph("Frame Time Graph", m_frameTimeGraphData.get());
    
    m_sectionTableData = make_unique<TableData>();
    m_sectionTableData->headers = {"Section", "Time (ms)", "Percentage"};
    m_sectionTableData->flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
	m_sectionTableData->updateCallback = [this, profiler]() {
		m_sectionTableData->rows.clear();

		const auto& sectionTimings = profiler->GetSectionTimings();
		for (const auto& [sectionName, sectionTime] : sectionTimings)
		{
			float percentage = profiler->GetSectionPercentage(sectionName);

			vector<string> row = {
			sectionName,
			to_string(sectionTime),
			to_string(percentage) + "%"
			};

			m_sectionTableData->rows.push_back(row);
		}
	};
    gui->BindTable("Section Table", m_sectionTableData.get());
    
    vector<string> elementIds = {
        "Current FPS", 
        "Average FPS", 
        "Frame Time",
        "Avg Frame Time",
        "FPS Graph", 
        "Frame Time Graph", 
        "Section Table"
    };
    gui->BindWindow("Performance Window", "Performance Monitor", &m_showPerformanceWindow, elementIds);
    
    LOG_FUNCTION_EXIT();
}

} // namespace Lunar
