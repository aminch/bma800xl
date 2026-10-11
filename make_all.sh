#!/bin/bash
#
# Builds the third party libraries (circle-stdlib: Circle and newlib, and the
# Circle addons) and then the BMA800XL kernel, kernel8-32.img.
#
#   ./make_all.sh [pi3] [--skip-patches]
#
# The Pi 3 kernel also runs on the Pi Zero 2 W. --skip-patches leaves Circle
# as it is instead of resetting it and applying src/patches/ again.
# Run clean_all.sh first for a completely fresh build.
#

set -e

SRC_DIR=$(cd "$(dirname "$0")" && pwd)
CIRCLE_STDLIB=$SRC_DIR/third_party/circle-stdlib
CIRCLE_HOME=$CIRCLE_STDLIB/libs/circle

BOARD=""
SKIP_PATCHES=0

for arg in "$@"
do
    case "$arg" in
        pi3) BOARD=$arg ;;
        --skip-patches) SKIP_PATCHES=1 ;;
        *) echo "Usage: $0 [pi3] [--skip-patches]" >&2; exit 1 ;;
    esac
done
BOARD=${BOARD:-pi3}
RASPI=3

if [ ! -f "$SRC_DIR/sdcard/config.txt" ]
then
    echo "Must be run from the BMA800XL root directory." >&2
    exit 1
fi

for tool in make patch makeinfo
do
    if ! command -v $tool >/dev/null 2>&1
    then
        echo "Missing $tool. On Debian/Ubuntu: sudo apt-get install build-essential texinfo" >&2
        exit 1
    fi
done

source "$SRC_DIR/get_gnu_toolchain.sh"

if [ ! -f "$CIRCLE_HOME/Rules.mk" ] || [ ! -f "$SRC_DIR/third_party/atari800/src/atari.c" ]
then
    echo "Fetching submodules..."
    git -C "$SRC_DIR" submodule update --init third_party/atari800 third_party/circle-stdlib
    git -C "$CIRCLE_STDLIB" submodule update --init libs/circle libs/circle-newlib
    git -C "$CIRCLE_HOME" submodule update --init addon/wlan/hostap
fi

echo "=============================================================="
echo "APPLY PATCHES"
echo "=============================================================="
if [ "$SKIP_PATCHES" = "1" ]
then
    echo "Skipping patches"
else
    cd "$CIRCLE_HOME"
    git reset -q --hard HEAD
    git clean -q -fd
    for p in "$SRC_DIR"/src/patches/circle_*.diff
    do
        echo "  $(basename "$p")"
        patch -s -p1 < "$p"
    done
fi

echo "=============================================================="
echo "BUILD CIRCLE-STDLIB for Raspberry Pi $RASPI"
echo "=============================================================="
cd "$CIRCLE_STDLIB"
find . -name config.cache -delete
./configure --raspberrypi=$RASPI --kernel-max-size 16
make -C libs/circle clean >/dev/null 2>&1 || true
make -j"$(nproc)"

echo "=============================================================="
echo "BUILD ADDONS"
echo "=============================================================="
for addon in fatfs linux vc4/vchiq vc4/interface/bcm_host vc4/interface/khronos \
             vc4/interface/vmcs_host vc4/interface/vcos wlan \
             wlan/hostap/wpa_supplicant
do
    if [ "$addon" = "wlan/hostap/wpa_supplicant" ]
    then
        make -C "$CIRCLE_HOME/addon/$addon" -f Makefile.circle -j"$(nproc)"
    else
        make -C "$CIRCLE_HOME/addon/$addon" -j"$(nproc)"
    fi
done

echo "=============================================================="
echo "BUILD KERNEL"
echo "=============================================================="
cd "$SRC_DIR"
make clean >/dev/null
make -j"$(nproc)"
echo
echo "Built kernel8-32.img for $BOARD"
