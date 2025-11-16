cmake_policy(SET CMP0072 NEW)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(PkgConfig REQUIRED)

if(NOT DEFINED OPENVR_INCLUDE_DIR OR NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr.h")
    get_filename_component(PROJECT_ROOT "${CMAKE_SOURCE_DIR}" DIRECTORY)
    
    if(NOT DEFINED OPENVR_INCLUDE_DIR OR NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr.h")
        set(OPENVR_INCLUDE_DIR "${PROJECT_ROOT}/../lib/openvr/headers" CACHE PATH "OpenVR include directory")
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr.h")
        set(OPENVR_INCLUDE_DIR "${PROJECT_ROOT}/../lib/openvr" CACHE PATH "OpenVR include directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr.h")
        set(OPENVR_INCLUDE_DIR "${PROJECT_ROOT}/../WindowsEdition/OpenVR-SpaceCalibrator/lib/openvr/headers" CACHE PATH "OpenVR include directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr.h")
        set(OPENVR_INCLUDE_DIR "$ENV{HOME}/.local/share/Steam/steamapps/common/SteamVR/headers" CACHE PATH "OpenVR include directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr.h")
        set(OPENVR_INCLUDE_DIR "$ENV{HOME}/.steam/steam/steamapps/common/SteamVR/headers" CACHE PATH "OpenVR include directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr.h")
        set(OPENVR_INCLUDE_DIR "$ENV{HOME}/.steam/root/steamapps/common/SteamVR/headers" CACHE PATH "OpenVR include directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr.h")
        set(OPENVR_INCLUDE_DIR "/usr/include/openvr" CACHE PATH "OpenVR include directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr.h")
        set(OPENVR_INCLUDE_DIR "/usr/local/include/openvr" CACHE PATH "OpenVR include directory" FORCE)
    endif()
endif()

if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr.h")
    message(FATAL_ERROR "OpenVR headers not found at: ${OPENVR_INCLUDE_DIR}\nPlease install OpenVR SDK or set OPENVR_INCLUDE_DIR to the correct path.")
endif()

message(STATUS "Using OpenVR headers from: ${OPENVR_INCLUDE_DIR}")

if(NOT DEFINED OPENVR_LIB_DIR)
    get_filename_component(PROJECT_ROOT "${CMAKE_SOURCE_DIR}" DIRECTORY)
    
    set(OPENVR_LIB_DIR "${PROJECT_ROOT}/../lib/openvr/lib/linux64" CACHE PATH "OpenVR library directory")
    
    if(NOT EXISTS "${OPENVR_LIB_DIR}/libopenvr_api.so")
        set(OPENVR_LIB_DIR "${PROJECT_ROOT}/../WindowsEdition/OpenVR-SpaceCalibrator/lib/openvr/lib/linux64" CACHE PATH "OpenVR library directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_LIB_DIR}/libopenvr_api.so")
        set(OPENVR_LIB_DIR "$ENV{HOME}/.local/share/Steam/steamapps/common/SteamVR/bin/linux64" CACHE PATH "OpenVR library directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_LIB_DIR}/libopenvr_api.so")
        set(OPENVR_LIB_DIR "$ENV{HOME}/.steam/steam/steamapps/common/SteamVR/bin/linux64" CACHE PATH "OpenVR library directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_LIB_DIR}/libopenvr_api.so")
        set(OPENVR_LIB_DIR "$ENV{HOME}/.steam/root/steamapps/common/SteamVR/bin/linux64" CACHE PATH "OpenVR library directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_LIB_DIR}/libopenvr_api.so")
        set(OPENVR_LIB_DIR "/usr/lib" CACHE PATH "OpenVR library directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_LIB_DIR}/libopenvr_api.so")
        set(OPENVR_LIB_DIR "/usr/local/lib" CACHE PATH "OpenVR library directory" FORCE)
    endif()
endif()

if(EXISTS "${OPENVR_LIB_DIR}/libopenvr_api.so")
    message(STATUS "Using OpenVR library from: ${OPENVR_LIB_DIR}")
else()
    message(WARNING "OpenVR library not found. The application will try to load it from SteamVR runtime.")
endif()

get_filename_component(PROJECT_ROOT "${CMAKE_SOURCE_DIR}" ABSOLUTE)
get_filename_component(PROJECT_ROOT "${PROJECT_ROOT}" DIRECTORY)

set(EIGEN_BASE_DIR "${PROJECT_ROOT}/WindowsEdition/OpenVR-SpaceCalibrator/lib")
set(EIGEN_DIR "${EIGEN_BASE_DIR}/Eigen")

if(EXISTS "${EIGEN_DIR}/Dense")
    message(STATUS "Using Eigen from: ${EIGEN_BASE_DIR}")
else()
    find_package(Eigen3 QUIET)
    if(Eigen3_FOUND)
        message(STATUS "Using system Eigen3")
    else()
        message(FATAL_ERROR "Eigen3 not found at: ${EIGEN_DIR}\nPlease install Eigen3 for your distribution:\n  Ubuntu/Debian: sudo apt install libeigen3-dev\n  Fedora/RHEL: sudo dnf install eigen3-devel\n  Arch Linux: sudo pacman -S eigen\nOr ensure it's in the Windows Edition lib directory.")
    endif()
endif()

include_directories(
    ${OPENVR_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${PROJECT_ROOT}
    ${EIGEN_BASE_DIR}
)

file(GLOB IMGUI_SOURCES
    "${PROJECT_ROOT}/WindowsEdition/OpenVR-SpaceCalibrator/lib/imgui/*.cpp"
    "${PROJECT_ROOT}/WindowsEdition/OpenVR-SpaceCalibrator/lib/imgui/backends/imgui_impl_glfw.cpp"
    "${PROJECT_ROOT}/WindowsEdition/OpenVR-SpaceCalibrator/lib/imgui/backends/imgui_impl_opengl3.cpp"
)

if(NOT IMGUI_SOURCES)
    message(WARNING "ImGui sources not found at expected location. Trying alternative...")
    file(GLOB IMGUI_SOURCES
        "${PROJECT_ROOT}/lib/imgui/*.cpp"
        "${PROJECT_ROOT}/lib/imgui/backends/imgui_impl_glfw.cpp"
        "${PROJECT_ROOT}/lib/imgui/backends/imgui_impl_opengl3.cpp"
    )
endif()

if(NOT IMGUI_SOURCES)
    message(FATAL_ERROR "ImGui sources not found. Searched:\n  ${PROJECT_ROOT}/WindowsEdition/OpenVR-SpaceCalibrator/lib/imgui/\n  ${PROJECT_ROOT}/lib/imgui/\nPlease ensure ImGui is available.")
endif()

set(SOURCES
    main.cpp
    VRState.cpp
    VRState.h
    Configuration.cpp
    Configuration.h
    Telemetry.cpp
    Telemetry.h
    Calibration.cpp
    Calibration.h
    CalibrationCalc.cpp
    CalibrationCalc.h
    CalibrationMetrics.cpp
    CalibrationMetrics.h
    CalibrationUI.cpp
    CalibrationUI.h
    CalibrationDebug.cpp
    CalibrationDebug.h
    PlayspaceMovement.cpp
    PlayspaceMovement.h
    imgui_extensions.cpp
    imgui_extensions.h
    ProfileManager.cpp
    Logging.cpp
    Logging.h
    IPCClient.cpp
    IPCClient.h
    ${CMAKE_SOURCE_DIR}/src/common/Version.h
    ${CMAKE_SOURCE_DIR}/src/common/Protocol.h
)

add_executable(space-calibrator ${SOURCES} ${IMGUI_SOURCES})

target_include_directories(space-calibrator PRIVATE
    ${OPENVR_INCLUDE_DIR}
    ${PROJECT_ROOT}/WindowsEdition/OpenVR-SpaceCalibrator/lib/imgui
    ${PROJECT_ROOT}/WindowsEdition/OpenVR-SpaceCalibrator/lib/imgui/backends
    ${CMAKE_SOURCE_DIR}/src/common
    ${EIGEN_BASE_DIR}
)

target_link_libraries(space-calibrator
    ${OPENGL_LIBRARIES}
    glfw
    pthread
    dl
    rt
    stdc++fs
)

if(EXISTS "${OPENVR_LIB_DIR}/libopenvr_api.so")
    target_link_directories(space-calibrator PRIVATE ${OPENVR_LIB_DIR})
    target_link_libraries(space-calibrator openvr_api)
    set_target_properties(space-calibrator PROPERTIES
        INSTALL_RPATH "${OPENVR_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

if(LINUX)
    target_link_libraries(space-calibrator
        X11
        Xrandr
        Xinerama
        Xcursor
        Xi
    )
endif()

target_compile_definitions(space-calibrator PRIVATE
    SPACECAL_VERSION_STRING="1.5.1-linux"
)

# Copy manifest and actions.json to build directory
add_custom_command(
    TARGET space-calibrator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/src/overlay/manifest.vrmanifest
    ${CMAKE_BINARY_DIR}/manifest.vrmanifest
    COMMENT "Copying overlay manifest to build directory"
)

add_custom_command(
    TARGET space-calibrator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/src/overlay/actions.json
    ${CMAKE_BINARY_DIR}/actions.json
    COMMENT "Copying actions manifest to build directory"
)

