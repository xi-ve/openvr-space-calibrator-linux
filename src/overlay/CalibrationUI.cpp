#include "CalibrationUI.h"
#include "Calibration.h"
#include "VRState.h"
#include "CalibrationMetrics.h"
#include "CalibrationDebug.h"
#include "imgui_extensions.h"
#include "../common/Version.h"
#include "imgui.h"
#include <openvr.h>
#include <algorithm>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <cmath>
#include <cfloat>
#define EIGEN_MPL2_ONLY
#include <Eigen/Dense>

const char* GetPrettyTrackingSystemName(const std::string& system)
{
	if (system == "lighthouse")
		return "Lighthouse";
	if (system == "oculus")
		return "Oculus";
	if (system == "aapvr")
		return "Pimax";
	if (system == "Pimax Crystal HMD")
		return "Pimax Crystal HMD";
	if (system == "Pimax Crystal Controllers")
		return "Pimax Crystal Controllers";
	return system.c_str();
}

void AppendSeparated(std::string &buffer, const std::string &suffix)
{
	if (!buffer.empty())
		buffer += " | ";
	buffer += suffix;
}

std::string LabelString(const VRDevice &device)
{
	std::string label;
	AppendSeparated(label, device.model);
	AppendSeparated(label, device.serial);
	return label;
}

std::string LabelString(const StandbyDevice& device) {
	std::string label("< ");
	label += device.model;
	AppendSeparated(label, device.serial);
	label += " >";
	return label;
}

void TextWithWidth(const char *label, const char *text, float width)
{
	ImGui::BeginChild(label, ImVec2(width, ImGui::GetTextLineHeightWithSpacing()));
	ImGui::Text("%s", text);
	ImGui::EndChild();
}

VRState LoadVRState() {
	VRState state = VRState::Load();
	auto& trackingSystems = state.trackingSystems;

	if (CalCtx.state == CalibrationState::ContinuousStandby) {
		auto existing = std::find(trackingSystems.begin(), trackingSystems.end(), CalCtx.referenceTrackingSystem);
		if (existing == trackingSystems.end()) {
			trackingSystems.push_back(CalCtx.referenceTrackingSystem);
		}

		existing = std::find(trackingSystems.begin(), trackingSystems.end(), CalCtx.targetTrackingSystem);
		if (existing == trackingSystems.end()) {
			trackingSystems.push_back(CalCtx.targetTrackingSystem);
		}
	}

	return state;
}

