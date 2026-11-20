#!/bin/bash
#
# run_tests.sh
#
# BMA800XL - A bare metal Atari 800XL emulator for Raspberry Pi
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
# Tests the shared Pi 4/5 ARM smooth scaling and CRT shader (src/av/scaler.*)
# and how a frame is shared between cores (src/av/frameshare.h) on the PC.
# The pictures go to tools/pi4scaler/out/.
#
#   tools/pi4scaler/run_tests.sh
#

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
make -s -C "$HERE"
rm -rf "$HERE/out"
mkdir -p "$HERE/out"
"$HERE/build/pi4scaler_test" "$HERE/out"
echo
# ThreadSanitizer stops on some newer Linux kernels' address layout; run it
# without address space randomisation where setarch is available.
run_share_test() {
    if command -v setarch > /dev/null
    then
        setarch "$(uname -m)" -R "$1"
    else
        "$1"
    fi
}

# Kernels with an address space size ThreadSanitizer does not support (the
# Pi 5's 64-bit Raspberry Pi OS kernel) run the test without it instead.
LOG="$HERE/build/share_test.log"
STATUS=0
run_share_test "$HERE/build/share_test" > "$LOG" 2>&1 || STATUS=$?
cat "$LOG"
if [ "$STATUS" -ne 0 ]
then
    if ! grep -q "ThreadSanitizer: unsupported VMA range" "$LOG"
    then
        exit "$STATUS"
    fi
    echo
    echo "ThreadSanitizer does not support this kernel's address space;"
    echo "running the frame sharing test without it."
    run_share_test "$HERE/build/share_test_plain"
fi
