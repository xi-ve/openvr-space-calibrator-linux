#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CLEAN_BUILD=true

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-clean|--incremental|-i)
            CLEAN_BUILD=false
            shift
            ;;
        *)
            shift
            ;;
    esac
done

cd "$PROJECT_ROOT"

if [ "$CLEAN_BUILD" = true ]; then
    echo "[?] Cleaning build directory..."
    rm -rf build
fi

if ! command -v cmake &> /dev/null; then
    echo "[-] CMake not found"
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo "[-] Make not found"
    exit 1
fi

MISSING_DEPS=()
if ! command -v pkg-config &> /dev/null; then
    echo "[-] pkg-config not found. Please install it for your distribution:"
    echo "    Ubuntu/Debian: sudo apt install pkg-config"
    echo "    Fedora/RHEL: sudo dnf install pkgconfig"
    echo "    Arch Linux: sudo pacman -S pkgconf"
    exit 1
fi

if ! pkg-config --exists glfw3 2>/dev/null; then
    MISSING_DEPS+=("glfw3 (Ubuntu/Debian: libglfw3-dev, Fedora/RHEL: glfw-devel, Arch: glfw)")
fi
if ! pkg-config --exists gl 2>/dev/null; then
    MISSING_DEPS+=("OpenGL (Ubuntu/Debian: libgl1-mesa-dev, Fedora/RHEL: mesa-libGL-devel, Arch: mesa)")
fi

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo "[-] Missing dependencies:"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "    - $dep"
    done
    exit 1
fi

if [ ! -d "build" ]; then
    mkdir build
fi

cd build

if [ -f "CMakeCache.txt" ]; then
    CACHED_SOURCE=$(grep "^CMAKE_HOME_DIRECTORY:" CMakeCache.txt 2>/dev/null | cut -d "=" -f 2 | tr -d '\n' || echo "")
    CURRENT_SOURCE="$(cd .. && pwd)"
    if [ -n "$CACHED_SOURCE" ] && [ "$CACHED_SOURCE" != "$CURRENT_SOURCE" ]; then
        echo "[?] Removing stale CMakeCache.txt (source directory changed)..."
        rm -f CMakeCache.txt CMakeFiles/ 2>/dev/null || true
    fi
fi

OPENVR_HEADERS=""
if [ -f "$PROJECT_ROOT/../lib/openvr/headers/openvr.h" ]; then
    OPENVR_HEADERS="$PROJECT_ROOT/../lib/openvr/headers"
elif [ -f "$PROJECT_ROOT/../WindowsEdition/OpenVR-SpaceCalibrator/lib/openvr/headers/openvr.h" ]; then
    OPENVR_HEADERS="$PROJECT_ROOT/../WindowsEdition/OpenVR-SpaceCalibrator/lib/openvr/headers"
elif [ -f "$HOME/.local/share/Steam/steamapps/common/SteamVR/headers/openvr.h" ]; then
    OPENVR_HEADERS="$HOME/.local/share/Steam/steamapps/common/SteamVR/headers"
elif [ -f "$HOME/.steam/steam/steamapps/common/SteamVR/headers/openvr.h" ]; then
    OPENVR_HEADERS="$HOME/.steam/steam/steamapps/common/SteamVR/headers"
elif [ -f "/usr/include/openvr/openvr.h" ]; then
    OPENVR_HEADERS="/usr/include/openvr"
elif [ -f "/usr/local/include/openvr/openvr.h" ]; then
    OPENVR_HEADERS="/usr/local/include/openvr"
fi

CMAKE_ARGS=""
if [ -n "$OPENVR_HEADERS" ]; then
    CMAKE_ARGS="-DOPENVR_INCLUDE_DIR=\"$OPENVR_HEADERS\""
fi

echo "[?] Configuring CMake..."
if ! eval "cmake .. $CMAKE_ARGS"; then
    echo "[-] CMake configuration failed"
    exit 1
fi

NUM_JOBS=1
if command -v nproc &> /dev/null; then
    NUM_JOBS=$(nproc)
elif [ -f /proc/cpuinfo ]; then
    NUM_JOBS=$(grep -c processor /proc/cpuinfo 2>/dev/null || echo 1)
fi

if ! make -j"$NUM_JOBS"; then
    echo "[-] Build failed"
    exit 1
fi

BUILD_DIR="${PROJECT_ROOT}/build"
cp "${PROJECT_ROOT}/manifest.vrmanifest" "${BUILD_DIR}/manifest.vrmanifest" 2>/dev/null || true

echo "[+] Build complete"
