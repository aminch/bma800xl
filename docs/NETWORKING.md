# Calling BBSes

BMA800XL can call telnet BBSes over the Pi's network. The Atari sees an 850
interface (the R: device) with a modem on it; the modem dials internet
addresses instead of phone numbers. Any terminal program that uses R: works.
This page uses [Ice-T](https://github.com/itaych/Ice-T) and
[BobTerm](https://atari.fox-1.nl/atari-400-800-xl-xe/400-800-xl-xe-tools/bobterm-1-21/)
as examples.

## What has to be set up

1. **Network.** Join your network over Ethernet or Wi-Fi in the BMA800XL menu's
   *Network* (see *Networking and the web UI* in the
   [User Guide](USER_GUIDE.md)). The Pi's address shows on screen once it is
   connected.
2. **R: device.** In the *Emulator Menu*, *Emulator Settings*, turn on
   *R: device (Atari850 via net)*.
3. **Machine.** Ice-T needs 128K: in *System Settings*, choose the
   *Atari 130XE (128 KB)* and turn BASIC off. BobTerm runs on any machine
   with 48K and turns BASIC off itself.
4. Use *Save Configuration* in the Emulator Menu to keep these settings.

## Connecting with Ice-T

1. Copy `icet.xex` (from Ice-T's `bin` folder) to the SD card, or upload it
   with the web UI.
2. Start it with *Run Atari Program* (Alt+R), or click it in the web UI.
   Press a key at the title screen.
3. In *Settings*, choose the *Emulation* the BBS expects (*ANSI-BBS* for most
   BBSes), set *Baud rate* to the highest, *Status calls* to *Constant* and
   *Local Echo* off. For PC BBSes, set *Special* in *Options* to *ANSI colors*
   or *IBM-PC*.
4. Go to *Terminal* and type:

   ```
   ATDI bbs.example.com 23
   ```

   `CONNECT` means you are on line; `NO CARRIER` that the call failed or
   ended.

Or use Ice-T's dialer (*Options*, *Dialing*): add an entry with the text
`I bbs.example.com 23` (capital I) and press Return on it to dial.

To hang up, use Ice-T's *Hangup* (Ctrl+Shift+H in the terminal). Shift+Esc
switches between the terminal and the menus.

## Connecting with BobTerm

1. Copy BobTerm (`BOBTERM.COM`) to the SD card and start it with *Run Atari
   Program*. Turn on the R: device first, or BobTerm stops with "No modem
   handler!!!".
2. BobTerm starts in its menu. Press **A** to choose the translation:
   *Atari* (ATASCII) for Atari BBSes, *ASCII* for others. Press **B** for the
   highest baud rate and **D** for full duplex.
3. Press Return for the terminal and type:

   ```
   ATDT bbs.example.com:23
   ```

   Wait for `CONNECT`, then press Return to see the BBS.

To hang up from the terminal, wait a second, type `+++`, wait another
second, then type `ATH` and press Return. *Hang Up* (**H**) in BobTerm's
menu works too.

## Modem commands

Type these in the terminal when not connected, ending with Return:

| Command | What it does |
|---|---|
| `ATDI host port` | Call `host` on `port` (23 if left out) |
| `ATDT host:port` | The same |
| `ATH` | Hang up |
| `+++` | With a second's pause before and after: back to commands, still connected |
| `ATO` | Back on line after `+++` |
| `ATE0`, `ATE1` | Echo off, on |
| `ATZ` | Hang up and reset |

A key pressed while it dials cancels the call. Each call is written to the
log.
