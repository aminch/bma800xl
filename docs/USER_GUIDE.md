# BMA800XL User Guide

## The SD card

| File or folder   | What it is                                                 |
|------------------|------------------------------------------------------------|
| `kernel8-32.img` | The emulator                                               |
| `bootcode.bin`, `start.elf`, `fixup.dat` | Raspberry Pi firmware              |
| `config.txt`     | Raspberry Pi settings: video mode                          |
| `cmdline.txt`    | Emulator options (see below)                               |
| `atari800.cfg`   | atari800's settings, written by *Save Configuration* in the menu |
| `ROM/`           | Optional real Atari ROM images                             |
| `disks/`, `programs/`, `carts/`, `tapes/`, `states/` | Suggested places for your files |

The card must be FAT32. BMA800XL writes to it when you save the
configuration, save state, or write to a disk image from the Atari. Open
files are flushed to the card every second, so it is safe to switch off at
any time, except in the second after something was written.

## The menu

Press **F1** (or **Home** on a gamepad, or **Select+Start** together) for
atari800's menu. Everything is in there: loading disks, programs, cartridges
and tapes, choosing the machine (System Settings), sound, display colours,
save states and so on. **Esc** goes back.

Settings changed in the menu last until you switch off. Use *Save
Configuration* to keep them; they are stored in `atari800.cfg`.

In the file browser, use the arrow keys or D-pad, **Return**/**A** to choose,
**Esc**/**B** to go back. Typing a letter jumps to files starting with it.

## Keyboard

The keys follow atari800's usual layout:

