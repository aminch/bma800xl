# Building BMA800XL

BMA800XL builds on Linux (or WSL) for the Raspberry Pi 3.

## What you need

- git, make, python3, curl, xz-utils
- `texinfo` (newlib's build runs `makeinfo`)
- The Arm GNU Toolchain 15.2.rel1 (`arm-none-eabi`). `get_toolchain.sh`
  downloads it into `~/.cache/bma800xl/toolchains` the first time.
- Optional: `zip` (release archive), a C compiler and ImageMagick for the PC
  tests

On Debian or Ubuntu:

```
sudo apt-get install build-essential git python3 curl xz-utils texinfo zip imagemagick
```

## Getting the source

```
git clone --recursive https://github.com/aminch/bma800xl
```

or, in an existing clone, `git submodule update --init --recursive`.

## Building

The quick way:

```
./build.sh
```

does all of the steps below and leaves the SD card files in `sdcard_out/` and
a `bma800xl-<version>.zip`.

Step by step:

1. `./build_deps.sh pi3` patches and builds circle-stdlib (Circle and newlib)
   and the Circle addons. It takes a few minutes and only needs running again
   after updating circle-stdlib.
2. `source get_toolchain.sh && make` builds `kernel8-32.img`.
3. `make sdcard` (or `./make_sdcard.sh`) collects the SD card files.

`make clean` removes the kernel's objects; `make distclean` also removes the
compiled atari800 library and the SD card staging folder.

If the pinned toolchain can't be downloaded, `BMA800XL_ALLOW_ANY_TOOLCHAIN=1`
lets the build use whatever `arm-none-eabi-gcc` is on the PATH (for example
Ubuntu's `gcc-arm-none-eabi` package, version 13.2).

## Testing on a PC

`tools/hostsim` builds the emulator port (atari800 plus `src/port/`) for the
PC, with a simulated display, sound and input in place of the Raspberry Pi.
It runs scripted tests: booting to BASIC, typing, the menus, gamepads,
loading a program, and checking the sound stays in step with the display.

```
tools/hostsim/run_tests.sh
```

It needs a C compiler and, to compare screenshots, ImageMagick's `compare`.
Screenshots are written to `tools/hostsim/out/`.

## Layout

| Path                         | What                                        |
|------------------------------|---------------------------------------------|
| `src/main.cpp`, `kernel.*`   | Circle kernel; USB, GPIO, SD card, cores    |
| `src/fbl.*`, `crt_pi_*`      | Display layer and CRT shaders (from BMC64)  |
| `src/vchiqsound*`            | Sound through the VideoCore (from BMC64)    |
| `src/options.*`              | `cmdline.txt` options                       |
| `src/host.h`                 | Interface between kernel and emulator       |
| `src/port/platform.c`        | atari800 platform layer: display, sound, timing, main loop |
| `src/port/keyboard.c`        | Keyboard, gamepads, joysticks               |
| `src/port/config.h`          | atari800's build configuration              |
| `src/port/atari800.mk`       | Which atari800 sources are built            |
| `patches/`                   | Changes applied to Circle by `build_deps.sh` |
| `sdcard/`                    | Files for the SD card                       |
| `third_party/atari800`       | atari800 (submodule, unmodified)            |
| `third_party/circle-stdlib`  | Circle and newlib (submodule)               |
| `tools/hostsim`              | PC test harness                             |
