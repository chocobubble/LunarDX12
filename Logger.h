#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Lunar {

enum class LogLevel
{
	DEBUG,
	WARN,
	ERR
};

class Logger
{
public:
	static Logger& GetInstance()
	{
		static Logger instance;
		return instance;
	}

	template <typename... Args>
	void Log(LogLevel level, const std::string& functionName, Args... args)
	{
		std::stringstream message;
		message << GetCurrentTime() << " [" << LogLevelToString(level) << "] "
			<< "[" << functionName << "] ";
		LogMessage(message, args...);
		std::cout << message.str() << std::endl;

		if (m_logToFile && m_logFile.is_open())
		{
			m_logFile << message.str() << std::endl;
		}
	}

	void LogFunctionEntry(const std::string& functionName)
	{
		std::stringstream message;
		message << GetCurrentTime() << " [DEBUG] "
			<< "[" << functionName << "] >>> Function Entry";
		std::cout << message.str() << "\n";

		if (m_logToFile && m_logFile.is_open())
		{
			m_logFile << message.str() << "\n";
		}
	}

	void LogFunctionExit(const std::string& functionName)
	{
		std::stringstream message;
		message << GetCurrentTime() << " [DEBUG] "
			<< "[" << functionName << "] <<< Function Exit";
		std::cout << message.str() << "\n";

		if (m_logToFile && m_logFile.is_open())
		{
			m_logFile << message.str() << "\n";
		}
	}

	void EnableFileLogging(const std::string& filename)
	{
		m_logFile.open(filename, std::ios::out | std::ios::app);
		m_logToFile = m_logFile.is_open();
	}

	void DisableFileLogging()
	{
		if (m_logFile.is_open())
		{
			m_logFile.close();
		}
		m_logToFile = false;
	}

private:
	Logger()
		: m_logToFile(false)
	{
		CreateLogsDirectory();

		std::string timestamp = GetCurrentTimeForFilename();
		std::string logFilename = "logs/log_" + timestamp + ".txt";
		EnableFileLogging(logFilename);
	}

	void CreateLogsDirectory()
	{
		#ifdef _WIN32
		system("if not exist logs mkdir logs");
		#else
        system("mkdir -p logs");
		#endif
	}

	~Logger()
	{
		if (m_logFile.is_open())
		{
			m_logFile.close();
		}
	}

	std::string GetCurrentTimeForFilename()
	{
		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);

		std::stringstream ss;
		std::tm           tm_buf;
		auto              e = localtime_s(&tm_buf, &time);

		ss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
		return ss.str();
	}

	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	std::string GetCurrentTime()
	{
		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

		std::stringstream ss;
		std::tm           tm_buf;
		auto              e = localtime_s(&tm_buf, &time);

		ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "."
			<< std::setfill('0') << std::setw(3) << ms.count();
		return ss.str();
	}

	std::string LogLevelToString(LogLevel level)
	{
		switch (level)
		{
			case LogLevel::DEBUG:
				return "DEBUG";
			case LogLevel::WARN:
				return "WARNING";
			case LogLevel::ERR:
				return "ERROR";
			default:
				return "UNKNOWN";
		}
	}

	void LogMessage(std::stringstream& ss) {}

	template <typename T, typename... Args>
	void LogMessage(std::stringstream& ss, T value, Args... args)
	{
		ss << value;
		LogMessage(ss, args...);
	}

	std::ofstream m_logFile;
	bool          m_logToFile;
};

#define LOG_DEBUG(...)    Logger::GetInstance().Log(LogLevel::DEBUG, __func__, __VA_ARGS__)
#define LOG_WARNING(...)  Logger::GetInstance().Log(LogLevel::WARN, __func__, __VA_ARGS__)
#define LOG_ERROR(...)    Logger::GetInstance().Log(LogLevel::ERR, __func__, __VA_ARGS__)

#define LOG_FUNCTION_ENTRY()  Logger::GetInstance().LogFunctionEntry(__func__)
#define LOG_FUNCTION_EXIT()   Logger::GetInstance().LogFunctionExit(__func__)

} // namespace Lunar