# Raspberry Pi 5, Pi 500 and Pi 500+

BMA800XL runs on the Pi 5, Pi 500 and Pi 500+ as a 64-bit kernel,
`kernel_2712.img`. It shares the Pi 4's code (`src/pi4/`, see
[PI4.md](PI4.md)) wherever the hardware is the same; the emulator, menus,
USB keyboards and gamepads, networking (Ethernet and Wi-Fi) and the web UI
work as on the other boards.

The Pi 5's firmware is in its EEPROM, so the card only adds the Pi 5's
device trees and `overlays/bcm2712d0.dtbo`.

## Picture

BMA800XL shows the picture through the Pi 5's HVS with its own display
driver (`src/pi5kms/`), as on the Pi 4 (see `video_driver` in
[PI4.md](PI4.md)), at the resolution the firmware chooses for the display;
`video_driver=firmware` uses the firmware's framebuffer instead, and it is
the fall back if the driver can't take over. The CRT shader, and smooth
scaling on the framebuffer, are drawn by the Pi 5's 3D GPU (V3D 7.1), with
the same options as on the Pi 4 (`crt_gpu`, `crt_gpu_test`); if the GPU
doesn't start, the ARM cores draw them.

## Sound

Sound goes out over HDMI, on the port the display is plugged into (HDMI0
if both are). The Pi 5 and Pi 500 have no headphone jack.

## Shut Down

*Shut Down* (menu, web UI, a keyboard's power key, or a short press of the
Pi 5's power button or the Pi 500's power key) halts the Pi; the power
button or key starts it again.

## Not yet

Setting the video mode to the Atari's frame rate (`video_driver=kms`) is
for later: on the Pi 5 `kms` keeps the firmware's mode, as `hvs` does.
