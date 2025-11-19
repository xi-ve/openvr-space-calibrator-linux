#include "PlayspaceMovement.h"
#include "Logging.h"
#include <cmath>
#include <filesystem>
#include <unistd.h>
#include <cstring>
#include <chrono>

namespace {
	Eigen::Vector3d ExtractTranslation(const vr::HmdMatrix34_t& matrix)
	{
		return Eigen::Vector3d(matrix.m[0][3], matrix.m[1][3], matrix.m[2][3]);
	}

	Eigen::Matrix3d ExtractRotation(const vr::HmdMatrix34_t& matrix)
	{
		Eigen::Matrix3d rot;
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				rot(i, j) = matrix.m[i][j];
			}
		}
		return rot;
	}

	vr::HmdMatrix34_t ComposeMatrix(const Eigen::Matrix3d& rotation, const Eigen::Vector3d& translation)
	{
		vr::HmdMatrix34_t matrix;
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				matrix.m[i][j] = rotation(i, j);
			}
			matrix.m[i][3] = translation(i);
		}
		return matrix;
	}

	vr::HmdQuad_t TransformQuad(const vr::HmdQuad_t& quad, const Eigen::Matrix3d& rotation, const Eigen::Vector3d& translation)
	{
		vr::HmdQuad_t result;
		for (int i = 0; i < 4; i++) {
			Eigen::Vector3d point(quad.vCorners[i].v[0], quad.vCorners[i].v[1], quad.vCorners[i].v[2]);
			Eigen::Vector3d transformed = rotation * point + translation;
			result.vCorners[i].v[0] = transformed.x();
			result.vCorners[i].v[1] = transformed.y();
			result.vCorners[i].v[2] = transformed.z();
		}
		return result;
	}
}

PlayspaceMovement::PlayspaceMovement()
	: m_initialized(false)
	, m_actionSet(vr::k_ulInvalidActionSetHandle)
	, m_actionResetPlayspace(vr::k_ulInvalidActionHandle)
	, m_actionPullPlayspace(vr::k_ulInvalidActionHandle)
	, m_actionControllerPose(vr::k_ulInvalidActionHandle)
	, m_pullActive(false)
	, m_resetPressedLastFrame(false)
	, m_hasSavedReference(false)
{
	memset(&m_lastControllerPose, 0, sizeof(m_lastControllerPose));
	memset(&m_initialControllerPose, 0, sizeof(m_initialControllerPose));
}

PlayspaceMovement::~PlayspaceMovement()
{
	Shutdown();
}

bool PlayspaceMovement::Initialize()
{
	if (m_initialized) {
		return true;
	}

	if (!vr::VRInput()) {
		LOG_ERROR("VRInput not available");
		return false;
	}

	char exePath[1024];
	ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
	if (count == -1) {
		LOG_ERROR("Could not determine executable path");
		return false;
	}
	exePath[count] = '\0';
	
	std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
	std::string actionsPath = (exeDir / "actions.json").string();

	if (!std::filesystem::exists(actionsPath)) {
		LOG_WARNING("actions.json not found at: " + actionsPath + " - playspace movement bindings may not work");
		LOG_WARNING("Make sure actions.json is installed in the same directory as the executable");
		return false;
	}

	vr::EVRInputError err = vr::VRInput()->SetActionManifestPath(actionsPath.c_str());
	if (err != vr::VRInputError_None) {
		LOG_ERROR(std::string("Failed to set action manifest path: ") + std::to_string(err) + " (path: " + actionsPath + ")");
		return false;
	}
	
	LOG_DEBUG("Action manifest loaded from: " + actionsPath);

	err = vr::VRInput()->GetActionSetHandle("/actions/playspace", &m_actionSet);
	if (err != vr::VRInputError_None) {
		LOG_ERROR(std::string("Failed to get action set handle: ") + std::to_string(err));
		return false;
	}

	err = vr::VRInput()->GetActionHandle("/actions/playspace/in/ResetPlayspace", &m_actionResetPlayspace);
	if (err != vr::VRInputError_None) {
		LOG_ERROR(std::string("Failed to get ResetPlayspace action handle: ") + std::to_string(err));
		return false;
	}

	err = vr::VRInput()->GetActionHandle("/actions/playspace/in/PullPlayspace", &m_actionPullPlayspace);
	if (err != vr::VRInputError_None) {
		LOG_ERROR(std::string("Failed to get PullPlayspace action handle: ") + std::to_string(err));
		return false;
	}

	err = vr::VRInput()->GetActionHandle("/actions/playspace/in/ControllerPose", &m_actionControllerPose);
	if (err != vr::VRInputError_None) {
		LOG_ERROR(std::string("Failed to get ControllerPose action handle: ") + std::to_string(err));
		return false;
	}

	m_initialized = true;
	LOG_INFO("PlayspaceMovement initialized successfully");
	LOG_INFO(std::string("Action handles - Reset: ") + std::to_string(m_actionResetPlayspace) + 
		", Pull: " + std::to_string(m_actionPullPlayspace) + 
		", Pose: " + std::to_string(m_actionControllerPose) +
		", ActionSet: " + std::to_string(m_actionSet));
	return true;
}

