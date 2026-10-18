#!/bin/bash
#
# Runs the emulator port on the PC (see host_sim.c) through a set of
# scripted tests and checks the results.
#
#   tools/hostsim/run_tests.sh                  run the tests
#   tools/hostsim/run_tests.sh --update         also replace the reference images
#   tools/hostsim/run_tests.sh --skip-patches   don't reset atari800 and apply
#                                               src/patches/atari800_*.diff
#
# Screenshots are compared with the images in tools/hostsim/expected/.
# Output goes to tools/hostsim/out/.
#

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$HERE/out
SIM=$HERE/build/bma800xl_sim
UPDATE=0
SKIP_PATCHES=0
for arg in "$@"
do
    case "$arg" in
        --update) UPDATE=1 ;;
        --skip-patches) SKIP_PATCHES=1 ;;
        *) echo "Usage: $0 [--update] [--skip-patches]" >&2; exit 1 ;;
    esac
done

if [ $SKIP_PATCHES = 0 ]; then
    echo "Applying atari800 patches"
    "$HERE/../apply_atari800_patches.sh" || exit 1
fi

make -s -C "$HERE" -j"$(nproc 2>/dev/null || echo 2)" || exit 1
make -s -C "$HERE" test-mouse || exit 1

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

echo "Norwegian keyboard"
# tap FRAME USAGE [MODIFIER]: script lines pressing a key (USB HID usage) at
# FRAME, with MODIFIER (0xe1 Shift, 0xe6 AltGr) held around it.
tap() {
    if [ $# -gt 2 ]; then
        printf '%d key %s 1\\n%d key %s 1\\n%d key %s 0\\n%d key %s 0\\n' \
            $1 $3 $(($1 + 1)) $2 $(($1 + 3)) $2 $(($1 + 4)) $3
    else
        printf '%d key %s 1\\n%d key %s 0\\n' $1 $2 $(($1 + 2)) $2
    fi
}
# Shift+7, AltGr+2, Shift+0, +, Shift+¨, AltGr+8, |, Shift+<, å, ø, a type
# "/@=+^[|>A" (å and ø have no Atari key).
run keyboardno 50 "keyboard=no" "$(tap 160 0x24 0xe1)$(tap 168 0x1f 0xe6)$(tap 176 0x27 0xe1)$(tap 184 0x2d)$(tap 190 0x30 0xe1)$(tap 198 0x25 0xe6)$(tap 206 0x35)$(tap 212 0x64 0xe1)$(tap 220 0x2f)$(tap 226 0x33)$(tap 232 0x04)260 shot typed.ppm\n270 quit\n"
check_image keyboardno typed
# In the menus å, ø and æ can be typed: Network (one up from Emulator Menu),
# Wi-Fi Network (two down), type "åøæÅa" and Return.
run keyboardnomenu 50 "keyboard=no" "150 key 0x3a 1\n152 key 0x3a 0\n170 key 0x52 1\n172 key 0x52 0\n180 key 0x28 1\n182 key 0x28 0\n200 key 0x51 1\n202 key 0x51 0\n208 key 0x51 1\n210 key 0x51 0\n216 key 0x28 1\n218 key 0x28 0\n$(tap 240 0x2f)$(tap 248 0x33)$(tap 256 0x34)$(tap 264 0x2f 0xe1)$(tap 272 0x04)300 shot edit.ppm\n310 key 0x28 1\n312 key 0x28 0\n340 quit\n"
check_image keyboardnomenu edit
if grep -q 'ssid="åøæÅa"' "$OUT/keyboardnomenu/wpa_supplicant.conf" 2>/dev/null; then
    pass "keyboardnomenu: name typed with å, ø and æ saved"
else
    fail "keyboardnomenu" "wpa_supplicant.conf doesn't have ssid=\"åøæÅa\" (see out/keyboardnomenu)"
fi

echo "BMA800XL menu with the keyboard"
run menu 50 "" '150 key 0x3a 1\n152 key 0x3a 0\n180 shot menu.ppm\n190 key 0x29 1\n192 key 0x29 0\n220 shot back.ppm\n230 quit\n'
check_image menu menu
check_image menu back

echo "atari800's menu from the BMA800XL menu"
run emumenu 50 "" '150 key 0x3a 1\n152 key 0x3a 0\n180 key 0x28 1\n182 key 0x28 0\n210 shot menu.ppm\n220 quit\n'
check_image emumenu menu

echo "GPIO joystick menu navigation"
for port in 0 1; do
    run "gpiomenu$port" 50 "gpio_joysticks=1" "150 key 0x3a 1\n152 key 0x3a 0\n170 gpio $port 0x02\n172 gpio $port 0\n190 gpio $port 0x01\n192 gpio $port 0\n210 gpio $port 0x10\n212 gpio $port 0\n240 shot menu.ppm\n250 quit\n"
    if cmp -s "$OUT/gpiomenu$port/menu.ppm" "$OUT/emumenu/menu.ppm"; then
        pass "gpiomenu$port: down, up and fire open the emulator menu"
    else
        fail "gpiomenu$port" "GPIO navigation differs from keyboard navigation"
    fi
done

echo "Alt+D goes straight to atari800's disk menu"
run altd 50 "" '150 key 0xe2 1\n152 key 0x07 1\n154 key 0x07 0\n156 key 0xe2 0\n190 shot disks.ppm\n200 quit\n'
check_image altd disks

echo "Joystick test"
# Controllers is one down from Emulator Menu; Joystick Test is eight down
# from the top of Controllers.
run joytest 50 "" '1 pad 0 0\n150 key 0x3a 1\n152 key 0x3a 0\n170 key 0x51 1\n172 key 0x51 0\n180 key 0x28 1\n182 key 0x28 0\n200 key 0x51 1\n202 key 0x51 0\n208 key 0x51 1\n210 key 0x51 0\n216 key 0x51 1\n218 key 0x51 0\n224 key 0x51 1\n226 key 0x51 0\n232 key 0x51 1\n234 key 0x51 0\n240 key 0x51 1\n242 key 0x51 0\n248 key 0x51 1\n250 key 0x51 0\n256 key 0x51 1\n258 key 0x51 0\n264 key 0x28 1\n266 key 0x28 0\n300 pad 0 0x8000\n320 shot right.ppm\n330 pad 0 0x8001\n350 shot fire.ppm\n360 pad 0 0\n370 pad 0 0x2\n375 pad 0 0\n400 shot back.ppm\n410 quit\n'
check_image joytest right
check_image joytest fire
check_image joytest back

echo "Wi-Fi scan"
# Network is one up from Emulator Menu; Scan is one down in Network. Choose
# the strongest network, which needs a password: the Network page comes
# back with the password next, which is typed there.
mkdir -p "$OUT/wifiscan"
(cd "$OUT/wifiscan" && printf '150 key 0x3a 1\n152 key 0x3a 0\n170 key 0x52 1\n172 key 0x52 0\n180 key 0x28 1\n182 key 0x28 0\n200 key 0x51 1\n202 key 0x51 0\n210 key 0x28 1\n212 key 0x28 0\n240 shot list.ppm\n250 key 0x28 1\n252 key 0x28 0\n280 shot network.ppm\n290 key 0x28 1\n292 key 0x28 0\n310 type secret123\\n\n370 quit\n' \
    | BMA_WIFI=1 BMA_REFRESH=50 BMA_CMDLINE="root=." timeout 600 "$SIM" > log.txt 2>&1)
check_image wifiscan list
check_image wifiscan network
if grep -q 'ssid="StdWiFiPoint"' "$OUT/wifiscan/wpa_supplicant.conf" 2>/dev/null \
   && grep -q 'psk="secret123"' "$OUT/wifiscan/wpa_supplicant.conf" \
   && grep -q '^network=wifi' "$OUT/wifiscan/bma800xl.cfg" 2>/dev/null; then
    pass "wifiscan: network and password saved"
else
    fail "wifiscan" "wpa_supplicant.conf or bma800xl.cfg not as expected (see out/wifiscan)"
fi
# A network name in UTF-8: listed and shown with its own letters (the list's
# columns still line up), and the line editor deletes a whole character.
# Choose it (one down), then edit the name: Left 3 times puts the cursor
# after "Kåffé", Backspace leaves "KåffØst".
mkdir -p "$OUT/wifiutf8"
(cd "$OUT/wifiutf8" && printf '150 key 0x3a 1\n152 key 0x3a 0\n170 key 0x52 1\n172 key 0x52 0\n180 key 0x28 1\n182 key 0x28 0\n200 key 0x51 1\n202 key 0x51 0\n210 key 0x28 1\n212 key 0x28 0\n240 key 0x51 1\n242 key 0x51 0\n250 shot list.ppm\n260 key 0x28 1\n262 key 0x28 0\n290 shot network.ppm\n300 key 0x51 1\n302 key 0x51 0\n310 key 0x28 1\n312 key 0x28 0\n320 key 0x50 1\n322 key 0x50 0\n328 key 0x50 1\n330 key 0x50 0\n336 key 0x50 1\n338 key 0x50 0\n380 shot edit.ppm\n390 key 0x2a 1\n392 key 0x2a 0\n400 shot edited.ppm\n410 key 0x28 1\n412 key 0x28 0\n440 quit\n' \
    | BMA_WIFI=1 BMA_REFRESH=50 BMA_CMDLINE="root=." timeout 600 "$SIM" > log.txt 2>&1)
check_image wifiutf8 list
check_image wifiutf8 network
check_image wifiutf8 edit
check_image wifiutf8 edited
if grep -q 'ssid="KåffØst"' "$OUT/wifiutf8/wpa_supplicant.conf" 2>/dev/null; then
    pass "wifiutf8: edited name saved"
else
    fail "wifiutf8" "wpa_supplicant.conf doesn't have ssid=\"KåffØst\" (see out/wifiutf8)"
fi
run wifioff 50 "" '150 key 0x3a 1\n152 key 0x3a 0\n170 key 0x52 1\n172 key 0x52 0\n180 key 0x28 1\n182 key 0x28 0\n200 key 0x51 1\n202 key 0x51 0\n210 key 0x28 1\n212 key 0x28 0\n240 shot off.ppm\n250 quit\n'
check_image wifioff off

echo "A setting changed in the menu is saved"
# Controllers, then Keypad is Joystick 1 (four down) to switch it off.
run setting 50 "" '150 key 0x3a 1\n152 key 0x3a 0\n170 key 0x51 1\n172 key 0x51 0\n180 key 0x28 1\n182 key 0x28 0\n200 key 0x51 1\n202 key 0x51 0\n210 key 0x51 1\n212 key 0x51 0\n220 key 0x51 1\n222 key 0x51 0\n230 key 0x51 1\n232 key 0x51 0\n240 key 0x28 1\n242 key 0x28 0\n270 shot controllers.ppm\n280 quit\n'
if grep -q "^keypad_joystick=0" "$OUT/setting/bma800xl.cfg" 2>/dev/null; then
    pass "setting: bma800xl.cfg has keypad_joystick=0"
else
    fail "setting" "keypad_joystick=0 not saved to bma800xl.cfg"
fi
check_image setting controllers

echo "Logging switched to a file in the menu"
# Logging (four down from Emulator Menu), then File (two down from Off).
run logging 50 "" '150 key 0x3a 1\n152 key 0x3a 0\n170 key 0x51 1\n172 key 0x51 0\n180 key 0x51 1\n182 key 0x51 0\n190 key 0x51 1\n192 key 0x51 0\n200 key 0x51 1\n202 key 0x51 0\n210 key 0x28 1\n212 key 0x28 0\n230 key 0x51 1\n232 key 0x51 0\n236 key 0x51 1\n238 key 0x51 0\n246 key 0x28 1\n248 key 0x28 0\n270 shot menu.ppm\n280 quit\n'
check_log logging "logging: file" "the log goes to the file at once"
if grep -q "^enable_logging=file" "$OUT/logging/bma800xl.cfg" 2>/dev/null; then
    pass "logging: bma800xl.cfg has enable_logging=file"
else
    fail "logging" "enable_logging=file not saved to bma800xl.cfg"
fi
check_image logging menu

echo "Menu and file browser with a gamepad"
mkdir -p "$OUT/pad/games"
# Home, then A for the Emulator Menu and A for Run Atari Program.
run pad 50 "" '1 pad 0 0\n150 pad 0 0x100\n155 pad 0 0\n180 shot menu.ppm\n190 pad 0 1\n195 pad 0 0\n210 pad 0 1\n215 pad 0 0\n250 shot browser.ppm\n260 quit\n'
check_image pad menu
check_log pad "frame 250: saved browser.ppm" "file browser opened"

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

echo "A crash is written to the log"
mkdir -p "$OUT/crash"
python3 - "$OUT/crash/crash.xex" <<'EOF'
import sys
# LDA #$34 / LDX #$56 / CIM: the crash menu
code = bytes([0xA9, 0x34, 0xA2, 0x56, 0x02])
xex = bytes([0xFF, 0xFF, 0x00, 0x20]) + (0x2000 + len(code) - 1).to_bytes(2, "little") + code
xex += bytes([0xE0, 0x02, 0xE1, 0x02, 0x00, 0x20])
open(sys.argv[1], "wb").write(xex)
EOF
run crash 50 "autostart=crash.xex" '200 quit\n'
check_log crash 'Atari crashed: code \$02 \(CIM\) at \$2004' "the crash is logged"
check_log crash "A=34 X=56 " "the registers are logged"
check_log crash '\$1FFC: .* A9 34 A2 56 02 ' "the code around the crash is logged"

echo "A drive reply received at the wrong speed has a framing error"
# A boot disk that sends a status command through POKEY itself, as
# MyPicoDos does, and receives the reply at the drive's speed (divisor $28)
# and then at 1050 Turbo speed (divisor 6). The first must be OK, the
# second a framing error: MyPicoDos 4.05 crashed (CIM at $174D) when its
# 1050 Turbo probe succeeded (patch atari800_sio_framing_error.diff).
mkdir -p "$OUT/siospeed"
python3 - "$OUT/siospeed/sio.atr" <<'EOF'
import sys
def text(s):  # screen codes, $FF ends
    return [ord(c) - 0x20 for c in s] + [0xFF]
boot = bytes([
    0x00, 0x02, 0x00, 0x07, 0x06, 0x07,  # 2 boot sectors at $0700, run $0706
    0x78,                    # SEI
    0xA9, 0x28, 0x8D, 0x08, 0xD2,  # AUDCTL = $28 (channels 3+4, 1.79 MHz)
    0xA9, 0x00, 0x8D, 0x06, 0xD2,  # AUDF4 = 0
    0xA9, 0xA0, 0x8D, 0x07, 0xD2,  # AUDC4 = $A0 (silent)
    0xA2, 0x28, 0x20, 0x2D, 0x07,  # LDX #$28 / JSR probe
    0xA0, 0x50, 0x20, 0x9D, 0x07,  # LDY #80 / JSR show (line 2)
    0xA2, 0x06, 0x20, 0x2D, 0x07,  # LDX #$06 / JSR probe
    0xA0, 0x78, 0x20, 0x9D, 0x07,  # LDY #120 / JSR show (line 3)
    0x4C, 0x2A, 0x07,        # JMP *
    # $072D probe: status command, reply received at divisor X; returns
    # the SKSTAT bits of all 7 reply bytes ANDed (bit 7 clear: framing error)
    0xA9, 0x28, 0x8D, 0x04, 0xD2,  # AUDF3 = $28
    0xA9, 0x34, 0x8D, 0x03, 0xD3,  # PBCTL = $34 (command line on)
    0xA9, 0x23, 0x8D, 0x0F, 0xD2,  # SKCTL = $23 (send)
    0x8D, 0x0A, 0xD2,        # SKRES
    0xA0, 0x00,              # LDY #0
    0xA9, 0x00, 0x8D, 0x0E, 0xD2,  # send: IRQEN = 0
    0xA9, 0x10, 0x8D, 0x0E, 0xD2,  # IRQEN = $10 (output needed)
    0xB9, 0xB0, 0x07,        # LDA frame,Y
    0x8D, 0x0D, 0xD2,        # STA SEROUT
    0xAD, 0x0E, 0xD2, 0x29, 0x10, 0xD0, 0xF9,  # wait for output needed
    0xC8, 0xC0, 0x05, 0xD0, 0xE4,  # INY / CPY #5 / BNE send
    0xAD, 0x0E, 0xD2, 0x29, 0x08, 0xD0, 0xF9,  # wait for output done
    0x8E, 0x04, 0xD2,        # AUDF3 = X
    0xA9, 0x13, 0x8D, 0x0F, 0xD2,  # SKCTL = $13 (receive)
    0x8D, 0x0A, 0xD2,        # SKRES
    0xA9, 0x3C, 0x8D, 0x03, 0xD3,  # PBCTL = $3C (command line off)
    0xA9, 0xFF, 0x85, 0x80,  # $80 = $FF
    0xA0, 0x07,              # LDY #7 (ACK, C, 4 bytes, checksum)
    0xA9, 0x00, 0x8D, 0x0E, 0xD2,  # recv: IRQEN = 0
    0xA9, 0x20, 0x8D, 0x0E, 0xD2,  # IRQEN = $20 (input ready)
    0xAD, 0x0E, 0xD2, 0x29, 0x20, 0xD0, 0xF9,  # wait for input ready
    0xAD, 0x0F, 0xD2, 0x25, 0x80, 0x85, 0x80,  # $80 &= SKSTAT
    0x88, 0xD0, 0xE5,        # DEY / BNE recv
    0xA9, 0x00, 0x8D, 0x0E, 0xD2,  # IRQEN = 0
    0xA5, 0x80, 0x60,        # LDA $80 / RTS
    # $079D show: "OK" or "FRAMING ERROR" at SAVMSC+Y from SKSTAT bit 7
    0xA2, 0x00, 0x0A, 0xB0, 0x02, 0xA2, 0x03,  # X = 0 (OK) or 3 (error)
    0xBD, 0xB5, 0x07, 0x30, 0x06,  # print: LDA texts,X / BMI done
    0x91, 0x58, 0xE8, 0xC8, 0xD0, 0xF5,  # STA (SAVMSC),Y / INX / INY / BNE
    0x60,                    # done: RTS
    0x31, 0x53, 0x00, 0x00, 0x84,  # $07B0 frame: D1 status, checksum
] + text("OK") + text("FRAMING ERROR"))  # $07B5 texts
sectors = 720
image = boot + bytes(sectors * 128 - len(boot))
paragraphs = len(image) // 16
header = bytes([0x96, 0x02, paragraphs & 0xFF, paragraphs >> 8 & 0xFF, 0x80, 0x00,
                paragraphs >> 16]) + bytes(9)
open(sys.argv[1], "wb").write(header + image)
EOF
run siospeed 50 "autostart=sio.atr" '300 shot result.ppm\n310 quit\n'
check_image siospeed result

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

echo "USB mouse as a CX22 Trak-Ball"
mkdir -p "$OUT/trakball"
python3 - "$OUT/trakball/trak.xex" <<'EOF'
import sys
# Background colour from the trackball's direction bits in STICK0 (bit 0
# right, bit 2 down) times 16, or red while its button is held.
code = bytes([
    0xAD, 0x78, 0x02,        # loop: LDA STICK0
    0x29, 0x05,              #       AND #$05
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
# Alt+J (Controller Configuration), down twice to Mouse device, Trak-ball
# (eight down), Escape back to the program. Then the mouse moves right and
# down, left only (down stays), and clicks.
TRAK_KEYS='150 key 0xe2 1\n152 key 0x0d 1\n154 key 0x0d 0\n156 key 0xe2 0\n'
TRAK_KEYS+='170 key 0x51 1\n172 key 0x51 0\n178 key 0x51 1\n180 key 0x51 0\n190 key 0x28 1\n192 key 0x28 0\n'
for i in 0 1 2 3 4 5 6 7; do
    TRAK_KEYS+="$((210 + 6 * i)) key 0x51 1\n$((212 + 6 * i)) key 0x51 0\n"
done
TRAK_KEYS+='270 key 0x28 1\n272 key 0x28 0\n290 key 0x29 1\n292 key 0x29 0\n'
run trakball 50 "autostart=trak.xex" "${TRAK_KEYS}320 mouse 20 20 0\n350 shot rightdown.ppm\n360 mouse -20 0 0\n390 shot left.ppm\n400 mouse 0 0 1\n420 shot click.ppm\n430 quit\n"
check_image trakball rightdown
check_image trakball left
check_image trakball click

echo "The Home press that opens the menu isn't also Escape in it"
run padhold 50 "" '1 pad 0 0\n150 pad 0 0x100\n250 pad 0 0\n280 shot menu.ppm\n290 quit\n'
check_image padhold menu

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

echo "The power key (Pi 400: Fn+F10) shuts down"
run powerkey 50 "" '150 key 0x66 1\n152 key 0x66 0\n250 quit\n'
check_log powerkey "Shutting down" "the power key while playing"
check_log powerkey "power off requested" "the power key powers off"
run powerkeymenu 50 "" '150 key 0x3a 1\n152 key 0x3a 0\n180 key 0x66 1\n182 key 0x66 0\n250 quit\n'
check_log powerkeymenu "power off requested" "the power key in the menu powers off"

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
