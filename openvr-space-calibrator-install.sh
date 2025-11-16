#!/bin/bash

set -e

USER_HOME="${1:-$HOME}"

if [ "$USER_HOME" = "/root" ] && [ -n "$SUDO_USER" ]; then
    USER_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
fi

if [ "$USER_HOME" = "/root" ] && [ -z "$SUDO_USER" ]; then
    REAL_USER=$(who am i | awk '{print $1}')
    if [ -n "$REAL_USER" ]; then
        USER_HOME=$(getent passwd "$REAL_USER" | cut -d: -f6)
    fi
fi

STEAMVR_DRIVERS_DIR="$USER_HOME/.local/share/SteamVR/drivers/01spacecalibrator"
PKG_DIR="/usr/lib/openvr-space-calibrator-linux"

if [ ! -d "$PKG_DIR" ]; then
    echo "Error: Package files not found at $PKG_DIR"
    echo "Please ensure openvr-space-calibrator-linux is installed."
    exit 1
fi

mkdir -p "$STEAMVR_DRIVERS_DIR/bin/linux64"

echo "Installing Space Calibrator to SteamVR..."
cp "$PKG_DIR/driver_01spacecalibrator.so" "$STEAMVR_DRIVERS_DIR/bin/linux64/"
cp "$PKG_DIR/manifest.vrmanifest" "$STEAMVR_DRIVERS_DIR/bin/linux64/"
cp "$PKG_DIR/driver.vrdrivermanifest" "$STEAMVR_DRIVERS_DIR/"
cp -r "$PKG_DIR/resources" "$STEAMVR_DRIVERS_DIR/"

VRPATHREG=""
for path in \
    "$USER_HOME/.local/share/Steam/steamapps/common/SteamVR/bin/linux64/vrpathreg" \
    "$USER_HOME/.steam/steam/steamapps/common/SteamVR/bin/linux64/vrpathreg" \
    "$USER_HOME/.steam/root/steamapps/common/SteamVR/bin/linux64/vrpathreg" \
    "/usr/local/bin/vrpathreg" \
    "/usr/bin/vrpathreg"; do
    if [ -f "$path" ]; then
        VRPATHREG="$path"
        break
    fi
done

if [ -z "$VRPATHREG" ] && command -v vrpathreg &> /dev/null; then
    VRPATHREG="vrpathreg"
fi

if [ -n "$VRPATHREG" ]; then
    OPENVR_LIB=""
    if [ -f "$VRPATHREG" ]; then
        OPENVR_LIB="$(dirname "$VRPATHREG")"
    fi
    
    if [ -n "$OPENVR_LIB" ] && [ "$OPENVR_LIB" != "." ]; then
        if LD_LIBRARY_PATH="$OPENVR_LIB:$LD_LIBRARY_PATH" "$VRPATHREG" adddriver "$STEAMVR_DRIVERS_DIR" 2>/dev/null; then
            echo "Driver registered with SteamVR"
        else
            echo "Note: Driver registration failed (may already be registered)"
        fi
    else
        if "$VRPATHREG" adddriver "$STEAMVR_DRIVERS_DIR" 2>/dev/null; then
            echo "Driver registered with SteamVR"
        else
            echo "Note: Driver registration failed (may already be registered)"
        fi
    fi
else
    echo "Warning: vrpathreg not found - manual registration may be required"
fi

echo "Installation complete!"
echo "Restart SteamVR to activate."