void BuildSystemSelection(const VRState &state)
{
	if (state.trackingSystems.empty())
	{
		ImGui::Text("No tracked devices are present");
		return;
	}

	ImGui::Text("Reference Space");
	ImGui::SameLine();
	ImGui::Text("Target Space");

	int currentReferenceSystem = -1;
	int currentTargetSystem = -1;
	int firstReferenceSystemNotTargetSystem = -1;

	std::vector<const char *> referenceSystems;
	std::vector<const char *> referenceSystemsUi;
	for (const std::string& str : state.trackingSystems)
	{
		if (str == CalCtx.referenceTrackingSystem)
		{
			currentReferenceSystem = (int) referenceSystems.size();
		}
		else if (firstReferenceSystemNotTargetSystem == -1 && str != CalCtx.targetTrackingSystem)
		{
			firstReferenceSystemNotTargetSystem = (int) referenceSystems.size();
		}
		referenceSystems.push_back(str.c_str());
		referenceSystemsUi.push_back(GetPrettyTrackingSystemName(str));
	}

	if (currentReferenceSystem == -1 && CalCtx.referenceTrackingSystem == "")
	{
		if (CalCtx.state == CalibrationState::ContinuousStandby) {
			auto iter = std::find(state.trackingSystems.begin(), state.trackingSystems.end(), CalCtx.referenceStandby.trackingSystem);
			if (iter != state.trackingSystems.end()) {
				currentReferenceSystem = (int) (iter - state.trackingSystems.begin());
			}
		}
		else {
			currentReferenceSystem = firstReferenceSystemNotTargetSystem;
		}
	}

	if (referenceSystemsUi.size() > 0) {
		ImGui::Combo("##ReferenceTrackingSystem", &currentReferenceSystem, &referenceSystemsUi[0], (int)referenceSystemsUi.size());
	}

	if (currentReferenceSystem != -1 && currentReferenceSystem < (int) referenceSystems.size())
	{
		CalCtx.referenceTrackingSystem = std::string(referenceSystems[currentReferenceSystem]);
		if (CalCtx.referenceTrackingSystem == CalCtx.targetTrackingSystem)
			CalCtx.targetTrackingSystem = "";
	}

	if (CalCtx.targetTrackingSystem == "") {
		if (CalCtx.state == CalibrationState::ContinuousStandby) {
			auto iter = std::find(state.trackingSystems.begin(), state.trackingSystems.end(), CalCtx.targetStandby.trackingSystem);
			if (iter != state.trackingSystems.end()) {
				currentTargetSystem = (int) (iter - state.trackingSystems.begin());
			}
		}
		else {
			currentTargetSystem = 0;
		}
	}

	std::vector<const char *> targetSystems;
	std::vector<const char *> targetSystemsUi;
	for (const std::string& str : state.trackingSystems)
	{
		if (str != CalCtx.referenceTrackingSystem)
		{
			if (str != "" && str == CalCtx.targetTrackingSystem)
				currentTargetSystem = (int) targetSystems.size();
			targetSystems.push_back(str.c_str());
			targetSystemsUi.push_back(GetPrettyTrackingSystemName(str));
		}
	}

	ImGui::SameLine();
	if (targetSystemsUi.size() > 0) {
		ImGui::Combo("##TargetTrackingSystem", &currentTargetSystem, &targetSystemsUi[0], (int)targetSystemsUi.size());
	}

	if (currentTargetSystem != -1 && currentTargetSystem < (int)targetSystems.size())
	{
		CalCtx.targetTrackingSystem = std::string(targetSystems[currentTargetSystem]);
	}
}

void BuildDeviceSelection(const VRState &state, int &initialSelected, const std::string &system, StandbyDevice &standbyDevice)
{
	int selected = initialSelected;
	ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Devices from: %s", GetPrettyTrackingSystemName(system));

	if (selected != -1)
	{
		bool matched = false;
		for (auto &device : state.devices)
		{
			if (device.trackingSystem != system)
				continue;

			if (selected == device.id)
			{
				matched = true;
				break;
			}
		}

		if (!matched)
		{
			selected = -1;
		}
	}

	bool standby = CalCtx.state == CalibrationState::ContinuousStandby;

	if (selected == -1 && !standby)
	{
		for (auto &device : state.devices)
		{
			if (device.trackingSystem != system)
				continue;

			if (device.controllerRole == vr::TrackedControllerRole_LeftHand)
			{
				selected = device.id;
				break;
			}
		}

		if (selected == -1) {
			for (auto& device : state.devices)
			{
				if (device.trackingSystem != system)
					continue;
				
				selected = device.id;
				break;
			}
		}
	}

	uint64_t iterator = 0;
	if (selected == -1 && standby) {
		bool present = false;
		for (auto& device : state.devices)
		{
			if (device.trackingSystem != system)
				continue;

			if (standbyDevice.model != device.model) continue;
			if (standbyDevice.serial != device.serial) continue;

			present = true;
			break;
		}

		if (!present) {
			auto label = LabelString(standbyDevice);
			std::string uniqueId = label + "_pass0_" + std::to_string(iterator);
			iterator++;
			ImGui::PushID(uniqueId.c_str());
			ImGui::Selectable(label.c_str(), true);
			ImGui::PopID();
		}
	}

	iterator = 0;

	for (auto &device : state.devices)
	{
		if (device.trackingSystem != system)
			continue;

		auto label = LabelString(device);
		std::string uniqueId = label + "_pass1_" + std::to_string(iterator);
		iterator++;
		ImGui::PushID(uniqueId.c_str());
		if (ImGui::Selectable(label.c_str(), selected == device.id)) {
			selected = device.id;
		}
		ImGui::PopID();
	}
	if (selected != initialSelected) {
		const auto& device = std::find_if(state.devices.begin(), state.devices.end(), [&](const auto& d) { return d.id == selected; });
		if (device == state.devices.end()) return;

		initialSelected = selected;
		standbyDevice.trackingSystem = system;
		standbyDevice.model = device->model;
		standbyDevice.serial = device->serial;
	}
}

