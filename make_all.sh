#!/bin/bash
#
# Builds the third party libraries (circle-stdlib: Circle and newlib, and the
# Circle addons) and then the BMA800XL kernel for one board:
#
#   ./make_all.sh [pi2|pi3] [--skip-patches]
#
#   pi3 (default)  kernel8-32.img: Pi 3 and Pi Zero 2 W
#   pi2            kernel7.img: Pi 2
#
# --skip-patches leaves Circle and atari800 as they are instead of resetting
# them and applying src/patches/ again.
# Run clean_all.sh first for a completely fresh build, and before building
# for a different board: the libraries are built for one board at a time.
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
        pi2|pi3) BOARD=$arg ;;
        --skip-patches) SKIP_PATCHES=1 ;;
        *) echo "Usage: $0 [pi2|pi3] [--skip-patches]" >&2; exit 1 ;;
    esac
done
BOARD=${BOARD:-pi3}
RASPI=${BOARD#pi}
case "$RASPI" in
    2) KERNEL=kernel7.img ;;
    3) KERNEL=kernel8-32.img ;;
esac

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

# newlib and the addons would keep objects built for the other board.
BUILT_FOR=$(sed -n 's/^RASPPI = //p' "$CIRCLE_STDLIB/Config.mk" 2>/dev/null || true)
if [ -n "$BUILT_FOR" ] && [ "$BUILT_FOR" != "$RASPI" ]
then
    echo "circle-stdlib is built for the Pi $BUILT_FOR. Run ./clean_all.sh first." >&2
    exit 1
fi

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
    "$SRC_DIR/tools/apply_atari800_patches.sh"
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
echo "Built $KERNEL for $BOARD"
