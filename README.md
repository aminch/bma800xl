# BMA800XL

BMA800XL is a bare metal Atari 800XL emulator for the Raspberry Pi 2, 3 and 4. It runs
the [atari800](https://github.com/atari800/atari800) emulator directly on the
hardware, with no Linux underneath, using the same approach as
[BMC64](https://github.com/randyrossi/bmc64) does for the Commodore 64.

- Boots straight into Atari BASIC
- Frames locked to the display's refresh for perfectly smooth scrolling (50 Hz
  for PAL, 60 Hz for NTSC), with the sound kept in step
- Low latency between input and picture/sound
- No shutdown needed: just switch it off (disk writes are flushed every second)
- Runs without any ROM files, using the free Altirra OS and BASIC built into
  atari800; real Atari ROMs can be added
- Emulates the 400/800, 1200XL, 800XL, 130XE, 320XE, XEGS and 5200
- Disk images (ATR, ATX, XFD, DCM, PRO), programs (XEX, COM, EXE, BAS),
  cartridges (CAR, ROM, BIN) and tapes (CAS)
- USB keyboards and gamepads, the numeric keypad as a joystick, and real
  Atari joysticks wired to the GPIO header
- A BMA800XL menu (F1) for network, controllers, video and sound settings,
  with atari800's own menus one item away for everything else
- Optional Ethernet or Wi-Fi with BMC64's web UI: manage the SD card from a
  browser (upload, download, edit config files, start programs, reboot)

## Supported hardware

Raspberry Pi 3 Model B, B+ and A+, and the Raspberry Pi Zero 2 W
(`kernel8-32.img`), and the Raspberry Pi 2 Model B (`kernel7.img`). The card
has a kernel for each and each Pi starts its own. Video and sound go out over
HDMI; sound can also
go to the 3.5mm jack.

The Raspberry Pi 4 Model B and Pi 400 (`kernel8-rpi4.img`, 64-bit) are
supported, with the display run at the Atari's exact refresh rate on either
HDMI port, and sound over HDMI or the Pi 4 B's 3.5mm jack. See
[docs/PI4.md](docs/PI4.md).

The Raspberry Pi 5, Pi 500 and Pi 500+ (`kernel_2712.img`, 64-bit) are
supported too. See [docs/PI5.md](docs/PI5.md).

## Getting started

1. Format an SD card as FAT32.
2. Copy the contents of a release zip (or of `build/sdcard/` after building) to
   the root of the card.
3. Put your Atari files on the card. The `disks`, `programs`, `carts` and
   `tapes` folders are there for them, but any folder works.
4. Boot the Pi with a USB keyboard connected. You get the BASIC `READY`
   prompt. Press **F1** for the menu, then *Run Atari Program* or *Disk
   Management* to load something.

The display is set to 720p at 50 Hz to suit a PAL Atari, which is the default.
For an NTSC Atari, see the [User Guide](docs/USER_GUIDE.md#pal-and-ntsc).
[docs/INSTALLATION.md](docs/INSTALLATION.md) has more on preparing the card,
ROMs and upgrading.

## Keys

| Key            | Does                                                 |
|----------------|------------------------------------------------------|
| F1             | BMA800XL menu, with atari800's menu one item away    |
| F2 / F3 / F4   | Option / Select / Start                              |
| F5             | Reset (Shift+F5: power off and on)                   |
| F6             | Help                                                 |
| F7, Pause      | Break                                                |
| F11            | On-screen keyboard                                   |
| F12            | Turbo on/off                                         |
| Alt+D, Alt+C, Alt+R, Alt+S, Alt+L | Disks, Cartridge, Run program, Save state, Load state |
| Numeric keypad | Joystick 1 (8/2/4/6 and diagonals, 0 or 5 to fire)   |
| ` (backtick) or Windows key | Atari (inverse video) key               |

Gamepads are joysticks 1 to 4: the D-pad or left stick moves, **A** fires,
**B** is Space, **Y** is Return, **X** is Option, **Select**/**Start** are the
Atari's Select/Start. **Home**, or **Select+Start** together, opens the menu,
where the D-pad moves, **A** chooses and **B** goes back.

See the [User Guide](docs/USER_GUIDE.md) for all the details, including the
options in `cmdline.txt` and the GPIO joystick wiring.

## Building

```
./build_sdcard.sh
```

builds everything (the kernels for both boards; `./build_sdcard.sh pi3` or
`pi2` builds one) and stages the SD card files in `build/sdcard/`. See
[docs/BUILDING.md](docs/BUILDING.md).

## Updating atari800

The atari800 sources are used from a git submodule pinned to a
release tag. Everything specific to BMA800XL lives in `src/`, so moving to a
new atari800 release is usually a matter of changing the submodule and
rebuilding. See [docs/UPDATING.md](docs/UPDATING.md).

## How it fits together

```
src/main.cpp, kernel.cpp   Circle kernel: USB, GPIO, SD card; runs atari800 on core 1
src/fbl.cpp                Display (dispmanx layer, optional CRT shader), from BMC64
src/vchiqsound*.cpp        Sound over HDMI or the headphone jack, from BMC64
src/network.cpp, webui/    Ethernet / Wi-Fi and the web UI, from BMC64
src/host.h                 What the kernel offers the emulator
src/port/                  atari800's platform layer (PLATFORM_* functions),
                           its config.h and the list of atari800 sources
third_party/atari800       atari800 (submodule)
third_party/circle-stdlib  Circle and newlib (submodule)
src/pi4/                   Pi 4 display, smooth scaling / CRT shader, HDMI sound
src/pi4kms/                Pi 4 direct display driver (docs/PI4.md)
tools/hostsim              Runs the emulator port on a PC for testing
tools/pi4scaler            Tests the Pi 4's smooth scaling and CRT shader on a PC
tools/pi4kms               Tests the Pi 4 display driver's calculations on a PC
```

## Licence

BMA800XL is free software under the GNU General Public License version 3; see
[LICENSE](LICENSE). It includes software from other projects under their own
licences; see [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).
