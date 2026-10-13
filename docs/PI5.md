# Raspberry Pi 5, Pi 500 and Pi 500+

BMA800XL runs on the Pi 5, Pi 500 and Pi 500+ as a 64-bit kernel,
`kernel_2712.img`. It shares the Pi 4's code (`src/pi4/`, see
[PI4.md](PI4.md)) wherever the hardware is the same; the emulator, menus,
USB keyboards and gamepads, networking (Ethernet and Wi-Fi) and the web UI
work as on the other boards.

The Pi 5's firmware is in its EEPROM, so the card only adds the Pi 5's
device trees and `overlays/bcm2712d0.dtbo`.

## Picture

The picture is the firmware's framebuffer, at the resolution the firmware
chooses for the display. Smooth scaling and the CRT shader are drawn by the
ARM cores, as on the Pi 4 with `crt_gpu=0`.

## Not yet

Sound, the 3D GPU and BMA800XL's own display driver (`video_driver`) are
for later.