void BuildDeviceSelections(const VRState &state)
{
	ImGuiStyle &style = ImGui::GetStyle();
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float paneWidth = avail.x / 2 - style.FramePadding.x;
	
	int referenceDeviceCount = 0;
	int targetDeviceCount = 0;
	bool referenceStandby = CalCtx.state == CalibrationState::ContinuousStandby && 
		std::find_if(state.devices.begin(), state.devices.end(), [&](const auto& d) {
			return d.trackingSystem == CalCtx.referenceTrackingSystem &&
			       d.model == CalCtx.referenceStandby.model &&
			       d.serial == CalCtx.referenceStandby.serial;
		}) == state.devices.end();
	bool targetStandby = CalCtx.state == CalibrationState::ContinuousStandby &&
		std::find_if(state.devices.begin(), state.devices.end(), [&](const auto& d) {
			return d.trackingSystem == CalCtx.targetTrackingSystem &&
			       d.model == CalCtx.targetStandby.model &&
			       d.serial == CalCtx.targetStandby.serial;
		}) == state.devices.end();
	
	for (const auto& device : state.devices) {
		if (device.trackingSystem == CalCtx.referenceTrackingSystem) {
			referenceDeviceCount++;
		}
		if (device.trackingSystem == CalCtx.targetTrackingSystem) {
			targetDeviceCount++;
		}
	}
	
	if (referenceStandby) referenceDeviceCount++;
	if (targetStandby) targetDeviceCount++;
	
	float buttonHeight = ImGui::GetTextLineHeightWithSpacing() + 4.0f + style.ItemSpacing.y;
	float maxPaneHeight = avail.y - buttonHeight - style.ItemSpacing.y;
	
	float itemHeight = ImGui::GetTextLineHeightWithSpacing();
	float headerHeight = itemHeight + style.ItemSpacing.y;
	float referencePaneHeight = headerHeight + (referenceDeviceCount * itemHeight) + style.FramePadding.y * 2;
	float targetPaneHeight = headerHeight + (targetDeviceCount * itemHeight) + style.FramePadding.y * 2;
	float paneHeight = std::min(std::max(referencePaneHeight, targetPaneHeight), maxPaneHeight);
	
	ImVec2 paneSize(paneWidth, paneHeight);

	ImGui::BeginChild("left device pane", paneSize, ImGuiChildFlags_Borders);
	BuildDeviceSelection(state, CalCtx.referenceID, CalCtx.referenceTrackingSystem, CalCtx.referenceStandby);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("right device pane", paneSize, ImGuiChildFlags_Borders);
	BuildDeviceSelection(state, CalCtx.targetID, CalCtx.targetTrackingSystem, CalCtx.targetStandby);
	ImGui::EndChild();

	if (ImGui::Button("Identify selected devices (blinks LED or vibrates)", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() + 4.0f)))
	{
		for (unsigned i = 0; i < 100; ++i)
		{
			if (CalCtx.targetID >= 0) {
				vr::VRSystem()->TriggerHapticPulse(CalCtx.targetID, 0, 2000);
			}
			if (CalCtx.referenceID >= 0) {
				vr::VRSystem()->TriggerHapticPulse(CalCtx.referenceID, 0, 2000);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}
}

void BuildProfileEditor()
{
	ImGuiStyle &style = ImGui::GetStyle();
	float width = ImGui::GetContentRegionAvail().x / 3.0f - style.FramePadding.x;
	float widthF = width - style.FramePadding.x;

	TextWithWidth("YawLabel", "Yaw", width);
	ImGui::SameLine();
	TextWithWidth("PitchLabel", "Pitch", width);
	ImGui::SameLine();
	TextWithWidth("RollLabel", "Roll", width);

	ImGui::PushItemWidth(widthF);
	ImGui::InputDouble("##Yaw", &CalCtx.calibratedRotation(1), 0.1, 1.0, "%.8f");
	ImGui::SameLine();
	ImGui::InputDouble("##Pitch", &CalCtx.calibratedRotation(2), 0.1, 1.0, "%.8f");
	ImGui::SameLine();
	ImGui::InputDouble("##Roll", &CalCtx.calibratedRotation(0), 0.1, 1.0, "%.8f");

	TextWithWidth("XLabel", "X", width);
	ImGui::SameLine();
	TextWithWidth("YLabel", "Y", width);
	ImGui::SameLine();
	TextWithWidth("ZLabel", "Z", width);

	ImGui::InputDouble("##X", &CalCtx.calibratedTranslation(0), 1.0, 10.0, "%.8f");
	ImGui::SameLine();
	ImGui::InputDouble("##Y", &CalCtx.calibratedTranslation(1), 1.0, 10.0, "%.8f");
	ImGui::SameLine();
	ImGui::InputDouble("##Z", &CalCtx.calibratedTranslation(2), 1.0, 10.0, "%.8f");

	TextWithWidth("ScaleLabel", "Scale", width);

	ImGui::InputDouble("##Scale", &CalCtx.calibratedScale, 0.0001, 0.01, "%.8f");
	ImGui::PopItemWidth();
}

void BuildCalibrationMenu()
{
	ImGuiStyle &style = ImGui::GetStyle();
	ImGui::Text("");

	if (CalCtx.state == CalibrationState::None)
	{
		if (CalCtx.validProfile && !CalCtx.enabled)
		{
			ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1), "Reference (%s) HMD not detected, profile disabled", GetPrettyTrackingSystemName(CalCtx.referenceTrackingSystem));
			ImGui::Text("");
		}

		float width = ImGui::GetContentRegionAvail().x;
		int buttonCount = CalCtx.validProfile ? 4 : 3;
		float buttonWidth = (width - style.FramePadding.x * (buttonCount - 1)) / buttonCount;

		if (ImGui::Button("Start Calibration", ImVec2(buttonWidth, ImGui::GetTextLineHeight() * 2)))
		{
			ImGui::OpenPopup("Calibration Progress");
			StartCalibration();
		}

		ImGui::SameLine();
		if (ImGui::Button("Continuous Calibration", ImVec2(buttonWidth, ImGui::GetTextLineHeight() * 2))) {
			StartContinuousCalibration();
		}

		if (CalCtx.validProfile)
		{
			ImGui::SameLine();
			if (ImGui::Button("Edit Calibration", ImVec2(buttonWidth, ImGui::GetTextLineHeight() * 2)))
			{
				CalCtx.state = CalibrationState::Editing;
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Clear Calibration", ImVec2(buttonWidth, ImGui::GetTextLineHeight() * 2)))
		{
			CalCtx.Clear();
			ResetAllDeviceTransforms();
			ScanAndApplyProfile(CalCtx);
			SaveProfile(CalCtx);
		}

		width = ImGui::GetContentRegionAvail().x;
		float scale = 1.0f;
		if (CalCtx.chaperone.valid)
		{
			width -= style.FramePadding.x * 2.0f;
			scale = 0.5f;
		}

		ImGui::Text("");
		if (ImGui::Button("Copy Chaperone Bounds to profile", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
		{
			LoadChaperoneBounds();
			SaveProfile(CalCtx);
		}

		if (CalCtx.chaperone.valid)
		{
			ImGui::SameLine();
			if (ImGui::Button("Paste Chaperone Bounds", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
			{
				ApplyChaperoneBounds();
			}

			if (ImGui::Checkbox(" Paste Chaperone Bounds automatically when geometry resets", &CalCtx.chaperone.autoApply))
			{
				SaveProfile(CalCtx);
			}
		}

		ImGui::Text("");
		auto speed = CalCtx.calibrationSpeed;

		ImGui::Columns(4, nullptr, false);
		ImGui::Text("Calibration Speed");

		ImGui::NextColumn();
		if (ImGui::RadioButton(" Fast          ", speed == CalibrationContext::FAST))
			CalCtx.calibrationSpeed = CalibrationContext::FAST;

		ImGui::NextColumn();
		if (ImGui::RadioButton(" Slow          ", speed == CalibrationContext::SLOW))
			CalCtx.calibrationSpeed = CalibrationContext::SLOW;

		ImGui::NextColumn();
		if (ImGui::RadioButton(" Very Slow     ", speed == CalibrationContext::VERY_SLOW))
			CalCtx.calibrationSpeed = CalibrationContext::VERY_SLOW;

		ImGui::Columns(1);
	}
	else if (CalCtx.state == CalibrationState::Editing)
	{
		BuildProfileEditor();

		if (ImGui::Button("Save Profile", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 2)))
		{
			SaveProfile(CalCtx);
			CalCtx.state = CalibrationState::None;
		}
	}

	if (CalCtx.state == CalibrationState::None || CalCtx.state == CalibrationState::Editing)
	{
		ImGui::Text("");
		ImGui::Separator();
		ImGui::Text("Profile Management");
		
		static char exportPath[512] = "";
		static char importPath[512] = "";
		
		ImGui::Text("Export Profile:");
		ImGui::InputText("##ExportPath", exportPath, sizeof(exportPath));
		ImGui::SameLine();
		if (ImGui::Button("Export", ImVec2(100, 0)))
		{
			if (strlen(exportPath) > 0)
			{
				if (ExportProfile(CalCtx, std::string(exportPath)))
				{
					exportPath[0] = '\0';
				}
			}
		}
		
		ImGui::Text("Import Profile:");
		ImGui::InputText("##ImportPath", importPath, sizeof(importPath));
		ImGui::SameLine();
		if (ImGui::Button("Import", ImVec2(100, 0)))
		{
			if (strlen(importPath) > 0)
			{
				if (ImportProfile(CalCtx, std::string(importPath)))
				{
					ScanAndApplyProfile(CalCtx);
					SaveProfile(CalCtx);
					importPath[0] = '\0';
				}
			}
		}
	}
	else
	{
		ImGui::Button("Calibration in progress...", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 2));
	}

	auto& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 40.0f, io.DisplaySize.y - 40.0f), ImGuiCond_Always);
	if (ImGui::BeginPopupModal("Calibration Progress", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse))
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImVec4(0, 0, 0, 1));
		for (auto &message : CalCtx.messages)
		{
			switch (message.type)
			{
			case CalibrationContext::Message::String:
				ImGui::TextWrapped("%s", message.str.c_str());
				break;
			case CalibrationContext::Message::Progress:
				float fraction = (float)message.progress / (float)message.target;
				ImGui::Text("");
				ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), "");
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetFontSize() - style.FramePadding.y * 2);
				ImGui::Text(" %d%%", (int)(fraction * 100));
				break;
			}
		}
		ImGui::PopStyleColor();

		if (CalCtx.state == CalibrationState::None)
		{
			ImGui::Text("");
			if (ImGui::Button("Close", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 2)))
				ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

