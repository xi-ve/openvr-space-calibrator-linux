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
    std::string json;
    while (std::getline(file, line)) {
        json += line + "\n";
    }
    
    if (json.find("\"autoRefresh\"") != std::string::npos) {
        if (json.find("\"autoRefresh\"") != std::string::npos) {
            size_t pos = json.find("\"autoRefresh\"");
            if (json.find("true", pos) != std::string::npos) {
                config.autoRefresh = true;
            } else if (json.find("false", pos) != std::string::npos) {
                config.autoRefresh = false;
            }
        }
    }
    
    if (json.find("\"refreshInterval\"") != std::string::npos) {
        size_t pos = json.find("\"refreshInterval\"");
        pos = json.find_first_of("0123456789", pos);
        if (pos != std::string::npos) {
            size_t end = json.find_first_not_of("0123456789", pos);
            if (end == std::string::npos) {
                end = json.length();
            }
            try {
                config.refreshInterval = std::stoi(json.substr(pos, end - pos));
            } catch (...) {
            }
        }
    }
    
    if (json.find("\"playspaceMovementEnabled\"") != std::string::npos) {
        size_t pos = json.find("\"playspaceMovementEnabled\"");
        if (json.find("true", pos) != std::string::npos) {
            config.playspaceMovementEnabled = true;
        } else if (json.find("false", pos) != std::string::npos) {
            config.playspaceMovementEnabled = false;
        }
    }
    
    if (json.find("\"playspaceMovementMultiplier\"") != std::string::npos) {
        size_t pos = json.find("\"playspaceMovementMultiplier\"");
        pos = json.find_first_of("0123456789.", pos);
        if (pos != std::string::npos) {
            size_t end = json.find_first_not_of("0123456789.", pos);
            if (end == std::string::npos) {
                end = json.length();
            }
            try {
                config.playspaceMovementMultiplier = std::stof(json.substr(pos, end - pos));
            } catch (...) {
            }
        }
    }
    
    if (json.find("\"playspaceForceStrength\"") != std::string::npos || json.find("\"playspacePullStrength\"") != std::string::npos) {
        if (json.find("\"playspacePullStrength\"") != std::string::npos) {
            size_t pos = json.find("\"playspacePullStrength\"");
            pos = json.find_first_of("0123456789.", pos);
            if (pos != std::string::npos) {
                size_t end = json.find_first_not_of("0123456789.", pos);
                if (end == std::string::npos) {
                    end = json.length();
                }
                try {
                    config.playspaceMovementMultiplier = std::stof(json.substr(pos, end - pos));
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
    file << "  \"refreshInterval\": " << refreshInterval << ",\n";
    file << "  \"playspaceMovementEnabled\": " << (playspaceMovementEnabled ? "true" : "false") << ",\n";
    file << "  \"playspaceMovementMultiplier\": " << playspaceMovementMultiplier << "\n";
    file << "}\n";
    
    file.close();
}

