#!/bin/bash

set -e

echo "Docker test script for local source build (Debian/PopOS)"
echo "This builds from your local source directory instead of cloning from git"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

DOCKER_IMAGE_NAME="${DOCKER_IMAGE_NAME:-space-calibrator-debian-test-local}"

echo "[?] Building Docker image from local source: ${DOCKER_IMAGE_NAME}"
docker build \
    -f "${SCRIPT_DIR}/Dockerfile.debian-test-local" \
    -t "${DOCKER_IMAGE_NAME}" \
    "${PROJECT_ROOT}"

echo "[?] Running build test in container..."
docker run --rm \
    -v "${SCRIPT_DIR}/build:/workspace/src/LinuxEditionSpaceClibrator/build" \
    "${DOCKER_IMAGE_NAME}"

echo "[+] Build test complete!"
echo "[?] Build artifacts are in: ${SCRIPT_DIR}/build"