void PlayspaceMovement::Shutdown()
{
	if (!m_initialized) {
		return;
	}

	if (m_pullReference.workingCopyInitialized && vr::VRChaperoneSetup()) {
		vr::VRChaperoneSetup()->RevertWorkingCopy();
		m_pullReference.workingCopyInitialized = false;
	}

	m_actionSet = vr::k_ulInvalidActionSetHandle;
	m_actionResetPlayspace = vr::k_ulInvalidActionHandle;
	m_actionPullPlayspace = vr::k_ulInvalidActionHandle;
	m_actionControllerPose = vr::k_ulInvalidActionHandle;
	m_initialized = false;
	m_pullActive = false;
	m_resetPressedLastFrame = false;
}

void PlayspaceMovement::SaveOriginalPlayspace()
{
	if (!vr::VRChaperoneSetup()) {
		LOG_ERROR("VRChaperoneSetup not available");
		return;
	}

	vr::VRChaperoneSetup()->RevertWorkingCopy();

	uint32_t quadCount = 0;
	vr::VRChaperoneSetup()->GetLiveCollisionBoundsInfo(nullptr, &quadCount);

	settings.original.geometry.resize(quadCount);
	vr::VRChaperoneSetup()->GetLiveCollisionBoundsInfo(&settings.original.geometry[0], &quadCount);
	vr::VRChaperoneSetup()->GetWorkingStandingZeroPoseToRawTrackingPose(&settings.original.standingCenter);
	vr::VRChaperoneSetup()->GetWorkingPlayAreaSize(&settings.original.playSpaceSize.v[0], &settings.original.playSpaceSize.v[1]);
	
	settings.original.saved = true;
	m_hasSavedReference = true;
	LOG_INFO("Original playspace saved");
}

void PlayspaceMovement::ResetPlayspace()
{
	if (!settings.original.saved) {
		LOG_WARNING("Cannot reset playspace: original not saved");
		return;
	}

	if (!vr::VRChaperoneSetup()) {
		LOG_ERROR("VRChaperoneSetup not available");
		return;
	}

	vr::VRChaperoneSetup()->RevertWorkingCopy();
	vr::VRChaperoneSetup()->SetWorkingCollisionBoundsInfo(&settings.original.geometry[0], (uint32_t)settings.original.geometry.size());
	vr::VRChaperoneSetup()->SetWorkingStandingZeroPoseToRawTrackingPose(&settings.original.standingCenter);
	vr::VRChaperoneSetup()->SetWorkingPlayAreaSize(settings.original.playSpaceSize.v[0], settings.original.playSpaceSize.v[1]);
	vr::VRChaperoneSetup()->CommitWorkingCopy(vr::EChaperoneConfigFile_Live);
	
	m_hasSavedReference = false;
	settings.original.saved = false;
	m_pullActive = false;
	LOG_INFO("Playspace reset to original - next pull will save reset position as new original");
}

