#include "VRState.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>

VRState VRState::Load()
{
	VRState state;
	auto& trackingSystems = state.trackingSystems;

	char buffer[vr::k_unMaxPropertyStringSize] = {};

	for (uint32_t id = 0; id < vr::k_unMaxTrackedDeviceCount; ++id)
	{
		vr::ETrackedPropertyError err = vr::TrackedProp_Success;
		auto deviceClass = vr::VRSystem()->GetTrackedDeviceClass(id);
		if (deviceClass == vr::TrackedDeviceClass_Invalid)
			continue;

		if (deviceClass != vr::TrackedDeviceClass_TrackingReference)
		{
			vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_TrackingSystemName_String, buffer, vr::k_unMaxPropertyStringSize, &err);

			if (err == vr::TrackedProp_Success)
			{
				std::string system(buffer);

				if (deviceClass == vr::TrackedDeviceClass_HMD && system == "aapvr") {
					vr::HmdMatrix34_t eyeToHeadLeft = vr::VRSystem()->GetEyeToHeadTransform(vr::Eye_Left);
					bool isCrystalHmd =
						eyeToHeadLeft.m[0][0] == 1 && eyeToHeadLeft.m[0][1] == 0 && eyeToHeadLeft.m[0][2] == 0 &&
						eyeToHeadLeft.m[1][0] == 0 && eyeToHeadLeft.m[1][1] == 1 && eyeToHeadLeft.m[1][2] == 0 && eyeToHeadLeft.m[1][3] == 0 &&
						eyeToHeadLeft.m[2][0] == 0 && eyeToHeadLeft.m[2][1] == 0 && eyeToHeadLeft.m[2][2] == 1 && eyeToHeadLeft.m[2][3] == 0;

					if (isCrystalHmd) {
						system = "Pimax Crystal HMD";
					}
				} else if (deviceClass == vr::TrackedDeviceClass_Controller && system == "oculus") {
					vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_RenderModelName_String, buffer, vr::k_unMaxPropertyStringSize, &err);
					std::string renderModel(buffer);
					vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_ConnectedWirelessDongle_String, buffer, vr::k_unMaxPropertyStringSize, &err);
					std::string connectedWirelessDongle(buffer);

					if (renderModel.find("{aapvr}") != std::string::npos &&
						renderModel.find("crystal") != std::string::npos &&
						connectedWirelessDongle.find("lighthouse") != std::string::npos) {
						system = "Pimax Crystal Controllers";
					}
				}

				auto existing = std::find(trackingSystems.begin(), trackingSystems.end(), system);
				if (existing != trackingSystems.end())
				{
					if (deviceClass == vr::TrackedDeviceClass_HMD)
					{
						trackingSystems.erase(existing);
						trackingSystems.insert(trackingSystems.begin(), system);
					}
				}
				else
				{
					trackingSystems.push_back(system);
				}

				VRDevice device;
				device.id = id;
				device.deviceClass = deviceClass;
				device.trackingSystem = system;

				vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_ModelNumber_String, buffer, vr::k_unMaxPropertyStringSize, &err);
				device.model = std::string(buffer);

				vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_SerialNumber_String, buffer, vr::k_unMaxPropertyStringSize, &err);
				device.serial = std::string(buffer);

				device.controllerRole = (vr::ETrackedControllerRole)vr::VRSystem()->GetInt32TrackedDeviceProperty(id, vr::Prop_ControllerRoleHint_Int32, &err);

				vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
				vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
				if (id < vr::k_unMaxTrackedDeviceCount) {
					device.isTracking = poses[id].bPoseIsValid && poses[id].eTrackingResult == vr::TrackingResult_Running_OK;
					
					// Check if device is rapidly moving (for tracker identification)
					if (device.isTracking && device.deviceClass == vr::TrackedDeviceClass_GenericTracker) {
						const auto& pose = poses[id];
						double velocity = 0.0;
						if (pose.bPoseIsValid) {
							// Calculate velocity magnitude from velocity vector
							double vx = pose.vVelocity.v[0];
							double vy = pose.vVelocity.v[1];
							double vz = pose.vVelocity.v[2];
							velocity = sqrt(vx * vx + vy * vy + vz * vz);
						}
						// Threshold: 0.5 m/s (500 mm/s) for "rapidly moving"
						device.isMoving = velocity > 0.5;
					} else {
						device.isMoving = false;
					}
				}

				state.devices.push_back(device);
			}
			else
			{
				printf("failed to get tracking system name for id %d\n", id);
			}
		}
	}

	return state;
}

int VRState::FindDevice(const std::string& trackingSystem, const std::string& model, const std::string& serial) const {
	for (size_t i = 0; i < devices.size(); i++) {
		const auto& device = devices[i];
		
		uint8_t matches = 0;

		if (device.model == model) {
			matches++;
		}
		if (device.serial == serial) {
			matches++;
		}

		if (device.trackingSystem == trackingSystem &&
			((matches == 2 && device.deviceClass != vr::TrackedDeviceClass::TrackedDeviceClass_HMD) ||
			(matches >= 1 && device.deviceClass == vr::TrackedDeviceClass::TrackedDeviceClass_HMD))) {
			return device.id;
		}
	}

	return -1;
}

