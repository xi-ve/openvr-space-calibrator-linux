#include "CalibrationMetrics.h"
#include <fstream>
#include <vector>
#include <climits>
#include <ctime>

namespace Metrics {
	double TimeSpan = 30, CurrentTime = 0;

	TimeSeries<Eigen::Vector3d> posOffset_rawComputed;
	TimeSeries<Eigen::Vector3d> posOffset_currentCal;
	TimeSeries<Eigen::Vector3d> posOffset_lastSample;
	TimeSeries<Eigen::Vector3d> posOffset_byRelPose;

	TimeSeries<double> error_rawComputed, error_currentCal, error_byRelPose, error_currentCalRelPose;
	TimeSeries<double> axisIndependence;
	TimeSeries<double> computationTime;
	TimeSeries<double> jitterRef, jitterTarget;

	TimeSeries<bool> calibrationApplied;

	bool enableLogs = false;

	double timestamp() {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return ts.tv_sec + ts.tv_nsec / 1e9;
	}

	void RecordTimestamp() {
		CurrentTime = timestamp();
	}

	void WriteLogAnnotation(const char* s) {
		if (!enableLogs) return;
	}

	void WriteLogEntry() {
		if (!enableLogs) return;
	}
}

