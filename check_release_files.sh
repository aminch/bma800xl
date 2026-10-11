#!/bin/bash
#
# Checks that a staged SD card (build_sdcard.sh's build/sdcard by default)
# has everything a release needs, as BMC64's check_release_files.sh does.
# The release workflow runs it before zipping.
#
#   ./check_release_files.sh [RELEASE_DIRECTORY]
#

SRC_DIR=$(cd "$(dirname "$0")" && pwd)

# Files that must be there and not empty.
REQUIRED_FILES=(
    # Raspberry Pi firmware and the kernel
    "bootcode.bin"
    "start.elf"
    "fixup.dat"
    "config.txt"
    "cmdline.txt"
    "kernel8-32.img"

    # Documents and settings examples
    "LICENSE.txt"
    "README.txt"
    "wpa_supplicant.conf.example"

    # Wi-Fi firmware and redistribution licences (build_sdcard.sh only
    # warns if these could not be fetched)
    "firmware/brcmfmac43430-sdio.bin"
    "firmware/brcmfmac43430-sdio.clm_blob"
    "firmware/brcmfmac43430-sdio.txt"
    "firmware/brcmfmac43436-sdio.bin"
    "firmware/brcmfmac43436-sdio.clm_blob"
    "firmware/brcmfmac43436-sdio.txt"
    "firmware/brcmfmac43436s-sdio.bin"
    "firmware/brcmfmac43436s-sdio.txt"
    "firmware/brcmfmac43455-sdio.bin"
    "firmware/brcmfmac43455-sdio.clm_blob"
    "firmware/brcmfmac43455-sdio.txt"
    "firmware/LICENCE.broadcom_bcm43xx"
    "firmware/LICENCE.cypress"
    "firmware/LICENCE.synaptics"

    # User folders: the READMEs keep them in zip files
    "disks/README.txt"
    "programs/README.txt"
    "carts/README.txt"
    "tapes/README.txt"
    "states/README.txt"
    "ROM/README.txt"
)

if [ "$#" -gt 1 ]
then
    echo "Usage: $0 [RELEASE_DIRECTORY]" >&2
    exit 2
fi

RELEASE_DIRECTORY=${1:-$SRC_DIR/build/sdcard}
RELEASE_DIRECTORY=${RELEASE_DIRECTORY%/}

if [ ! -d "$RELEASE_DIRECTORY" ]
then
    echo "Release directory does not exist: $RELEASE_DIRECTORY" >&2
    exit 2
fi

problems=0

for required_file in "${REQUIRED_FILES[@]}"
do
    if [ ! -f "$RELEASE_DIRECTORY/$required_file" ]
    then
        echo "Missing: $required_file"
        problems=1
    elif [ ! -s "$RELEASE_DIRECTORY/$required_file" ]
    then
        echo "Empty: $required_file"
        problems=1
    fi
done

# The kernel must be this version, built from committed files.
version="BMA800XL $("$SRC_DIR/tools/version.sh" --semver) ($("$SRC_DIR/tools/version.sh" --git),"
kernel="$RELEASE_DIRECTORY/kernel8-32.img"
if [ -f "$kernel" ] && ! grep -qaF "$version" "$kernel"
then
    echo "kernel8-32.img is not \"$version ...\": rebuild it"
    problems=1
fi
if [ -f "$kernel" ] && grep -qaE "BMA800XL [^ ]+ \([0-9a-f]+-dirty," "$kernel"
then
    echo "kernel8-32.img was built with uncommitted changes (-dirty)"
    problems=1
fi

if [ "$problems" -ne 0 ]
then
    echo "Release files in $RELEASE_DIRECTORY are not complete." >&2
    exit 1
fi

echo "Release files in $RELEASE_DIRECTORY are complete."