static void ScaledDragFloat(const char* label, double& f, double scale, double min, double max, int flags = ImGuiSliderFlags_AlwaysClamp) {
	float v = (float) (f * scale);
	std::string labelStr = std::string(label);

	if (labelStr.size() > 2 && labelStr[0] == '#' && labelStr[1] == '#') {
		ImGui::SliderFloat(label, &v, (float)min, (float)max, "%1.2f", flags);
	} else {
		ImGui::Text(label);
		ImGui::SameLine();
		ImGui::PushID((std::string(label) + "_id").c_str());
		constexpr uint32_t LABEL_CURSOR = 100;
		uint32_t cursorPosX = (int) ImGui::GetCursorPosX();
		uint32_t roundedPosition = ((cursorPosX + LABEL_CURSOR / 2) / LABEL_CURSOR) * LABEL_CURSOR;
		ImGui::SetCursorPosX((float) roundedPosition);
		ImGui::SliderFloat((std::string("##") + label).c_str(), &v, (float)min, (float)max, "%1.2f", flags);
		ImGui::PopID();
	}
	
	f = v / scale;
}

void DrawVectorElement(const std::string id, const char* text, double* value, int defaultValue, const char* defaultValueStr) {
	constexpr float CONTINUOUS_CALIBRATION_TRACKER_OFFSET_DELTA = 0.01f;

	ImGui::Text(text);
	ImGui::SameLine();

	ImGui::PushID((id + text + "_btn_reset").c_str());
	if (ImGui::Button(defaultValueStr)) {
		*value *= defaultValue;
	}
	ImGui::PopID();
	ImGui::SameLine();
	if (ImGui::ArrowButton((id + text + "_decrease").c_str(), ImGuiDir_Down)) {
		*value -= CONTINUOUS_CALIBRATION_TRACKER_OFFSET_DELTA;
	}
	ImGui::SameLine();
	ImGui::PushItemWidth(100);
	ImGui::PushID((id + text + "_text_field").c_str());
	ImGui::InputDouble("##label", value, 0, 0, "%.2f");
	ImGui::PopID();
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if (ImGui::ArrowButton((id + text + "_increase").c_str(), ImGuiDir_Up)) {
		*value += CONTINUOUS_CALIBRATION_TRACKER_OFFSET_DELTA;
	}
}


