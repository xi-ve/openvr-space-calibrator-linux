#pragma once

#include <openvr_driver.h>

class VRWatchdogProvider : public vr::IVRWatchdogProvider
{
	virtual vr::EVRInitError Init(vr::IVRDriverContext *pDriverContext) override
	{
		VR_INIT_WATCHDOG_DRIVER_CONTEXT(pDriverContext);
		return vr::VRInitError_None;
	}

	virtual void Cleanup() override
	{
		VR_CLEANUP_WATCHDOG_DRIVER_CONTEXT()
	}
};

