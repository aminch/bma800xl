# BMA800XL User Guide

## The SD card

| File or folder   | What it is                                                 |
|------------------|------------------------------------------------------------|
| `kernel8-32.img` | The emulator                                               |
| `bootcode.bin`, `start.elf`, `fixup.dat` | Raspberry Pi firmware              |
| `config.txt`     | Raspberry Pi settings: video mode                          |
| `cmdline.txt`    | Emulator options (see below)                               |
| `bma800xl.cfg`   | BMA800XL's settings, written by the BMA800XL menu          |
| `atari800.cfg`   | atari800's settings, written by *Save Configuration* in atari800's menu |
| `ROM/`           | Optional real Atari ROM images                             |
| `disks/`, `programs/`, `carts/`, `tapes/`, `states/` | Suggested places for your files |

The card must be FAT32. BMA800XL writes to it when you save the
configuration, save state, or write to a disk image from the Atari. Open
files are flushed to the card every second, so it is safe to switch off at
any time, except in the second after something was written.

## The menu

Press **F1** (or **Home** on a gamepad, or **Select+Start** together) for the
BMA800XL menu. It shows the machine and display, the network status, and
BMA800XL's own settings:

| Item | What it does |
|------|--------------|
| About BMA800XL, Licence | Version and credits; licence terms |
| Network | Ethernet or Wi-Fi, **Scan for Wi-Fi Networks**, the Wi-Fi network, password and country (written to `wpa_supplicant.conf`), the web UI and its PIN, the time zone |
| Emulator Menu (atari800) | atari800's own menu (see below) |
| Controllers | Which gamepad is which joystick, the keypad and arrow keys as joystick 1, GPIO joysticks, the keyboard layout, and a **Joystick Test** that shows live what the Atari sees on each joystick port |
| Video | Locking frames to the display, the border, the CRT shader, smooth scaling |
| Sound | HDMI or headphones, the sound buffer |
| Reboot, Shut Down | |

Settings are saved to `bma800xl.cfg` as soon as you change them. Most take
effect at once; those marked *(reboot)*, and the network settings, need a
reboot, which the menu offers when you leave it.

**Emulator Menu** opens atari800's menu, which has everything about the
Atari: loading disks, programs, cartridges and tapes, choosing the machine
(System Settings), sound, display colours, save states and so on. The Alt
shortcuts (Alt+D for disks, Alt+R to run a program, and so on) go straight
to it. **Esc** goes back to the Atari. Settings changed there last until you
switch off; use *Save Configuration* to keep them in `atari800.cfg`.

In the file browser, use the arrow keys or D-pad, **Return**/**A** to choose,
**Esc**/**B** to go back. Typing a letter jumps to files starting with it.

## Keyboard

The keys follow atari800's usual layout:

| Key                    | Atari                                         |
|------------------------|-----------------------------------------------|
| F1                     | BMA800XL menu (atari800's is one item away)   |
| F2                     | Option                                        |
| F3                     | Select                                        |
| F4                     | Start                                         |
| F5                     | Reset (warm start)                            |
| Shift+F5               | Power off and on (cold start)                 |
| F6                     | Help                                          |
| F7, Pause/Break        | Break                                         |
| F11                    | On-screen keyboard (pick a key with the arrows) |
| F12                    | Turbo (run as fast as possible) on/off        |
| ` (backtick), left Windows key | Atari key (inverse video); on a Norwegian keyboard only the Windows key, as \| is on the backtick key |
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
on a US keyboard. The layout is chosen in the menu's *Controllers*, or with
`keyboard=uk` (UK) or `keyboard=no` (Norwegian) in `cmdline.txt`. On a
Norwegian keyboard AltGr (right Alt) types `@ $ { [ ] } \`, and `^` types
at once. Characters the Atari has no key for, such as `å ø æ £`, can only be
typed in the menus (Wi-Fi network names and passwords, file names).

To boot a game with BASIC switched off, hold **F2** (Option) while it starts,
or turn BASIC off in *System Settings*.

## Joysticks

Each of the Atari's joystick ports takes input from:

| Atari port | Inputs                                                     |
|------------|------------------------------------------------------------|
| 1          | The first gamepad used, GPIO joystick 1, numeric keypad, arrow keys if enabled |
| 2          | The second gamepad used, GPIO joystick 2                   |
| 3, 4       | The third and fourth gamepads used (for the 400/800, or MultiJoy) |

Gamepads get a joystick port the first time a button or direction is
pressed, in that order. Some USB devices also show up as a gamepad that
never sends anything (the 8BitDo Retro Keyboard's receiver does), and this
way they don't take a port. The serial log says which gamepad became which
joystick. To fix the assignment instead, choose a gamepad for each joystick
in the menu's *Controllers*; the gamepad numbers are the ones in the log
(`Gamepad 2 connected: ...`). *Joystick Test* there shows what each port
receives.

**Numeric keypad**: 8/2/4/6 and 7/9/1/3 for diagonals, 0 or 5 to fire. On by
default; `keypad_joystick=0` turns it off.

**Arrow keys**: with `arrow_joystick=1` the arrow keys are joystick 1 and
right Ctrl or right Alt fires (only right Ctrl with a Norwegian layout,
where right Alt is AltGr). Useful on keyboards without a numeric keypad.
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
2. In the Emulator Menu, *System Settings*, set the video system to NTSC and *Save
   Configuration* (or put `tv=ntsc` in `cmdline.txt`).

If the display and the Atari don't match (a PAL Atari on a 60 Hz display),
the emulator still runs at the right speed but frames can't be locked to the
display, so scrolling isn't as smooth.

## Networking and the web UI

BMA800XL can join your network over Ethernet or Wi-Fi and serve a web page
for managing the SD card from any browser on the same network, so files can
be copied to and from the Pi without taking the card out. It is the web UI
from BMC64.

Turn it on in the menu's *Network*. For Ethernet, set *Network* to Ethernet
and reboot. For Wi-Fi, choose *Scan for Wi-Fi Networks*: the first time,
it offers to switch Wi-Fi on and reboot; after that it lists the networks
it can hear, strongest first. Pick yours: it fills in *Wi-Fi Network*, and
if the network needs a password, *Wi-Fi Password* is next. Then reboot.
Set *Wi-Fi Country* to your two letter country code too; it decides which
channels are allowed. A hidden network can be typed in under *Wi-Fi
Network* instead. Or, in `cmdline.txt` (on the same single line as the other options):

- Ethernet: `network=ethernet`
- Wi-Fi: `network=wifi`, and copy `wpa_supplicant.conf.example` to
  `wpa_supplicant.conf` in the root of the card with your network's name,
  password and country.

Wi-Fi needs the files in the `firmware` folder, which `build_sdcard.sh` puts
there.

Once the Pi has an address, it is shown at the bottom of the screen for a
few seconds (`WEB UI: HTTP://192.168.1.50/`) and written to the serial log.
Open that address in a browser.

The web UI has:

- **Dashboard**: the Pi's model, address, uptime, temperature, power and
  throttling warnings and free space on the card. Buttons to **Hard Reset**
  the Atari (power it off and on), **Reboot** BMA800XL, **Shut Down** (the
  Pi halts; switch it off, then on again to restart), and **Disable Web UI**
  until the next reboot.
- **Files**: browse the SD card; upload (or drag and drop) files into a
  folder, download, rename, delete, and create folders. Clicking a disk
  image, program, cartridge or tape **starts it** on the Atari, the same as
  *Run Atari Program*. `cmdline.txt`, `config.txt`, `bma800xl.cfg`,
  `atari800.cfg` and `wpa_supplicant.conf` can be **edited** in the browser; changes take
  effect after a reboot, and the previous version is kept as `.bak`.

Uploads can't overwrite `cmdline.txt`, `config.txt` or `wpa_supplicant.conf`
(edit them instead), or anything in `firmware`. Everything else, including
`kernel8-32.img`, can be replaced, so a new build of BMA800XL can be
uploaded and started with **Reboot**. A mistake in `cmdline.txt` or
`config.txt` can stop the Pi starting, and then needs the card in a computer
(the `.bak` copy makes that easy).

The web UI is open to anyone on your network. To protect it with a PIN, set
`webui_pin=1234` in `cmdline.txt`; the browser then asks for a user name
(anything) and the PIN. To network without the web UI, set `webui=0`.

The network also sets the Pi's clock, so files saved on the card get the
right date and time. Set `timezone` to your offset from UTC in minutes, for
example `timezone=600` for UTC+10.

## Options

`cmdline.txt` is a single line of `name=value` options separated by spaces.
The BMA800XL menu saves the options it changes to `bma800xl.cfg`, one
`name=value` per line, and those win over `cmdline.txt`. Either file can be
edited from the web UI.

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
| `audio_out`         | `auto`, `hdmi`, `headphones` | `auto`   | Where the sound goes |
| `volume`            | 0 to 100                     | `100`    | Sound volume |
| `audio_buffer_ms`   | 10 or more                   | `40`     | Sound buffer; raise it if the sound breaks up |
| `keyboard`          | `us`, `uk`, `no`             | `us`     | PC keyboard layout |
| `joy1_pad` ... `joy4_pad` | `0` automatic, `1`-`4` a gamepad, `-1` none | `0` | Gamepad for each Atari joystick port |
| `keypad_joystick`   | `0`, `1`                     | `1`      | Numeric keypad as joystick 1 |
| `arrow_joystick`    | `0`, `1`                     | `0`      | Arrow keys as joystick 1 |
| `gpio_joysticks`    | `0`, `1`                     | `0`      | Real joysticks on the GPIO header |
| `serial_monitor`    | `0`, `1`                     | `0`      | Allow atari800's monitor over the serial port (GPIO14/15, 115200 baud) |
| `network`           | `off`, `ethernet`, `wifi`    | `off`    | Networking (see Networking and the web UI) |
| `webui`             | `0`, `1`                     | `1`      | Web UI when networking is on |
| `webui_pin`         | digits                       | none     | PIN for the web UI |
| `timezone`          | minutes east of UTC          | `0`      | Time zone for the network clock |
| `usbpowerdelay`     | milliseconds                 | `1000`   | Circle: wait for USB devices to power up |


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
