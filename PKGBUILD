# Maintainer: xi-ve <https://github.com/xi-ve>
pkgname=openvr-space-calibrator-linux
pkgver=1.5.1
pkgrel=1
pkgdesc="Linux port of OpenVR-SpaceCalibrator - synchronize multiple VR playspaces in SteamVR"
arch=('x86_64')
url="https://github.com/xi-ve/openvr-space-calibrator-linux"
license=('MIT')
depends=('glfw-x11' 'mesa' 'libx11' 'libxrandr' 'libxinerama' 'libxcursor' 'libxi')
makedepends=('cmake' 'base-devel' 'eigen' 'pkgconf')
source=("${pkgname}-${pkgver}.tar.gz::https://github.com/xi-ve/openvr-space-calibrator-linux/archive/main.tar.gz")
sha256sums=('SKIP')

prepare() {
  cd "${srcdir}/${pkgname}-main"
  
  if [ -d "build" ]; then
    rm -rf build
  fi
}

build() {
  cd "${srcdir}/${pkgname}-main"
  
  mkdir -p build
  cd build
  
  OPENVR_HEADERS=""
  for path in \
    "$HOME/.local/share/Steam/steamapps/common/SteamVR/headers" \
    "$HOME/.steam/steam/steamapps/common/SteamVR/headers" \
    "$HOME/.steam/root/steamapps/common/SteamVR/headers" \
    "/usr/include/openvr" \
    "/usr/local/include/openvr"; do
    if [ -f "$path/openvr.h" ] || [ -f "$path/openvr_driver.h" ]; then
      OPENVR_HEADERS="$path"
      break
    fi
  done
  
  if [ -z "$OPENVR_HEADERS" ]; then
    echo "ERROR: OpenVR headers not found. Please install SteamVR or openvr package."
    return 1
  fi
  
  cmake .. -DOPENVR_INCLUDE_DIR="$OPENVR_HEADERS"
  make -j$(nproc)
}

package() {
  cd "${srcdir}/${pkgname}-main"
  
  install -Dm755 build/bin/space-calibrator "${pkgdir}/usr/bin/space-calibrator"
  install -Dm755 build/lib/driver_01spacecalibrator.so "${pkgdir}/usr/lib/${pkgname}/driver_01spacecalibrator.so"
  install -Dm644 build/manifest.vrmanifest "${pkgdir}/usr/lib/${pkgname}/manifest.vrmanifest"
  install -Dm644 driver_01spacecalibrator/driver.vrdrivermanifest "${pkgdir}/usr/lib/${pkgname}/driver.vrdrivermanifest"
  cp -r driver_01spacecalibrator/resources "${pkgdir}/usr/lib/${pkgname}/"
  
  install -Dm644 README.md "${pkgdir}/usr/share/doc/${pkgname}/README.md"
}

