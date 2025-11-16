#include "Logging.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <pwd.h>
#include <algorithm>

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : currentLogLevel(INFO) {
}

Logger::~Logger() {
    Shutdown();
}

void Logger::Initialize() {
    std::lock_guard<std::mutex> lock(logMutex);
    
    std::string logDir = GetLogDirectory();
    std::filesystem::create_directories(logDir);
    
    logFilePath = logDir + "/overlay.log";
    
    logFile.open(logFilePath, std::ios::app);
    if (!logFile.is_open()) {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time, &tm_buf);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "[%Y-%m-%d %H:%M:%S]");
    logFile << oss.str() << " Logger initialized" << std::endl;
    logFile.flush();
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile << "Logger shutting down" << std::endl;
        logFile.close();
    }
}

void Logger::SetLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(logMutex);
    currentLogLevel = level;
}

void Logger::Log(LogLevel level, const std::string& message) {
    if (level < currentLogLevel) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(logMutex);
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time, &tm_buf);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "[%Y-%m-%d %H:%M:%S]");
    std::string timestamp = oss.str();
    
    std::string levelStr = GetLogLevelString(level);
    std::string logLine = timestamp + " [" + levelStr + "] " + message;
    
    if (logFile.is_open()) {
        logFile << logLine << std::endl;
        logFile.flush();
    }
    
    logBuffer.push_back(logLine);
    if (logBuffer.size() > MAX_BUFFER_SIZE) {
        logBuffer.pop_front();
    }
    
    std::cout << logLine << std::endl;
}

void Logger::Debug(const std::string& message) {
    Log(DEBUG, message);
}

void Logger::Info(const std::string& message) {
    Log(INFO, message);
}

void Logger::Warning(const std::string& message) {
    Log(WARNING, message);
}

void Logger::Error(const std::string& message) {
    Log(ERROR, message);
}

std::vector<std::string> Logger::GetRecentLogs(int maxLines) const {
    std::lock_guard<std::mutex> lock(logMutex);
    
    std::vector<std::string> result;
    int startIdx = std::max(0, (int)logBuffer.size() - maxLines);
    
    for (int i = startIdx; i < (int)logBuffer.size(); ++i) {
        result.push_back(logBuffer[i]);
    }
    
    return result;
}

void Logger::ClearLogBuffer() {
    std::lock_guard<std::mutex> lock(logMutex);
    logBuffer.clear();
}

std::string Logger::GetLogFilePath() const {
    std::lock_guard<std::mutex> lock(logMutex);
    return logFilePath;
}

void Logger::RotateLogs() {
    std::lock_guard<std::mutex> lock(logMutex);
    
    if (logFile.is_open()) {
        logFile.close();
    }
    
    std::string logDir = GetLogDirectory();
    
    for (int i = MAX_LOG_FILES - 1; i >= 1; --i) {
        std::string oldFile = logDir + "/overlay.log." + std::to_string(i);
        std::string newFile = logDir + "/overlay.log." + std::to_string(i + 1);
        
        if (std::filesystem::exists(oldFile)) {
            if (i + 1 > MAX_LOG_FILES) {
                std::filesystem::remove(oldFile);
            } else {
                std::filesystem::rename(oldFile, newFile);
            }
        }
    }
    
    std::string currentLog = logDir + "/overlay.log";
    if (std::filesystem::exists(currentLog)) {
        std::filesystem::rename(currentLog, logDir + "/overlay.log.1");
    }
    
    logFile.open(currentLog, std::ios::app);
    logFilePath = currentLog;
}

void Logger::WriteToFile(LogLevel level, const std::string& message) {
    if (!logFile.is_open()) {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time, &tm_buf);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "[%Y-%m-%d %H:%M:%S]");
    logFile << oss.str() << " [" << GetLogLevelString(level) << "] " << message << std::endl;
    logFile.flush();
}

std::string Logger::GetLogLevelString(LogLevel level) const {
    switch (level) {
        case DEBUG: return "DEBUG";
        case INFO: return "INFO";
        case WARNING: return "WARN";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::GetLogDirectory() const {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }
    
    if (!home) {
        return std::string(".");
    }
    
    return std::string(home) + "/.local/share/space-calibrator";
}

