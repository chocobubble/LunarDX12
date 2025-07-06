#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <memory>

namespace Lunar
{
class PerformanceProfiler
{
private:
    struct ProfileSection
    {
        std::string name;
        std::chrono::high_resolution_clock::time_point startTime;
        float totalTime = 0.0f;
        float averageTime = 0.0f;
        int callCount = 0;
        bool isActive = false;
    };

public:
    PerformanceProfiler();
    ~PerformanceProfiler() = default;

    void Initialize(); 
    
    double Tick(); 
    
    // Frame-level profiling
    void BeginFrame();
    void EndFrame();
    
    // Section-level profiling
    void BeginSection(const std::string& name);
    void EndSection(const std::string& name);
    
    float GetCurrentFPS() const { return m_currentFPS; }
    float GetAverageFPS() const { return m_averageFPS; }
    float GetCurrentFrameTime() const { return m_currentFrameTime; }
    float GetAverageFrameTime() const { return m_averageFrameTime; }
    
    const float* GetCurrentFPSPtr() const { return &m_currentFPS; }
    const float* GetAverageFPSPtr() const { return &m_averageFPS; }
    const float* GetCurrentFrameTimePtr() const { return &m_currentFrameTime; }
    const float* GetAverageFrameTimePtr() const { return &m_averageFrameTime; }
    
    const std::vector<float>& GetFPSHistory() const { return m_fpsHistory; }
    const std::vector<float>& GetFrameTimeHistory() const { return m_frameTimeHistory; }
    
    float GetSectionTime(const std::string& name) const;
    float GetSectionPercentage(const std::string& name) const;
    const std::unordered_map<std::string, float>& GetSectionTimings() const { return m_sectionTimings; }
    
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

private:
    bool m_enabled = true;
    
    std::chrono::high_resolution_clock::time_point m_frameStartTime;
    float m_currentFrameTime = 0.0f;
    float m_averageFrameTime = 0.0f;
    float m_currentFPS = 0.0f;
    float m_averageFPS = 0.0f;
    
    static const int HISTORY_SIZE = 120; // approximately 2 seconds at 60fps
    std::vector<float> m_frameTimeHistory;
    std::vector<float> m_fpsHistory;
    int m_historyIndex = 0;
    
    float m_totalFrameTime = 0.0f;
    float m_totalFPS = 0.0f;
    
    std::unordered_map<std::string, std::unique_ptr<ProfileSection>> m_sections;
    std::unordered_map<std::string, float> m_sectionTimings; // For UI access
    
    void UpdateSectionStatistics();
};

	#define PROFILE_SCOPE(profiler, name) Lunar::ScopedProfiler _prof(profiler, name)
	#define PROFILE_FUNCTION(profiler) Lunar::ScopedProfiler _prof(profiler, __FUNCTION__)

	// RAII helper
	class ScopedProfiler
	{
	public:
		ScopedProfiler(PerformanceProfiler* profiler, const std::string& name)
			: m_profiler(profiler), m_name(name)
		{
			if (m_profiler && m_profiler->IsEnabled())
			{
				m_profiler->BeginSection(m_name);
			}
		}
    
		~ScopedProfiler()
		{
			if (m_profiler && m_profiler->IsEnabled())
			{
				m_profiler->EndSection(m_name);
			}
		}

	private:
		PerformanceProfiler* m_profiler;
		std::string m_name;
	};
} // namespace Lunar
