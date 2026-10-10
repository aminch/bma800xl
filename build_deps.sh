#!/bin/bash
#
# Builds the libraries BMA800XL links against: circle-stdlib (Circle and
# newlib) and the Circle addons used by the kernel. Run this once, and again
# after changing board or updating circle-stdlib. Then run make.
#
#   ./build_deps.sh [pi3]
#
# The Pi 3 build (kernel8-32.img) also runs on the Pi Zero 2 W.
#

set -e

SRC_DIR=$(cd "$(dirname "$0")" && pwd)
CIRCLE_STDLIB=$SRC_DIR/third_party/circle-stdlib
CIRCLE_HOME=$CIRCLE_STDLIB/libs/circle

BOARD=${1:-pi3}
case "$BOARD" in
    pi3) RASPI=3 ;;
    *) echo "Usage: $0 [pi3]" >&2; exit 1 ;;
esac

for tool in make patch makeinfo
do
    if ! command -v $tool >/dev/null 2>&1
    then
        echo "Missing $tool. On Debian/Ubuntu: sudo apt-get install build-essential texinfo" >&2
        exit 1
    fi
done

source "$SRC_DIR/get_toolchain.sh"

if [ ! -f "$CIRCLE_HOME/Rules.mk" ] || [ ! -f "$SRC_DIR/third_party/atari800/src/atari.c" ]
then
    echo "Fetching submodules..."
    git -C "$SRC_DIR" submodule update --init third_party/atari800 third_party/circle-stdlib
    git -C "$CIRCLE_STDLIB" submodule update --init libs/circle libs/circle-newlib
    git -C "$CIRCLE_HOME" submodule update --init addon/wlan/hostap
fi

echo "=============================================================="
echo "Applying patches to Circle"
echo "=============================================================="
cd "$CIRCLE_HOME"
git reset -q --hard HEAD
git clean -q -fd
for p in "$SRC_DIR"/patches/circle_*.diff
do
    echo "  $(basename "$p")"
    patch -s -p1 < "$p"
done

echo "=============================================================="
echo "Building circle-stdlib for Raspberry Pi $RASPI"
echo "=============================================================="
cd "$CIRCLE_STDLIB"
find . -name config.cache -delete
./configure --raspberrypi=$RASPI --kernel-max-size 16
make -C libs/circle clean >/dev/null 2>&1 || true
make -j"$(nproc)"

echo "=============================================================="
echo "Building Circle addons"
echo "=============================================================="
for addon in fatfs linux vc4/vchiq vc4/interface/bcm_host vc4/interface/khronos \
             vc4/interface/vmcs_host vc4/interface/vcos
do
    make -C "$CIRCLE_HOME/addon/$addon" -j"$(nproc)"
done

echo "$BOARD" > "$SRC_DIR/.board"
echo
echo "Dependencies built for $BOARD. Now run: make"
