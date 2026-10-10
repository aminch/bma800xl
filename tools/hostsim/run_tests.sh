#!/bin/bash
#
# Runs the emulator port on the PC (see host_sim.c) through a set of
# scripted tests and checks the results.
#
#   tools/hostsim/run_tests.sh            run the tests
#   tools/hostsim/run_tests.sh --update   also replace the reference images
#
# Screenshots are compared with the images in tools/hostsim/expected/.
# Output goes to tools/hostsim/out/.
#

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$HERE/out
SIM=$HERE/build/bma800xl_sim
UPDATE=0
[ "${1:-}" = "--update" ] && UPDATE=1

make -s -C "$HERE" -j"$(nproc 2>/dev/null || echo 2)" || exit 1

rm -rf "$OUT"
mkdir -p "$OUT"

FAILED=0
pass() { echo "PASS  $1"; }
fail() { echo "FAIL  $1: $2"; FAILED=$((FAILED + 1)); }

# run NAME REFRESH CMDLINE SCRIPT: runs the simulator in out/NAME.
run() {
    local name=$1 refresh=$2 cmdline=$3 script=$4
    mkdir -p "$OUT/$name"
    (cd "$OUT/$name" && printf "$script" | BMA_REFRESH=$refresh \
        BMA_CMDLINE="root=. $cmdline" timeout 600 "$SIM" > log.txt 2>&1)
}

# check_image NAME FILE: compares out/NAME/FILE.ppm with expected/NAME-FILE.png
check_image() {
    local name=$1 file=$2
    local got=$OUT/$name/$file.ppm
    local want=$HERE/expected/$name-$file.png
    if [ ! -f "$got" ]; then
        fail "$name" "no screenshot $file"
        return
    fi
    if [ $UPDATE = 1 ]; then
        python3 -c "from PIL import Image; Image.open('$got').save('$want')"
        echo "      updated expected/$name-$file.png"
        return
    fi
    if [ ! -f "$want" ]; then
        fail "$name" "no reference image expected/$name-$file.png (run with --update)"
        return
    fi
    if python3 - "$got" "$want" <<'EOF'
import sys
from PIL import Image, ImageChops
a = Image.open(sys.argv[1]).convert("RGB")
b = Image.open(sys.argv[2]).convert("RGB")
sys.exit(0 if a.size == b.size and ImageChops.difference(a, b).getbbox() is None else 1)
EOF
    then
        pass "$name: $file matches"
    else
        fail "$name" "$file differs from expected/$name-$file.png"
    fi
}

# check_log NAME REGEX MESSAGE: the log must contain REGEX
check_log() {
    if grep -Eq "$2" "$OUT/$1/log.txt"; then
        pass "$1: $3"
    else
        fail "$1" "$3 (see out/$1/log.txt)"
    fi
}

# check_audio NAME: no audio underruns and every display frame presented
check_audio() {
    local log=$OUT/$1/log.txt
    if grep -q "audio underrun" "$log"; then
        fail "$1" "audio underrun (see out/$1/log.txt)"
    else
        pass "$1: no audio underruns"
    fi
    local line frame presented
    line=$(grep "frames presented" "$log" | tail -1)
    frame=$(echo "$line" | sed 's/.*frame \([0-9]*\):.*/\1/')
    presented=$(echo "$line" | sed 's/.*: \([0-9]*\) frames presented/\1/')
    if [ -n "$frame" ] && [ "$presented" -ge "$frame" ]; then
        pass "$1: no dropped frames ($presented/$frame)"
    else
        fail "$1" "frames presented: $line"
    fi
}

echo "Boot to BASIC and type"
run basic 50 "" '150 shot boot.ppm\n160 type PRINT 2+2\\n\n220 shot print.ppm\n230 quit\n'
check_log basic "synchronised to display" "locked to a 50 Hz display"
check_image basic boot
check_image basic print

echo "Menu with the keyboard"
run menu 50 "" '150 key 0x3a 1\n152 key 0x3a 0\n180 shot menu.ppm\n190 key 0x29 1\n192 key 0x29 0\n220 shot back.ppm\n230 quit\n'
check_image menu menu
check_image menu back

echo "Menu and file browser with a gamepad"
mkdir -p "$OUT/pad/games"
run pad 50 "" '1 pad 0 0\n150 pad 0 0x100\n155 pad 0 0\n180 shot menu.ppm\n190 pad 0 1\n195 pad 0 0\n230 shot browser.ppm\n240 quit\n'
check_image pad menu
check_log pad "frame 230: saved browser.ppm" "file browser opened"

echo "Autostart a program"
mkdir -p "$OUT/xex"
python3 - "$OUT/xex/red.xex" <<'EOF'
import sys
# LDA #$34 / STA COLOR4 / STA COLOR2 / JMP * : a red screen
code = bytes([0xA9, 0x34, 0x8D, 0xC8, 0x02, 0x8D, 0xC6, 0x02, 0x4C, 0x08, 0x20])
xex = bytes([0xFF, 0xFF, 0x00, 0x20]) + (0x2000 + len(code) - 1).to_bytes(2, "little") + code
xex += bytes([0xE0, 0x02, 0xE1, 0x02, 0x00, 0x20])
open(sys.argv[1], "wb").write(xex)
EOF
run xex 50 "autostart=red.xex" '200 shot red.ppm\n210 quit\n'
check_image xex red

echo "NTSC on a 60 Hz display"
run ntsc 60 "tv=ntsc" '150 shot boot.ppm\n3000 audio\n3001 quit\n'
check_log ntsc "Atari 59.92 Hz, synchronised to display" "NTSC locked to a 60 Hz display"
check_audio ntsc

echo "PAL on a 50 Hz display, one minute"
run pal 50 "tv=pal" '3000 audio\n3001 quit\n'
check_audio pal

echo "Display slightly off its first measurement, 15 minutes"
mkdir -p "$OUT/drift"
(cd "$OUT/drift" && printf '45000 audio\n45001 quit\n' | BMA_REFRESH=50 BMA_REFRESH_LATER=50.003 \
    BMA_CMDLINE="root=. tv=pal" timeout 600 "$SIM" > log.txt 2>&1)
check_log drift "display refresh rate 50.003" "refresh rate refined"
check_audio drift

echo "PAL on a 60 Hz display (not locked)"
run mismatch 60 "tv=pal" '600 audio\n601 quit\n'
check_log mismatch "timer paced" "falls back to timer pacing"

echo "Configuration saved"
check_log basic "Writing config file" "atari800.cfg written"
if grep -q "^DISABLE_BASIC=0" "$OUT/basic/atari800.cfg" 2>/dev/null; then
    pass "basic: BASIC on by default"
else
    fail "basic" "BASIC not on by default"
fi

echo
if [ $FAILED = 0 ]; then
    echo "All tests passed."
else
    echo "$FAILED test(s) failed."
    exit 1
fi
