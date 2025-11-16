#pragma once

#include "../common/Protocol.h"
#include <vector>
#include <map>
#include <ctime>

struct VRState;
struct VRDevice;

struct DeviceTelemetry
{
    int deviceId;
    bool hasPose;
    vr::DriverPose_t pose;
    timespec lastUpdate;
    double position[3];
    double rotation[4];
    double velocity[3];
    double angularVelocity[3];
    bool poseIsValid;
    int trackingResult;
};

class TelemetryData
{
public:
    static TelemetryData& GetInstance();
    
    void Update();
    const std::map<int, DeviceTelemetry>& GetDeviceTelemetry() const { return deviceTelemetry; }
    bool IsConnected() const { return shmemConnected; }
    
private:
    TelemetryData();
    protocol::DriverPoseShmem shmem;
    std::map<int, DeviceTelemetry> deviceTelemetry;
    bool shmemConnected;
    double lastUpdateTime;
};

void BuildTelemetryWindow(VRState& vrState);

