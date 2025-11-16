#include "CalibrationDebug.h"
#include "CalibrationMetrics.h"
#include "CalibrationCalc.h"
#include "Calibration.h"
#include "imgui.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cfloat>
#define EIGEN_MPL2_ONLY
#include <Eigen/Dense>

namespace {
	double refTime;

	template<typename F>
	void PlotLineG(const char* name, const F& f, int points) {
		if (points > 0) {
			std::vector<float> plotData;
			plotData.reserve(points);
			for (int i = 0; i < points; i++) {
				auto point = f(i);
				plotData.push_back((float)point.y);
			}
			
			ImGui::PlotLines(name, plotData.data(), (int)plotData.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(-1, 80));
		}
	}

	void PlotLineG(const char* name, const Metrics::TimeSeries<double>& ts) {
		if (ts.size() > 0) {
			std::vector<float> plotData;
			plotData.reserve(ts.size());
			for (int i = 0; i < ts.size(); i++) {
				const auto& p = ts[i];
				plotData.push_back((float)p.second);
			}
			ImGui::PlotLines(name, plotData.data(), (int)plotData.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(-1, 80));
		}
	}

	void PlotVector(const char* namePrefix, const Metrics::TimeSeries<Eigen::Vector3d>& ts) {
		if (ts.size() > 0) {
			std::vector<float> plotDataX, plotDataY, plotDataZ;
			plotDataX.reserve(ts.size());
			plotDataY.reserve(ts.size());
			plotDataZ.reserve(ts.size());
			
			for (int i = 0; i < ts.size(); i++) {
				const auto& p = ts[i];
				plotDataX.push_back((float)p.second(0));
				plotDataY.push_back((float)p.second(1));
				plotDataZ.push_back((float)p.second(2));
			}
			
			std::string nameX = std::string(namePrefix) + "X";
			ImGui::PlotLines(nameX.c_str(), plotDataX.data(), (int)plotDataX.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(-1, 60));
			
			std::string nameY = std::string(namePrefix) + "Y";
			ImGui::PlotLines(nameY.c_str(), plotDataY.data(), (int)plotDataY.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(-1, 60));
			
			std::string nameZ = std::string(namePrefix) + "Z";
			ImGui::PlotLines(nameZ.c_str(), plotDataZ.data(), (int)plotDataZ.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(-1, 60));
		}
	}

	double lastMouseX = -INFINITY;
	bool wasHovered;

	std::vector<double> calAppliedTimeBuffer, calByRelPoseTimeBuffer;

	void PrepApplyTicks() {
		calAppliedTimeBuffer.clear();
		calByRelPoseTimeBuffer.clear();

		for (auto t : Metrics::calibrationApplied.data()) {
			if (t.second) {
				calAppliedTimeBuffer.push_back(t.first - refTime);
			}
			else {
				calByRelPoseTimeBuffer.push_back(t.first - refTime);
			}
		}
	}

	void AddApplyTicks() {
		if (!calAppliedTimeBuffer.empty()) {
			ImGui::Text("Calibration applied at: ");
			for (size_t i = 0; i < calAppliedTimeBuffer.size() && i < 5; i++) {
				if (i > 0) ImGui::SameLine();
				ImGui::Text("%.2f", calAppliedTimeBuffer[i]);
			}
		}
	}

	struct GraphInfo {
		const char* name;
		void (*callback)();
	};

	void G_PosOffset_RawComputed() {
		ImGui::Text("Position Offset: Raw Computed (mm)");
		AddApplyTicks();
		PlotVector("", Metrics::posOffset_rawComputed);
	}

	void G_PosOffset_CurrentCal() {
		ImGui::Text("Position Offset: Current Calibration (mm)");
		AddApplyTicks();
		PlotVector("", Metrics::posOffset_currentCal);
	}

	void G_PosOffset_LastSample() {
		ImGui::Text("Position Offset: Last Sample (mm)");
		AddApplyTicks();
		PlotVector("", Metrics::posOffset_lastSample);
	}

	void G_PosOffset_ByRelPose() {
		ImGui::Text("Position Offset: By Rel Pose (mm)");
		AddApplyTicks();
		PlotVector("", Metrics::posOffset_byRelPose);
	}

