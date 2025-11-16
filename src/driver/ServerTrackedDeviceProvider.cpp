#include "ServerTrackedDeviceProvider.h"
#include "Logging.h"
#include "InterfaceHookInjector.h"
#include "IsometryTransform.h"

#include <random>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <filesystem>
#include <fstream>
#include <limits.h>
#ifndef _WIN32
#include <time.h>
#endif

#ifndef _WIN32
static inline void QueryPerformanceCounter(LARGE_INTEGER *value) {
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	value->QuadPart = static_cast<long long>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

static inline void QueryPerformanceFrequency(LARGE_INTEGER *value) {
	value->QuadPart = 1000000000LL;
}
#endif

vr::EVRInitError ServerTrackedDeviceProvider::Init(vr::IVRDriverContext *pDriverContext)
{
	TRACE("ServerTrackedDeviceProvider::Init()");
	VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);

	memset(transforms, 0, vr::k_unMaxTrackedDeviceCount * sizeof(DeviceTransform));
	for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i) {
		transforms[i].scale = 1.0;
		transforms[i].transform = IsoTransform();
		transforms[i].targetTransform = IsoTransform();
	}
	memset(&alignmentSpeedParams, 0, sizeof alignmentSpeedParams);

	alignmentSpeedParams.thr_rot_tiny = 0.1f * (EIGEN_PI / 180.0f);
	alignmentSpeedParams.thr_rot_small = 1.0f * (EIGEN_PI / 180.0f);
	alignmentSpeedParams.thr_rot_large = 5.0f * (EIGEN_PI / 180.0f);

	alignmentSpeedParams.thr_trans_tiny = 0.1f / 1000.0;
	alignmentSpeedParams.thr_trans_small = 1.0f / 1000.0;
	alignmentSpeedParams.thr_trans_large = 20.0f / 1000.0;
	
	alignmentSpeedParams.align_speed_tiny = 0.05f;
	alignmentSpeedParams.align_speed_small = 0.2f;
	alignmentSpeedParams.align_speed_large = 2.0f;

	InjectHooks(this, pDriverContext);
	server.Run();
	shmem.Create(OPENVR_SPACECALIBRATOR_SHMEM_NAME);

	debugTransform = Eigen::Vector3d::Zero();
	debugRotation = Eigen::Quaterniond::Identity();

	// Launch overlay application
	LaunchOverlay();

	return vr::VRInitError_None;
}

void ServerTrackedDeviceProvider::Cleanup()
{
	TRACE("ServerTrackedDeviceProvider::Cleanup()");
	StopOverlay();
	server.Stop();
	shmem.Close();
	DisableHooks();
	VR_CLEANUP_SERVER_DRIVER_CONTEXT();
}

