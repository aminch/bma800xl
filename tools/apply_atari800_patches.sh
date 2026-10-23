#!/bin/bash
#
# apply_atari800_patches.sh
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
# Resets the atari800 submodule to its pinned revision and applies
# src/patches/atari800_*.diff to it, as make_all.sh does for Circle.
# Called by make_all.sh and tools/hostsim/run_tests.sh; with --skip-patches
# they leave the submodule as it is, keeping any changes made by hand.
#

set -e

SRC_DIR=$(cd "$(dirname "$0")/.." && pwd)

cd "$SRC_DIR/third_party/atari800"
git reset -q --hard HEAD
git clean -q -fd
for p in "$SRC_DIR"/src/patches/atari800_*.diff
do
    [ -e "$p" ] || continue
    echo "  $(basename "$p")"
    patch -s -p1 < "$p"
done
