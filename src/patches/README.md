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

atari800 is changed only when there is no other way. Its patches are plain
diffs against the pinned tag, described in the table below.
`tools/apply_atari800_patches.sh` resets the
`third_party/atari800` submodule and applies them in it, like the Circle
patches; `make_all.sh` and `tools/hostsim/run_tests.sh` run it, and with
`--skip-patches` leave the submodule as it is, so changes made there by hand
are kept. `git -C third_party/atari800 diff` shows what is patched.

| Patch | Description |
| --- | --- |
| `atari800_sio_framing_error.diff` | A reply from an emulated drive that POKEY receives at a speed the drive doesn't use arrives garbled with a framing error, as on a real Atari. atari800 only checked the speed of command frames, so a loader probing for a faster drive always found one: MyPicoDos 4.05 took the drive for a 1050 Turbo, read the wrong sectors and crashed (CIM at `$174D`). Now its probe fails and it uses XF551 high speed. |
| `atari800_ui_basic_utf8.diff` | The menus show UTF-8 text, so Wi-Fi network and file names like "café" no longer come out as "cafC)". Adds a Latin-1 font (U+00A0-U+00FF) in the Atari style, with the accented letters of AltirraOS's international character set; other characters show as `?`, and bytes that aren't valid UTF-8 are taken as Latin-1. Widths, list columns and the line editor count characters, not bytes. |
| `atari800_ui_basic_utf8_typing.diff` | Characters with no Atari key (å, ø, æ on a Norwegian keyboard) can be typed in the menus: `PLATFORM_Keyboard()` returns `UI_BASIC_KEY_CHAR(c)` for them and the line editor inserts them as UTF-8. Needs the patch above. |

