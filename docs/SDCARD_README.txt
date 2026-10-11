BMA800XL - bare metal Atari 800XL emulator for the Raspberry Pi 3
==================================================================

Copy everything here to the root of a FAT32 SD card and boot a Raspberry
Pi 3 (or Zero 2 W) from it, with a USB keyboard or gamepad connected.

It starts in Atari BASIC. Press F1 for the menu: load disks, programs,
cartridges and tapes, change the machine and settings, save states.

  F1            Menu                F5 / Shift+F5   Reset / power cycle
  F2 F3 F4      Option Select Start F6 / F7         Help / Break
  F11           On-screen keyboard  F12             Turbo
  Keypad        Joystick 1 (8 2 4 6 to move, 0 or 5 to fire)

Gamepads: D-pad and A to fire, Home (or Select+Start) for the menu.

Put your files in the disks, programs, carts and tapes folders (or
anywhere). Real Atari ROMs can go in the ROM folder; they are optional.

Video is 720p at 50 Hz for a PAL Atari. For NTSC, set hdmi_mode=4 in
config.txt and choose NTSC in the menu's System Settings.

Networking: add network=ethernet (or network=wifi, with your network in
wpa_supplicant.conf) to cmdline.txt, then open the address shown on screen
in a browser to manage the files on this card.

Emulator options are in cmdline.txt. Full documentation:
https://github.com/aminch/bma800xl/blob/main/docs/USER_GUIDE.md
