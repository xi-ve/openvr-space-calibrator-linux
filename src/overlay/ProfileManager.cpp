#include "Calibration.h"
#include "Logging.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <iostream>

static std::string GetProfileDirectory()
{
	const char* home = getenv("HOME");
	if (!home) {
		struct passwd* pw = getpwuid(getuid());
		if (pw) {
			home = pw->pw_dir;
		}
	}
	
	if (!home) {
		return std::string(".");
	}
	
	std::string profileDir = std::string(home) + "/.config/space-calibrator";
	std::filesystem::create_directories(profileDir);
	return profileDir;
}

static std::string GetProfileFilePath()
{
	return GetProfileDirectory() + "/profile.json";
}

static void WriteProfile(CalibrationContext &ctx, std::ostream &out)
{
	if (!ctx.validProfile) {
		return;
	}

	out << "{\n";
	out << "  \"reference_tracking_system\": \"" << ctx.referenceTrackingSystem << "\",\n";
	out << "  \"target_tracking_system\": \"" << ctx.targetTrackingSystem << "\",\n";
	out << "  \"roll\": " << ctx.calibratedRotation(0) << ",\n";
	out << "  \"yaw\": " << ctx.calibratedRotation(1) << ",\n";
	out << "  \"pitch\": " << ctx.calibratedRotation(2) << ",\n";
	out << "  \"x\": " << ctx.calibratedTranslation(0) << ",\n";
	out << "  \"y\": " << ctx.calibratedTranslation(1) << ",\n";
	out << "  \"z\": " << ctx.calibratedTranslation(2) << ",\n";
	out << "  \"scale\": " << ctx.calibratedScale << ",\n";
	out << "  \"reference_device\": {\n";
	out << "    \"tracking_system\": \"" << ctx.referenceStandby.trackingSystem << "\",\n";
	out << "    \"model\": \"" << ctx.referenceStandby.model << "\",\n";
	out << "    \"serial\": \"" << ctx.referenceStandby.serial << "\"\n";
	out << "  },\n";
	out << "  \"target_device\": {\n";
	out << "    \"tracking_system\": \"" << ctx.targetStandby.trackingSystem << "\",\n";
	out << "    \"model\": \"" << ctx.targetStandby.model << "\",\n";
	out << "    \"serial\": \"" << ctx.targetStandby.serial << "\"\n";
	out << "  },\n";
	out << "  \"autostart_continuous_calibration\": " << (ctx.state == CalibrationState::Continuous || ctx.state == CalibrationState::ContinuousStandby ? "true" : "false") << ",\n";
	out << "  \"quash_target_in_continuous\": " << (ctx.quashTargetInContinuous ? "true" : "false") << ",\n";
	out << "  \"require_trigger_press_to_apply\": " << (ctx.requireTriggerPressToApply ? "true" : "false") << ",\n";
	out << "  \"ignore_outliers\": " << (ctx.ignoreOutliers ? "true" : "false") << ",\n";
	out << "  \"continuous_calibration_target_offset_x\": " << ctx.continuousCalibrationOffset(0) << ",\n";
	out << "  \"continuous_calibration_target_offset_y\": " << ctx.continuousCalibrationOffset(1) << ",\n";
	out << "  \"continuous_calibration_target_offset_z\": " << ctx.continuousCalibrationOffset(2) << ",\n";
	out << "  \"static_calibration\": " << (ctx.enableStaticRecalibration ? "true" : "false") << ",\n";
	out << "  \"jitter_threshold\": " << ctx.jitterThreshold << ",\n";
	out << "  \"max_relative_error_threshold\": " << ctx.maxRelativeErrorThreshold << ",\n";
	out << "  \"calibration_speed\": " << (int)ctx.calibrationSpeed << ",\n";
	out << "  \"relative_pos_calibrated\": " << (ctx.relativePosCalibrated ? "true" : "false") << ",\n";
	out << "  \"lock_relative_position\": " << (ctx.lockRelativePosition ? "true" : "false") << "\n";
	out << "}\n";
}

