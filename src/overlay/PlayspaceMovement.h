#pragma once

#include <openvr.h>
#include <Eigen/Dense>
#include <vector>

struct PlayspaceMovementSettings
{
	bool enabled = false;
	float movementMultiplier = 1.0f;
	
	struct OriginalPlayspace
	{
		bool saved = false;
		std::vector<vr::HmdQuad_t> geometry;
		vr::HmdMatrix34_t standingCenter;
		vr::HmdVector2_t playSpaceSize;
	} original;
};

class PlayspaceMovement
{
public:
	PlayspaceMovement();
	~PlayspaceMovement();
	
	bool Initialize();
	void Shutdown();
	
	void Update();
	void SaveOriginalPlayspace();
	void ResetPlayspace();
	void PullPlayspace(const vr::HmdMatrix34_t& controllerPose);
	
	PlayspaceMovementSettings settings;
	
private:
	bool m_initialized;
	vr::VRActionSetHandle_t m_actionSet;
	vr::VRActionHandle_t m_actionResetPlayspace;
	vr::VRActionHandle_t m_actionPullPlayspace;
	vr::VRActionHandle_t m_actionControllerPose;
	
	vr::HmdMatrix34_t m_lastControllerPose;
	vr::HmdMatrix34_t m_initialControllerPose;
	Eigen::Vector3d m_smoothedControllerPos;
	bool m_pullActive;
	bool m_resetPressedLastFrame;
	bool m_hasSavedReference;
	
	struct PullReference
	{
		std::vector<vr::HmdQuad_t> geometry;
		vr::HmdMatrix34_t standingCenter;
		vr::HmdVector2_t playSpaceSize;
		bool workingCopyInitialized;
	} m_pullReference;
};

