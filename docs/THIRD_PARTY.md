# Third party software

BMA800XL is made from these projects. Thanks to everyone who worked on them.

| Component | Where | Licence |
|-----------|-------|---------|
| [atari800](https://github.com/atari800/atari800), the Atari 8-bit emulator | `third_party/atari800` (submodule) | GPL version 2 or later |
| Altirra OS and Altirra BASIC ROM replacements by Avery Lee, included in atari800 | `third_party/atari800/src/roms` | Free to copy and distribute with the copyright notice (see each file) |
| [Circle](https://github.com/rsta2/circle), bare metal environment for the Raspberry Pi, by Rene Stange | `third_party/circle-stdlib/libs/circle` | GPL version 3 |
| [circle-stdlib](https://github.com/smuehlst/circle-stdlib), Circle with newlib, by Stephan Mühlstrasser | `third_party/circle-stdlib` | GPL version 3 |
| newlib C library | `third_party/circle-stdlib/libs/circle-newlib` | BSD style licences (see its COPYING.NEWLIB) |
| [BMC64](https://github.com/randyrossi/bmc64) by Randy Rossi and contributors: display layer, CRT shaders, VCHIQ sound, networking and web UI, SD card helpers, Circle patches, build scripts, GPIO joystick wiring | `src/fbl.*`, `src/crt_pi_*`, `src/vchiqsound*`, `src/network*`, `src/webui/`, `src/sdcard/`, `src/patches/`, `tools/gen_webui_assets.py`, `make_wifi_firmware.sh` | Apache License 2.0 (file headers); BMC64 as a whole is GPL version 3 |
| Raspberry Pi firmware (`bootcode.bin`, `start.elf`, `fixup.dat`) | `sdcard/firmware` | Broadcom's redistributable binary licence, from the [Raspberry Pi firmware repository](https://github.com/raspberrypi/firmware) |
| Wi-Fi firmware for the Pi's wireless chip, fetched at build time | `firmware/` on the SD card | Broadcom, Cypress and Synaptics redistributable licences, copied next to the firmware |

Because it combines GPL version 2 or later and GPL version 3 code, BMA800XL
as a whole is distributed under the GNU General Public License version 3.
