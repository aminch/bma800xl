#!/bin/bash
#
# Puts together everything that goes on the SD card:
#
#   ./make_sdcard.sh [kernel image]
#
# The files end up in sdcard_out/ and in bma800xl-<version>.zip. Copy the
# contents of sdcard_out/ to the root of a FAT32 formatted SD card.
#

set -e

SRC_DIR=$(cd "$(dirname "$0")" && pwd)
KERNEL=${1:-kernel8-32.img}
OUT=$SRC_DIR/sdcard_out
VERSION=$(git -C "$SRC_DIR" describe --tags --always --dirty 2>/dev/null || echo dev)

if [ ! -f "$SRC_DIR/$KERNEL" ] && [ ! -f "$KERNEL" ]
then
    echo "Kernel image $KERNEL not found. Run make first." >&2
    exit 1
fi
[ -f "$KERNEL" ] || KERNEL=$SRC_DIR/$KERNEL

rm -rf "$OUT"
mkdir -p "$OUT"

cp "$SRC_DIR"/sdcard/firmware/* "$OUT/"
cp "$SRC_DIR/sdcard/config.txt" "$SRC_DIR/sdcard/cmdline.txt" "$OUT/"
cp "$KERNEL" "$OUT/"
cp "$SRC_DIR/LICENSE" "$OUT/LICENSE.txt"
cp "$SRC_DIR/docs/SDCARD_README.txt" "$OUT/README.txt"

# Folders for the user's files. The file browser starts at the root.
for dir in disks programs carts tapes states ROM
do
    mkdir -p "$OUT/$dir"
done
cp "$SRC_DIR/docs/ROM_README.txt" "$OUT/ROM/README.txt"

ZIP=$SRC_DIR/bma800xl-$VERSION.zip
rm -f "$ZIP"
if command -v zip >/dev/null 2>&1
then
    (cd "$OUT" && zip -qr "$ZIP" .)
    echo "SD card files in $OUT and $ZIP"
else
    echo "SD card files in $OUT (install zip to also get a .zip)"
fi
