#!/bin/bash
#
# Builds BMA800XL from scratch: circle-stdlib, the kernel and the SD card
# files (in sdcard_out/ and bma800xl-<version>.zip).
#
#   ./build.sh            build everything
#   ./build.sh --kernel   only rebuild the kernel and SD card files
#

set -e

SRC_DIR=$(cd "$(dirname "$0")" && pwd)
cd "$SRC_DIR"

source ./get_toolchain.sh

if [ "$1" != "--kernel" ] || [ ! -f third_party/circle-stdlib/Config.mk ]
then
    ./build_deps.sh pi3
fi

make -j"$(nproc)"
./make_sdcard.sh kernel8-32.img
