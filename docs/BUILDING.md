# Build Instructions

These instructions have been tested on Debian and Ubuntu (and WSL). On other
distributions, adjust the package commands.

## Prerequisites

```
sudo apt-get install build-essential git python3 curl wget xz-utils texinfo imagemagick python3-pil
```

`texinfo` is needed because newlib's build runs `makeinfo`; `wget` fetches
the Wi-Fi firmware; ImageMagick and Pillow are only used by the PC tests.

The Arm GNU Toolchain 15.2.rel1 is downloaded into
`~/.cache/bma800xl/toolchains` the first time a build needs it
(`get_gnu_toolchain.sh`): `arm-none-eabi` for the 32-bit Pi 2 and Pi 3
kernels, `aarch64-none-elf` for the 64-bit Pi 4 and 5 kernels. If it can't be
downloaded, `BMA800XL_ALLOW_ANY_TOOLCHAIN=1` lets the build use whatever
`arm-none-eabi-gcc` (or `aarch64-none-elf-gcc`) is on the PATH.

## Downloading the files

```
git clone --recursive https://github.com/aminch/bma800xl
```

or, in an existing clone, `git submodule update --init --recursive`.

## Building an SD card

```
./build_sdcard.sh
```

builds everything from clean and stages the boot partition in
`build/sdcard`, with a kernel for each board: `kernel8-32.img` for the Pi 3
and Pi Zero 2 W, `kernel7.img` for the Pi 2, `kernel.img` for the Pi Zero
and Zero W, `kernel8-rpi4.img` (with `armstub8-rpi4.bin`) for the Pi 4 B
and Pi 400, and `kernel_2712.img` for the Pi 5, Pi 500 and Pi 500+.
`./build_sdcard.sh pi3`, `./build_sdcard.sh pi2`, `./build_sdcard.sh pi0`,
`./build_sdcard.sh pi4` or `./build_sdcard.sh pi5` builds just one. Copy the contents of that folder to the root of a FAT32
formatted SD card.

