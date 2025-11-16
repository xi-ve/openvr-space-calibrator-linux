find_package(PkgConfig REQUIRED)

get_filename_component(PROJECT_ROOT "${CMAKE_SOURCE_DIR}" ABSOLUTE)
get_filename_component(PROJECT_ROOT "${PROJECT_ROOT}" DIRECTORY)

if(NOT DEFINED OPENVR_INCLUDE_DIR)
    set(OPENVR_INCLUDE_DIR "${PROJECT_ROOT}/lib/openvr/headers" CACHE PATH "OpenVR include directory")
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr_driver.h")
        set(OPENVR_INCLUDE_DIR "${PROJECT_ROOT}/WindowsEdition/OpenVR-SpaceCalibrator/lib/openvr/headers" CACHE PATH "OpenVR include directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr_driver.h")
        set(OPENVR_INCLUDE_DIR "$ENV{HOME}/.local/share/Steam/steamapps/common/SteamVR/headers" CACHE PATH "OpenVR include directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr_driver.h")
        set(OPENVR_INCLUDE_DIR "$ENV{HOME}/.steam/steam/steamapps/common/SteamVR/headers" CACHE PATH "OpenVR include directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr_driver.h")
        set(OPENVR_INCLUDE_DIR "$ENV{HOME}/.steam/root/steamapps/common/SteamVR/headers" CACHE PATH "OpenVR include directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr_driver.h")
        set(OPENVR_INCLUDE_DIR "/usr/include/openvr" CACHE PATH "OpenVR include directory" FORCE)
    endif()
    
    if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr_driver.h")
        set(OPENVR_INCLUDE_DIR "/usr/local/include/openvr" CACHE PATH "OpenVR include directory" FORCE)
    endif()
endif()

if(NOT EXISTS "${OPENVR_INCLUDE_DIR}/openvr_driver.h")
    message(FATAL_ERROR "OpenVR driver headers not found. Please install OpenVR SDK or set OPENVR_INCLUDE_DIR to the correct path.")
endif()

message(STATUS "Using OpenVR driver headers from: ${OPENVR_INCLUDE_DIR}")

set(EIGEN_BASE_DIR "${PROJECT_ROOT}/WindowsEdition/OpenVR-SpaceCalibrator/lib")
set(EIGEN_DIR "${EIGEN_BASE_DIR}/Eigen")

if(EXISTS "${EIGEN_DIR}/Dense")
    include_directories("${EIGEN_BASE_DIR}")
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
    ${CMAKE_SOURCE_DIR}/../common
    ${CMAKE_SOURCE_DIR}
    ${PROJECT_ROOT}
    ${EIGEN_BASE_DIR}
)

set(DRIVER_SOURCES
    OpenVR-SpaceCalibratorDriver.cpp
    ServerTrackedDeviceProvider.cpp
    IPCServer.cpp
    InterfaceHookInjector.cpp
    Hooking.cpp
    Logging.cpp
)

add_library(openvr_spacecalibrator_driver SHARED ${DRIVER_SOURCES})

target_include_directories(openvr_spacecalibrator_driver PRIVATE
    ${OPENVR_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/../common
    ${CMAKE_SOURCE_DIR}
    ${EIGEN_BASE_DIR}
)

target_link_libraries(openvr_spacecalibrator_driver
    pthread
    rt
    dl
)

set_target_properties(openvr_spacecalibrator_driver PROPERTIES
    PREFIX ""
    OUTPUT_NAME "driver_01spacecalibrator"
)

target_compile_definitions(openvr_spacecalibrator_driver PRIVATE
    SPACECAL_VERSION_STRING="1.5.1-linux"
)

