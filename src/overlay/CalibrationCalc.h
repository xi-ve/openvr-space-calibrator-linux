#pragma once

#include "Eigen/Dense"
#include <openvr.h>
#include <vector>
#include <deque>
#include <iostream>

struct Pose
{
	Eigen::Matrix3d rot;
	Eigen::Vector3d trans;

	Pose() { }
	Pose(const Eigen::AffineCompact3d& transform) {
		rot = transform.rotation();
		trans = transform.translation();
	}
	
	Pose(vr::HmdMatrix34_t hmdMatrix)
	{
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				rot(i, j) = hmdMatrix.m[i][j];
			}
		}
		trans = Eigen::Vector3d(hmdMatrix.m[0][3], hmdMatrix.m[1][3], hmdMatrix.m[2][3]);
	}
	Pose(vr::HmdQuaternion_t rot, const double *trans) {
		this->rot = Eigen::Matrix3d(Eigen::Quaterniond(rot.w, rot.x, rot.y, rot.z));
		this->trans = Eigen::Vector3d(trans[0], trans[1], trans[2]);
	}
	Pose(double x, double y, double z) : trans(Eigen::Vector3d(x, y, z)) { }

	Eigen::Matrix4d ToAffine() const {
		Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();

		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				matrix(i, j) = rot(i, j);
			}
			matrix(i, 3) = trans(i);
		}

		return matrix;
	}
};

struct Sample
{
	Pose ref, target;
	bool valid;
	double timestamp;
	Sample() : valid(false), timestamp(0) { }
	Sample(Pose ref, Pose target, double timestamp) : valid(true), ref(ref), target(target), timestamp(timestamp){ }
};

class CalibrationCalc {
public:
	static const double AxisVarianceThreshold;

	bool enableStaticRecalibration;
	bool lockRelativePosition = false;
	
	const Eigen::AffineCompact3d Transformation() const 
	{
		return m_estimatedTransformation;
	}

	const Eigen::Vector3d EulerRotation() const {
		auto rot = m_estimatedTransformation.rotation();
		return rot.eulerAngles(2, 1, 0) * 180.0 / EIGEN_PI;
	}

	bool isValid() const {
		return m_isValid;
	}
	
	const Eigen::AffineCompact3d RelativeTransformation() const 
	{
		return m_refToTargetPose;
	}

	bool isRelativeTransformationCalibrated() const
	{
		return m_relativePosCalibrated;
	}

	void setRelativeTransformation(const Eigen::AffineCompact3d transform, bool calibrated)
	{
		m_refToTargetPose = transform;
		m_relativePosCalibrated = calibrated;
	}

	void PushSample(const Sample& sample);
	void Clear();

	double ReferenceJitter() const;
	double TargetJitter() const;

	bool ComputeOneshot(const bool ignoreOutliers);
	bool ComputeIncremental(bool &lerp, double threshold, double relPoseMaxError, const bool ignoreOutliers);

	size_t SampleCount() const {
		return m_samples.size();
	}

	void ShiftSample() {
		if (!m_samples.empty()) m_samples.pop_front();
	}

	CalibrationCalc() : m_isValid(false), m_calcCycle(0), enableStaticRecalibration(true) {}

	Eigen::Vector3d m_posOffset;
	double m_axisVariance = 0.0;
	long m_calcCycle;

private:
	bool m_isValid;
	Eigen::AffineCompact3d m_estimatedTransformation;
	bool m_relativePosCalibrated = false;

	Eigen::AffineCompact3d m_refToTargetPose = Eigen::AffineCompact3d::Identity();

	std::deque<Sample> m_samples;

	std::vector<bool> DetectOutliers() const;
	Eigen::Vector3d CalibrateRotation(const bool ignoreOutliers) const;
	Eigen::Vector3d CalibrateTranslation(const Eigen::Matrix3d &rotation) const;
	void CalibrateScaleOffset(const Eigen::Matrix3d &rotation, Eigen::Vector3d* out_scaleOffset, float* out_scaleFactor) const;

	Eigen::AffineCompact3d ComputeCalibration(const bool ignoreOutliers) const;

	double RetargetingErrorRMS(const Eigen::Vector3d& hmdToTargetPos, const Eigen::AffineCompact3d& calibration) const;
	Eigen::Vector3d ComputeRefToTargetOffset(const Eigen::AffineCompact3d& calibration) const;

	Eigen::Vector4d ComputeAxisVariance(const Eigen::AffineCompact3d& calibration) const;

	[[nodiscard]] bool ValidateCalibration(const Eigen::AffineCompact3d& calibration, double *errorOut = nullptr, Eigen::Vector3d* posOffsetV = nullptr);
	void ComputeInstantOffset();

	Eigen::AffineCompact3d EstimateRefToTargetPose(const Eigen::AffineCompact3d& calibration) const;
	bool CalibrateByRelPose(Eigen::AffineCompact3d &out) const;
};

