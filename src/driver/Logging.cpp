#include "Logging.h"
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <pwd.h>
#include <filesystem>

FILE *LogFile;

void OpenLogFile()
{
	const char* home = getenv("HOME");
	if (!home) {
		struct passwd* pw = getpwuid(getuid());
		if (pw) {
			home = pw->pw_dir;
		}
	}
	
	std::string logPath;
	if (home) {
		std::string logDir = std::string(home) + "/.local/share/space-calibrator";
		std::filesystem::create_directories(logDir);
		logPath = logDir + "/driver.log";
	} else {
		logPath = "space_calibrator_driver.log";
	}
	
	LogFile = fopen(logPath.c_str(), "a");
	if (LogFile == nullptr)
	{
		LogFile = stderr;
	}
}

tm TimeForLog()
{
	auto now = std::chrono::system_clock::now();
	auto nowTime = std::chrono::system_clock::to_time_t(now);
	tm value;
	localtime_r(&nowTime, &value);
	return value;
}

void LogFlush()
{
	fflush(LogFile);
}

