#include "LunarTimer.h"
#include <windows.h>

#include <algorithm>

namespace lunar
{

LunarTimer::LunarTimer() : 
	m_startTime(0), m_lastTime(0), m_currentTime(0), m_deltaTime(0.0f), m_secondsPerCount(0.0f)
{
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	m_secondsPerCount = 1.0 / (double)countsPerSec;
}

void LunarTimer::Reset()
{
	__int64 currentTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentTime));
	m_startTime = currentTime;
	m_lastTime = currentTime;
	m_currentTime = currentTime;
}

void LunarTimer::Tick()
{
	__int64 currentTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentTime));
	m_currentTime = currentTime;
	m_deltaTime = static_cast<double>(m_currentTime - m_lastTime) * m_secondsPerCount;
	m_lastTime = currentTime;

	m_deltaTime = std::max<double>(m_deltaTime, 0.0);
}

float LunarTimer::GetTotalTime() const
{
	return static_cast<float>(m_currentTime - m_startTime) * m_secondsPerCount;
}

float LunarTimer::GetDeltaTime() const
{
	return static_cast<float>(m_deltaTime); 
}
} // namespace lunar