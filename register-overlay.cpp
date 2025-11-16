#include <openvr.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <limits.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <manifest_path>" << std::endl;
        return 1;
    }

    const char* manifestPath = argv[1];
    
    vr::EVRInitError initError = vr::VRInitError_None;
    vr::IVRSystem* vrSystem = vr::VR_Init(&initError, vr::VRApplication_Utility);
    
    if (initError != vr::VRInitError_None) {
        std::cerr << "Failed to initialize OpenVR: " << vr::VR_GetVRInitErrorAsEnglishDescription(initError) << std::endl;
        std::cerr << "Note: SteamVR may need to be running for overlay registration." << std::endl;
        return 1;
    }
    
    if (!vrSystem) {
        std::cerr << "Failed to initialize VR system" << std::endl;
        vr::VR_Shutdown();
        return 1;
    }
    
    vr::IVRApplications* vrApplications = vr::VRApplications();
    if (!vrApplications) {
        std::cerr << "Failed to get VRApplications interface" << std::endl;
        vr::VR_Shutdown();
        return 1;
    }
    
    const char* appKey = "spacecalibrator.linux";
    bool alreadyInstalled = vrApplications->IsApplicationInstalled(appKey);
    
    if (alreadyInstalled) {
        char oldManifestPath[PATH_MAX];
        vr::EVRApplicationError appErr = vrApplications->GetApplicationPropertyString(
            appKey, vr::VRApplicationProperty_ManifestPath_String, 
            oldManifestPath, PATH_MAX, &appErr);
        
        if (appErr == vr::VRApplicationError_None && strcmp(oldManifestPath, manifestPath) != 0) {
            std::cout << "Removing old manifest: " << oldManifestPath << std::endl;
            vrApplications->RemoveApplicationManifest(oldManifestPath);
        } else if (appErr == vr::VRApplicationError_None) {
            std::cout << "Overlay already registered with this manifest" << std::endl;
            vr::VR_Shutdown();
            return 0;
        }
    }
    
    std::cout << "Registering manifest: " << manifestPath << std::endl;
    vr::EVRApplicationError appError = vrApplications->AddApplicationManifest(manifestPath);
    if (appError != vr::VRApplicationError_None) {
        std::cerr << "Failed to add manifest: " << vrApplications->GetApplicationsErrorNameFromEnum(appError) << std::endl;
        vr::VR_Shutdown();
        return 1;
    }
    
    std::cout << "Manifest registered successfully" << std::endl;
    
    vr::EVRApplicationError autoLaunchError = vrApplications->SetApplicationAutoLaunch(appKey, true);
    if (autoLaunchError != vr::VRApplicationError_None) {
        std::cerr << "Warning: Failed to set auto-launch: " << vrApplications->GetApplicationsErrorNameFromEnum(autoLaunchError) << std::endl;
    } else {
        std::cout << "Auto-launch enabled" << std::endl;
    }
    
    vr::VR_Shutdown();
    return 0;
}

