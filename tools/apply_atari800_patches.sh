#!/bin/bash
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
