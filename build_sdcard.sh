#!/bin/bash
#
# Builds everything and stages the files for the SD card's boot partition in
# build/sdcard. Copy the contents of that folder to the root of a FAT32
# formatted SD card.
#
#   ./build_sdcard.sh [pi3] [--skip-patches]
#
# Without --skip-patches this starts from clean_all.sh. With it, the third
# party libraries already built are kept and only the kernel is rebuilt,
# which is much quicker while developing.
#

set -euo pipefail

SRC_DIR=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR="$SRC_DIR/build"
STAGING_DIR="$BUILD_DIR/sdcard"
SKIP_PATCHES=0

for arg in "$@"
do
    case "$arg" in
        pi3) ;;
        --skip-patches) SKIP_PATCHES=1 ;;
        *) echo "Usage: $0 [pi3] [--skip-patches]" >&2; exit 1 ;;
    esac
done

cd "$SRC_DIR"

echo "=============================================================="
echo "BUILDING pi3"
echo "=============================================================="
if [ "$SKIP_PATCHES" -eq 1 ] && [ -f third_party/circle-stdlib/Config.mk ]
then
    source "$SRC_DIR/get_gnu_toolchain.sh"
    make -j"$(nproc)"
else
    ./clean_all.sh
    ./make_all.sh pi3
fi

if [ ! -f "$SRC_DIR/kernel8-32.img" ]
then
    echo "Expected kernel was not created: kernel8-32.img" >&2
    exit 1
fi

rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR"

# Raspberry Pi firmware, Wi-Fi firmware, settings and the kernel.
cp "$SRC_DIR"/sdcard/firmware/* "$STAGING_DIR/"
if ! "$SRC_DIR/make_wifi_firmware.sh" "$STAGING_DIR"
then
    echo "Wi-Fi firmware could not be fetched; Wi-Fi won't work from this card." >&2
fi
cp "$SRC_DIR/sdcard/config.txt" "$SRC_DIR/sdcard/cmdline.txt" \
   "$SRC_DIR/sdcard/wpa_supplicant.conf.example" "$STAGING_DIR/"
cp "$SRC_DIR/kernel8-32.img" "$STAGING_DIR/"
cp "$SRC_DIR/LICENSE" "$STAGING_DIR/LICENSE.txt"
cp "$SRC_DIR/docs/SDCARD_README.txt" "$STAGING_DIR/README.txt"

# Folders for the user's files. The file browser starts at the root.
for dir in disks programs carts tapes states ROM
do
    mkdir -p "$STAGING_DIR/$dir"
done
cp "$SRC_DIR/docs/ROM_README.txt" "$STAGING_DIR/ROM/README.txt"

echo "Staged boot partition files in $STAGING_DIR ($(tools/version.sh))"
