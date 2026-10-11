# BMA800XL Patches

## Circle

The following patches are applied by `make_all.sh`, in this order, while
preparing the Circle dependency. They carry changes that are not part of the
checked-out upstream revision. `make_all.sh --skip-patches` (and
`build_sdcard.sh --skip-patches`) leave Circle as it is.

Patches are applied to the submodule revisions pinned by circle-stdlib v21
(Circle 51.1.1, tag `Step51.1.1`), the same revisions BMC64 uses.

| Patch | Description |
| --- | --- |
| `circle_bolt_keyboard_patch.diff` | Adds Logitech Bolt keyboard receiver support (from BMC64). |
| `circle_linux_bug_patch.diff` | Turns off `BUG_ON`. Circle's Linux emulation, which VCHIQ uses for the display and sound, asserts with `BUG_ON` that it only runs on core 0; BMA800XL calls it from core 1, where the emulator runs (as BMC64 does with VICE). Without it the Pi halts with `mutex.cpp(9): assertion failed`. From BMC64's `circle_patch.diff`. |
| `circle_sysconfig_patch.diff` | Enables multi-core support and the USB FIQ in `sysconfig.h`. From BMC64's `circle_patch.diff`. |

## atari800

atari800 is changed only when there is no other way. Its patches are made
with `git format-patch` in a clone of atari800 at the pinned tag, so they can
be sent upstream as they are. The Makefiles (through `src/port/atari800.mk`)
copy the files a patch changes into `build/atari800-patched/` (or
`tools/hostsim/build/atari800-patched/`), apply the patches there and compile
those copies; the submodule itself is never modified. Adding or removing a
patch rebuilds atari800.

| Patch | Description |
| --- | --- |
| `atari800_ui_basic_utf8.diff` | The menus show UTF-8 text, so Wi-Fi network and file names like "café" no longer come out as "cafC)". Adds a Latin-1 font (U+00A0-U+00FF) in the Atari style, with the accented letters of AltirraOS's international character set; other characters show as `?`, and bytes that aren't valid UTF-8 are taken as Latin-1. Widths, list columns and the line editor count characters, not bytes. |