void ServerTrackedDeviceProvider::LaunchOverlay()
{
#ifndef _WIN32
	if (overlayProcessId != 0) {
		// Already launched
		return;
	}

	// Try to find overlay binary relative to driver location
	// Driver is typically in: ~/.local/share/SteamVR/drivers/01spacecalibrator/bin/linux64/
	// Overlay should be in the same directory: ~/.local/share/SteamVR/drivers/01spacecalibrator/bin/linux64/space-calibrator
	std::string overlayPath;
	
	// Try multiple possible locations
	const char* home = getenv("HOME");
	if (home) {
		// Primary location: Flatpak/standard SteamVR
		std::string basePath = std::string(home) + "/.local/share/SteamVR/drivers/01spacecalibrator/bin/linux64/space-calibrator";
		if (std::filesystem::exists(basePath)) {
			overlayPath = basePath;
		}
	}
	
	// Also try alternative SteamVR location
	if (overlayPath.empty() && home) {
		std::string altPath = std::string(home) + "/.steam/steam/steamapps/common/SteamVR/drivers/01spacecalibrator/bin/linux64/space-calibrator";
		if (std::filesystem::exists(altPath)) {
			overlayPath = altPath;
		}
	}
	
	// Try to get driver library path from /proc/self/maps and derive overlay path
	if (overlayPath.empty()) {
		// Read /proc/self/maps to find driver library path
		std::ifstream maps("/proc/self/maps");
		std::string line;
		while (std::getline(maps, line)) {
			if (line.find("driver_01spacecalibrator.so") != std::string::npos) {
				// Extract path from maps line (format: address perms offset dev inode pathname)
				size_t lastSpace = line.find_last_of(' ');
				if (lastSpace != std::string::npos) {
					std::string driverLibPath = line.substr(lastSpace + 1);
					if (std::filesystem::exists(driverLibPath)) {
						// Get directory and construct overlay path
						std::filesystem::path driverPath(driverLibPath);
						std::filesystem::path overlayPathObj = driverPath.parent_path() / "space-calibrator";
						if (std::filesystem::exists(overlayPathObj)) {
							overlayPath = overlayPathObj.string();
							break;
						}
					}
				}
			}
		}
	}
	
	if (overlayPath.empty()) {
		LOG("Warning: Could not find overlay binary, overlay will not auto-launch");
		return;
	}

	LOG("Launching overlay: %s", overlayPath.c_str());

	pid_t pid = fork();
	if (pid == 0) {
		// Child process - launch overlay
		// Detach from parent process group
		setsid();
		
		// Close standard file descriptors
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		
		// Redirect to /dev/null
		open("/dev/null", O_RDONLY);
		open("/dev/null", O_WRONLY);
		open("/dev/null", O_WRONLY);
		
		// Execute overlay
		execl(overlayPath.c_str(), "space-calibrator", (char*)nullptr);
		
		// If execl fails, exit
		_exit(1);
	} else if (pid > 0) {
		// Parent process - store PID
		overlayProcessId = pid;
		LOG("Overlay launched with PID: %d", pid);
	} else {
		LOG("Error: Failed to fork process for overlay launch");
	}
#else
	// Windows implementation would go here if needed
#endif
}

void ServerTrackedDeviceProvider::StopOverlay()
{
#ifndef _WIN32
	if (overlayProcessId == 0) {
		return;
	}

	LOG("Stopping overlay process (PID: %d)", overlayProcessId);
	
	// Send SIGTERM to overlay process
	kill(overlayProcessId, SIGTERM);
	
	// Wait for process to exit (with timeout)
	int status;
	pid_t result = waitpid(overlayProcessId, &status, WNOHANG);
	if (result == 0) {
		// Process still running, wait a bit
		usleep(500000); // 500ms
		result = waitpid(overlayProcessId, &status, WNOHANG);
		if (result == 0) {
			// Still running, force kill
			LOG("Overlay process did not exit, sending SIGKILL");
			kill(overlayProcessId, SIGKILL);
			waitpid(overlayProcessId, &status, 0);
		}
	}
	
	overlayProcessId = 0;
	LOG("Overlay process stopped");
#else
	// Windows implementation would go here if needed
#endif
}

namespace {

	vr::HmdQuaternion_t convert(const Eigen::Quaterniond& q) {
		vr::HmdQuaternion_t result;
		result.w = q.w();
		result.x = q.x();
		result.y = q.y();
		result.z = q.z();
		return result;
	}

	vr::HmdVector3_t convert(const Eigen::Vector3d& v) {
		vr::HmdVector3_t result;
		result.v[0] = (float) v.x();
		result.v[1] = (float) v.y();
		result.v[2] = (float) v.z();
		return result;
	}

	Eigen::Quaterniond convert(const vr::HmdQuaternion_t& q) {
		return Eigen::Quaterniond(q.w, q.x, q.y, q.z);
	}

	Eigen::Vector3d convert(const vr::HmdVector3d_t& v) {
		return Eigen::Vector3d(v.v[0], v.v[1], v.v[2]);
	}

	Eigen::Vector3d convert(const double* arr) {
		return Eigen::Vector3d(arr[0], arr[1], arr[2]);
	}

	IsoTransform toIsoWorldTransform(const vr::DriverPose_t& pose) {
		Eigen::Quaterniond rot(pose.qWorldFromDriverRotation.w, pose.qWorldFromDriverRotation.x, pose.qWorldFromDriverRotation.y, pose.qWorldFromDriverRotation.z);
		Eigen::Vector3d trans(pose.vecWorldFromDriverTranslation[0], pose.vecWorldFromDriverTranslation[1], pose.vecWorldFromDriverTranslation[2]);

		return IsoTransform(rot, trans);
	}

