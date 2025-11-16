#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <fstream>

class Logger {
public:
    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3
    };

    static Logger& GetInstance();
    
    void Initialize();
    void Shutdown();
    
    void Log(LogLevel level, const std::string& message);
    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warning(const std::string& message);
    void Error(const std::string& message);
    
    void SetLogLevel(LogLevel level);
    LogLevel GetLogLevel() const { return currentLogLevel; }
    
    std::vector<std::string> GetRecentLogs(int maxLines = 1000) const;
    void ClearLogBuffer();
    
    std::string GetLogFilePath() const;
    void RotateLogs();

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void WriteToFile(LogLevel level, const std::string& message);
    std::string GetLogLevelString(LogLevel level) const;
    std::string GetLogDirectory() const;
    
    mutable std::mutex logMutex;
    std::ofstream logFile;
    std::string logFilePath;
    LogLevel currentLogLevel;
    std::deque<std::string> logBuffer;
    static const size_t MAX_BUFFER_SIZE = 5000;
    static const int MAX_LOG_FILES = 5;
};

#define LOG_DEBUG(msg) Logger::GetInstance().Debug(msg)
#define LOG_INFO(msg) Logger::GetInstance().Info(msg)
#define LOG_WARNING(msg) Logger::GetInstance().Warning(msg)
#define LOG_ERROR(msg) Logger::GetInstance().Error(msg)