void PlayspaceMovement::PullPlayspace(const vr::HmdMatrix34_t& controllerPose)
{
	if (!vr::VRChaperoneSetup()) {
		LOG_ERROR("VRChaperoneSetup not available");
		return;
	}

	if (!m_pullActive) {
		vr::VRChaperoneSetup()->RevertWorkingCopy();
		
		uint32_t quadCount = 0;
		vr::VRChaperoneSetup()->GetLiveCollisionBoundsInfo(nullptr, &quadCount);
		
		m_pullReference.geometry.resize(quadCount);
		vr::VRChaperoneSetup()->GetLiveCollisionBoundsInfo(&m_pullReference.geometry[0], &quadCount);
		vr::VRChaperoneSetup()->GetWorkingStandingZeroPoseToRawTrackingPose(&m_pullReference.standingCenter);
		vr::VRChaperoneSetup()->GetWorkingPlayAreaSize(&m_pullReference.playSpaceSize.v[0], &m_pullReference.playSpaceSize.v[1]);
		m_pullReference.workingCopyInitialized = false;
		
		if (!settings.original.saved) {
			settings.original.geometry = m_pullReference.geometry;
			settings.original.standingCenter = m_pullReference.standingCenter;
			settings.original.playSpaceSize = m_pullReference.playSpaceSize;
			settings.original.saved = true;
			LOG_INFO("PullPlayspace started - saved current playspace as original (first pull)");
		} else {
			LOG_INFO("PullPlayspace started - saved current playspace as pull reference");
		}
		
		m_hasSavedReference = true;
		
		m_initialControllerPose = controllerPose;
		m_lastControllerPose = controllerPose;
		m_smoothedControllerPos = ExtractTranslation(controllerPose);
		m_pullActive = true;
		return;
	}

	Eigen::Vector3d initialPos = ExtractTranslation(m_initialControllerPose);
	Eigen::Vector3d currentPos = ExtractTranslation(controllerPose);
	
	double smoothingFactor = 0.85;
	if (settings.movementMultiplier > 3.0f) {
		smoothingFactor = 0.95 - (settings.movementMultiplier - 3.0f) * 0.01;
		if (smoothingFactor < 0.90) {
			smoothingFactor = 0.90;
		}
	}
	m_smoothedControllerPos = m_smoothedControllerPos * smoothingFactor + currentPos * (1.0 - smoothingFactor);
	
	Eigen::Vector3d controllerDelta = m_smoothedControllerPos - initialPos;
	
	if (controllerDelta.norm() < 0.001f) {
		m_lastControllerPose = controllerPose;
		return;
	}
	
	if (settings.movementMultiplier <= 0.0f || settings.movementMultiplier > 100.0f) {
		LOG_ERROR(std::string("Invalid movement multiplier: ") + std::to_string(settings.movementMultiplier));
		m_lastControllerPose = controllerPose;
		return;
	}
	
	Eigen::Vector3d totalDelta = controllerDelta * settings.movementMultiplier;
	
	double maxDeltaMagnitude = 2.0;
	if (settings.movementMultiplier > 2.5f) {
		maxDeltaMagnitude = 2.0 + (settings.movementMultiplier - 2.5f) * 0.5;
		if (maxDeltaMagnitude > 5.0) {
			maxDeltaMagnitude = 5.0;
		}
	}
	
	if (totalDelta.norm() > maxDeltaMagnitude) {
		totalDelta = totalDelta.normalized() * maxDeltaMagnitude;
		static int clampCount = 0;
		if (clampCount++ % 60 == 0) {
			LOG_WARNING(std::string("Delta clamped to prevent excessive movement. Magnitude: ") + 
				std::to_string(totalDelta.norm()) + ", Controller delta: " + std::to_string(controllerDelta.norm()) +
				", Multiplier: " + std::to_string(settings.movementMultiplier) + ", Max allowed: " + std::to_string(maxDeltaMagnitude));
		}
	}
	
	static auto lastCommitTime = std::chrono::steady_clock::now();
	auto currentTime = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastCommitTime).count();
	
	if (elapsed < 33) {
		m_lastControllerPose = controllerPose;
		return;
	}
	lastCommitTime = currentTime;

	Eigen::Vector3d referenceCenterTranslation = ExtractTranslation(m_pullReference.standingCenter);
	Eigen::Vector3d newCenterTranslation = referenceCenterTranslation + totalDelta;
	vr::HmdMatrix34_t newStandingCenter = ComposeMatrix(ExtractRotation(m_pullReference.standingCenter), newCenterTranslation);

	std::vector<vr::HmdQuad_t> quads = m_pullReference.geometry;
	for (size_t i = 0; i < quads.size(); i++) {
		quads[i] = TransformQuad(quads[i], Eigen::Matrix3d::Identity(), totalDelta);
	}

	if (!m_pullReference.workingCopyInitialized) {
		vr::VRChaperoneSetup()->RevertWorkingCopy();
		m_pullReference.workingCopyInitialized = true;
	}
	
	vr::VRChaperoneSetup()->SetWorkingCollisionBoundsInfo(&quads[0], (uint32_t)quads.size());
	vr::VRChaperoneSetup()->SetWorkingStandingZeroPoseToRawTrackingPose(&newStandingCenter);
	vr::VRChaperoneSetup()->SetWorkingPlayAreaSize(m_pullReference.playSpaceSize.v[0], m_pullReference.playSpaceSize.v[1]);
	
	bool success = vr::VRChaperoneSetup()->CommitWorkingCopy(vr::EChaperoneConfigFile_Live);
	if (!success) {
		LOG_ERROR("Failed to commit playspace changes");
		return;
	}
	
	m_lastControllerPose = controllerPose;
}