static void ParseProfile(CalibrationContext &ctx, std::istream &stream)
{
	std::string line;
	std::string json;
	while (std::getline(stream, line)) {
		json += line + "\n";
	}

	size_t pos;
	
	pos = json.find("\"reference_tracking_system\"");
	if (pos != std::string::npos) {
		pos = json.find("\"", pos + 28);
		if (pos != std::string::npos) {
			size_t end = json.find("\"", pos + 1);
			if (end != std::string::npos) {
				ctx.referenceTrackingSystem = json.substr(pos + 1, end - pos - 1);
			}
		}
	}

	pos = json.find("\"target_tracking_system\"");
	if (pos != std::string::npos) {
		pos = json.find("\"", pos + 24);
		if (pos != std::string::npos) {
			size_t end = json.find("\"", pos + 1);
			if (end != std::string::npos) {
				ctx.targetTrackingSystem = json.substr(pos + 1, end - pos - 1);
			}
		}
	}

	pos = json.find("\"roll\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.calibratedRotation(0) = std::stod(json.substr(pos));
		}
	}

	pos = json.find("\"yaw\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.calibratedRotation(1) = std::stod(json.substr(pos));
		}
	}

	pos = json.find("\"pitch\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.calibratedRotation(2) = std::stod(json.substr(pos));
		}
	}

	pos = json.find("\"x\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.calibratedTranslation(0) = std::stod(json.substr(pos));
		}
	}

	pos = json.find("\"y\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.calibratedTranslation(1) = std::stod(json.substr(pos));
		}
	}

	pos = json.find("\"z\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.calibratedTranslation(2) = std::stod(json.substr(pos));
		}
	}

	pos = json.find("\"scale\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.calibratedScale = std::stod(json.substr(pos));
		}
	}

	pos = json.find("\"autostart_continuous_calibration\"");
	if (pos != std::string::npos) {
		if (json.find("true", pos) != std::string::npos) {
			ctx.state = CalibrationState::ContinuousStandby;
		}
	}

	pos = json.find("\"quash_target_in_continuous\"");
	if (pos != std::string::npos) {
		ctx.quashTargetInContinuous = (json.find("true", pos) != std::string::npos);
	}

	pos = json.find("\"require_trigger_press_to_apply\"");
	if (pos != std::string::npos) {
		ctx.requireTriggerPressToApply = (json.find("true", pos) != std::string::npos);
	}

	pos = json.find("\"ignore_outliers\"");
	if (pos != std::string::npos) {
		ctx.ignoreOutliers = (json.find("true", pos) != std::string::npos);
	}

	pos = json.find("\"continuous_calibration_target_offset_x\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.continuousCalibrationOffset(0) = std::stod(json.substr(pos));
		}
	}

	pos = json.find("\"continuous_calibration_target_offset_y\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.continuousCalibrationOffset(1) = std::stod(json.substr(pos));
		}
	}

	pos = json.find("\"continuous_calibration_target_offset_z\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.continuousCalibrationOffset(2) = std::stod(json.substr(pos));
		}
	}

	pos = json.find("\"static_calibration\"");
	if (pos != std::string::npos) {
		ctx.enableStaticRecalibration = (json.find("true", pos) != std::string::npos);
	}

	pos = json.find("\"jitter_threshold\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.jitterThreshold = std::stof(json.substr(pos));
		}
	}

	pos = json.find("\"max_relative_error_threshold\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			ctx.maxRelativeErrorThreshold = std::stof(json.substr(pos));
		}
	}

	pos = json.find("\"calibration_speed\"");
	if (pos != std::string::npos) {
		pos = json.find_first_of("0123456789-", pos);
		if (pos != std::string::npos) {
			int speed = std::stoi(json.substr(pos));
			ctx.calibrationSpeed = (CalibrationContext::Speed)speed;
		}
	}

	// Load reference_device standby information
	size_t refDevicePos = json.find("\"reference_device\"");
	if (refDevicePos != std::string::npos) {
		size_t refEnd = json.find("}", refDevicePos);
		if (refEnd != std::string::npos) {
			std::string refDeviceBlock = json.substr(refDevicePos, refEnd - refDevicePos);
			
			pos = refDeviceBlock.find("\"tracking_system\"");
			if (pos != std::string::npos) {
				pos = refDeviceBlock.find("\"", pos + 18);
				if (pos != std::string::npos) {
					size_t end = refDeviceBlock.find("\"", pos + 1);
					if (end != std::string::npos) {
						ctx.referenceStandby.trackingSystem = refDeviceBlock.substr(pos + 1, end - pos - 1);
					}
				}
			}
			pos = refDeviceBlock.find("\"model\"");
			if (pos != std::string::npos) {
				pos = refDeviceBlock.find("\"", pos + 8);
				if (pos != std::string::npos) {
					size_t end = refDeviceBlock.find("\"", pos + 1);
					if (end != std::string::npos) {
						ctx.referenceStandby.model = refDeviceBlock.substr(pos + 1, end - pos - 1);
					}
				}
			}
			pos = refDeviceBlock.find("\"serial\"");
			if (pos != std::string::npos) {
				pos = refDeviceBlock.find("\"", pos + 9);
				if (pos != std::string::npos) {
					size_t end = refDeviceBlock.find("\"", pos + 1);
					if (end != std::string::npos) {
						ctx.referenceStandby.serial = refDeviceBlock.substr(pos + 1, end - pos - 1);
					}
				}
			}
		}
	}

	// Load target_device standby information
	size_t targetDevicePos = json.find("\"target_device\"");
	if (targetDevicePos != std::string::npos) {
		size_t targetEnd = json.find("}", targetDevicePos);
		if (targetEnd != std::string::npos) {
			std::string targetDeviceBlock = json.substr(targetDevicePos, targetEnd - targetDevicePos);
			
			pos = targetDeviceBlock.find("\"tracking_system\"");
			if (pos != std::string::npos) {
				pos = targetDeviceBlock.find("\"", pos + 18);
				if (pos != std::string::npos) {
					size_t end = targetDeviceBlock.find("\"", pos + 1);
					if (end != std::string::npos) {
						ctx.targetStandby.trackingSystem = targetDeviceBlock.substr(pos + 1, end - pos - 1);
					}
				}
			}
			pos = targetDeviceBlock.find("\"model\"");
			if (pos != std::string::npos) {
				pos = targetDeviceBlock.find("\"", pos + 8);
				if (pos != std::string::npos) {
					size_t end = targetDeviceBlock.find("\"", pos + 1);
					if (end != std::string::npos) {
						ctx.targetStandby.model = targetDeviceBlock.substr(pos + 1, end - pos - 1);
					}
				}
			}
			pos = targetDeviceBlock.find("\"serial\"");
			if (pos != std::string::npos) {
				pos = targetDeviceBlock.find("\"", pos + 9);
				if (pos != std::string::npos) {
					size_t end = targetDeviceBlock.find("\"", pos + 1);
					if (end != std::string::npos) {
						ctx.targetStandby.serial = targetDeviceBlock.substr(pos + 1, end - pos - 1);
					}
				}
			}
		}
	}

	// Load additional fields
	pos = json.find("\"relative_pos_calibrated\"");
	if (pos != std::string::npos) {
		ctx.relativePosCalibrated = (json.find("true", pos) != std::string::npos);
	}

	pos = json.find("\"lock_relative_position\"");
	if (pos != std::string::npos) {
		ctx.lockRelativePosition = (json.find("true", pos) != std::string::npos);
	}

	ctx.validProfile = true;
}