void CCal_BasicInfo() {
	if (ImGui::BeginTable("DeviceInfo", 2, 0)) {
		ImGui::TableSetupColumn("Reference device");
		ImGui::TableSetupColumn("Target device");
		ImGui::TableHeadersRow();

		const char* refTrackingSystem = GetPrettyTrackingSystemName(CalCtx.referenceStandby.trackingSystem);
		const char* targetTrackingSystem = GetPrettyTrackingSystemName(CalCtx.targetStandby.trackingSystem);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::BeginGroup();
		ImGui::Text("%s / %s / %s",
			refTrackingSystem,
			CalCtx.referenceStandby.model.c_str(),
			CalCtx.referenceStandby.serial.c_str()
		);
		const char* status;
		if (CalCtx.referenceID < 0) {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0xFF000080);
			status = "NOT FOUND";
		} else if (!CalCtx.ReferencePoseIsValidSimple()) {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0xFFFF0080);
			status = "NOT TRACKING";
		} else {
			status = "OK";
		}
		ImGui::Text("Status: %s", status);
		ImGui::EndGroup();

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginGroup();
		ImGui::Text("%s / %s / %s",
			targetTrackingSystem,
			CalCtx.targetStandby.model.c_str(),
			CalCtx.targetStandby.serial.c_str()
		);
		if (CalCtx.targetID < 0) {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0xFF000080);
			status = "NOT FOUND";
		}
		else if (!CalCtx.TargetPoseIsValidSimple()) {
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0xFFFF0080);
			status = "NOT TRACKING";
		}
		else {
			status = "OK";
		}
		ImGui::Text("Status: %s", status);
		ImGui::EndGroup();

		ImGui::EndTable();
	}

	float width = ImGui::GetContentRegionAvail().x;

	int buttonCount = Metrics::enableLogs ? 3 : 2;
	if (ImGui::BeginTable("##CCal_Cancel", buttonCount, 0, ImVec2(width, ImGui::GetTextLineHeight() * 2))) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		if (ImGui::Button("Cancel Continuous Calibration", ImVec2(-FLT_MIN, 0.0f))) {
			EndContinuousCalibration();
		}

		ImGui::TableSetColumnIndex(1);
		if (ImGui::Button("Debug: Force break calibration", ImVec2(-FLT_MIN, 0.0f))) {
			DebugApplyRandomOffset();
		}

		if (Metrics::enableLogs) {
			ImGui::TableSetColumnIndex(2);
			if (ImGui::Button("Debug: Mark logs", ImVec2(-FLT_MIN, 0.0f))) {
				Metrics::WriteLogAnnotation("MARK LOGS");
			}
		}

		ImGui::EndTable();
	}

	ImGui::Checkbox("Hide tracker", &CalCtx.quashTargetInContinuous);
	ImGui::SameLine();
	ImGui::Checkbox("Static recalibration", &CalCtx.enableStaticRecalibration);
	ImGui::SameLine();
	ImGui::Checkbox("Enable debug logs", &Metrics::enableLogs);
	ImGui::SameLine();
	ImGui::Checkbox("Lock relative transform", &CalCtx.lockRelativePosition);
	ImGui::SameLine();
	ImGui::Checkbox("Require triggers", &CalCtx.requireTriggerPressToApply);
	ImGui::Checkbox("Ignore outliers", &CalCtx.ignoreOutliers);

	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 1));

	for (const auto& msg : CalCtx.messages) {
		if (msg.type == CalibrationContext::Message::String) {
			ImGui::TextWrapped("> %s", msg.str.c_str());
		}
	}

	ImGui::PopStyleColor();

	ShowCalibrationDebug(1, 3);
}

