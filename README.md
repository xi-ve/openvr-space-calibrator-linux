# Space Calibrator - Linux Edition

This is a Linux port of [OpenVR-SpaceCalibrator](https://github.com/hyblocker/OpenVR-SpaceCalibrator) by [@hyblocker](https://github.com/hyblocker). This program allows you to synchronise multiple playspaces with one another in SteamVR, and supports [continuous calibration](#continuous-calibration).

**Linux Port Maintainer:** [@xi-ve](https://github.com/xi-ve)

Continuous calibration is a tracking mode which automatically aligns playspaces together, using a tracker on the headset.

## Installation

### Arch Linux (AUR)

This package is available on the Arch User Repository (AUR):

```bash
yay -S openvr-space-calibrator-linux
```

Or using `makepkg`:

```bash
git clone https://aur.archlinux.org/openvr-space-calibrator-linux.git
cd openvr-space-calibrator-linux
makepkg -si
```

**AUR Package:** [openvr-space-calibrator-linux](https://aur.archlinux.org/packages/openvr-space-calibrator-linux)

The AUR package will automatically:
- Install binaries to `/usr/bin`
- Install the driver and overlay to SteamVR
- Register the overlay with SteamVR on installation (if SteamVR is running)
- Handle updates and uninstallation

**Note:** If SteamVR is not running during installation, you may need to:
1. Launch `space-calibrator` once manually, OR
2. Manually enable the "Space Calibrator" overlay in SteamVR Settings > Startup/Shutdown > Choose Startup Overlay Apps

After the first launch or manual enabling, the overlay will auto-launch with SteamVR on future starts.

### Building from Source

If you prefer to build from source or are using a different distribution, see the [Building](#building) section below.

## Requirements

- Linux (tested on Arch Linux)
- SteamVR installed and configured
- OpenGL 3.2+ support
- C++17 compatible compiler (GCC 7+ or Clang 5+)
- At least one VR device connected (HMD, controller, or tracker)

## Building

### Prerequisites

Install build dependencies:

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y build-essential cmake libglfw3-dev libgl1-mesa-dev \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev pkg-config
```

**Fedora/RHEL:**
```bash
sudo dnf install -y gcc-c++ cmake glfw-devel mesa-libGL-devel \
    libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake glfw-x11 mesa libx11 libxrandr libxinerama libxcursor libxi
```

### Build Steps

1. Clone the repository and navigate to the LinuxEdition directory
2. Build the project:
   ```bash
   ./scripts/build.sh
   ```

This will build both the driver and overlay components.

### Manual Installation (from source)

1. Build the project (see above)
2. Install the driver and overlay:
   ```bash
   ./scripts/install.sh
   ```
3. If SteamVR is running, the overlay will be registered automatically. If not:
   - Launch `space-calibrator` once manually, OR
   - Manually enable the "Space Calibrator" overlay in SteamVR Settings > Startup/Shutdown > Choose Startup Overlay Apps
4. Restart SteamVR

The installation script will:
- Copy the driver to your SteamVR drivers directory
- Copy the overlay binary and manifest
- Register the overlay with SteamVR (if SteamVR is running)

After the first launch or manual enabling, the overlay will auto-launch with SteamVR on future starts.

## Usage

### Starting the Application

The overlay will automatically launch when the driver is active. You can also run it manually:

```bash
cd build && ./bin/space-calibrator
```

### Calibration

If you do not wish to use continuous calibration, you will have to use regular calibration. This means that every so often you will have to sync your headset's playspace with your tracker's playspace.

To calibrate:
1. Copy the chaperone/guardian bounds from your HMD's play space
   > You will only have to do this once. Connect your VR headset and start SteamVR. Then open the Space Calibrator window (it will be minimized when overlay is active), and click the "Copy Chaperone" button.

2. Open the SteamVR dashboard. At the bottom, click on the Space Calibrator icon.
3. In the Space Calibrator overlay, you'll see two lists at the top. On the left `Reference Space` column, select the controller you'll be calibrating along (e.g. Quest controller, Pico controller). On the right `Target Space`, select your SteamVR tracker (e.g. Vive Ultimate Tracker, Vive Tracker 3.0, Vive Ultimate Tracker). You can use the Identify button to make the controllers vibrate and tracker LEDs flash to see if you've selected the correct ones.
4. Click the "Start calibration" button, and start calibrating.

## Continuous Calibration

> [!IMPORTANT]  
> **A tracker attached on your headset is required for this.**

To enable continuous calibration mode, first select your headset on the left column, then the tracker on your headset on the right column. Once you've done so, click `Start Calibration`, and click cancel. Then click `Continuous Calibration` to enable continuous calibration.

1. Start SteamVR with the VR headset you wish to use.
2. Turn on **ONLY** the tracker which is attached on the VR headset.
3. Select the VR headset and tracker and calibrate.
4. Turn on your other devices.
5. You should see them line up with you as you after moving around your playspace for a bit for an initial calibration.

## Playspace Movement

Space Calibrator includes a playspace movement feature that allows you to shift your playspace boundaries using controller input. This is useful for adjusting your playspace position without recalibrating.

### Features

- **Pull Playspace**: Hold a button and move your controller to shift the playspace in the direction of movement
- **Reset Playspace**: Reset the playspace to its original saved position
- **Movement Multiplier**: Adjustable multiplier to control how much the playspace moves relative to controller movement
- **Global Actions**: Works across all VR applications (Steam Home, VRChat, etc.)

### Setup

1. Open the Space Calibrator overlay
2. Navigate to the "Playspace Movement" tab
3. Enable "Enable Playspace Movement"
4. Adjust the "Movement Multiplier" slider (1.0 = 1:1 movement, higher values amplify movement)
5. Optionally click "Save Original Playspace" to manually save the current playspace as the reset point

### Controller Bindings

The playspace movement actions are global and work across all VR applications. To configure bindings:

1. Open SteamVR Settings > Controllers
2. Select "Space Calibrator" application
3. Bind buttons to:
   - **Reset Playspace**: Resets playspace to saved original position
   - **Pull Playspace**: Hold and move controller to shift playspace

> **Note:** Enable "Experimental overlay input overrides" in Developer settings for best compatibility.

### Usage

- **Pulling the Playspace**: Hold the bound "Pull Playspace" button and move your controller. The playspace will shift in the direction of movement, scaled by the movement multiplier. The first pull automatically saves the current playspace as a reference.
- **Resetting**: Press the bound "Reset Playspace" button to return to the original saved position. After resetting, the next pull will save the reset position as a new reference.

## Credits

- **Original Project:** [OpenVR-SpaceCalibrator](https://github.com/hyblocker/OpenVR-SpaceCalibrator) by [@hyblocker](https://github.com/hyblocker)
- **Linux Port:** [@xi-ve](https://github.com/xi-ve)