void LoadProfile(CalibrationContext &ctx)
{
	ctx.validProfile = false;

	std::string profileFile = GetProfileFilePath();
	LOG_DEBUG("Loading profile from: " + profileFile);
	std::ifstream file(profileFile);
	
	if (!file.is_open()) {
		LOG_INFO("Profile file not found, starting with empty profile");
		std::cout << "Profile file not found, starting with empty profile" << std::endl;
		ctx.Clear();
		return;
	}

	try {
		ParseProfile(ctx, file);
		LOG_INFO("Profile loaded successfully from " + profileFile);
		std::cout << "Loaded profile from " << profileFile << std::endl;
	} catch (const std::exception &e) {
		LOG_ERROR("Error loading profile: " + std::string(e.what()));
		std::cerr << "Error loading profile: " << e.what() << std::endl;
		ctx.Clear();
	}
	
	file.close();
	ScanAndApplyProfile(ctx);
}

void SaveProfile(CalibrationContext &ctx)
{
	std::string profileFile = GetProfileFilePath();
	
	if (!ctx.validProfile) {
		if (std::filesystem::exists(profileFile)) {
			LOG_INFO("Deleting invalid profile file: " + profileFile);
			std::filesystem::remove(profileFile);
			std::cout << "Deleted profile file: " << profileFile << std::endl;
		}
		return;
	}

	bool isContinuousMode = (ctx.state == CalibrationState::Continuous || ctx.state == CalibrationState::ContinuousStandby);
	
	if (!isContinuousMode) {
		LOG_DEBUG("Saving profile to: " + profileFile);
	}
	
	std::ofstream file(profileFile);
	
	if (!file.is_open()) {
		LOG_ERROR("Could not open profile file for writing: " + profileFile);
		std::cerr << "Error: Could not open profile file for writing: " << profileFile << std::endl;
		return;
	}

	WriteProfile(ctx, file);
	file.close();
	
	if (!isContinuousMode) {
		LOG_INFO("Profile saved successfully to " + profileFile);
		std::cout << "Saved profile to " << profileFile << std::endl;
	}
}

bool ExportProfile(CalibrationContext &ctx, const std::string &filePath)
{
	if (!ctx.validProfile) {
		std::cerr << "Error: No valid profile to export" << std::endl;
		return false;
	}

	std::ofstream file(filePath);
	
	if (!file.is_open()) {
		std::cerr << "Error: Could not open file for writing: " << filePath << std::endl;
		return false;
	}

	WriteProfile(ctx, file);
	file.close();
	
	std::cout << "Exported profile to " << filePath << std::endl;
	return true;
}

bool ImportProfile(CalibrationContext &ctx, const std::string &filePath)
{
	std::ifstream file(filePath);
	
	if (!file.is_open()) {
		std::cerr << "Error: Could not open file for reading: " << filePath << std::endl;
		return false;
	}

	try {
		ParseProfile(ctx, file);
		std::cout << "Imported profile from " << filePath << std::endl;
		file.close();
		return true;
	} catch (const std::exception &e) {
		std::cerr << "Error importing profile: " << e.what() << std::endl;
		file.close();
		ctx.Clear();
		return false;
	}
}

