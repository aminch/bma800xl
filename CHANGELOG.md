## 1.4.4

- Move the Pi5 Power button onto src/pi45/board.cpp
- Add PWM fan control for Pi 5

## 1.4.3

- Add support for USB paddles
- Add `fast=true` to `cmdline.txt` to ensure the Pi is running at normal speed

## 1.4.2

- R: device (850 interface and modem) to call telnet BBSes over Ethernet or Wi-Fi. See `docs/NETWORKING.md`
- Enable menu navigation from GPIO connected joysticks
- Per-port gamepad button remapping, with press-to-detect and saved action and keyboard bindings
- Named gamepad devices and controller profiles
- Device lists update when controllers are connected or disconnected
- Per-port joystick direction source, axis pair, dead zone and diagonal zone settings
- L opens the menu by default on gamepads and can be reassigned
- Document how to put the 8BitDo M30 2.4G receiver in 6-button mode if it is detected as a Switch Pro controller

## 1.4.1 – 2026-10-17

- Implement USB mouse as an Atari CX22 Trak-ball or a joystick (Alt+J, *Mouse device*)
- Smoother Trak-ball movement and more accurate pulse timing
- `build_sdcard.sh` builds several boards at once, e.g. `./build_sdcard.sh pi4 pi5`

## 1.4.0 – 2026-10-14

- Raspberry Pi Zero and Zero W support (`kernel.img`)

## 1.3.0 – 2026-10-14

- Raspberry Pi 5, Pi 500 and Pi 500+ support
  - Own display driver, with the display run at the Atari's exact refresh rate (PAL and NTSC) on either HDMI port
  - Resolution in the Video menu: Auto, 1920x1080 or 1280x720 (the Pi 5's firmware ignores `hdmi_mode`)
  - The 3D GPU draws the CRT shader and smooth scaling
  - Sound on the HDMI port with the display
  - The power button (Pi 5) and power key (Pi 500) shut down
  - Ethernet and Wi-Fi with the WebUI
- Wi-Fi left off, rather than hanging, if its firmware is missing from the SD card
- Logging is off unless `enable_logging` is set; File logs to the serial port too
- The Pi 5's serial port is on GPIO14/15, as on the other boards
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
