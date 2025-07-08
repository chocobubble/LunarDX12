#pragma once

namespace Lunar
{
class LunarTimer
{
public:
	LunarTimer();
	void Reset();
	void Tick();
	float GetTotalTime() const;
	float GetDeltaTime() const;
private:
	__int64 m_startTime;
	__int64 m_lastTime;
	__int64 m_currentTime;
	double m_deltaTime;
	double m_secondsPerCount;
	
};
} // namespace Lunar
