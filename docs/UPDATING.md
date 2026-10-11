# Updating atari800 and Circle

## atari800

atari800 is a git submodule in `third_party/atari800`, pinned to a release
tag, and is built **unmodified**. Nothing in that folder is edited; all of
BMA800XL's own code is in `src/`. The connection points are:

| File                   | Ties BMA800XL to                                    |
|------------------------|-----------------------------------------------------|
| `src/port/atari800.mk` | The list of atari800 source files to compile        |
| `src/port/config.h`    | atari800's build options (normally made by configure) |
| `src/port/platform.c`  | `PLATFORM_*` functions from atari800's `platform.h` |
| `src/port/keyboard.c`  | `PLATFORM_Keyboard`, `PLATFORM_PORT`, `PLATFORM_TRIG` |

### Steps

1. Move the submodule to the new release:

   ```
   cd third_party/atari800
   git fetch --tags
   git checkout ATARI800_7_3_0        # the new release tag
   cd ../..
   ```

2. Look at what changed in the parts BMA800XL depends on:

   ```
   cd third_party/atari800
   git diff ATARI800_7_2_1 ATARI800_7_3_0 --stat -- src/Makefile.am configure.ac \
       src/platform.h src/sound.h src/input.h src/akey.h src/ui.h src/ui_basic.h
   ```

   - **New or removed source files** (`src/Makefile.am`): add or remove them
     in `src/port/atari800.mk`. Front ends for other systems (`sdl/`,
     `atari_x11.c` and so on) are not needed.
   - **New build options** (`configure.ac`, `AC_DEFINE`): decide whether
     they belong in `src/port/config.h`. A quick way to see the full list is
     to generate atari800's own `config.h` in a scratch copy:

     ```
     cp -r third_party/atari800 /tmp/a8 && cd /tmp/a8
     ./autogen.sh && ./configure --with-video=sdl2 --with-sound=sdl2
     grep '^#define' src/config.h
     ```

     Never run configure inside `third_party/atari800`: the generated
     `src/config.h` would be used instead of BMA800XL's (the build stops if
     it finds one).
   - **Changed `PLATFORM_*` functions** (`platform.h`): update
     `src/port/platform.c` or `src/port/keyboard.c`. atari800's SDL port
     (`src/sdl/`) is the reference implementation to compare with.

3. Build and test:

   ```
   tools/hostsim/run_tests.sh      # on the PC
   ./build.sh                      # the Pi kernel
   ```

   If the screenshots differ only because the new release looks different
   (a new Altirra ROM version string, for example), refresh the reference
   images with `tools/hostsim/run_tests.sh --update` and check the new ones
   by eye.

4. Commit the submodule change together with any port changes:

   ```
   git add third_party/atari800 src/port
   git commit -m "Update atari800 to 7.3.0"
   ```

## circle-stdlib (Circle and newlib)

circle-stdlib is a submodule in `third_party/circle-stdlib`, pinned to the
same revision BMC64 uses. `build_deps.sh` applies the patches in `patches/`
to Circle before building:

- `circle_sysconfig_patch.diff` enables multi-core and the USB FIQ
- `circle_linux_bug_patch.diff` turns off `BUG_ON`, whose checks in Circle's
  Linux emulation (used by VCHIQ for display and sound) only allow core 0;
  the emulator calls them from core 1, as BMC64 does
- `circle_bolt_keyboard_patch.diff` adds Logitech Bolt keyboard support
  (from BMC64)

To update, move the submodule to a new circle-stdlib release, run
`git submodule update --init --recursive`, then `./build_deps.sh pi3`. If a
patch no longer applies, update it from the matching BMC64 patch or by
hand. `src/fbl.cpp` and `src/vchiqsoundbasedevice.cpp` use Circle's VC4
addon and may need the same changes BMC64 makes when it updates Circle.
