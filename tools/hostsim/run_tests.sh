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

echo "Gamepad in a game, with an idle extra pad in slot 1"
mkdir -p "$OUT/joy"
python3 - "$OUT/joy/joy.xex" <<'EOF'
import sys
# Background colour from joystick 1: STICK0 * 16, or red while fire is held.
code = bytes([
    0xAD, 0x78, 0x02,        # loop: LDA STICK0
    0x0A, 0x0A, 0x0A, 0x0A,  #       ASL x4
    0xAE, 0x84, 0x02,        #       LDX STRIG0
    0xD0, 0x02,              #       BNE store
    0xA9, 0x34,              #       LDA #$34 (red)
    0x8D, 0xC8, 0x02,        # store: STA COLOR4
    0x8D, 0xC6, 0x02,        #       STA COLOR2
    0x4C, 0x00, 0x20,        #       JMP loop
])
xex = bytes([0xFF, 0xFF, 0x00, 0x20]) + (0x2000 + len(code) - 1).to_bytes(2, "little") + code
xex += bytes([0xE0, 0x02, 0xE1, 0x02, 0x00, 0x20])
open(sys.argv[1], "wb").write(xex)
EOF
# Pad 1 is connected but idle; pad 2 is the one being used.
run joy 50 "autostart=joy.xex" '1 pad 0 0\n1 pad 1 0\n200 pad 1 0x8000\n230 shot right.ppm\n240 pad 1 0x8001\n260 shot fire.ppm\n270 quit\n'
check_log joy "Gamepad 2 is joystick 1" "the pad in use becomes joystick 1"
check_image joy right
check_image joy fire

echo "The Home press that opens the menu isn't also Escape in it"
run padhold 50 "" '1 pad 0 0\n150 pad 0 0x100\n250 pad 0 0\n280 quit\n'
if grep -q 'Menu opened' "$OUT/padhold/log.txt" && ! grep -q 'Menu key 0x29 down' "$OUT/padhold/log.txt"; then
    pass "padhold: no Escape while Home is held"
else
    fail "padhold" "menu not opened, or Escape sent while Home was held (see out/padhold/log.txt)"
fi

echo "Web UI requests while the menu is open"
run menureset 50 "" '150 key 0x3a 1\n152 key 0x3a 0\n200 webui coldstart\n400 shot after.ppm\n401 quit\n'
check_log menureset "Web UI: cold start" "cold start leaves the menu and runs"
run menureboot 50 "" '150 key 0x3a 1\n152 key 0x3a 0\n200 webui reboot\n1000 quit\n'
check_log menureboot "reboot requested at frame 2[0-9]{2}$" "reboot happens with the menu open"

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

echo "The menu answers at once after a long game"
# atari800's frame clock used to drift ahead while the display paced the
# emulation, and the first menu sleep then waited for it (25 s after 10
# minutes here, a minute or more on a Pi).
mkdir -p "$OUT/longplay"
(cd "$OUT/longplay" && printf '30000 key 0x3a 1\n30002 key 0x3a 0\n30060 key 0x29 1\n30062 key 0x29 0\n30120 shot back.ppm\n30121 quit\n' \
    | BMA_SOUND_HALF=1 BMA_REFRESH=50 BMA_CMDLINE="root=. tv=pal" timeout 600 "$SIM" > log.txt 2>&1)
if python3 - "$OUT/longplay/back.ppm" "$HERE/expected/menu-back.png" <<'EOF'
import sys
from PIL import Image, ImageChops
a = Image.open(sys.argv[1]).convert("RGB")
b = Image.open(sys.argv[2]).convert("RGB")
sys.exit(0 if ImageChops.difference(a, b).getbbox() is None else 1)
EOF
then
    pass "longplay: menu opened and closed within a second"
else
    fail "longplay" "menu still showing a second after Escape (see out/longplay)"
fi
if grep -q "clock was" "$OUT/longplay/log.txt"; then
    fail "longplay" "atari800's clock got ahead (see out/longplay/log.txt)"
else
    pass "longplay: atari800's clock kept with real time"
fi

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
