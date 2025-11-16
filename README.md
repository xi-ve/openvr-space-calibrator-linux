# Space Calibrator - Linux Edition

This is a Linux port of [OpenVR-SpaceCalibrator](https://github.com/hyblocker/OpenVR-SpaceCalibrator) by [@hyblocker](https://github.com/hyblocker). This program allows you to synchronise multiple playspaces with one another in SteamVR, and supports [continuous calibration](#continuous-calibration).

**Linux Port Maintainer:** [@xi-ve](https://github.com/xi-ve)

Continuous calibration is a tracking mode which automatically aligns playspaces together, using a tracker on the headset.

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

## Installation

1. Build the project (see above)
2. Install the driver and overlay:
   ```bash
   ./scripts/install.sh
   ```
3. Restart SteamVR

The installation script will:
- Copy the driver to your SteamVR drivers directory
- Copy the overlay binary and manifest
- Register the overlay with SteamVR

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

## Credits

- **Original Project:** [OpenVR-SpaceCalibrator](https://github.com/hyblocker/OpenVR-SpaceCalibrator) by [@hyblocker](https://github.com/hyblocker)
- **Linux Port:** [@xi-ve](https://github.com/xi-ve)