While developing, `./build_sdcard.sh --skip-patches` keeps the third party
libraries already built and only rebuilds the kernel, which takes seconds.
The libraries are built for one board at a time, so without a board it
rebuilds the kernel for the board built last; a board they aren't built for
is built from clean. With
[networking](USER_GUIDE.md#networking-and-the-web-ui) on, a new kernel can
then be uploaded to the Pi from the web UI and the Pi
rebooted from there, without touching the SD card.

## The individual steps

1. `./clean_all.sh` cleans everything, including the third party builds,
   and resets Circle to its pinned revision.
2. `./make_all.sh [pi0|pi2|pi3|pi4|pi5] [--skip-patches]` applies `src/patches/` to
   Circle, builds circle-stdlib (Circle and newlib) and the Circle addons
   for one board (pi3 if none is given), then the kernel: `kernel8-32.img`
   for pi3, `kernel7.img` for pi2, `kernel.img` for pi0 (Circle's Raspberry
   Pi 1), `kernel8-rpi4.img` (64-bit) for pi4,
   which also builds Circle's `armstub8-rpi4.bin`, `kernel_2712.img`
   (64-bit) for pi5. It only needs running again after
   `clean_all.sh` or a change to the patches or circle-stdlib. To change
   board, run `clean_all.sh` first (make_all.sh refuses otherwise).
3. `source get_gnu_toolchain.sh && make` rebuilds just the kernel (for the
   Pi 4 and 5: `BMA800XL_AARCH=64 source get_gnu_toolchain.sh && make`).
4. `./make_wifi_firmware.sh DIR` puts the Wi-Fi firmware in `DIR/firmware`
   (`build_sdcard.sh` does this).

The Pi 3 kernel also runs on the Pi Zero 2 W. `config.txt` has a section
per board that picks its kernel. The Pi Zero has one core, so its kernel
runs the emulator on core 0 and looks after USB between frames, where the
other boards use core 1 for the emulator. See [PI4.md](PI4.md) and [PI5.md](PI5.md)
for how the Pi 4 and 5 builds differ.

## Testing on a PC

`tools/hostsim` builds the emulator port (atari800 plus `src/port/`) for the
PC, with a simulated display, sound and input in place of the Raspberry Pi.
It runs scripted tests: booting to BASIC, typing, the menus, gamepads,
loading a program, and checking the sound stays in step with the display.

```
tools/hostsim/run_tests.sh
```

Screenshots are written to `tools/hostsim/out/` and compared with
`tools/hostsim/expected/`; `--update` replaces the reference images.

`tools/pi4scaler` tests the shared Pi 4/5 ARM smooth scaling and CRT shader
(`src/av/scaler.*`): every pixel is checked against crt-pi worked out in
floating point, and the pictures are written to `tools/pi4scaler/out/`.

```
tools/pi4scaler/run_tests.sh
```

`tools/pi4kms` tests the Pi 4/5 direct display drivers (`src/kms/`, see
[PI4.md](PI4.md) and [PI5.md](PI5.md)): shared filter kernels, EDID, mode
choice, HDMI packets and audio clocks, and each board's display lists and
PHY settings.

```
tools/pi4kms/run_tests.sh
```

`tools/update/test/run_tests.py` builds the BMA800XL Updater (`src/update/`)
with the PC's C compiler (`cc`, or `HOST_CC`, with AddressSanitizer) and runs
it on test cards: updates, downgrades, repairs, bad packages and a power cut
at every step. With Node.js on the PATH it also tests the web UI's Update
page. `--cache DIR` adds a test with two real releases (see the script).

```
python3 tools/update/test/run_tests.py
```

`build_sdcard.sh` runs these first and stops if any test fails;
`--skip-tests` leaves them out.

## The web UI

The web UI's pages are in `src/webui/assets/`. They are compiled into the
kernel through `src/webui/webui_assets.c`, which the Makefile regenerates
with `tools/gen_webui_assets.py` whenever an asset changes.

`tools/webui_dev_server.py` serves the pages on the PC with mock `/api/*`
answers, so the whole web UI (dashboard, files, Update page) can be tried
without a Pi; the file browser shows a real folder (`--root`, the repository
by default). The browser reloads when an asset changes.

```
python3 tools/webui_dev_server.py      # http://localhost:8000/
```

## The BMA800XL Updater

The updater ([UPDATER.md](UPDATER.md)) is configured by `updater.cfg` in the
top folder:

| `updater` | What is built |
| --- | --- |
| `on` | The updater and the web UI's Update page. |
| `kernel_only` | The updater only, with no Update page: an update is a release zip copied to the card as `bma800xl-update.zip`. |
| `off` | No updater: `src/update/` and zlib are left out, `bma800xl-update.zip` is ignored, there is no Update page, the SD card gets no `bma800xl-manifest.txt`, and the updater's tests and release check are skipped. No `updater.cfg` is the same. |

`repo` is the GitHub repository whose releases are official. The Update page
lists and trusts its releases, and `make_all.sh` runs
`tools/update/gen_update_manifest.py sync`, which adds newly published
releases to `release/manifest_history.txt` and `release/release_digests.txt`;
commit those. Leave `repo` empty to never contact GitHub. A fork with its own
releases sets `repo` to its repository, empties `release/release_digests.txt`
and runs `gen_update_manifest.py seed`.

`build_sdcard.sh` writes `bma800xl-manifest.txt` (this version's files and
every earlier release's) into `build/sdcard`, so release zips carry it. See
[tools/update/GEN_UPDATE_MANIFEST.md](../tools/update/GEN_UPDATE_MANIFEST.md).

To try an update before a release:

```
./build_test_update.sh [version]
```

turns `build/sdcard` into `build/bma800xl-update.zip`, labelled one version
above `VERSION` (the same version is a repair, a lower one a downgrade). Put
it in the top folder of a card running a build with the updater, by hand or
with the web UI's Update page (which warns that it isn't an official
release), and boot.

## Layout

| Path                         | What                                        |
|------------------------------|---------------------------------------------|
| `src/main.cpp`, `kernel.*`   | Circle kernel; USB, GPIO, SD card, cores    |
| `src/network.*`              | Ethernet / Wi-Fi start up (from BMC64)      |
| `src/network_time_sync.*`    | Clock from the network (from BMC64)         |
| `src/webui/`                 | Web UI server, file manager and pages (from BMC64) |
| `src/sdcard/`                | SD card helpers used by the web UI and the updater (from BMC64) |
| `src/update/`                | The BMA800XL Updater (from BMC64)           |
| `src/fbl.*`, `crt_pi_*`      | Display layer and CRT shaders (from BMC64)  |
| `src/vchiqsound*`            | Sound through the VideoCore (from BMC64)    |
| `src/options.*`              | `cmdline.txt` options                       |
| `src/host.h`                 | Interface between kernel and emulator       |
| `src/port/platform.c`        | atari800 platform layer: display, sound, timing, main loop |
| `src/port/keyboard.c`        | Keyboard, gamepads, joysticks               |
| `src/port/config.h`          | atari800's build configuration              |
| `src/port/atari800.mk`       | Which atari800 sources are built            |
| `src/patches/`               | Changes applied to Circle by `make_all.sh`  |
| `sdcard/`                    | Files for the SD card                       |
| `third_party/atari800`       | atari800 (submodule)                        |
| `third_party/circle-stdlib`  | Circle and newlib (submodule)               |
| `third_party/zlib`           | zlib's decompression, for the updater (`tools/vendor_zlib.py`) |
| `release/`                   | The updater's history of every release's files |
| `src/av/`                   | Shared Pi 4/5 video, ARM scaling / CRT shader and sound |
| `src/v3d/`                  | Shared Pi 4/5 GPU driver and CRT rendering   |
| `src/kms/common/`           | Shared Pi 4/5 filter kernels and mode calculations |
| `src/kms/pi4/`              | Pi 4 direct display driver and PHY          |
| `src/kms/pi5/`              | Pi 5 direct display driver and PHY          |
| `tools/hostsim`              | PC test harness                             |
| `tools/pi4scaler`            | PC test of shared Pi 4/5 ARM scaling and CRT rendering |
| `tools/pi4kms`               | PC test of Pi 4/5 display driver calculations |
| `tools/update`               | The updater's release tools and PC tests     |
| `tools/webui_dev_server.py`  | The web UI on the PC, with a mock API        |
