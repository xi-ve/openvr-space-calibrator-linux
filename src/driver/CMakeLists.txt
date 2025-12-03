find_package(PkgConfig REQUIRED)

get_filename_component(PROJECT_ROOT "${CMAKE_SOURCE_DIR}" ABSOLUTE)
get_filename_component(PROJECT_ROOT "${PROJECT_ROOT}" DIRECTORY)

if(NOT DEFINED OPENVR_INCLUDE_DIR)
    set(OPENVR_INCLUDE_DIR "${PROJECT_ROOT}/lib/openvr/headers" CACHE PATH "OpenVR include directory")
    
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

if(EIGEN_BASE_DIR)
    include_directories("${EIGEN_BASE_DIR}")
endif()

include_directories(
    ${OPENVR_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/../common
    ${CMAKE_SOURCE_DIR}
    ${PROJECT_ROOT}
)

if(EIGEN_BASE_DIR)
    include_directories(${EIGEN_BASE_DIR})
endif()

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
)

if(EIGEN_BASE_DIR)
    target_include_directories(openvr_spacecalibrator_driver PRIVATE ${EIGEN_BASE_DIR})
elseif(EIGEN_USE_TARGET)
    target_link_libraries(openvr_spacecalibrator_driver PRIVATE Eigen3::Eigen)
endif()

target_link_libraries(openvr_spacecalibrator_driver PRIVATE
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

