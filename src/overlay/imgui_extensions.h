#pragma once

#include "imgui.h"

namespace ImGui
{
	// Group panels taken from https://github.com/ocornut/imgui/issues/1496#issuecomment-655048353
	// Licensed under CC0 license as per https://github.com/ocornut/imgui/issues/1496#issuecomment-1287772456

	void BeginGroupPanel(const char* name, const ImVec2& size = ImVec2(0.0f, 0.0f));
	void EndGroupPanel();

	inline float GetWindowContentRegionWidth() { return ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x; }
}


