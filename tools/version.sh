#!/bin/sh
#
# Prints the BMA800XL version: the git tag or commit, with "-dirty" if
# BMA800XL's own files have uncommitted changes. Changes inside the
# submodules (build_deps.sh patches Circle) don't count.
#
cd "$(dirname "$0")/.." || exit 1
version=$(git describe --tags --always 2>/dev/null) || { echo dev; exit 0; }
if ! git diff --quiet --ignore-submodules HEAD 2>/dev/null; then
    version="$version-dirty"
fi
echo "$version"