void CCal_DrawSettings() {
	ImVec2 panel_size { ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x, 0 };

	ImGui::Text("Hover over settings to learn more about them!");
	ImGui::Separator();

	ImGui::Text("Calibration speeds");
	ImGui::Separator();
	ImGui::TextWrapped("SpaceCalibrator uses up to three different speeds at which it drags the calibration back into position when drift occurs.");
	
	auto speed = CalCtx.calibrationSpeed;
	ImGui::Columns(3, nullptr, false);
	if (ImGui::RadioButton(" Fast          ", speed == CalibrationContext::FAST)) {
		CalCtx.calibrationSpeed = CalibrationContext::FAST;
	}
	ImGui::NextColumn();
	if (ImGui::RadioButton(" Slow          ", speed == CalibrationContext::SLOW)) {
		CalCtx.calibrationSpeed = CalibrationContext::SLOW;
	}
	ImGui::NextColumn();
	if (ImGui::RadioButton(" Very Slow     ", speed == CalibrationContext::VERY_SLOW)) {
		CalCtx.calibrationSpeed = CalibrationContext::VERY_SLOW;
	}
	ImGui::Columns(1);

	if (ImGui::BeginTable("SpeedThresholds", 3, 0)) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("Translation (mm)");
		ImGui::TableSetColumnIndex(2);
		ImGui::Text("Rotation (degrees)");

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Decel");
		ImGui::TableSetColumnIndex(1);
		ScaledDragFloat("##TransDecel", CalCtx.alignmentSpeedParams.thr_trans_tiny, 1000.0, 0, 20.0);
		ImGui::TableSetColumnIndex(2);
		ScaledDragFloat("##RotDecel", CalCtx.alignmentSpeedParams.thr_rot_tiny, 180.0 / EIGEN_PI, 0, 5.0);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Slow");
		ImGui::TableSetColumnIndex(1);
		ScaledDragFloat("##TransSlow", CalCtx.alignmentSpeedParams.thr_trans_small, 1000.0,
			CalCtx.alignmentSpeedParams.thr_trans_tiny * 1000.0, 20.0);
		ImGui::TableSetColumnIndex(2);
		ScaledDragFloat("##RotSlow", CalCtx.alignmentSpeedParams.thr_rot_small, 180.0 / EIGEN_PI,
			CalCtx.alignmentSpeedParams.thr_rot_tiny * (180.0 / EIGEN_PI), 10.0);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Fast");
		ImGui::TableSetColumnIndex(1);
		ScaledDragFloat("##TransFast", CalCtx.alignmentSpeedParams.thr_trans_large, 1000.0,
			CalCtx.alignmentSpeedParams.thr_trans_small * 1000.0, 50.0);
		ImGui::TableSetColumnIndex(2);
		ScaledDragFloat("##RotFast", CalCtx.alignmentSpeedParams.thr_rot_large, 180.0 / EIGEN_PI,
			CalCtx.alignmentSpeedParams.thr_rot_small * (180.0 / EIGEN_PI), 20.0);

		ImGui::EndTable();
	}

	ImGui::Separator();
	ImGui::Text("Alignment speeds");
	ImGui::Separator();
	ScaledDragFloat("Decel", CalCtx.alignmentSpeedParams.align_speed_tiny, 1.0, 0, 2.0, 0);
	ScaledDragFloat("Slow", CalCtx.alignmentSpeedParams.align_speed_small, 1.0, 0, 2.0, 0);
	ScaledDragFloat("Fast", CalCtx.alignmentSpeedParams.align_speed_large, 1.0, 0, 2.0, 0);

	ImGui::Separator();
	ImGui::Text("Continuous calibration");
	ImGui::Separator();
	
	ImGui::Text("Recalibration threshold");
	ImGui::SameLine();
	ImGui::PushID("recalibration_threshold");
	ImGui::SliderFloat("##recalibration_threshold_slider", &CalCtx.continuousCalibrationThreshold, 1.01f, 10.0f, "%1.1f", 0);
	if (ImGui::IsItemHovered(0)) {
		ImGui::SetTooltip("Controls how good the calibration must be before realigning the trackers.\n"
			"Higher values cause calibration to happen less often, and may be useful for systems with lots of tracking drift.");
	}
	ImGui::PopID();

	ImGui::Text("Max relative error threshold");
	ImGui::SameLine();
	ImGui::PushID("max_relative_error_threshold");
	ImGui::SliderFloat("##max_relative_error_threshold_slider", &CalCtx.maxRelativeErrorThreshold, 0.01f, 1.0f, "%1.1f", 0);
	if (ImGui::IsItemHovered(0)) {
		ImGui::SetTooltip("Controls the maximum acceptable relative error. If the error from the relative calibration is too poor, the calibration will be discarded.");
	}
	ImGui::PopID();

	ImGui::Text("Jitter threshold");
	ImGui::SameLine();
	ImGui::PushID("jitter_threshold");
	ImGui::SliderFloat("##jitter_threshold_slider", &CalCtx.jitterThreshold, 0.1f, 10.0f, "%1.1f", 0);
	if (ImGui::IsItemHovered(0)) {
		ImGui::SetTooltip("Controls how much jitter will be allowed for calibration.\n"
			"Higher values allow worse tracking to calibrate, but may result in poorer tracking.");
	}
	ImGui::PopID();

	ImGui::Text("Tracker offset");
	ImGui::Separator();
	DrawVectorElement("cc_tracker_offset", "X", &CalCtx.continuousCalibrationOffset.x());
	DrawVectorElement("cc_tracker_offset", "Y", &CalCtx.continuousCalibrationOffset.y());
	DrawVectorElement("cc_tracker_offset", "Z", &CalCtx.continuousCalibrationOffset.z());

	ImGui::Text("Playspace scale");
	ImGui::Separator();
	DrawVectorElement("cc_playspace_scale", "Playspace Scale", &CalCtx.calibratedScale, 1, " 1 ");

	ImGui::NewLine();
	if (ImGui::Button("Reset settings")) {
		CalCtx.ResetConfig();
	}
}

