# Raspberry Pi 4 B and Pi 400

BMA800XL runs on the Pi 4 B and Pi 400 as a 64-bit kernel,
`kernel8-rpi4.img`. The emulator, menus, USB keyboards and gamepads,
networking and the web UI work as on the Pi 3.

## How it differs from the Pi 2 and 3

| | Pi 2 and 3 | Pi 4 B and Pi 400 |
|---|---|---|
| Kernel | 32-bit | 64-bit, with Circle's `armstub8-rpi4.bin` |
| Picture | VideoCore dispmanx layer | BMA800XL's own display driver, or the firmware's framebuffer |
| Smooth scaling | The GPU | The display hardware's scaler (the HVS) |
| CRT shader | crt-pi on the GPU | crt-pi recalculated on the ARM cores |
| Sound | VideoCore audio service | HDMI (either port) or the Pi 4 B's headphone jack |

The emulator itself (`src/port/`) is the same on every board.

## Picture

BMA800XL has its own driver for the Pi 4's display hardware
(`src/kms/pi4/`, with shared calculations in `src/kms/common/`). It takes
over the HDMI port in use (HDMI0 if both are),
shows the Atari picture through the HVS, the Pi's hardware scaler, and can
run the display at exactly the Atari's frame rate. It is chosen with
`video_driver` in `cmdline.txt`:

| `video_driver` | What it does |
|---|---|
| `kms` (default) | As `hvs`, and also sets the video mode so the display runs at the Atari's rate: 49.86 Hz for PAL, 59.92 Hz for NTSC, switching when the machine does. This works for 720p and 1080p when the display lists the 50 Hz or 60 Hz mode; otherwise the firmware's mode is kept. |
| `hvs` | Keeps the mode set in `config.txt`. Use it for a TV that goes blank for a while when the refresh rate changes. |
| `firmware` | The firmware's framebuffer, with the picture scaled by the ARM. Use it if the others give trouble. |

If something isn't as expected (an unknown mode, a step that doesn't
complete), the driver falls back to the firmware's mode or framebuffer.
The log (serial, or `bma800xl.log`) shows what happened, for example:

```
kms: HDMI0, HVS channel 1, 1280x720
kms: EDID (HDMI0): 2 block(s), HDMI, 9 video modes
kms: Video mode VIC 19, 1280x720 at 74043228 Hz pixel clock (49.860 Hz)
bma800xl: Display refresh rate: 49.861 Hz
```

The driver was written by examining the Linux `vc4` driver and the BCM2711
device tree, documenting the hardware's behaviour, and writing BMA800XL's
own code from that.

A bare metal kernel can't use the Pi 4's OpenGL ES, so BMA800XL drives
the Pi 4's 3D GPU itself (`src/v3d/`, shared with the Pi 5) to draw the smooth and CRT
pictures (`smooth`, `crt_shader`, `crt_curvature`): crt-pi, with the
settings BMA800XL uses on the Pi 3, at up to 1080 lines and scaled up to
fill larger screens. If the GPU doesn't start or can't draw a picture,
ARM cores 1 to 3 draw it instead (`src/av/scaler.cpp`; up to 1080 lines,
720 with curvature and smooth both on); `crt_gpu=0` always uses the ARM.
The log's `v3d:` lines and *Drawn by* line say which. `crt_gpu_test=1`
also compares the GPU's pictures with the ARM's at start up.

The CRT shader looks best at 1080p: `hdmi_mode=31` in `config.txt`
(`hdmi_mode=16` for NTSC). At 720p an Atari line is three pixels high,
which curvature turns into faint rings. Sharp pixels are shown at whole
multiples of the Atari's size, so at 1080p they have black bars above and
below.

## Sound

`audio_out` chooses where the sound goes:

- `auto` (the default): HDMI, on the port the display is plugged into
  (HDMI0 if both are); the headphone jack if the display takes no sound.
- `hdmi`.
- `headphones`: the Pi 4 B's 3.5 mm jack. The Pi 400 has none.

## Shut Down

*Shut Down* (menu, web UI, or a keyboard's power key, which is Fn+F10 on
the Pi 400) halts the Pi the way Linux powers it off. The Pi 400 switches
itself off; Fn+F10 turns it back on. The Pi 4 B halts; unplug it, or
connect GPIO3 to ground, to start it again.
