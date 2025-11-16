#include "OpenVR-SpaceCalibratorDriver.h"
#include "ServerTrackedDeviceProvider.h"
#include "VRWatchdogProvider.h"
#include "Logging.h"

#include <cstdio>
#include <cstring>
#include <openvr_driver.h>

OPENVRSPACECALIBRATORDRIVER_API void *HmdDriverFactory(const char *pInterfaceName, int *pReturnCode)
{
	// Initialize logging on first call
	static bool loggingInitialized = false;
	if (!loggingInitialized) {
		OpenLogFile();
		loggingInitialized = true;
		LOG("HmdDriverFactory called - driver library loaded");
	}
	
	TRACE("HmdDriverFactory(%s)", pInterfaceName);
	LOG("HmdDriverFactory requested interface: %s", pInterfaceName);

	static ServerTrackedDeviceProvider server;
	static VRWatchdogProvider watchdog;

	if (std::strcmp(vr::IServerTrackedDeviceProvider_Version, pInterfaceName) == 0)
	{
		return &server;
	}
	else if (std::strcmp(vr::IVRWatchdogProvider_Version, pInterfaceName) == 0)
	{
		return &watchdog;
	}

	if (pReturnCode)
	{
		*pReturnCode = vr::VRInitError_Init_InterfaceNotFound;
	}
	return nullptr;
}

