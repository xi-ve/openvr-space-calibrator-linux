#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <openvr.h>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <cctype>
#include "VRState.h"
#include "Configuration.h"
#include "../common/Version.h"
#include "Telemetry.h"
#include "Calibration.h"
#include "CalibrationUI.h"
#include "PlayspaceMovement.h"
#include "Logging.h"
#include <thread>
#include <chrono>
#include <time.h>

#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

// Load OpenGL functions that might not be in the base header
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif

// Function pointers for framebuffer functions
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
PFNGLFRAMEBUFFERTEXTUREPROC glFramebufferTexture = nullptr;
PFNGLDRAWBUFFERSPROC glDrawBuffers = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = nullptr;
PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer = nullptr;
#include <filesystem>
#include <unistd.h>
#include <limits.h>
#include <cstdlib>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static GLFWwindow* glfwWindow = nullptr;
static vr::VROverlayHandle_t overlayMainHandle = 0, overlayThumbnailHandle = 0;
static GLuint fboHandle = 0, fboTextureHandle = 0;
static int fboTextureWidth = 1200, fboTextureHeight = 800;

#define OPENVR_APPLICATION_KEY "spacecalibrator.linux"

void OpenURL(const char* url)
{
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", O_WRONLY);
        
        const char* waylandDisplay = getenv("WAYLAND_DISPLAY");
        const char* xdgSessionType = getenv("XDG_SESSION_TYPE");
        const char* desktop = getenv("XDG_CURRENT_DESKTOP");
        const char* hyprlandInstance = getenv("HYPRLAND_INSTANCE_SIGNATURE");
        
        bool isWayland = (waylandDisplay != nullptr && strlen(waylandDisplay) > 0) ||
                         (xdgSessionType != nullptr && strcmp(xdgSessionType, "wayland") == 0);
        bool isHyprland = (hyprlandInstance != nullptr && strlen(hyprlandInstance) > 0);
        
        const char* program = nullptr;
        const char* args[3] = {nullptr, url, nullptr};
        
        if (isHyprland || (isWayland && desktop && strstr(desktop, "Hyprland"))) {
            program = "xdg-open";
            args[0] = "xdg-open";
        } else if (isWayland) {
            if (desktop && (strstr(desktop, "GNOME") || strstr(desktop, "gnome"))) {
                program = "gio";
                args[0] = "gio";
                args[1] = "open";
                args[2] = url;
            } else if (desktop && (strstr(desktop, "KDE") || strstr(desktop, "kde"))) {
                program = "kde-open5";
                args[0] = "kde-open5";
            } else {
                program = "xdg-open";
                args[0] = "xdg-open";
            }
        } else {
            program = "xdg-open";
            args[0] = "xdg-open";
        }
        
        if (args[2] == nullptr) {
            execlp(program, args[0], args[1], (char*)nullptr);
        } else {
            execlp(program, args[0], args[1], args[2], (char*)nullptr);
        }
        
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

void GLFWErrorCallback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void InitVR()
{
    auto initError = vr::VRInitError_None;
    vr::VR_Init(&initError, vr::VRApplication_Overlay);
    if (initError != vr::VRInitError_None) {
        auto error = vr::VR_GetVRInitErrorAsEnglishDescription(initError);
        std::string errorMsg = "OpenVR initialization failed: " + std::string(error);
        errorMsg += "\n\nCommon causes:";
        errorMsg += "\n- SteamVR is not running";
        errorMsg += "\n- No VR headset connected";
        errorMsg += "\n- OpenVR runtime not found";
        throw std::runtime_error(errorMsg);
    }

    if (!vr::VR_IsInterfaceVersionValid(vr::IVRSystem_Version)) {
        throw std::runtime_error("OpenVR error: Outdated IVRSystem_Version. Please update SteamVR.");
    } else if (!vr::VR_IsInterfaceVersionValid(vr::IVRSettings_Version)) {
        throw std::runtime_error("OpenVR error: Outdated IVRSettings_Version. Please update SteamVR.");
    } else if (!vr::VR_IsInterfaceVersionValid(vr::IVROverlay_Version)) {
        throw std::runtime_error("OpenVR error: Outdated IVROverlay_Version. Please update SteamVR.");
    }
    
    LOG_INFO("OpenVR initialized successfully as overlay application");
}

void CreateGLFWWindow()
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // Allow resizing for desktop mode

    // Create window with reasonable default size for desktop mode
    // FBO will be used for VR overlay rendering
    glfwWindow = glfwCreateWindow(1280, 720, "Space Calibrator - Linux Edition", nullptr, nullptr);
    if (!glfwWindow)
        throw std::runtime_error("Failed to create window");

    glfwMakeContextCurrent(glfwWindow);
    glfwSwapInterval(1);
    
    // Load OpenGL extension functions
    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)glfwGetProcAddress("glGenFramebuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)glfwGetProcAddress("glBindFramebuffer");
    glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC)glfwGetProcAddress("glFramebufferTexture");
    glDrawBuffers = (PFNGLDRAWBUFFERSPROC)glfwGetProcAddress("glDrawBuffers");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glfwGetProcAddress("glCheckFramebufferStatus");
    glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)glfwGetProcAddress("glBlitFramebuffer");
    
    if (!glGenFramebuffers || !glBindFramebuffer || !glFramebufferTexture || !glDrawBuffers || !glCheckFramebufferStatus || !glBlitFramebuffer) {
        throw std::runtime_error("Failed to load required OpenGL framebuffer functions");
    }
    
    // Don't minimize window - let it be visible for desktop mode
    // Window will be minimized when overlay is active, but we need it visible for desktop use

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    ImGui::StyleColorsDark();
    
    // Create FBO and texture for overlay rendering
    glGenTextures(1, &fboTextureHandle);
    glBindTexture(GL_TEXTURE_2D, fboTextureHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fboTextureWidth, fboTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &fboHandle);
    glBindFramebuffer(GL_FRAMEBUFFER, fboHandle);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fboTextureHandle, 0);

    GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        throw std::runtime_error("OpenGL framebuffer incomplete");
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VerifySetupCorrect() {
    // Register the manifest so that it shows up in the overlays menu
    if (!vr::VRApplications()->IsApplicationInstalled(OPENVR_APPLICATION_KEY)) {
        char exePath[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", exePath, PATH_MAX);
        if (count != -1) {
            exePath[count] = '\0';
            std::string exeDir = std::filesystem::path(exePath).parent_path().string();
            std::string manifestPath = exeDir + "/manifest.vrmanifest";
            
            LOG_INFO("Registering overlay manifest: " + manifestPath);
            
            // If manifest is not installed, try installing it, and set it to auto-start with SteamVR
            auto vrAppErr = vr::VRApplications()->AddApplicationManifest(manifestPath.c_str());
            if (vrAppErr != vr::VRApplicationError_None) {
                LOG_ERROR(std::string("Failed to add manifest: ") + vr::VRApplications()->GetApplicationsErrorNameFromEnum(vrAppErr));
            } else {
                vr::VRApplications()->SetApplicationAutoLaunch(OPENVR_APPLICATION_KEY, true);
                LOG_INFO("Overlay manifest registered successfully");
            }
        } else {
            LOG_WARNING("Could not determine executable path for manifest registration");
        }
    } else {
        LOG_INFO("Space Calibrator overlay already registered with SteamVR");
    }
}

void TryCreateVROverlay() {
    if (overlayMainHandle || !vr::VROverlay())
        return;

    vr::VROverlayError error = vr::VROverlay()->CreateDashboardOverlay(
        OPENVR_APPLICATION_KEY, "Space Calibrator",
        &overlayMainHandle, &overlayThumbnailHandle
    );

    if (error == vr::VROverlayError_KeyInUse) {
        LOG_WARNING("Another instance of Space Calibrator may already be running");
        return;
    } else if (error != vr::VROverlayError_None) {
        LOG_ERROR(std::string("Error creating VR overlay: ") + vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
        return;
    }

    vr::VROverlay()->SetOverlayWidthInMeters(overlayMainHandle, 3.0f);
    vr::VROverlay()->SetOverlayInputMethod(overlayMainHandle, vr::VROverlayInputMethod_Mouse);
    vr::VROverlay()->SetOverlayFlag(overlayMainHandle, vr::VROverlayFlags_SendVRDiscreteScrollEvents, true);
    
    LOG_INFO("VR overlay created successfully");
}

static char searchFilter[256] = "";
static int deviceClassFilter = -1;

void BuildLogViewer()
{
    static bool autoScroll = true;
    static int logLevelFilter = Logger::INFO;
    static char logFilterText[256] = "";
    
    ImGui::BeginGroup();
    if (ImGui::Button("Clear Logs"))
    {
        Logger::GetInstance().ClearLogBuffer();
    }
    ImGui::SameLine();
    if (ImGui::Button("Rotate Logs"))
    {
        Logger::GetInstance().RotateLogs();
        LOG_INFO("Log files rotated");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll);
    ImGui::SameLine();
    ImGui::Text("Log Level:");
    ImGui::SameLine();
    const char* logLevels[] = { "DEBUG", "INFO", "WARNING", "ERROR" };
    if (ImGui::Combo("##LogLevel", &logLevelFilter, logLevels, 4))
    {
        Logger::GetInstance().SetLogLevel((Logger::LogLevel)logLevelFilter);
    }
    ImGui::EndGroup();
    
    ImGui::Text("Filter:");
    ImGui::SameLine();
    ImGui::InputText("##LogFilter", logFilterText, sizeof(logFilterText));
    
    ImGui::Separator();
    
    ImGui::Text("Log File: %s", Logger::GetInstance().GetLogFilePath().c_str());
    
    ImGui::Separator();
    
    ImGui::BeginChild("LogContent", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    auto logs = Logger::GetInstance().GetRecentLogs(1000);
    
    ImGuiListClipper clipper;
    clipper.Begin((int)logs.size());
    
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const std::string& logLine = logs[i];
            
            if (strlen(logFilterText) > 0 && logLine.find(logFilterText) == std::string::npos)
            {
                continue;
            }
            
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            if (logLine.find("[ERROR]") != std::string::npos)
            {
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            }
            else if (logLine.find("[WARN]") != std::string::npos)
            {
                color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
            }
            else if (logLine.find("[INFO]") != std::string::npos)
            {
                color = ImVec4(0.7f, 0.9f, 1.0f, 1.0f);
            }
            else if (logLine.find("[DEBUG]") != std::string::npos)
            {
                color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            }
            
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(logLine.c_str());
            ImGui::PopStyleColor();
        }
    }
    
    clipper.End();
    
    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
    {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
}

void BuildDevicesTab(VRState& vrState, AppConfig& config)
{
    ImGui::BeginGroup();
    if (ImGui::Button("Refresh Devices"))
    {
        vrState = VRState::Load();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Refresh", &config.autoRefresh);
    ImGui::EndGroup();

    ImGui::Text("Detected Devices: %zu", vrState.devices.size());

    ImGui::Separator();
    
    ImGui::Text("Filter:");
    ImGui::SameLine();
    ImGui::InputText("##Search", searchFilter, sizeof(searchFilter));
    ImGui::SameLine();
    const char* classFilterItems[] = { "All", "HMD", "Controller", "Tracker", "Base Station" };
    if (ImGui::Combo("##ClassFilter", &deviceClassFilter, classFilterItems, IM_ARRAYSIZE(classFilterItems)))
    {
        if (deviceClassFilter == 0) deviceClassFilter = -1;
    }

    ImGui::Separator();

    if (ImGui::BeginTable("Devices", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Model");
        ImGui::TableSetupColumn("Serial");
        ImGui::TableSetupColumn("Tracking System");
        ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Movement", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (const auto& device : vrState.devices)
        {
            bool showDevice = true;
            
            if (searchFilter[0] != '\0')
            {
                std::string searchLower = searchFilter;
                std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
                
                std::string modelLower = device.model;
                std::transform(modelLower.begin(), modelLower.end(), modelLower.begin(), ::tolower);
                std::string serialLower = device.serial;
                std::transform(serialLower.begin(), serialLower.end(), serialLower.begin(), ::tolower);
                std::string systemLower = device.trackingSystem;
                std::transform(systemLower.begin(), systemLower.end(), systemLower.begin(), ::tolower);
                
                if (modelLower.find(searchLower) == std::string::npos &&
                    serialLower.find(searchLower) == std::string::npos &&
                    systemLower.find(searchLower) == std::string::npos)
                {
                    showDevice = false;
                }
            }
            
            if (deviceClassFilter > 0)
            {
                vr::TrackedDeviceClass expectedClass = vr::TrackedDeviceClass_Invalid;
                switch (deviceClassFilter)
                {
                    case 1: expectedClass = vr::TrackedDeviceClass_HMD; break;
                    case 2: expectedClass = vr::TrackedDeviceClass_Controller; break;
                    case 3: expectedClass = vr::TrackedDeviceClass_GenericTracker; break;
                    case 4: expectedClass = vr::TrackedDeviceClass_TrackingReference; break;
                }
                if (device.deviceClass != expectedClass)
                {
                    showDevice = false;
                }
            }
            
            if (!showDevice) continue;
            
            ImGui::TableNextRow();
            
            ImGui::TableNextColumn();
            ImGui::Text("%d", device.id);
            
            ImGui::TableNextColumn();
            const char* classStr = "Unknown";
            switch (device.deviceClass) {
                case vr::TrackedDeviceClass_HMD:
                    classStr = "HMD";
                    break;
                case vr::TrackedDeviceClass_Controller:
                    classStr = "Controller";
                    break;
                case vr::TrackedDeviceClass_GenericTracker:
                    classStr = "Tracker";
                    break;
                case vr::TrackedDeviceClass_TrackingReference:
                    classStr = "Base Station";
                    break;
                default:
                    classStr = "Unknown";
                    break;
            }
            ImGui::Text("%s", classStr);
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", device.model.c_str());
            
            ImGui::TableNextColumn();
            // Color serial green if tracker is rapidly moving
            if (device.deviceClass == vr::TrackedDeviceClass_GenericTracker && device.isMoving) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", device.serial.c_str());
            } else {
                ImGui::Text("%s", device.serial.c_str());
            }
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", device.trackingSystem.c_str());
            
            ImGui::TableNextColumn();
            const char* roleStr = "N/A";
            switch (device.controllerRole) {
                case vr::TrackedControllerRole_LeftHand:
                    roleStr = "Left";
                    break;
                case vr::TrackedControllerRole_RightHand:
                    roleStr = "Right";
                    break;
                case vr::TrackedControllerRole_OptOut:
                    roleStr = "Opt Out";
                    break;
                case vr::TrackedControllerRole_Treadmill:
                    roleStr = "Treadmill";
                    break;
                case vr::TrackedControllerRole_Stylus:
                    roleStr = "Stylus";
                    break;
                default:
                    roleStr = "N/A";
                    break;
            }
            ImGui::Text("%s", roleStr);
            
            ImGui::TableNextColumn();
            if (device.isTracking) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Tracking");
            } else {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Not Tracking");
            }
            
            ImGui::TableNextColumn();
            // Show movement status for trackers
            if (device.deviceClass == vr::TrackedDeviceClass_GenericTracker) {
                if (device.isMoving) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Moving");
                } else {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Still");
                }
            } else {
                ImGui::Text("N/A");
            }
        }

        ImGui::EndTable();
    }
}

static PlayspaceMovement* g_playspaceMovement = nullptr;

AppConfig* g_appConfig = nullptr;

void BuildMainWindow(VRState& vrState, AppConfig& config, bool runningInOverlay = false)
{
	g_appConfig = &config;
    auto& io = ImGui::GetIO();
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;
    
    // Only disable resizing in overlay mode
    if (runningInOverlay) {
        windowFlags |= ImGuiWindowFlags_NoResize;
    }
    
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, runningInOverlay ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    
    ImGui::Begin("Space Calibrator - Linux Edition", nullptr, windowFlags);

    ImGui::Text("Version: %s", SPACECAL_VERSION_STRING);
    if (runningInOverlay)
    {
        ImGui::SameLine();
        ImGui::Text("- close VR overlay to use mouse");
    }
    ImGui::Separator();

    if (ImGui::BeginTabBar("MainTabs"))
    {
        if (ImGui::BeginTabItem("Devices"))
        {
            BuildDevicesTab(vrState, config);
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Calibration"))
        {
            BuildCalibrationTab(vrState);
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Telemetry"))
        {
            BuildTelemetryWindow(vrState);
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Logs"))
        {
            BuildLogViewer();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Playspace Movement"))
        {
            if (g_playspaceMovement) {
                BuildPlayspaceMovementTab(*g_playspaceMovement);
            }
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    
    float creditsHeight = ImGui::GetTextLineHeightWithSpacing();
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - creditsHeight - ImGui::GetStyle().WindowPadding.y);
    
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Based on ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "OpenVR-SpaceCalibrator");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), " by ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "@hyblocker");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), " | Linux port by ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "@xi-ve");

    ImGui::End();
}

void RunLoop()
{
    Logger::GetInstance().Initialize();
    LOG_INFO("Space Calibrator starting - Version: " + std::string(SPACECAL_VERSION_STRING));
    LOG_INFO("Log file location: " + Logger::GetInstance().GetLogFilePath());
    
    // Register overlay manifest if not already registered
    VerifySetupCorrect();
    
    LOG_DEBUG("Loading VR state and configuration");
    VRState vrState = VRState::Load();
    AppConfig config = AppConfig::Load();
    
    InitCalibrator();
    LoadProfile(CalCtx);
    
    static PlayspaceMovement playspaceMovement;
    playspaceMovement.settings.enabled = config.playspaceMovementEnabled;
    playspaceMovement.settings.movementMultiplier = config.playspaceMovementMultiplier;
    
    g_playspaceMovement = &playspaceMovement;
    if (!playspaceMovement.Initialize()) {
        LOG_WARNING("Failed to initialize PlayspaceMovement");
        g_playspaceMovement = nullptr;
    }
    
    static double lastRefreshTime = 0.0;
    double currentTime = glfwGetTime();
    static char textBuf[4096] = {0};
    static bool keyboardOpen = false, keyboardJustClosed = false;

    while (!glfwWindowShouldClose(glfwWindow))
    {
        TryCreateVROverlay();
        glfwPollEvents();
        
        currentTime = glfwGetTime();
        if (config.autoRefresh && (currentTime - lastRefreshTime) > (config.refreshInterval / 1000.0))
        {
            vrState = VRState::Load();
            lastRefreshTime = currentTime;
        }

        CalibrationTick(currentTime);
        if (g_playspaceMovement) {
            g_playspaceMovement->Update();
        }
        
        int width, height;
        glfwGetFramebufferSize(glfwWindow, &width, &height);
        bool windowVisible = (width > 0 && height > 0);
        bool dashboardVisible = false;

        if (overlayMainHandle && vr::VROverlay())
        {
            dashboardVisible = vr::VROverlay()->IsActiveDashboardOverlay(overlayMainHandle);
            
            // Minimize window when overlay is active to save resources
            if (dashboardVisible && !glfwGetWindowAttrib(glfwWindow, GLFW_ICONIFIED)) {
                glfwIconifyWindow(glfwWindow);
                windowVisible = false; // Window is minimized, don't render to it
            } else if (!dashboardVisible && glfwGetWindowAttrib(glfwWindow, GLFW_ICONIFIED)) {
                // Restore window when overlay is not active
                glfwRestoreWindow(glfwWindow);
                glfwGetFramebufferSize(glfwWindow, &width, &height);
                windowVisible = (width > 0 && height > 0);
            }
            
            // Only handle VR input events when dashboard is actually visible
            if (dashboardVisible)
            {
                auto &io = ImGui::GetIO();

                // Handle keyboard input
                if (keyboardJustClosed && keyboardOpen)
                {
                    ImGui::ClearActiveID();
                    keyboardOpen = false;
                }
                else if (keyboardJustClosed)
                {
                    keyboardJustClosed = false;
                }
                else if (!io.WantTextInput)
                {
                    keyboardOpen = false;
                }
                else if (io.WantTextInput && !keyboardOpen && !keyboardJustClosed)
                {
                    int id = ImGui::GetActiveID();
                    auto textInfo = ImGui::GetInputTextState(id);
                    if (textInfo != nullptr) {
                        textBuf[0] = 0;
                        int len = std::min((int)textInfo->TextA.Size, (int)sizeof(textBuf) - 1);
                        if (len > 0) {
                            memcpy(textBuf, textInfo->TextA.Data, len);
                        }
                        textBuf[len] = 0;

                        uint32_t unFlags = 0;
                        vr::VROverlay()->ShowKeyboardForOverlay(
                            overlayMainHandle, vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine,
                            unFlags, "Space Calibrator Overlay", sizeof textBuf, textBuf, 0
                        );
                        keyboardOpen = true;
                    }
                }

                // Handle VR events (only when dashboard is visible)
                vr::VREvent_t vrEvent;
                while (vr::VROverlay()->PollNextOverlayEvent(overlayMainHandle, &vrEvent, sizeof(vrEvent)))
                {
                    switch (vrEvent.eventType) {
                    case vr::VREvent_MouseMove: {
                        // Mouse coordinates from VR are in texture space (0 to texture width/height)
                        // Flip Y coordinate because VR mouse coordinates use bottom-left origin
                        // but ImGui uses top-left origin
                        float mouseX = (float)vrEvent.data.mouse.x;
                        float mouseY = (float)(fboTextureHeight - vrEvent.data.mouse.y);
                        io.AddMousePosEvent(mouseX, mouseY);
                        break;
                    }
                    case vr::VREvent_MouseButtonDown:
                        io.AddMouseButtonEvent((vrEvent.data.mouse.button & vr::VRMouseButton_Left) == vr::VRMouseButton_Left ? 0 : 1, true);
                        break;
                    case vr::VREvent_MouseButtonUp:
                        io.AddMouseButtonEvent((vrEvent.data.mouse.button & vr::VRMouseButton_Left) == vr::VRMouseButton_Left ? 0 : 1, false);
                        break;
                    case vr::VREvent_ScrollDiscrete:
                    {
                        float x = vrEvent.data.scroll.xdelta * 360.0f * 8.0f;
                        float y = vrEvent.data.scroll.ydelta * 360.0f * 8.0f;
                        io.AddMouseWheelEvent(x, y);
                        break;
                    }
                    case vr::VREvent_KeyboardDone: {
                        vr::VROverlay()->GetKeyboardText(textBuf, sizeof textBuf);
                        int id = ImGui::GetActiveID();
                        auto textInfo = ImGui::GetInputTextState(id);
                        if (textInfo != nullptr) {
                            textInfo->TextA.clear();
                            size_t len = strlen(textBuf);
                            for (size_t i = 0; i < len; ++i) {
                                textInfo->TextA.push_back(textBuf[i]);
                            }
                            textInfo->TextA.push_back('\0');
                        }
                        keyboardJustClosed = true;
                        break;
                    }
                    case vr::VREvent_Quit:
                        goto exit_loop;
                    }
                }
            }
        }
        
        // Render for VR overlay (always render to FBO when overlay is active)
        if (dashboardVisible)
        {
            auto& io = ImGui::GetIO();

            // These change state now, so we must execute these before doing our own modifications to the io state for VR
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();

            io.DisplaySize = ImVec2((float)fboTextureWidth, (float)fboTextureHeight);
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
            io.ConfigFlags = io.ConfigFlags | ImGuiConfigFlags_NoMouseCursorChange;

            ImGui::NewFrame();
            BuildMainWindow(vrState, config, true);
            ImGui::Render();

            // Render to FBO for VR overlay
            glBindFramebuffer(GL_FRAMEBUFFER, fboHandle);
            glViewport(0, 0, fboTextureWidth, fboTextureHeight);
            
            // Set clip origin to upper-left to match ImGui's coordinate system
            // This ensures the texture is stored with top-left origin instead of bottom-left
            // Fallback: If glClipControl is not available (OpenGL < 4.5), use texture bounds flip
            typedef void (*PFNGLCLIPCONTROLPROC)(GLenum origin, GLenum depth);
            static PFNGLCLIPCONTROLPROC glClipControl = nullptr;
            static bool glClipControlChecked = false;
            static bool useTextureBoundsFlip = false;
            
            if (!glClipControlChecked) {
                glClipControl = (PFNGLCLIPCONTROLPROC)glfwGetProcAddress("glClipControl");
                glClipControlChecked = true;
                useTextureBoundsFlip = (glClipControl == nullptr);
                if (useTextureBoundsFlip) {
                    LOG_INFO("glClipControl not available (OpenGL < 4.5), using texture bounds flip fallback");
                }
            }
            
            if (glClipControl) {
                glClipControl(0x8CA2, 0x935E); // GL_UPPER_LEFT, GL_ZERO_TO_ONE
            }
            
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            // Restore clip origin to lower-left (default)
            if (glClipControl) {
                glClipControl(0x8CA1, 0x935E); // GL_LOWER_LEFT, GL_ZERO_TO_ONE
            }
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Set overlay texture
            vr::Texture_t vrTex = {
                .handle = (void*)(uint64_t)fboTextureHandle,
                .eType = vr::TextureType_OpenGL,
                .eColorSpace = vr::ColorSpace_Auto,
            };

            vr::HmdVector2_t mouseScale = { (float) fboTextureWidth, (float) fboTextureHeight };
            vr::VROverlay()->SetOverlayTexture(overlayMainHandle, &vrTex);
            
            // If glClipControl is not available, flip texture bounds as fallback
            if (useTextureBoundsFlip) {
                vr::VRTextureBounds_t textureBounds;
                textureBounds.uMin = 0.0f;
                textureBounds.vMin = 1.0f;  // Start at top (flip)
                textureBounds.uMax = 1.0f;
                textureBounds.vMax = 0.0f;  // End at bottom (flip)
                vr::VROverlay()->SetOverlayTextureBounds(overlayMainHandle, &textureBounds);
            }
            
            vr::VROverlay()->SetOverlayMouseScale(overlayMainHandle, &mouseScale);
        }
        
        // Render for desktop window (separate rendering path)
        if (windowVisible && !dashboardVisible)
        {
            auto& io = ImGui::GetIO();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();

            io.DisplaySize = ImVec2((float)width, (float)height);
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
            io.ConfigFlags = io.ConfigFlags & ~ImGuiConfigFlags_NoMouseCursorChange;

            ImGui::NewFrame();
            BuildMainWindow(vrState, config, false);
            
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                config.Save();
                SaveProfile(CalCtx);
                break;
            }

            ImGui::Render();

            // Render directly to window
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(glfwWindow);
        }

        const double dashboardInterval = 1.0 / 90.0; // 90 fps for VR
        double waitEventsTimeout = std::max(CalCtx.wantedUpdateInterval, dashboardInterval);
        if (dashboardVisible && waitEventsTimeout > dashboardInterval)
            waitEventsTimeout = dashboardInterval;

        glfwWaitEventsTimeout(waitEventsTimeout);
    }
    
exit_loop:
    SaveProfile(CalCtx);
    Logger::GetInstance().Shutdown();
}

int main(int argc, char* argv[])
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    glfwSetErrorCallback(GLFWErrorCallback);

    try {
        InitVR();
        CreateGLFWWindow();
        RunLoop();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        vr::VR_Shutdown();
    }
    catch (std::runtime_error& e)
    {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        std::cerr << "Make sure SteamVR is running and VR devices are connected." << std::endl;
        
        if (glfwWindow)
        {
            glfwDestroyWindow(glfwWindow);
        }
        glfwTerminate();
        return 1;
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        
        if (glfwWindow)
        {
            glfwDestroyWindow(glfwWindow);
        }
        glfwTerminate();
        return 1;
    }

    if (glfwWindow)
        glfwDestroyWindow(glfwWindow);

    glfwTerminate();
    return 0;
}

