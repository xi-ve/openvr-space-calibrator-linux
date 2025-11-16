#include "Configuration.h"
#include <fstream>
#include <filesystem>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

std::string AppConfig::GetConfigDirectory()
{
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
    
    std::string configDir = std::string(home) + "/.config/space-calibrator";
    std::filesystem::create_directories(configDir);
    return configDir;
}

std::string AppConfig::GetConfigFilePath()
{
    return GetConfigDirectory() + "/config.json";
}

AppConfig AppConfig::Load()
{
    AppConfig config;
    config.configPath = GetConfigDirectory();
    
    std::string configFile = GetConfigFilePath();
    std::ifstream file(configFile);
    
    if (!file.is_open()) {
        return config;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("\"autoRefresh\"") != std::string::npos) {
            if (line.find("true") != std::string::npos) {
                config.autoRefresh = true;
            } else if (line.find("false") != std::string::npos) {
                config.autoRefresh = false;
            }
        } else if (line.find("\"refreshInterval\"") != std::string::npos) {
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                size_t end = line.find_first_not_of("0123456789", pos);
                if (end == std::string::npos) {
                    end = line.length();
                }
                try {
                    config.refreshInterval = std::stoi(line.substr(pos, end - pos));
                } catch (...) {
                }
            }
        }
    }
    
    file.close();
    return config;
}

void AppConfig::Save() const
{
    std::string configFile = GetConfigFilePath();
    std::ofstream file(configFile);
    
    if (!file.is_open()) {
        return;
    }
    
    file << "{\n";
    file << "  \"autoRefresh\": " << (autoRefresh ? "true" : "false") << ",\n";
    file << "  \"refreshInterval\": " << refreshInterval << "\n";
    file << "}\n";
    
    file.close();
}

