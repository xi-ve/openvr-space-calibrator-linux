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

set(EIGEN_BASE_DIR "")
set(EIGEN_USE_TARGET FALSE)

file(GLOB EIGEN_LOCAL_DIR "${PROJECT_ROOT}/lib/Eigen")
if(EIGEN_LOCAL_DIR AND EXISTS "${EIGEN_LOCAL_DIR}/Dense")
    set(EIGEN_BASE_DIR "${PROJECT_ROOT}/lib")
    message(STATUS "Using Eigen from: ${EIGEN_BASE_DIR}")
else()
    find_package(Eigen3 QUIET)
    if(Eigen3_FOUND)
        if(TARGET Eigen3::Eigen)
            set(EIGEN_USE_TARGET TRUE)
            message(STATUS "Using system Eigen3 via CMake target")
        elseif(EIGEN3_INCLUDE_DIR)
            set(EIGEN_BASE_DIR "${EIGEN3_INCLUDE_DIR}")
            message(STATUS "Using system Eigen3 from: ${EIGEN_BASE_DIR}")
        else()
            if(EXISTS "/usr/include/eigen3/Eigen/Dense")
                set(EIGEN_BASE_DIR "/usr/include/eigen3")
                message(STATUS "Using system Eigen3 from: ${EIGEN_BASE_DIR}")
            elseif(EXISTS "/usr/local/include/eigen3/Eigen/Dense")
                set(EIGEN_BASE_DIR "/usr/local/include/eigen3")
                message(STATUS "Using system Eigen3 from: ${EIGEN_BASE_DIR}")
            else()
                message(FATAL_ERROR "Eigen3 package found but headers not located. Please install Eigen3 for your distribution:\n  Ubuntu/Debian: sudo apt install libeigen3-dev\n  Fedora/RHEL: sudo dnf install eigen3-devel\n  Arch Linux: sudo pacman -S eigen")
            endif()
        endif()
    else()
        message(FATAL_ERROR "Eigen3 not found. Please install Eigen3 for your distribution:\n  Ubuntu/Debian: sudo apt install libeigen3-dev\n  Fedora/RHEL: sudo dnf install eigen3-devel\n  Arch Linux: sudo pacman -S eigen")
    endif()
endif()

include_directories(
    ${OPENVR_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${PROJECT_ROOT}
)

if(EIGEN_BASE_DIR)
    include_directories(${EIGEN_BASE_DIR})
endif()

set(IMGUI_DIR "")
set(IMGUI_SOURCES "")
set(IMGUI_INCLUDE_DIRS "")

file(GLOB IMGUI_SOURCES
    "${PROJECT_ROOT}/lib/imgui/*.cpp"
    "${PROJECT_ROOT}/lib/imgui/backends/imgui_impl_glfw.cpp"
    "${PROJECT_ROOT}/lib/imgui/backends/imgui_impl_opengl3.cpp"
)

if(IMGUI_SOURCES)
    set(IMGUI_DIR "${PROJECT_ROOT}/lib/imgui")
    message(STATUS "Using ImGui from: ${IMGUI_DIR}")
endif()

if(NOT IMGUI_SOURCES)
    pkg_check_modules(IMGUI_PKG imgui QUIET)
    if(IMGUI_PKG_FOUND)
        set(IMGUI_DIR "${IMGUI_PKG_INCLUDE_DIRS}")
        message(STATUS "Found ImGui via pkg-config: ${IMGUI_DIR}")
    else()
        if(EXISTS "/usr/include/imgui/imgui.h")
            set(IMGUI_DIR "/usr/include/imgui")
        elseif(EXISTS "/usr/local/include/imgui/imgui.h")
            set(IMGUI_DIR "/usr/local/include/imgui")
        elseif(EXISTS "/usr/include/imgui.h")
            set(IMGUI_DIR "/usr/include")
        elseif(EXISTS "/usr/local/include/imgui.h")
            set(IMGUI_DIR "/usr/local/include")
        endif()
    endif()

    if(IMGUI_DIR)
        if(EXISTS "${IMGUI_DIR}/imgui.cpp")
            file(GLOB IMGUI_SOURCES
                "${IMGUI_DIR}/*.cpp"
                "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
                "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
            )
        elseif(EXISTS "/usr/share/imgui/imgui.cpp")
            set(IMGUI_DIR "/usr/share/imgui")
            file(GLOB IMGUI_SOURCES
                "${IMGUI_DIR}/*.cpp"
                "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
                "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
            )
        elseif(EXISTS "/usr/src/imgui/imgui.cpp")
            set(IMGUI_DIR "/usr/src/imgui")
            file(GLOB IMGUI_SOURCES
                "${IMGUI_DIR}/*.cpp"
                "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
                "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
            )
        endif()

        if(IMGUI_SOURCES)
            message(STATUS "Using system ImGui sources from: ${IMGUI_DIR}")
        else()
            set(IMGUI_DIR "")
        endif()
    endif()
endif()

if(NOT IMGUI_SOURCES)
    message(FATAL_ERROR "ImGui sources not found. Searched:\n  ${PROJECT_ROOT}/lib/imgui/\n  System locations via pkg-config\n  /usr/include/imgui/\n  /usr/local/include/imgui/\n  /usr/share/imgui/\n  /usr/src/imgui/\n\nPlease ensure ImGui is available. Options:\n  1. Clone ImGui as a git submodule:\n     git submodule add https://github.com/ocornut/imgui.git lib/imgui\n  2. Download ImGui manually to lib/imgui/\n  3. Install a system package that provides ImGui source files")
endif()

if(NOT IMGUI_DIR)
    message(FATAL_ERROR "ImGui directory not set despite sources being found. This is a build system error.")
endif()

set(IMGUI_INCLUDE_DIRS "${IMGUI_DIR}" "${IMGUI_DIR}/backends")

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
    ${IMGUI_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src/common
)

if(EIGEN_BASE_DIR)
    target_include_directories(space-calibrator PRIVATE ${EIGEN_BASE_DIR})
elseif(EIGEN_USE_TARGET)
    target_link_libraries(space-calibrator PRIVATE Eigen3::Eigen)
endif()

target_link_libraries(space-calibrator PRIVATE
    ${OPENGL_LIBRARIES}
    glfw
    pthread
    dl
    rt
    stdc++fs
)

if(EXISTS "${OPENVR_LIB_DIR}/libopenvr_api.so")
    target_link_directories(space-calibrator PRIVATE ${OPENVR_LIB_DIR})
    target_link_libraries(space-calibrator PRIVATE openvr_api)
    set_target_properties(space-calibrator PROPERTIES
        INSTALL_RPATH "${OPENVR_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

if(LINUX)
    target_link_libraries(space-calibrator PRIVATE
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

