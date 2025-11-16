#include "VRState.h"
#include "Telemetry.h"
#include "CalibrationUI.h"
#include "imgui.h"
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstring>

TelemetryData& TelemetryData::GetInstance()
{
    static TelemetryData instance;
    return instance;
}

TelemetryData::TelemetryData() : shmemConnected(false), lastUpdateTime(0.0)
{
    try {
        shmem.Open(OPENVR_SPACECALIBRATOR_SHMEM_NAME);
        shmemConnected = true;
    } catch (const std::exception& e) {
        shmemConnected = false;
    }
}

void TelemetryData::Update()
{
    if (!shmemConnected) {
        try {
            shmem.Open(OPENVR_SPACECALIBRATOR_SHMEM_NAME);
            shmemConnected = true;
        } catch (const std::exception&) {
            shmemConnected = false;
            return;
        }
    }

    shmem.ReadNewPoses([this](const protocol::DriverPoseShmem::AugmentedPose& augmented_pose) {
        if (augmented_pose.deviceId >= 0 && augmented_pose.deviceId < vr::k_unMaxTrackedDeviceCount) {
            DeviceTelemetry& telemetry = deviceTelemetry[augmented_pose.deviceId];
            telemetry.deviceId = augmented_pose.deviceId;
            telemetry.pose = augmented_pose.pose;
            telemetry.lastUpdate = augmented_pose.sample_time;
            telemetry.hasPose = true;
            
            telemetry.position[0] = augmented_pose.pose.vecPosition[0];
            telemetry.position[1] = augmented_pose.pose.vecPosition[1];
            telemetry.position[2] = augmented_pose.pose.vecPosition[2];
            
            telemetry.rotation[0] = augmented_pose.pose.qRotation.w;
            telemetry.rotation[1] = augmented_pose.pose.qRotation.x;
            telemetry.rotation[2] = augmented_pose.pose.qRotation.y;
            telemetry.rotation[3] = augmented_pose.pose.qRotation.z;
            
            telemetry.velocity[0] = augmented_pose.pose.vecVelocity[0];
            telemetry.velocity[1] = augmented_pose.pose.vecVelocity[1];
            telemetry.velocity[2] = augmented_pose.pose.vecVelocity[2];
            
            telemetry.angularVelocity[0] = augmented_pose.pose.vecAngularVelocity[0];
            telemetry.angularVelocity[1] = augmented_pose.pose.vecAngularVelocity[1];
            telemetry.angularVelocity[2] = augmented_pose.pose.vecAngularVelocity[2];
            
                            telemetry.poseIsValid = augmented_pose.pose.poseIsValid;
            telemetry.trackingResult = (int)augmented_pose.pose.result;
        }
    });
}

