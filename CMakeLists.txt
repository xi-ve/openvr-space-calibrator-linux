cmake_minimum_required(VERSION 3.10)
project(SpaceCalibratorLinux VERSION 1.0.0)

cmake_policy(SET CMP0072 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if(LINUX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(src)

find_path(OPENVR_INCLUDE_DIR openvr.h
    PATHS
        ${CMAKE_SOURCE_DIR}/../lib/openvr/headers
        ${CMAKE_SOURCE_DIR}/../WindowsEdition/OpenVR-SpaceCalibrator/lib/openvr/headers
        $ENV{HOME}/.local/share/Steam/steamapps/common/SteamVR/headers
        $ENV{HOME}/.steam/steam/steamapps/common/SteamVR/headers
        $ENV{HOME}/.steam/root/steamapps/common/SteamVR/headers
        /usr/include/openvr
        /usr/local/include/openvr
    NO_DEFAULT_PATH
)

if(NOT OPENVR_INCLUDE_DIR)
    find_path(OPENVR_INCLUDE_DIR openvr.h)
endif()

if(OPENVR_LIB_DIR)
    set(OPENVR_LIB "${OPENVR_LIB_DIR}/libopenvr_api.so")
    if(EXISTS "${OPENVR_LIB}")
        message(STATUS "Using OpenVR library from: ${OPENVR_LIB}")
        set(OPENVR_LIB_FOUND TRUE)
    else()
        unset(OPENVR_LIB)
        set(OPENVR_LIB_FOUND FALSE)
    endif()
else()
    set(OPENVR_LIB_FOUND FALSE)
endif()

if(NOT OPENVR_LIB_FOUND)
    find_library(OPENVR_LIB openvr_api
        PATHS
            ${CMAKE_SOURCE_DIR}/../lib/openvr/bin/linux64
            $ENV{HOME}/.local/share/Steam/steamapps/common/SteamVR/bin/linux64
            $ENV{HOME}/.steam/steam/steamapps/common/SteamVR/bin/linux64
            $ENV{HOME}/.steam/root/steamapps/common/SteamVR/bin/linux64
            /usr/lib
            /usr/local/lib
        NO_DEFAULT_PATH
    )
    if(OPENVR_LIB)
        set(OPENVR_LIB_FOUND TRUE)
        message(STATUS "Found OpenVR library via find_library: ${OPENVR_LIB}")
    endif()
endif()

if(NOT OPENVR_LIB_FOUND)
    find_library(OPENVR_LIB openvr_api)
    if(OPENVR_LIB)
        set(OPENVR_LIB_FOUND TRUE)
        message(STATUS "Found OpenVR library via system search: ${OPENVR_LIB}")
    endif()
endif()

if(OPENVR_INCLUDE_DIR AND OPENVR_LIB_FOUND AND OPENVR_LIB)
    if(EXISTS "${OPENVR_LIB}")
        add_executable(register-overlay register-overlay.cpp)
        target_include_directories(register-overlay PRIVATE ${OPENVR_INCLUDE_DIR})
        if(IS_ABSOLUTE "${OPENVR_LIB}" AND "${OPENVR_LIB}" MATCHES "\\.so$")
            target_link_libraries(register-overlay "${OPENVR_LIB}")
        elseif(IS_ABSOLUTE "${OPENVR_LIB}")
            get_filename_component(OPENVR_LIB_DIR "${OPENVR_LIB}" DIRECTORY)
            get_filename_component(OPENVR_LIB_NAME "${OPENVR_LIB}" NAME)
            string(REGEX REPLACE "^lib" "" OPENVR_LIB_NAME "${OPENVR_LIB_NAME}")
            string(REGEX REPLACE "\\.so.*$" "" OPENVR_LIB_NAME "${OPENVR_LIB_NAME}")
            target_link_directories(register-overlay PRIVATE ${OPENVR_LIB_DIR})
            target_link_libraries(register-overlay ${OPENVR_LIB_NAME})
        else()
            target_link_libraries(register-overlay ${OPENVR_LIB})
        endif()
        set_target_properties(register-overlay PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            EXCLUDE_FROM_ALL TRUE
        )
        message(STATUS "Building register-overlay utility with OpenVR library: ${OPENVR_LIB}")
    else()
        message(WARNING "OpenVR library path does not exist: ${OPENVR_LIB}")
        message(STATUS "register-overlay will not be built (overlay can still register itself on first run).")
    endif()
elseif(OPENVR_INCLUDE_DIR)
    message(STATUS "OpenVR headers found, but library not found at build time.")
    message(STATUS "register-overlay will not be built (overlay can still register itself on first run).")
    message(STATUS "To build register-overlay, ensure libopenvr_api.so is available.")
else()
    message(STATUS "OpenVR headers not found. register-overlay will not be built.")
    message(STATUS "This is optional - the overlay will register itself on first run.")
endif()