	void G_PosOffset_PosError() {
		ImGui::Text("Position Error (mm RMS)");
		AddApplyTicks();
		PlotLineG("Candidate", Metrics::error_rawComputed);
		PlotLineG("Active", Metrics::error_currentCal);
		PlotLineG("By Rel Pose", Metrics::error_byRelPose);
		PlotLineG("CC Rel Pose", Metrics::error_currentCalRelPose);
	}

	void G_ComputationTime() {
		ImGui::Text("Computation Time (ms)");
		AddApplyTicks();
		PlotLineG("Time", Metrics::computationTime);
	}

	void G_JitterReference() {
		ImGui::Text("Reference Jitter");
		AddApplyTicks();
		PlotLineG("Reference Jitter", Metrics::jitterRef);
	}

	void G_JitterTarget() {
		ImGui::Text("Target Jitter");
		AddApplyTicks();
		PlotLineG("Target Jitter", Metrics::jitterTarget);
	}

	void G_AxisVariance() {
		ImGui::Text("Axis Variance");
		AddApplyTicks();
		
		std::vector<float> varianceData;
		std::vector<float> thresholdData;
		for (const auto& p : Metrics::axisIndependence.data()) {
			varianceData.push_back((float)p.second);
			thresholdData.push_back((float)CalibrationCalc::AxisVarianceThreshold);
		}
		
		if (!varianceData.empty()) {
			ImGui::PlotLines("Variance", varianceData.data(), (int)varianceData.size(), 0, nullptr, 0.0f, 0.003f, ImVec2(-1, 80));
			ImGui::PlotLines("Threshold", thresholdData.data(), (int)thresholdData.size(), 0, nullptr, 0.0f, 0.003f, ImVec2(-1, 80));
		}
	}

	const struct GraphInfo graphs[] = {
		{ "Position Error", G_PosOffset_PosError },
		{ "Axis Variance", G_AxisVariance },
		{ "Offset: Raw Computed", G_PosOffset_RawComputed },
		{ "Offset: Current Calibration", G_PosOffset_CurrentCal },
		{ "Offset: Last Sample", G_PosOffset_LastSample },
		{ "Offset: By Rel Pose", G_PosOffset_ByRelPose },
		{ "Processing time", G_ComputationTime },
		{ "Reference Jitter", G_JitterReference },
		{ "Target Jitter", G_JitterTarget }
	};

	const int N_GRAPHS = sizeof(graphs) / sizeof(graphs[0]);
}

void ShowCalibrationDebug(int rows, int cols) {
	static std::vector<int> curIndexes;

	double initMouseX = lastMouseX;
	wasHovered = false;

	for (int i = (int)curIndexes.size(); i < rows * cols; i++) {
		curIndexes.push_back(i % N_GRAPHS);
	}

	auto avail = ImGui::GetContentRegionAvail();

	auto bgCol = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);

	ImGui::PushStyleColor(ImGuiCol_TableRowBg, bgCol);
	ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, bgCol);

	ImGui::SetNextWindowBgAlpha(1);
	if (!ImGui::BeginChild("##CalibrationDebug", avail, false,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoTitleBar)) {
		ImGui::EndChild();
		return;
	}

	if (!ImGui::BeginTable("##CalibrationDebug", cols, ImGuiTableFlags_RowBg)) {
		return;
	}

	double t = refTime = Metrics::timestamp();
	PrepApplyTicks();

	for (int r = 0; r < rows; r++) {
		ImGui::TableNextRow();
		for (int c = 0; c < cols; c++) {
			int i = r * cols + c;
			ImGui::TableSetColumnIndex(c);

			ImGui::PushID(i);

			ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
			if (ImGui::BeginCombo("", graphs[curIndexes[i]].name, 0)) {
				for (int j = 0; j < N_GRAPHS; j++) {
					bool isSelected = j == curIndexes[i];
					if (ImGui::Selectable(graphs[j].name, isSelected)) {
						curIndexes[i] = j;
					}

					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			graphs[curIndexes[i]].callback();

			ImGui::PopID();
		}
	}
	
	ImGui::EndTable();
	ImGui::EndChild();

	ImGui::PopStyleColor(2);

	if (!wasHovered) {
		lastMouseX = -INFINITY;
	}
}

