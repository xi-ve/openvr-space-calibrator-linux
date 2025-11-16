#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

DRIVER_SO="$PROJECT_ROOT/build/lib/driver_01spacecalibrator.so"
OVERLAY_BINARY="$PROJECT_ROOT/build/bin/space-calibrator"
OVERLAY_MANIFEST="$PROJECT_ROOT/build/manifest.vrmanifest"
OVERLAY_ACTIONS="$PROJECT_ROOT/build/actions.json"
DRIVER_DIR="$PROJECT_ROOT/driver_01spacecalibrator"
STEAMVR_DRIVERS_DIR="$HOME/.local/share/SteamVR/drivers/01spacecalibrator"

if [ ! -f "$DRIVER_SO" ]; then
    echo "[-] Driver library not found. Run ./scripts/build.sh first"
    exit 1
fi

if [ ! -f "$OVERLAY_BINARY" ]; then
    echo "[-] Overlay binary not found. Run ./scripts/build.sh first"
    exit 1
fi

if [ ! -f "$OVERLAY_MANIFEST" ]; then
    echo "[-] Overlay manifest not found. Run ./scripts/build.sh first"
    exit 1
fi

if [ ! -d "$DRIVER_DIR" ]; then
    echo "[-] Driver directory not found"
    exit 1
fi

mkdir -p "$STEAMVR_DRIVERS_DIR/bin/linux64"

cp "$DRIVER_SO" "$STEAMVR_DRIVERS_DIR/bin/linux64/driver_01spacecalibrator.so"
cp "$OVERLAY_BINARY" "$STEAMVR_DRIVERS_DIR/bin/linux64/space-calibrator-real"
chmod +x "$STEAMVR_DRIVERS_DIR/bin/linux64/space-calibrator-real"

if [ -f "$OVERLAY_ACTIONS" ]; then
    cp "$OVERLAY_ACTIONS" "$STEAMVR_DRIVERS_DIR/bin/linux64/actions.json"
fi

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

cat > "$STEAMVR_DRIVERS_DIR/bin/linux64/manifest.vrmanifest" << 'MANIFEST_EOF'
{
	"source" : "builtin",
	"applications": [{
		"app_key": "spacecalibrator.linux",
		"launch_type": "binary",
		"binary_path_linux": "space-calibrator",
		"is_dashboard_overlay": true,
		"action_manifest_path": "actions.json",

		"strings": {
			"en_us": {
				"name": "Space Calibrator",
				"description": "Space Calibrator Overlay"
			}
		}
	}]
}
MANIFEST_EOF

cat > "$STEAMVR_DRIVERS_DIR/driver.vrdrivermanifest" << 'MANIFEST_EOF'
{
	"alwaysActivate": true,
	"name" : "01spacecalibrator",
	"directory" : "",
	"resourceOnly" : false,
	"activateOtherDriversWhenEnabled" : true
}
MANIFEST_EOF

cp -r "$DRIVER_DIR/resources" "$STEAMVR_DRIVERS_DIR/"

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
    OPENVR_LIB="$(dirname "$VRPATHREG" 2>/dev/null || echo "")"
    if [ -n "$OPENVR_LIB" ] && [ "$OPENVR_LIB" != "." ]; then
        if LD_LIBRARY_PATH="$OPENVR_LIB:$LD_LIBRARY_PATH" "$VRPATHREG" adddriver "$STEAMVR_DRIVERS_DIR" 2>/dev/null; then
            echo "[+] Driver registered"
        else
            echo "[?] Driver registration failed (may already be registered)"
        fi
        
        OVERLAY_MANIFEST_PATH="$STEAMVR_DRIVERS_DIR/bin/linux64/manifest.vrmanifest"
        echo "[+] Registering overlay application manifest..."
        
        APP_CONFIG="$HOME/.local/share/Steam/config/appconfig.json"
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
                    echo "[+] Overlay application registered"
                else
                    echo "[?] Overlay application registration failed"
                fi
            else
                echo "[+] Overlay application already registered"
            fi
        else
            echo "[?] appconfig.json not found - overlay will register itself on first launch"
        fi
    else
        if "$VRPATHREG" adddriver "$STEAMVR_DRIVERS_DIR" 2>/dev/null; then
            echo "[+] Driver registered"
        else
            echo "[?] Driver registration failed (may already be registered)"
        fi
        
        OVERLAY_MANIFEST_PATH="$STEAMVR_DRIVERS_DIR/bin/linux64/manifest.vrmanifest"
        echo "[+] Registering overlay application manifest..."
        
        APP_CONFIG="$HOME/.local/share/Steam/config/appconfig.json"
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
                    echo "[+] Overlay application registered"
                else
                    echo "[?] Overlay application registration failed"
                fi
            else
                echo "[+] Overlay application already registered"
            fi
        else
            echo "[?] appconfig.json not found - overlay will register itself on first launch"
        fi
    fi
else
    echo "[?] vrpathreg not found - manual registration may be required"
fi

echo "[+] Installation complete"
echo "[?] Restart SteamVR to activate"
