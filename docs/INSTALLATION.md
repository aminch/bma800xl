# Installation

## What you need

- A Raspberry Pi 3 Model B, B+ or A+, a Raspberry Pi Zero 2 W, a
  Raspberry Pi 2 Model B, or a Raspberry Pi Zero or Zero W
- A microSD card (any size; BMA800XL itself needs only a few MB)
- An HDMI display, ideally one that accepts 720p at 50 Hz
- A USB keyboard and/or USB gamepad

## Prepare the SD card

1. Download `bma800xl-v<version>.zip` from the
   [releases page](https://github.com/aminch/bma800xl/releases), or build
   it yourself (see [BUILDING.md](BUILDING.md)) and use `build/sdcard/`.
2. Format the SD card as FAT32.
3. Extract the zip to the root of the card, so that the kernels
   (`kernel8-32.img`, `kernel7.img`) and `config.txt` are at the top level,
   not in a folder.
4. Copy your Atari files to the card. The `disks`, `programs`, `carts` and
   `tapes` folders are there for them, but any folder works.
5. Put the card in the Pi, connect the display and keyboard, and switch on.

There is no image to write and nothing to install on the card beyond
copying the files.

## ROM files

None are needed. atari800 includes Altirra OS and Altirra BASIC, free
replacements for the Atari's ROMs, and BMA800XL boots into them.

For the best compatibility, real Atari ROM images, from a legal source, can
be copied into the `ROM` folder. Their file names don't matter: atari800
recognises them by their contents. The useful ones are:

| ROM | Used by |
| --- | --- |
| OS rev. 2 (part no. C061598B) | 800XL, early 65XE/130XE |
| OS rev. 3 (C300717) | late 65XE/130XE |
| OS rev. B NTSC, or rev. A PAL | 400/800 |
| OS rev. 10 or 11 | 1200XL |
| BASIC rev. C (C024947A) | 800XL, XE, XEGS |
| 5200 BIOS (C019156 or C019156A) | 5200 |
| XEGS OS and Missile Command (C101687) | XEGS |

ROMs are picked up when BMA800XL starts without an `atari800.cfg` in the
root of the card. If you already have one, delete it, or choose the ROMs in
the menu (**F1**, *Emulator Menu*, *System Settings*) and use *Save
Configuration*.

## First boot

The Pi starts straight into BASIC's `READY` prompt. Press **F1** (or
**Home**, or **Select+Start** on a gamepad) for the BMA800XL menu. Load
something from *Emulator Menu*, with *Run Atari Program* or *Disk
Management*.

USB keyboards and gamepads can be plugged in and out while it runs.

The display is set to 720p at 50 Hz for a PAL Atari. If you get no picture,
or you want an NTSC Atari, see
[PAL and NTSC](USER_GUIDE.md#pal-and-ntsc) and
[Troubleshooting](USER_GUIDE.md#troubleshooting) in the User Guide.

## Networking (optional)

To manage the card's files from a browser, turn on Ethernet or Wi-Fi under
*Network* in the menu, then reboot; the web UI's address is shown at the
bottom of the screen. For Wi-Fi, *Scan for Wi-Fi Networks* lists the
networks in range and asks for the password. See
[Networking and the web UI](USER_GUIDE.md#networking-and-the-web-ui).

## Upgrading

Copy the new release's files over the old ones, except for files you have
changed: `config.txt` and `cmdline.txt` if you edited them. Your settings
(`bma800xl.cfg`, `atari800.cfg`, `wpa_supplicant.conf`), ROMs and Atari
files are not in the release zip and stay as they are. To move the card to
a Pi 2, the `config.txt` must have the `[pi2]` section that picks
`kernel7.img`; releases before Pi 2 support don't.

With networking on, a new kernel (`kernel8-32.img` for a Pi 3 or Zero 2 W,
`kernel7.img` for a Pi 2, `kernel.img` for a Pi Zero or Zero W) can also be uploaded in the web UI's file manager
and started with **Reboot**. Updates that change other files (the Pi
firmware in the root or in `firmware/`) need the card in a computer.