	IsoTransform toIsoPose(const vr::DriverPose_t& pose) {
		auto worldXform = toIsoWorldTransform(pose);

		Eigen::Quaterniond rot(pose.qRotation.w, pose.qRotation.x, pose.qRotation.y, pose.qRotation.z);
		Eigen::Vector3d trans(pose.vecPosition[0], pose.vecPosition[1], pose.vecPosition[2]);

		return worldXform * IsoTransform(rot, trans);
	}
}

ServerTrackedDeviceProvider::DeltaSize ServerTrackedDeviceProvider::GetTransformDeltaSize(
	DeltaSize prior_delta,
	const IsoTransform& deviceWorldPose,
	const IsoTransform& src,
	const IsoTransform& target
) const {
	const auto src_pose = src * deviceWorldPose;
	const auto target_pose = target * deviceWorldPose;

	const auto trans_delta = (src_pose.translation - target_pose.translation).squaredNorm();
	const auto rot_delta = src_pose.rotation.angularDistance(target_pose.rotation);

	DeltaSize trans_level, rot_level;

	if (trans_delta > alignmentSpeedParams.thr_trans_large) trans_level = DeltaSize::LARGE;
	else if (trans_delta > alignmentSpeedParams.thr_trans_small) trans_level = DeltaSize::SMALL;
	else trans_level = DeltaSize::TINY;

	if (rot_delta > alignmentSpeedParams.thr_rot_large) rot_level = DeltaSize::LARGE;
	else if (rot_delta > alignmentSpeedParams.thr_rot_small) rot_level = DeltaSize::SMALL;
	else rot_level = DeltaSize::TINY;

	if (trans_level == DeltaSize::TINY && rot_level == DeltaSize::TINY) return DeltaSize::TINY;
	else return std::max(prior_delta, std::max(trans_level, rot_level));
}

double ServerTrackedDeviceProvider::GetTransformRate(DeltaSize delta) const {
	switch (delta) {
	case DeltaSize::TINY: return alignmentSpeedParams.align_speed_tiny;
	case DeltaSize::SMALL: return alignmentSpeedParams.align_speed_small;
	default: return alignmentSpeedParams.align_speed_large;
	}
}

void ServerTrackedDeviceProvider::BlendTransform(DeviceTransform& device, const IsoTransform &deviceWorldPose) const {
	LARGE_INTEGER timestamp, freq;
	QueryPerformanceCounter(&timestamp);
	QueryPerformanceFrequency(&freq);

	double lerp = (timestamp.QuadPart - device.lastPoll.QuadPart) / (double)freq.QuadPart;
	device.lastPoll = timestamp;
	
	lerp *= GetTransformRate(device.currentRate);
	if (lerp > 1.0)
		lerp = 1.0;
	if (lerp < 0 || std::isnan(lerp))
		lerp = 0;

	device.transform = device.transform.interpolateAround(lerp, device.targetTransform, deviceWorldPose.translation);
}

void ServerTrackedDeviceProvider::ApplyTransform(DeviceTransform& device, vr::DriverPose_t& devicePose) const {
	auto deviceWorldTransform = toIsoWorldTransform(devicePose);
	deviceWorldTransform = device.transform * deviceWorldTransform;
	devicePose.vecWorldFromDriverTranslation[0] = deviceWorldTransform.translation(0);
	devicePose.vecWorldFromDriverTranslation[1] = deviceWorldTransform.translation(1);
	devicePose.vecWorldFromDriverTranslation[2] = deviceWorldTransform.translation(2);
	devicePose.qWorldFromDriverRotation = convert(deviceWorldTransform.rotation);
}


inline vr::HmdQuaternion_t operator*(const vr::HmdQuaternion_t &lhs, const vr::HmdQuaternion_t &rhs) {
	return {
		(lhs.w * rhs.w) - (lhs.x * rhs.x) - (lhs.y * rhs.y) - (lhs.z * rhs.z),
		(lhs.w * rhs.x) + (lhs.x * rhs.w) + (lhs.y * rhs.z) - (lhs.z * rhs.y),
		(lhs.w * rhs.y) + (lhs.y * rhs.w) + (lhs.z * rhs.x) - (lhs.x * rhs.z),
		(lhs.w * rhs.z) + (lhs.z * rhs.w) + (lhs.x * rhs.y) - (lhs.y * rhs.x)
	};
}

