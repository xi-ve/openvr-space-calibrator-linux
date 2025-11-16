#pragma once

#include "Calibration.h"
#include "VRState.h"
#include <string>

const char* GetPrettyTrackingSystemName(const std::string& system);
void BuildCalibrationTab(VRState& vrState);
void BuildSystemSelection(const VRState &state);
void BuildDeviceSelections(const VRState &state);
void BuildCalibrationMenu();
std::string LabelString(const VRDevice &device);
std::string LabelString(const StandbyDevice& device);

void BuildContinuousCalDisplay();
void CCal_BasicInfo();
void CCal_DrawSettings();
void DrawVectorElement(const std::string id, const char* text, double* value, int defaultValue = 0, const char* defaultValueStr = " 0 ");

