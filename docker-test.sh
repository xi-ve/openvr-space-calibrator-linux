#!/bin/bash

set -e

echo "Docker test script for Debian/PopOS build"
echo "Usage:"
echo "  ./docker-test.sh                                    # Test with git submodule ImGui"
echo "  TEST_SYSTEM_IMGUI=true ./docker-test.sh            # Test with system ImGui detection"
echo "  INSTALL_OPENVR=true ./docker-test.sh               # Also install OpenVR SDK (for full build)"
echo "  GIT_REPO_URL=<url> GIT_BRANCH=<branch> ./docker-test.sh  # Test specific repo/branch"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR" && pwd)"

GIT_REPO_URL="${GIT_REPO_URL:-https://github.com/xi-ve/OpenVR-SpaceCalibrator.git}"
GIT_BRANCH="${GIT_BRANCH:-main}"
DOCKER_IMAGE_NAME="${DOCKER_IMAGE_NAME:-space-calibrator-debian-test}"

TEST_SYSTEM_IMGUI="${TEST_SYSTEM_IMGUI:-false}"
INSTALL_OPENVR="${INSTALL_OPENVR:-false}"

echo "[?] Building Docker image: ${DOCKER_IMAGE_NAME}"
echo "[?] Testing system ImGui detection: ${TEST_SYSTEM_IMGUI}"
echo "[?] Installing OpenVR SDK: ${INSTALL_OPENVR}"
docker build \
    -f "${PROJECT_ROOT}/Dockerfile.debian-test" \
    --build-arg GIT_REPO_URL="${GIT_REPO_URL}" \
    --build-arg GIT_BRANCH="${GIT_BRANCH}" \
    --build-arg TEST_SYSTEM_IMGUI="${TEST_SYSTEM_IMGUI}" \
    --build-arg INSTALL_OPENVR="${INSTALL_OPENVR}" \
    -t "${DOCKER_IMAGE_NAME}" \
    "${PROJECT_ROOT}/.."

echo "[?] Running build test in container..."
BUILD_DIR="${PROJECT_ROOT}/build"
mkdir -p "${BUILD_DIR}"
docker run --rm \
    -v "${BUILD_DIR}:/workspace/src/LinuxEditionSpaceClibrator/build" \
    "${DOCKER_IMAGE_NAME}"

echo "[+] Build test complete!"
echo "[?] Build artifacts are in: ${PROJECT_ROOT}/build"
