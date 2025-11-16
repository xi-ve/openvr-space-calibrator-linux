#pragma once

#define EIGEN_MPL2_ONLY

#include "IPCServer.h"
#include "../common/Protocol.h"
#include "IsometryTransform.h"

#include <Eigen/Dense>

#include <openvr_driver.h>

#ifndef _WIN32
#include <sys/types.h>
#endif

#ifndef _WIN32
struct LARGE_INTEGER {
	long long QuadPart;
};
#endif

class ServerTrackedDeviceProvider : public vr::IServerTrackedDeviceProvider
{
public:
	virtual vr::EVRInitError Init(vr::IVRDriverContext *pDriverContext) override;

	virtual void Cleanup() override;

	virtual const char * const *GetInterfaceVersions() { return vr::k_InterfaceVersions; }

	virtual void RunFrame() { }

	virtual bool ShouldBlockStandbyMode() { return false; }

	virtual void EnterStandby() { }

	virtual void LeaveStandby() { }

	ServerTrackedDeviceProvider() : server(this)
#ifndef _WIN32
		, overlayProcessId(0)
#endif
	{ }
	void SetDeviceTransform(const protocol::SetDeviceTransform &newTransform);
	bool HandleDevicePoseUpdated(uint32_t openVRID, vr::DriverPose_t &pose);
	void HandleApplyRandomOffset();
	void HandleSetAlignmentSpeedParams(const protocol::AlignmentSpeedParams params) {
		alignmentSpeedParams = params;
	}
	void LaunchOverlay();
	void StopOverlay();

private:
	IPCServer server;
	protocol::DriverPoseShmem shmem;

	enum DeltaSize {
		TINY,
		SMALL,
		LARGE
	};

	struct DeviceTransform
	{
		bool enabled = false;
		bool quash = false;
		IsoTransform transform, targetTransform;
		double scale;
		LARGE_INTEGER lastPoll;
		DeltaSize currentRate = DeltaSize::TINY;
	};

	DeviceTransform transforms[vr::k_unMaxTrackedDeviceCount];
	Eigen::Vector3d debugTransform;
	Eigen::Quaterniond debugRotation;

	DeltaSize currentDeltaSpeed[vr::k_unMaxTrackedDeviceCount];

	protocol::AlignmentSpeedParams alignmentSpeedParams;
	
#ifndef _WIN32
	pid_t overlayProcessId;
#endif

	DeltaSize GetTransformDeltaSize(
		DeltaSize prior_delta,
		const IsoTransform& deviceWorldPose,
		const IsoTransform& src,
		const IsoTransform& target
	) const;

	double GetTransformRate(DeltaSize delta) const;

	void BlendTransform(DeviceTransform& device, const IsoTransform& deviceWorldPose) const;
	void ApplyTransform(DeviceTransform& device, vr::DriverPose_t& devicePose) const;
};