void PlayspaceMovement::Update()
{
	if (!m_initialized) {
		return;
	}

	if (!settings.enabled) {
		return;
	}

	vr::VRActiveActionSet_t actionSet = { 0 };
	actionSet.ulActionSet = m_actionSet;
	actionSet.nPriority = vr::k_nActionSetOverlayGlobalPriorityMin;
	vr::EVRInputError err = vr::VRInput()->UpdateActionState(&actionSet, sizeof(actionSet), 1);
	if (err != vr::VRInputError_None) {
		if (err != vr::VRInputError_NoData) {
			static int errorCount = 0;
			if (errorCount++ % 300 == 0) {
				LOG_WARNING(std::string("Failed to update action state: ") + std::to_string(err) + " (VRInputError code)");
			}
		}
		return;
	}

	vr::InputDigitalActionData_t resetData = { 0 };
	err = vr::VRInput()->GetDigitalActionData(m_actionResetPlayspace, &resetData, sizeof(resetData), vr::k_ulInvalidInputValueHandle);
	if (err != vr::VRInputError_None && err != vr::VRInputError_NoData) {
		static int errorCount = 0;
		if (errorCount++ % 300 == 0) {
			LOG_WARNING(std::string("Failed to get ResetPlayspace action data: ") + std::to_string(err) + " (VRInputError code)");
		}
	}
	
	if (resetData.bState && !m_resetPressedLastFrame) {
		LOG_INFO("Reset Playspace action triggered");
		ResetPlayspace();
	}
	m_resetPressedLastFrame = resetData.bState;

	vr::InputDigitalActionData_t pullData = { 0 };
	err = vr::VRInput()->GetDigitalActionData(m_actionPullPlayspace, &pullData, sizeof(pullData), vr::k_ulInvalidInputValueHandle);
	if (err != vr::VRInputError_None && err != vr::VRInputError_NoData) {
		static int errorCount = 0;
		if (errorCount++ % 300 == 0) {
			LOG_WARNING(std::string("Failed to get PullPlayspace action data: ") + std::to_string(err) + " (VRInputError code)");
		}
	}

	if (pullData.bState) {
		vr::InputOriginInfo_t originInfo = { 0 };
		err = vr::VRInput()->GetOriginTrackedDeviceInfo(pullData.activeOrigin, &originInfo, sizeof(originInfo));
		
		if (err != vr::VRInputError_None) {
			static int errorCount = 0;
			if (errorCount++ % 300 == 0) {
				LOG_ERROR(std::string("Failed to get origin tracked device info: ") + std::to_string(err));
			}
			return;
		}
		
		if (originInfo.trackedDeviceIndex == vr::k_unTrackedDeviceIndexInvalid) {
			static int invalidCount = 0;
			if (invalidCount++ % 300 == 0) {
				LOG_WARNING("Invalid tracked device index from activeOrigin");
			}
			return;
		}
		
		vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
		vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
		
		if (originInfo.trackedDeviceIndex < vr::k_unMaxTrackedDeviceCount && 
		    poses[originInfo.trackedDeviceIndex].bPoseIsValid && 
		    poses[originInfo.trackedDeviceIndex].bDeviceIsConnected) {
			vr::HmdMatrix34_t controllerMatrix = poses[originInfo.trackedDeviceIndex].mDeviceToAbsoluteTracking;
			PullPlayspace(controllerMatrix);
		} else {
			static int invalidCount = 0;
			if (invalidCount++ % 300 == 0) {
				LOG_WARNING(std::string("Controller pose invalid. DeviceIndex: ") + 
					std::to_string(originInfo.trackedDeviceIndex) +
					", Valid: " + (poses[originInfo.trackedDeviceIndex].bPoseIsValid ? "true" : "false") +
					", Connected: " + (poses[originInfo.trackedDeviceIndex].bDeviceIsConnected ? "true" : "false"));
			}
		}
	} else {
		if (m_pullActive) {
			LOG_INFO("PullPlayspace action released");
		}
		m_pullActive = false;
	}
}

