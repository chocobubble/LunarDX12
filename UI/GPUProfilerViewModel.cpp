#include "GPUProfilerViewModel.h"
#include "../GPUProfiler.h"
#include <imgui.h>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Lunar
{

GPUProfilerViewModel::GPUProfilerViewModel()
{
    m_gpuTimeHistory.reserve(MAX_HISTORY_SIZE);
    m_cpuTimeHistory.reserve(MAX_HISTORY_SIZE);
    m_triangleHistory.reserve(MAX_HISTORY_SIZE);
    m_drawCallHistory.reserve(MAX_HISTORY_SIZE);
}

void GPUProfilerViewModel::Initialize(GPUProfiler* profiler)
{
    m_gpuProfiler = profiler;
}

void GPUProfilerViewModel::Update()
{
    if (!m_gpuProfiler) return;
    
    // 프로파일링 활성화/비활성화
    m_gpuProfiler->EnableProfiling(m_enableProfiling);
    
    // 프레임 통계 업데이트
    if (m_gpuProfiler->IsFrameComplete())
    {
        auto frameStats = m_gpuProfiler->GetLastFrameStats();
        
        // 히스토리 업데이트
        m_gpuTimeHistory.push_back(frameStats.totalGPUTime);
        m_cpuTimeHistory.push_back(frameStats.totalCPUTime);
        m_triangleHistory.push_back(static_cast<float>(frameStats.totalTriangles) / 1000.0f); // K 단위
        m_drawCallHistory.push_back(static_cast<float>(frameStats.totalDrawCalls));
        
        // 히스토리 크기 제한
        if (m_gpuTimeHistory.size() > MAX_HISTORY_SIZE)
        {
            m_gpuTimeHistory.erase(m_gpuTimeHistory.begin());
            m_cpuTimeHistory.erase(m_cpuTimeHistory.begin());
            m_triangleHistory.erase(m_triangleHistory.begin());
            m_drawCallHistory.erase(m_drawCallHistory.begin());
        }
    }
}

void GPUProfilerViewModel::Render()
{
    if (!ImGui::Begin("GPU Profiler"))
    {
        ImGui::End();
        return;
    }
    
    // 프로파일링 제어
    ImGui::Checkbox("Enable Profiling", &m_enableProfiling);
    ImGui::SameLine();
    
    if (ImGui::Button("Clear History"))
    {
        m_gpuTimeHistory.clear();
        m_cpuTimeHistory.clear();
        m_triangleHistory.clear();
        m_drawCallHistory.clear();
    }
    
    ImGui::Separator();
    
    // 섹션 토글
    ImGui::Checkbox("Frame Stats", &m_showFrameStats);
    ImGui::SameLine();
    ImGui::Checkbox("Event List", &m_showEventList);
    ImGui::SameLine();
    ImGui::Checkbox("Timing Graph", &m_showTimingGraph);
    ImGui::SameLine();
    ImGui::Checkbox("Memory Stats", &m_showMemoryStats);
    
    ImGui::Separator();
    
    if (m_showFrameStats)
        RenderFrameStats();
    
    if (m_showEventList)
        RenderEventList();
    
    if (m_showTimingGraph)
        RenderTimingGraph();
    
    if (m_showMemoryStats)
        RenderMemoryStats();
    
    ImGui::End();
}

void GPUProfilerViewModel::RenderFrameStats()
{
    if (!ImGui::CollapsingHeader("Frame Statistics", ImGuiTreeNodeFlags_DefaultOpen))
        return;
    
    if (!m_gpuProfiler || !m_gpuProfiler->IsFrameComplete())
    {
        ImGui::Text("No frame data available");
        return;
    }
    
    auto frameStats = m_gpuProfiler->GetLastFrameStats();
    
    ImGui::Columns(2, "FrameStatsColumns", false);
    
    // 왼쪽 열
    ImGui::Text("Total GPU Time:");
    ImGui::Text("Total CPU Time:");
    ImGui::Text("Total Draw Calls:");
    ImGui::Text("Total Triangles:");
    ImGui::Text("Events Count:");
    
    ImGui::NextColumn();
    
    // 오른쪽 열
    ImGui::Text("%.3f ms", frameStats.totalGPUTime);
    ImGui::Text("%.3f ms", frameStats.totalCPUTime);
    ImGui::Text("%llu", frameStats.totalDrawCalls);
    ImGui::Text("%llu", frameStats.totalTriangles);
    ImGui::Text("%zu", frameStats.events.size());
    
    ImGui::Columns(1);
    
    // 추가 통계
    if (frameStats.totalGPUTime > 0)
    {
        float fps = 1000.0f / frameStats.totalGPUTime;
        ImGui::Text("Estimated GPU FPS: %.1f", fps);
    }
    
    if (!frameStats.events.empty())
    {
        float avgGPUTime = frameStats.totalGPUTime / frameStats.events.size();
        ImGui::Text("Avg Event GPU Time: %.3f ms", avgGPUTime);
    }
}

void GPUProfilerViewModel::RenderEventList()
{
    if (!ImGui::CollapsingHeader("Event Details", ImGuiTreeNodeFlags_DefaultOpen))
        return;
    
    if (!m_gpuProfiler || !m_gpuProfiler->IsFrameComplete())
    {
        ImGui::Text("No event data available");
        return;
    }
    
    auto frameStats = m_gpuProfiler->GetLastFrameStats();
    auto events = frameStats.events;
    
    // 필터링
    ImGui::InputText("Filter", &m_eventFilter);
    ImGui::SameLine();
    ImGui::SliderFloat("Min Time (ms)", &m_minTimeThreshold, 0.0f, 10.0f, "%.3f");
    
    // 정렬 옵션
    const char* sortModes[] = { "Name", "GPU Time", "CPU Time", "Draw Calls", "Triangles" };
    int currentSort = static_cast<int>(m_sortMode);
    if (ImGui::Combo("Sort By", &currentSort, sortModes, IM_ARRAYSIZE(sortModes)))
    {
        m_sortMode = static_cast<SortMode>(currentSort);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Descending", &m_sortDescending);
    
    // 이벤트 필터링
    std::vector<GPUProfiler::TimingResult> filteredEvents;
    for (const auto& event : events)
    {
        if (!m_eventFilter.empty() && event.name.find(m_eventFilter) == std::string::npos)
            continue;
        
        if (event.gpuTimeMs < m_minTimeThreshold)
            continue;
        
        filteredEvents.push_back(event);
    }
    
    // 정렬
    std::sort(filteredEvents.begin(), filteredEvents.end(), [this](const auto& a, const auto& b) {
        bool result = false;
        switch (m_sortMode)
        {
        case SortMode::Name:
            result = a.name < b.name;
            break;
        case SortMode::GPUTime:
            result = a.gpuTimeMs < b.gpuTimeMs;
            break;
        case SortMode::CPUTime:
            result = a.cpuTimeMs < b.cpuTimeMs;
            break;
        case SortMode::DrawCalls:
            result = a.drawCalls < b.drawCalls;
            break;
        case SortMode::Triangles:
            result = a.triangleCount < b.triangleCount;
            break;
        }
        return m_sortDescending ? !result : result;
    });
    
    // 테이블 렌더링
    if (ImGui::BeginTable("EventTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Event Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("GPU (ms)", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("CPU (ms)", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Draw Calls", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Triangles", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();
        
        for (const auto& event : filteredEvents)
        {
            ImGui::TableNextRow();
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", event.name.c_str());
            
            ImGui::TableNextColumn();
            if (event.isComplete)
            {
                // GPU 시간에 따른 색상 코딩
                if (event.gpuTimeMs > 5.0f)
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%.3f", event.gpuTimeMs);
                else if (event.gpuTimeMs > 1.0f)
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%.3f", event.gpuTimeMs);
                else
                    ImGui::Text("%.3f", event.gpuTimeMs);
            }
            else
            {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "N/A");
            }
            
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", event.cpuTimeMs);
            
            ImGui::TableNextColumn();
            ImGui::Text("%llu", event.drawCalls);
            
            ImGui::TableNextColumn();
            ImGui::Text("%llu", event.triangleCount);
        }
        
        ImGui::EndTable();
    }
    
    ImGui::Text("Showing %zu / %zu events", filteredEvents.size(), events.size());
}

void GPUProfilerViewModel::RenderTimingGraph()
{
    if (!ImGui::CollapsingHeader("Timing Graphs"))
        return;
    
    if (m_gpuTimeHistory.empty())
    {
        ImGui::Text("No timing data available");
        return;
    }
    
    float maxGPUTime = *std::max_element(m_gpuTimeHistory.begin(), m_gpuTimeHistory.end());
    float maxCPUTime = *std::max_element(m_cpuTimeHistory.begin(), m_cpuTimeHistory.end());
    float maxTriangles = *std::max_element(m_triangleHistory.begin(), m_triangleHistory.end());
    float maxDrawCalls = *std::max_element(m_drawCallHistory.begin(), m_drawCallHistory.end());
    
    // GPU 시간 그래프
    ImGui::Text("GPU Time (ms)");
    ImGui::PlotLines("##GPUTime", m_gpuTimeHistory.data(), static_cast<int>(m_gpuTimeHistory.size()), 
                     0, nullptr, 0.0f, maxGPUTime * 1.1f, ImVec2(0, 80));
    
    // CPU 시간 그래프
    ImGui::Text("CPU Time (ms)");
    ImGui::PlotLines("##CPUTime", m_cpuTimeHistory.data(), static_cast<int>(m_cpuTimeHistory.size()), 
                     0, nullptr, 0.0f, maxCPUTime * 1.1f, ImVec2(0, 80));
    
    // 삼각형 수 그래프
    ImGui::Text("Triangles (K)");
    ImGui::PlotLines("##Triangles", m_triangleHistory.data(), static_cast<int>(m_triangleHistory.size()), 
                     0, nullptr, 0.0f, maxTriangles * 1.1f, ImVec2(0, 80));
    
    // Draw Call 그래프
    ImGui::Text("Draw Calls");
    ImGui::PlotLines("##DrawCalls", m_drawCallHistory.data(), static_cast<int>(m_drawCallHistory.size()), 
                     0, nullptr, 0.0f, maxDrawCalls * 1.1f, ImVec2(0, 80));
}

void GPUProfilerViewModel::RenderMemoryStats()
{
    if (!ImGui::CollapsingHeader("Memory Statistics"))
        return;
    
    // TODO: 메모리 추적 시스템 구현 후 연동
    ImGui::Text("Memory tracking not implemented yet");
    ImGui::Text("Planned features:");
    ImGui::BulletText("Texture memory usage");
    ImGui::BulletText("Buffer memory usage");
    ImGui::BulletText("Descriptor heap usage");
    ImGui::BulletText("Upload heap usage");
}

} // namespace Lunar