inline vr::HmdVector3d_t quaternionRotateVector(const vr::HmdQuaternion_t& quat, const double(&vector)[3]) {
	vr::HmdQuaternion_t vectorQuat = { 0.0, vector[0], vector[1] , vector[2] };
	vr::HmdQuaternion_t conjugate = { quat.w, -quat.x, -quat.y, -quat.z };
	auto rotatedVectorQuat = quat * vectorQuat * conjugate;
	return { rotatedVectorQuat.x, rotatedVectorQuat.y, rotatedVectorQuat.z };
}

void ServerTrackedDeviceProvider::SetDeviceTransform(const protocol::SetDeviceTransform& newTransform)
{
	auto &tf = transforms[newTransform.openVRID];
	tf.enabled = newTransform.enabled;

	if (newTransform.updateTranslation) {
		tf.targetTransform.translation = convert(newTransform.translation);
		if (!newTransform.lerp) {
			tf.transform.translation = tf.targetTransform.translation;
		}
	}

	if (newTransform.updateRotation) {
		tf.targetTransform.rotation = convert(newTransform.rotation);

		if (!newTransform.lerp) {
			tf.transform.rotation = tf.targetTransform.rotation;
		}
	}

	if (newTransform.updateScale)
		tf.scale = newTransform.scale;

	tf.quash = newTransform.quash;
}

bool ServerTrackedDeviceProvider::HandleDevicePoseUpdated(uint32_t openVRID, vr::DriverPose_t &pose)
{
	if (openVRID > 0) {
		double debugTransNorm = debugTransform.norm();
		double debugRotDist = debugRotation.angularDistance(Eigen::Quaterniond::Identity());
		if (debugTransNorm > 0.001 || debugRotDist > 0.001) {
			auto dbgPos = convert(pose.vecPosition) + debugTransform;
			auto dbgRot = convert(pose.qRotation) * debugRotation;
			pose.qRotation = convert(dbgRot);
			pose.vecPosition[0] = dbgPos(0);
			pose.vecPosition[1] = dbgPos(1);
			pose.vecPosition[2] = dbgPos(2);
		}
	}

	shmem.SetPose(openVRID, pose);

	auto& tf = transforms[openVRID];

	if (!tf.enabled && !tf.quash && openVRID > 0) {
		pose.vecWorldFromDriverTranslation[0] = 0.0;
		pose.vecWorldFromDriverTranslation[1] = 0.0;
		pose.vecWorldFromDriverTranslation[2] = 0.0;
		pose.qWorldFromDriverRotation.w = 1.0;
		pose.qWorldFromDriverRotation.x = 0.0;
		pose.qWorldFromDriverRotation.y = 0.0;
		pose.qWorldFromDriverRotation.z = 0.0;
	}

	if (tf.quash) {
		pose.vecPosition[0] = -pose.vecWorldFromDriverTranslation[0];
		pose.vecPosition[1] = -pose.vecWorldFromDriverTranslation[1] + 9001;
		pose.vecPosition[2] = -pose.vecWorldFromDriverTranslation[2];
	} else if (tf.enabled)
	{
		pose.vecPosition[0] *= tf.scale;
		pose.vecPosition[1] *= tf.scale;
		pose.vecPosition[2] *= tf.scale;

		auto deviceWorldPose = toIsoPose(pose);
		tf.currentRate = GetTransformDeltaSize(tf.currentRate, deviceWorldPose, tf.transform, tf.targetTransform);
		double lerp = GetTransformRate(tf.currentRate);

		BlendTransform(tf, deviceWorldPose);
		ApplyTransform(tf, pose);
	}

	return true;
}

void ServerTrackedDeviceProvider::HandleApplyRandomOffset() {
	std::random_device gen;
	std::uniform_real_distribution<double> d(-1, 1);
	auto init = Eigen::Vector3d(d(gen), d(gen), d(gen));
	auto posOffset = init * 0.25f;

	debugTransform = posOffset;
	debugRotation = Eigen::Quaterniond::Identity();

	std::ostringstream oss;
	oss << "Applied random offset: " << posOffset << " from init " << init << std::endl;
	LOG("%s", oss.str().c_str());
}
