#pragma once

#include <string>
#include <vector>

struct DeviceConfig
{
    int deviceId = -1;
    std::string name;
    bool enabled = false;
};

struct AppConfig
{
    std::string configPath;
    bool autoRefresh = true;
    int refreshInterval = 1000;
    std::vector<DeviceConfig> favoriteDevices;
    
    static AppConfig Load();
    void Save() const;
    
private:
    static std::string GetConfigDirectory();
    static std::string GetConfigFilePath();
};

