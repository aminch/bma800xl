#!/bin/bash
#
# make_wifi_firmware.sh
#
# BMC64 - A bare metal Commodore emulator for Raspberry Pi
# Copyright (C) 2026 aminch <https://github.com/aminch>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# Modified for BMA800XL from BMC64's make_wifi_firmware.sh: the firmware
# is cached outside the source tree, and the brcmfmac43456 files are
# added.
#

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
WLAN_FIRMWARE_DIR="$SCRIPT_DIR/third_party/circle-stdlib/libs/circle/addon/wlan/firmware"
FIRMWARE_CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/bma800xl/wlan-firmware"
FIRMWARE_LICENSE_URL_BASE="https://raw.githubusercontent.com/RPi-Distro/firmware-nonfree"

WLAN_FIRMWARE=(
    brcmfmac43430-sdio.bin
    brcmfmac43430-sdio.txt
    brcmfmac43430-sdio.clm_blob
    brcmfmac43436-sdio.bin
    brcmfmac43436-sdio.txt
    brcmfmac43436-sdio.clm_blob
    brcmfmac43436s-sdio.bin
    brcmfmac43436s-sdio.txt
    brcmfmac43455-sdio.bin
    brcmfmac43455-sdio.clm_blob
    brcmfmac43455-sdio.txt
    brcmfmac43456-sdio.bin
    brcmfmac43456-sdio.txt
    brcmfmac43456-sdio.clm_blob
)

WLAN_LICENSES=(
    LICENCE.broadcom_bcm43xx
    LICENCE.cypress
    LICENCE.synaptics
)

if [ "$#" -ne 1 ]
then
    echo "Usage: $0 OUTPUT_DIRECTORY" >&2
    exit 2
fi

OUTPUT_DIRECTORY=${1%/}
firmware_revision=$(sed -n 's/^FIRMWARE[[:space:]]*?=[[:space:]]*//p' \
    "$WLAN_FIRMWARE_DIR/Makefile")

if [ -z "$firmware_revision" ]
then
    echo "Could not determine the WLAN firmware revision." >&2
    exit 1
fi

# The firmware is cached outside the source tree: clean_all.sh resets the
# Circle submodule, where Circle's Makefile would download it.
firmware_cache_dir="$FIRMWARE_CACHE_DIR/$firmware_revision/firmware"
license_cache_dir="$FIRMWARE_CACHE_DIR/$firmware_revision/licenses"
copyright_cache_file="$license_cache_dir/copyright"
firmware_missing=0

for firmware in "${WLAN_FIRMWARE[@]}"
do
    if [ ! -s "$firmware_cache_dir/$firmware" ]
    then
        firmware_missing=1
        break
    fi
done

if [ "$firmware_missing" -eq 1 ]
then
    # Circle's Makefile downloads into the current folder. A temporary one,
    # so a failed download leaves nothing half done in the cache.
    mkdir -p "$FIRMWARE_CACHE_DIR"
    download_dir=$(mktemp -d "$FIRMWARE_CACHE_DIR/.downloading.XXXXXX")
    trap 'rm -rf "$download_dir"' EXIT
    echo "Downloading the Wi-Fi firmware ($firmware_revision)"
    make -s -C "$download_dir" -f "$WLAN_FIRMWARE_DIR/Makefile" \
        FIRMWARE="$firmware_revision" firmware
    mkdir -p "$(dirname "$firmware_cache_dir")"
    rm -rf "$firmware_cache_dir"
    mv "$download_dir" "$firmware_cache_dir"
    trap - EXIT
fi

license_missing=0
for license in "${WLAN_LICENSES[@]}"
do
    if [ ! -s "$license_cache_dir/$license" ]
    then
        license_missing=1
        break
    fi
done

if [ "$license_missing" -eq 1 ]
then
    mkdir -p "$license_cache_dir"
    if [ ! -s "$copyright_cache_file" ]
    then
        curl -fsSL --user-agent "Mozilla/5.0 (Windows NT 10.0; Win64; x64)" \
            "$FIRMWARE_LICENSE_URL_BASE/$firmware_revision/debian/copyright" \
            -o "$copyright_cache_file"
    fi

    extract_license()
    {
        local license_name=$1
        local output_file=$2
        awk -v license_name="$license_name" '
            $0 == "License: " license_name {
                printing = 1
                license_text = $0 ORS
                next
            }
            printing && /^License:/ { printing = 0 }
            printing { license_text = license_text $0 ORS }
            END { printf "%s", license_text }
        ' "$copyright_cache_file" > "$output_file"
        test -s "$output_file"
    }

    extract_license "binary-redist-Broadcom-wifi" \
        "$license_cache_dir/LICENCE.broadcom_bcm43xx"
    extract_license "binary-redist-Cypress" \
        "$license_cache_dir/LICENCE.cypress"
    extract_license "Synaptics" "$license_cache_dir/LICENCE.synaptics"
fi

mkdir -p "$OUTPUT_DIRECTORY/firmware"

for firmware in "${WLAN_FIRMWARE[@]}"
do
    cp "$firmware_cache_dir/$firmware" "$OUTPUT_DIRECTORY/firmware/"
done

# Circle looks for the Pi 5's (and Pi 500's) firmware under its own name:
# the 43455 files (Circle's firmware5 target downloads the same ones).
for ext in bin txt clm_blob
do
    cp "$firmware_cache_dir/brcmfmac43455-sdio.$ext" \
        "$OUTPUT_DIRECTORY/firmware/brcmfmac43455-sdio.raspberrypi,5-model-b.$ext"
done

for license in "${WLAN_LICENSES[@]}"
do
    cp "$license_cache_dir/$license" "$OUTPUT_DIRECTORY/firmware/"
done