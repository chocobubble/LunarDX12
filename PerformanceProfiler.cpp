#include "PerformanceProfiler.h"

using namespace std;

namespace Lunar
{

PerformanceProfiler::PerformanceProfiler()
{
    m_frameTimeHistory.resize(HISTORY_SIZE, 0.0f);
    m_fpsHistory.resize(HISTORY_SIZE, 0.0f);
}

void PerformanceProfiler::Initialize()
{
    m_frameStartTime = chrono::high_resolution_clock::now();
}

double PerformanceProfiler::Tick()
{
    if (!m_enabled) return 0.0;
    
    auto currentTime = chrono::high_resolution_clock::now();
    
    double deltaTime = 0.0;
    auto frameDuration = chrono::duration_cast<std::chrono::microseconds>(currentTime - m_frameStartTime);
    deltaTime = frameDuration.count() / 1000000.0; // convert to seconds
    
    m_frameStartTime = currentTime;
    return deltaTime;
}

void PerformanceProfiler::EndFrame()
{
    if (!m_enabled) return;
    
    auto frameEndTime = chrono::high_resolution_clock::now();
    auto frameDuration = chrono::duration_cast<std::chrono::microseconds>(frameEndTime - m_frameStartTime);
    
    m_currentFrameTime = frameDuration.count() / 1000.0f; // Convert to milliseconds
    m_currentFPS = (m_currentFrameTime > 0.0f) ? (1000.0f / m_currentFrameTime) : 0.0f;
    
    float oldFrameTime = m_frameTimeHistory[m_historyIndex];
    float oldFPS = m_fpsHistory[m_historyIndex];
    m_totalFrameTime -= oldFrameTime;
    m_totalFPS -= oldFPS;
    
    // Add new values
    m_frameTimeHistory[m_historyIndex] = m_currentFrameTime;
    m_fpsHistory[m_historyIndex] = m_currentFPS;
    
    m_totalFrameTime += m_currentFrameTime;
    m_totalFPS += m_currentFPS;
    
    m_averageFrameTime = m_totalFrameTime / HISTORY_SIZE;
    m_averageFPS = m_totalFPS / HISTORY_SIZE;
    
    m_historyIndex = (m_historyIndex + 1) % HISTORY_SIZE; // Update history index
    
    UpdateSectionStatistics();
}

void PerformanceProfiler::BeginSection(const string& name)
{
    if (!m_enabled) return;
    
    auto it = m_sections.find(name);
    if (it == m_sections.end())
    {
        m_sections[name] = make_unique<ProfileSection>();
        m_sections[name]->name = name;
    }
    
    auto& section = m_sections[name];
    section->startTime = chrono::high_resolution_clock::now();
    section->isActive = true;
}

void PerformanceProfiler::EndSection(const string& name)
{
    if (!m_enabled) return;
    
    auto endTime = chrono::high_resolution_clock::now();
    
    auto it = m_sections.find(name);
    if (it != m_sections.end() && it->second->isActive)
    {
        auto& section = it->second;
        auto duration = chrono::duration_cast<chrono::microseconds>(endTime - section->startTime);
        float sectionTime = duration.count() / 1000.0f; // Convert to milliseconds
        
        section->totalTime += sectionTime;
        section->callCount++;
        section->isActive = false;
    }
}

float PerformanceProfiler::GetSectionTime(const string& name) const
{
    auto it = m_sections.find(name);
    return (it != m_sections.end()) ? it->second->averageTime : 0.0f;
}

float PerformanceProfiler::GetSectionPercentage(const string& name) const
{
    if (m_currentFrameTime <= 0.0f) return 0.0f;
    
    float sectionTime = GetSectionTime(name);
    return (sectionTime / m_currentFrameTime) * 100.0f;
}

void PerformanceProfiler::UpdateSectionStatistics()
{
    m_sectionTimings.clear();
    
    for (auto& [name, section] : m_sections)
    {
        if (section->callCount > 0)
        {
            section->averageTime = section->totalTime / section->callCount;
            m_sectionTimings[name] = section->averageTime;
            
            // Reset for next frame
            section->totalTime = 0.0f;
            section->callCount = 0;
        }
    }
}

} // namespace Lunar