void BuildContinuousCalDisplay() {
	ImVec2 contentRegion;
	contentRegion.x = ImGui::GetContentRegionAvail().x;
	contentRegion.y = ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing() * 2.1f;

	if (!ImGui::BeginChild("CCalDisplayFrame", contentRegion, ImGuiChildFlags_None)) {
		ImGui::EndChild();
		return;
	}

	if (ImGui::BeginTabBar("CCalTabs", 0)) {
		if (ImGui::BeginTabItem("Status")) {
			CCal_BasicInfo();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("More Graphs")) {
			ShowCalibrationDebug(2, 3);
			ImGui::EndTabItem();
		}
		
		if (ImGui::BeginTabItem("Settings")) {
			CCal_DrawSettings();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::EndChild();
}

void BuildCalibrationTab(VRState& vrState)
{
	bool continuousCalibration = CalCtx.state == CalibrationState::Continuous || CalCtx.state == CalibrationState::ContinuousStandby;

	if (continuousCalibration) {
		BuildContinuousCalDisplay();
		return;
	}

	auto state = LoadVRState();

	ImGui::BeginDisabled(CalCtx.state == CalibrationState::Continuous);
	BuildSystemSelection(state);
	BuildDeviceSelections(state);
	ImGui::EndDisabled();
	BuildCalibrationMenu();
}

