#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

STEAMVR_DRIVERS_DIR="$HOME/.local/share/SteamVR/drivers/01spacecalibrator"

if [ ! -d "$STEAMVR_DRIVERS_DIR" ]; then
    echo "[?] Driver not found - may not be installed"
    exit 0
fi

VRPATHREG=""
STEAMVR_PATHS=(
    "$HOME/.local/share/Steam/steamapps/common/SteamVR/bin/linux64/vrpathreg"
    "$HOME/.steam/steam/steamapps/common/SteamVR/bin/linux64/vrpathreg"
    "$HOME/.steam/root/steamapps/common/SteamVR/bin/linux64/vrpathreg"
    "/usr/local/bin/vrpathreg"
    "/usr/bin/vrpathreg"
)

for path in "${STEAMVR_PATHS[@]}"; do
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
        OPENVR_LIB="$(dirname "$VRPATHREG" 2>/dev/null || echo "")"
    fi
    
    if [ -n "$OPENVR_LIB" ] && [ "$OPENVR_LIB" != "." ]; then
        export LD_LIBRARY_PATH="$OPENVR_LIB:$LD_LIBRARY_PATH"
        if "$VRPATHREG" removedriver "$STEAMVR_DRIVERS_DIR" 2>/dev/null; then
            echo "[+] Driver unregistered"
        else
            echo "[?] Driver unregistration failed (may not have been registered)"
        fi
    else
        if "$VRPATHREG" removedriver "$STEAMVR_DRIVERS_DIR" 2>/dev/null; then
            echo "[+] Driver unregistered"
        else
            echo "[?] Driver unregistration failed (may not have been registered)"
        fi
    fi
fi

OVERLAY_MANIFEST="$STEAMVR_DRIVERS_DIR/bin/linux64/manifest.vrmanifest"
if [ -f "$OVERLAY_MANIFEST" ] && [ -n "$VRPATHREG" ]; then
    if [ -n "$OPENVR_LIB" ] && [ "$OPENVR_LIB" != "." ]; then
        if LD_LIBRARY_PATH="$OPENVR_LIB:$LD_LIBRARY_PATH" "$VRPATHREG" removedriver "$(dirname "$OVERLAY_MANIFEST")" 2>/dev/null; then
            echo "[+] Overlay unregistered"
        fi
    else
        if "$VRPATHREG" removedriver "$(dirname "$OVERLAY_MANIFEST")" 2>/dev/null; then
            echo "[+] Overlay unregistered"
        fi
    fi
fi

if [ -d "$STEAMVR_DRIVERS_DIR" ]; then
    rm -rf "$STEAMVR_DRIVERS_DIR"
    echo "[+] Files removed"
fi

echo "[+] Uninstallation complete"
echo "[?] Restart SteamVR to complete"