| Key                    | Atari                                         |
|------------------------|-----------------------------------------------|
| F1                     | Menu                                          |
| F2                     | Option                                        |
| F3                     | Select                                        |
| F4                     | Start                                         |
| F5                     | Reset (warm start)                            |
| Shift+F5               | Power off and on (cold start)                 |
| F6                     | Help                                          |
| F7, Pause/Break        | Break                                         |
| F11                    | On-screen keyboard (pick a key with the arrows) |
| F12                    | Turbo (run as fast as possible) on/off        |
| ` (backtick), left Windows key | Atari key (inverse video)             |
| Caps Lock              | Caps                                          |
| Home                   | Clear                                         |
| Insert, Delete         | Insert/delete character (with Shift: line)    |
| Page Up / Page Down    | Shift+F1 / Shift+F2 (1200XL function keys)    |
| Arrow keys             | Cursor keys (the Atari's Ctrl+arrows)         |
| Alt+R                  | Run Atari program                             |
| Alt+D                  | Disk management                               |
| Alt+C                  | Cartridge management                          |
| Alt+T                  | Tape management                               |
| Alt+Y                  | System settings                               |
| Alt+O                  | Sound settings                                |
| Alt+S / Alt+L          | Save state / load state                       |
| Alt+J                  | Controller configuration                      |
| Alt+A                  | About                                         |

Characters are typed as they appear on the PC keyboard, so Shift+2 gives `@`
on a US keyboard. Set `keyboard=uk` in `cmdline.txt` for a UK keyboard.

To boot a game with BASIC switched off, hold **F2** (Option) while it starts,
or turn BASIC off in *System Settings*.

## Joysticks

Each of the Atari's joystick ports takes input from:

| Atari port | Inputs                                                     |
|------------|------------------------------------------------------------|
| 1          | Gamepad 1, GPIO joystick 1, numeric keypad, arrow keys if enabled |
| 2          | Gamepad 2, GPIO joystick 2                                 |
| 3, 4       | Gamepads 3 and 4 (for the 400/800, or MultiJoy)            |

**Numeric keypad**: 8/2/4/6 and 7/9/1/3 for diagonals, 0 or 5 to fire. On by
default; `keypad_joystick=0` turns it off.

**Arrow keys**: with `arrow_joystick=1` the arrow keys are joystick 1 and
right Ctrl or right Alt fires. Useful on keyboards without a numeric keypad.
The arrows then no longer move the Atari's cursor.

**Gamepads**: in a game, the D-pad or left stick is the joystick, and

| Button          | Atari                     |
|-----------------|---------------------------|
| A               | Fire                      |
| B               | Space                     |
| Y               | Return                    |
| X               | Option                    |
| Select          | Select                    |
| Start           | Start                     |
| Home, or Select+Start | Menu                |

In the menu, the D-pad moves, **A** chooses, and **B** or **Start** goes
back.

Most USB gamepads work, including Xbox and PlayStation pads and 8BitDo
controllers. Pads Circle doesn't know report their buttons in their own
order; BMA800XL then assumes buttons 1-4 are the face buttons, 9 is Select
and 10 is Start, which suits most simple USB pads.

**Real Atari joysticks on the GPIO header**: set `gpio_joysticks=1` in
`cmdline.txt`. The wiring is the same as BMC64's GPIO Config 1, so existing
BMC64 joystick adapters work. Each line is connected to ground when active.

| Joystick | Up     | Down   | Left   | Right  | Fire   |
|----------|--------|--------|--------|--------|--------|
| Port 1   | GPIO23 | GPIO27 | GPIO17 | GPIO18 | GPIO22 |
| Port 2   | GPIO20 | GPIO19 | GPIO16 | GPIO13 | GPIO26 |

Atari joystick connector (DB9): pin 1 up, 2 down, 3 left, 4 right, 6 fire,
8 ground.

## PAL and NTSC

BMA800XL emulates a PAL Atari unless told otherwise, and `config.txt` sets the
display to 50 Hz to match. When the display's refresh rate is within 2% of
the Atari's, each Atari frame is shown on exactly one display refresh, which
makes scrolling perfectly smooth.

For an NTSC Atari:

1. In `config.txt`, set `hdmi_mode=4` (720p at 60 Hz).
2. In the menu, *System Settings*, set the video system to NTSC and *Save
   Configuration* (or put `tv=ntsc` in `cmdline.txt`).

If the display and the Atari don't match (a PAL Atari on a 60 Hz display),
the emulator still runs at the right speed but frames can't be locked to the
display, so scrolling isn't as smooth.

## cmdline.txt options

`cmdline.txt` is a single line of `name=value` options separated by spaces.

| Option              | Values                       | Default  | What it does |
|---------------------|------------------------------|----------|--------------|
| `machine`           | `800`, `1200xl`, `xl`, `130xe`, `320xe`, `xegs`, `5200` | from `atari800.cfg` | Machine to emulate (overrides the saved setting) |
| `tv`                | `pal`, `ntsc`                | from `atari800.cfg` | Video system (overrides the saved setting) |
| `basic`             | `on`, `off`                  | from `atari800.cfg` | Built in BASIC (overrides the saved setting) |
| `autostart`         | path, e.g. `/disks/game.atr` | none     | Disk, program, cartridge or tape to start at boot |
| `border`            | `none`, `normal`, `full`     | `normal` | How much of the border to show |
| `aspect`            | number                       | `1.0`    | Multiplies the width of the picture |
| `scale`             | 0.1 to 1.0                   | `1.0`    | Height of the picture as a fraction of the screen |
| `smooth`            | `0`, `1`                     | `0`      | Bilinear filtering instead of sharp pixels |
| `crt_shader`        | `0`, `1`                     | `0`      | CRT look (scanlines) from BMC64 |
| `crt_curvature`     | `0`, `1`                     | `0`      | With `crt_shader`, a curved screen |
| `vsync`             | `auto`, `on`, `off`          | `auto`   | Lock frames to the display (see PAL and NTSC) |
| `audio_out`         | `auto`, `hdmi`, `analog`     | `auto`   | Where the sound goes |
| `volume`            | 0 to 100                     | `100`    | Sound volume |
| `audio_buffer_ms`   | 10 or more                   | `40`     | Sound buffer; raise it if the sound breaks up |
| `keyboard`          | `us`, `uk`                   | `us`     | PC keyboard layout |
| `keypad_joystick`   | `0`, `1`                     | `1`      | Numeric keypad as joystick 1 |
| `arrow_joystick`    | `0`, `1`                     | `0`      | Arrow keys as joystick 1 |
| `gpio_joysticks`    | `0`, `1`                     | `0`      | Real joysticks on the GPIO header |
| `serial_monitor`    | `0`, `1`                     | `0`      | Allow atari800's monitor over the serial port (GPIO14/15, 115200 baud) |
| `usbpowerdelay`     | milliseconds                 | `1000`   | Circle: wait for USB devices to power up |

The border, keypad and arrow joystick settings are also kept in
`atari800.cfg` (`BMA800XL_BORDER`, `BMA800XL_KEYPAD_JOYSTICK`,
`BMA800XL_ARROW_JOYSTICK`); `cmdline.txt` wins when both are set.

## ROMs

atari800 includes Altirra OS and Altirra BASIC, free replacements for the
Atari's ROMs, so BMA800XL works without any ROM files. They are very
compatible, but a few programs need the original ROMs.

To use real ROMs, copy the images into the `ROM` folder (or the root of the
card) and start BMA800XL. atari800 recognises them by their contents, not
their names, and picks them up the first time it starts without an
`atari800.cfg`; if you already have one, delete it or use *System Settings*
in the menu to choose the ROMs, then *Save Configuration*.

## Serial log

BMA800XL logs what it is doing to the serial port (GPIO14 TX, GPIO15 RX,
115200 baud), which helps when something doesn't work.

## Troubleshooting

**No picture**: uncomment `hdmi_safe=1` in `config.txt`. If that works, your
display doesn't accept 720p at 50 Hz; try `hdmi_mode=4` (60 Hz).

**No sound over HDMI**: try `hdmi_drive=2` in `config.txt`, or
`audio_out=hdmi` in `cmdline.txt`.

**Sound breaks up**: raise `audio_buffer_ms` in `cmdline.txt`, e.g. to 60.

**Keyboard or gamepad not working**: unplug it and plug it in again; devices
can come and go while the emulator runs. Some keyboards need a longer
`usbpowerdelay`.

**A game crashes or doesn't load**: try the real Atari ROMs (see ROMs), turn
BASIC off (hold F2 while it boots), or choose a different machine in *System
Settings*.