void BuildTelemetryWindow(VRState& vrState)
{
    static bool autoRefresh = true;
    static double lastRefreshTime = 0.0;
    
    auto& telemetry = TelemetryData::GetInstance();
    
    double currentTime = ImGui::GetTime();
    if (autoRefresh && (currentTime - lastRefreshTime) > 0.1) {
        telemetry.Update();
        lastRefreshTime = currentTime;
    }
    
    {
            ImGui::Text("Device Telemetry & Debug Information");
            ImGui::Separator();
            
            if (!telemetry.IsConnected()) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Driver not connected - Shared memory unavailable");
                ImGui::Text("Make sure the driver is installed and SteamVR is running.");
            } else {
                ImGui::BeginGroup();
                if (ImGui::Button("Refresh"))
                {
                    telemetry.Update();
                }
                ImGui::SameLine();
                ImGui::Checkbox("Auto Refresh", &autoRefresh);
                ImGui::EndGroup();
                
                ImGui::Separator();
                
                const auto& deviceData = telemetry.GetDeviceTelemetry();
                
                if (ImGui::BeginTable("Telemetry", 9, 
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                    ImGuiTableFlags_Resizable | 
                    ImGuiTableFlags_Sortable))
                {
                    ImGui::TableSetupColumn("Device");
                    ImGui::TableSetupColumn("Position (m)");
                    ImGui::TableSetupColumn("Rotation (quat)");
                    ImGui::TableSetupColumn("Velocity (m/s)");
                    ImGui::TableSetupColumn("Ang Vel (rad/s)");
                    ImGui::TableSetupColumn("Valid", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                    ImGui::TableSetupColumn("Result");
                    ImGui::TableSetupColumn("Connected", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableSetupColumn("Last Update");
                    ImGui::TableHeadersRow();
                    
                    for (const auto& device : vrState.devices)
                    {
                        ImGui::TableNextRow();
                        
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", LabelString(device).c_str());
                        
                        auto it = deviceData.find(device.id);
                        if (it != deviceData.end())
                        {
                            const auto& tel = it->second;
                            
                            ImGui::TableNextColumn();
                            ImGui::Text("%.3f, %.3f, %.3f", 
                                tel.position[0], tel.position[1], tel.position[2]);
                            
                            ImGui::TableNextColumn();
                            ImGui::Text("%.3f, %.3f, %.3f, %.3f",
                                tel.rotation[0], tel.rotation[1], tel.rotation[2], tel.rotation[3]);
                            
                            ImGui::TableNextColumn();
                            double speed = std::sqrt(tel.velocity[0]*tel.velocity[0] + 
                                                   tel.velocity[1]*tel.velocity[1] + 
                                                   tel.velocity[2]*tel.velocity[2]);
                            ImGui::Text("%.3f (%.3f, %.3f, %.3f)", speed,
                                tel.velocity[0], tel.velocity[1], tel.velocity[2]);
                            
                            ImGui::TableNextColumn();
                            double angSpeed = std::sqrt(tel.angularVelocity[0]*tel.angularVelocity[0] + 
                                                      tel.angularVelocity[1]*tel.angularVelocity[1] + 
                                                      tel.angularVelocity[2]*tel.angularVelocity[2]);
                            ImGui::Text("%.3f (%.3f, %.3f, %.3f)", angSpeed,
                                tel.angularVelocity[0], tel.angularVelocity[1], tel.angularVelocity[2]);
                            
                            ImGui::TableNextColumn();
                            if (tel.poseIsValid) {
                                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
                            } else {
                                ImGui::TextColored(ImVec4(1, 0, 0, 1), "No");
                            }
                            
                            ImGui::TableNextColumn();
                            const char* resultStr = "Unknown";
                            ImVec4 resultColor(1, 1, 1, 1);
                            switch (tel.trackingResult) {
                                case 0:
                                    resultStr = "Uninitialized";
                                    resultColor = ImVec4(0.5f, 0.5f, 0.5f, 1);
                                    break;
                                case 1:
                                    resultStr = "OK";
                                    resultColor = ImVec4(0, 1, 0, 1);
                                    break;
                                case 2:
                                    resultStr = "Out of Range";
                                    resultColor = ImVec4(1, 1, 0, 1);
                                    break;
                                case 3:
                                    resultStr = "Calibrating";
                                    resultColor = ImVec4(1, 0.5f, 0, 1);
                                    break;
                                case 4:
                                    resultStr = "Calib OOR";
                                    resultColor = ImVec4(1, 0, 0, 1);
                                    break;
                                case 5:
                                    resultStr = "Rotation Only";
                                    resultColor = ImVec4(0.5f, 0.5f, 1, 1);
                                    break;
                                default:
                                    resultStr = "Error";
                                    resultColor = ImVec4(1, 0, 0, 1);
                                    break;
                            }
                            ImGui::TextColored(resultColor, "%s", resultStr);
                            
                            ImGui::TableNextColumn();
                            if (tel.pose.deviceIsConnected) {
                                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
                            } else {
                                ImGui::TextColored(ImVec4(1, 0, 0, 1), "No");
                            }
                            
                            ImGui::TableNextColumn();
                            timespec now;
                            clock_gettime(CLOCK_MONOTONIC, &now);
                            double elapsed = (now.tv_sec - tel.lastUpdate.tv_sec) + 
                                           (now.tv_nsec - tel.lastUpdate.tv_nsec) / 1e9;
                            if (elapsed < 0.1) {
                                ImGui::TextColored(ImVec4(0, 1, 0, 1), "%.0f ms", elapsed * 1000);
                            } else if (elapsed < 1.0) {
                                ImGui::TextColored(ImVec4(1, 1, 0, 1), "%.0f ms", elapsed * 1000);
                            } else {
                                ImGui::TextColored(ImVec4(1, 0, 0, 1), "%.1f s", elapsed);
                            }
                        }
                        else
                        {
                            for (int i = 0; i < 8; i++) {
                                ImGui::TableNextColumn();
                                ImGui::TextDisabled("N/A");
                            }
                        }
                    }
                    
                    ImGui::EndTable();
                }
            }
    }
}

