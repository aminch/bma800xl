## 1.3.0 – 2026-10-14

- Raspberry Pi 5, Pi 500 and Pi 500+ support
  - Own display driver, with the display run at the Atari's exact refresh rate (PAL and NTSC) on either HDMI port
  - Resolution in the Video menu: Auto, 1920x1080 or 1280x720 (the Pi 5's firmware ignores `hdmi_mode`)
  - The 3D GPU draws the CRT shader and smooth scaling
  - Sound on the HDMI port with the display
  - The power button (Pi 5) and power key (Pi 500) shut down
  - Ethernet and Wi-Fi with the WebUI
- Wi-Fi left off, rather than hanging, if its firmware is missing from the SD card
- The log file is complete after a crash, a hang or switching off

## 1.2.0 – 2026-10-13

- Raspberry Pi 4 B and Pi 400: the 3D GPU draws the CRT shader and smooth scaling (`crt_gpu=0` in `cmdline.txt` for the ARM)
  - CRT shader with curvature and smooth scaling at the full 1080 lines
- CRT Curvature in the Video menu
- `config.txt`: 1080p recommended for the CRT shader
- Fix Pi 2/3 starting in 1080p instead of 720p 50 Hz

## 1.1.0 – 2026-10-12

- Raspberry Pi 4 B and Pi 400 support
  - Own display driver, with the display run at the Atari's exact refresh rate (PAL and NTSC) on either HDMI port
  - CRT shader and smooth scaling
  - Sound on either HDMI port or the headphone jack
  - Pi 400 Shut Down switches it off, Fn+F10 shuts down and starts it again
- Fix smooth setting bug for Pi2/3 (stuck on)

## 1.0.1 – 2026-10-11

- Raspberry Pi 2 support

## 1.0.0 – 2026-10-11

- Inital release: atari800 bare metal on the Raspberry Pi 3 and Zero 2 W
- Boots into BASIC, no ROM files needed
- Frames locked to the display refresh
- USB keyboards and gamepads, GPIO joysticks
- BMA800XL menu
- Ethernet / Wi-Fi with the WebUI
- CRT shaders
- Keyboard layouts (US, UK, Norwegian)
- Logging to UART or file
