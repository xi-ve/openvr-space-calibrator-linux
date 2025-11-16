#!/bin/bash

set -e

USER_HOME="${1:-$HOME}"

if [ -z "$USER_HOME" ] || [ "$USER_HOME" = "/root" ]; then
    if [ -n "$SUDO_USER" ]; then
        USER_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
    elif [ -n "$REAL_USER" ]; then
        USER_HOME=$(getent passwd "$REAL_USER" | cut -d: -f6)
    else
        REAL_USER=$(who am i 2>/dev/null | awk '{print $1}' || echo "")
        if [ -n "$REAL_USER" ]; then
            USER_HOME=$(getent passwd "$REAL_USER" | cut -d: -f6)
        fi
    fi
fi

if [ -z "$USER_HOME" ] || [ "$USER_HOME" = "/root" ]; then
    echo "Error: Could not determine user home directory. Please run as: openvr-space-calibrator-install /home/username" >&2
    exit 1
fi

if [ ! -d "$USER_HOME" ]; then
    echo "Error: User home directory does not exist: $USER_HOME" >&2
    exit 1
fi

STEAMVR_DRIVERS_DIR="$USER_HOME/.local/share/SteamVR/drivers/01spacecalibrator"
PKG_DIR="/usr/lib/openvr-space-calibrator-linux"

if [ ! -d "$PKG_DIR" ]; then
    echo "Error: Package files not found at $PKG_DIR"
    echo "Please ensure openvr-space-calibrator-linux is installed."
    exit 1
fi

mkdir -p "$STEAMVR_DRIVERS_DIR/bin/linux64"

echo "Installing Space Calibrator to SteamVR for $USER_HOME..."

OWNER_USER=$(stat -c '%U' "$USER_HOME" 2>/dev/null || echo "")
OWNER_GROUP=$(stat -c '%G' "$USER_HOME" 2>/dev/null || echo "")

cp "$PKG_DIR/driver_01spacecalibrator.so" "$STEAMVR_DRIVERS_DIR/bin/linux64/"
cp /usr/bin/space-calibrator "$STEAMVR_DRIVERS_DIR/bin/linux64/space-calibrator-real"
chmod +x "$STEAMVR_DRIVERS_DIR/bin/linux64/space-calibrator-real"

cat > "$STEAMVR_DRIVERS_DIR/bin/linux64/space-calibrator" << 'WRAPPER_EOF'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STEAMVR_LIB_DIR=""
for lib_dir in "$HOME/.local/share/Steam/steamapps/common/SteamVR/bin/linux64" \
               "$HOME/.steam/steam/steamapps/common/SteamVR/bin/linux64" \
               "$HOME/.steam/root/steamapps/common/SteamVR/bin/linux64"; do
    if [ -f "$lib_dir/libopenvr_api.so" ]; then
        STEAMVR_LIB_DIR="$lib_dir"
        break
    fi
done
export LD_LIBRARY_PATH="$SCRIPT_DIR:$STEAMVR_LIB_DIR:$LD_LIBRARY_PATH"
exec "$SCRIPT_DIR/space-calibrator-real" "$@"
WRAPPER_EOF
chmod +x "$STEAMVR_DRIVERS_DIR/bin/linux64/space-calibrator"

if [ -f "$PKG_DIR/manifest.vrmanifest" ] && [ -s "$PKG_DIR/manifest.vrmanifest" ]; then
    cp "$PKG_DIR/manifest.vrmanifest" "$STEAMVR_DRIVERS_DIR/bin/linux64/"
else
    cat > "$STEAMVR_DRIVERS_DIR/bin/linux64/manifest.vrmanifest" << 'MANIFEST_EOF'
{
	"source" : "builtin",
	"applications": [{
		"app_key": "spacecalibrator.linux",
		"launch_type": "binary",
		"binary_path_linux": "space-calibrator",
		"is_dashboard_overlay": true,

		"strings": {
			"en_us": {
				"name": "Space Calibrator",
				"description": "Space Calibrator Overlay"
			}
		}
	}]
}
MANIFEST_EOF
fi

cat > "$STEAMVR_DRIVERS_DIR/driver.vrdrivermanifest" << 'DRIVER_MANIFEST_EOF'
{
	"alwaysActivate": true,
	"name" : "01spacecalibrator",
	"directory" : "",
	"resourceOnly" : false,
	"activateOtherDriversWhenEnabled" : true
}
DRIVER_MANIFEST_EOF
cp -r "$PKG_DIR/resources" "$STEAMVR_DRIVERS_DIR/"

if [ -n "$OWNER_USER" ] && [ "$(id -u)" -eq 0 ]; then
    chown -R "$OWNER_USER:$OWNER_GROUP" "$STEAMVR_DRIVERS_DIR" 2>/dev/null || true
fi

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

OVERLAY_MANIFEST_PATH="$STEAMVR_DRIVERS_DIR/bin/linux64/manifest.vrmanifest"
echo ""
echo "Registering overlay application manifest..."

if [ -n "$VRPATHREG" ] && [ -f "$OVERLAY_MANIFEST_PATH" ]; then
    APP_CONFIG="$USER_HOME/.local/share/Steam/config/appconfig.json"
    if [ -f "$APP_CONFIG" ]; then
        if ! grep -q "$OVERLAY_MANIFEST_PATH" "$APP_CONFIG" 2>/dev/null; then
            python3 << PYEOF
import json
import os

config_path = "$APP_CONFIG"
manifest_path = "$OVERLAY_MANIFEST_PATH"

try:
    with open(config_path, 'r') as f:
        config = json.load(f)
    
    if 'manifest_paths' not in config:
        config['manifest_paths'] = []
    
    if manifest_path not in config['manifest_paths']:
        config['manifest_paths'].append(manifest_path)
        with open(config_path, 'w') as f:
            json.dump(config, f, indent=4)
        print("Overlay manifest added to appconfig.json")
    else:
        print("Overlay manifest already in appconfig.json")
except Exception as e:
    print(f"Failed to update appconfig.json: {e}")
    exit(1)
PYEOF
            if [ $? -eq 0 ]; then
                echo "Overlay application registered"
            else
                echo "Note: Overlay application registration failed"
            fi
        else
            echo "Overlay application already registered"
        fi
    else
        echo "Note: appconfig.json not found - overlay will register itself on first launch"
    fi
else
    echo "Note: Overlay application will register itself on first launch"
fi

echo ""
echo "Installation complete!"
echo "Restart SteamVR to activate the driver and overlay."

