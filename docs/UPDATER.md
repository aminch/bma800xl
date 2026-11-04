# The BMA800XL Updater

BMA800XL can update itself from a release zip. It never checks for,
downloads or installs anything on its own: you start every update.

Updating needs a BMA800XL version that includes the updater. It can install
any official release zip, including releases from before the updater (see
[Older versions](#older-versions)).

## Starting an update

### With the web UI

1. Open the web UI and choose **Update**. It shows the installed version and
   the latest stable and pre-release versions on GitHub.
2. Click **Download** next to the one you want. Your browser saves the zip.
3. Drop the zip onto the page (or click the box and choose it). The page
   checks that it is a complete, official BMA800XL release that matches
   GitHub, and that it fits on the card, then copies it to the card as
   `bma800xl-update.zip`.
4. Click **Reboot to update**, or reboot whenever it suits you.

The page recognises official releases from GitHub, and from a list built
into BMA800XL, which also covers pre-releases no longer on GitHub and works
when GitHub can't be reached. If the zip doesn't match an official release
(for example a test build), the page warns you, and you upload it with
**Upload anyway, at my own risk**. Installing such a zip is at your own risk.

Only the page talks to GitHub (your browser fetches the release list);
BMA800XL itself never connects to GitHub.

### By copying the file yourself

Copy the release zip to the top folder of the SD card and rename it to
`bma800xl-update.zip`. Then start BMA800XL.

## The update screen

When BMA800XL starts and finds `bma800xl-update.zip`, it shows the
**BMA800XL UPDATE** screen (or **DOWNGRADE** for an older version, **REPAIR**
for the same version) before the Atari starts. It lists every file that
differs from the new version:

| Label | Meaning | Default |
| --- | --- | --- |
| **Update** | A file from an earlier release that you haven't changed | Replace |
| **New** | A file the new version adds | Add |
| **Changed** | A file you changed (it matches no release) | Keep |

Use Up/Down (keyboard, gamepad or joystick) to move, and Return, Space, Fire
or the gamepad's A button to switch a file between Replace and Keep. A group
heading switches the whole group. `start.elf` and `fixup.dat` always switch
together, as do the Pi 4's `start4.elf` and `fixup4.dat`. Replacing
`config.txt` or `cmdline.txt` loses your changes to it; the settings saved
from the menu are in `bma800xl.cfg`, which is never replaced.

Then choose:

- **Update now and reboot**: after a confirmation, the chosen files are
  unpacked and checked, the originals are moved to `/backup`, the new files
  are moved into place, and BMA800XL restarts on the new version.
- **Not now**: nothing changes; the Atari starts, and the screen appears
  again next time.
- **Skip this update**: renames the file to `bma800xl-update.zip.skipped`
  so it isn't offered again.

Your own files (`bma800xl.cfg`, `atari800.cfg`, `wpa_supplicant.conf`, ROMs,
disks, programs, cartridges, tapes, saved states, ...) are never touched.

## Older versions

Release zips from before the updater have no `bma800xl-manifest.txt`.
BMA800XL recognises them from the card's own `bma800xl-manifest.txt`, which
lists every earlier release, so you can go back to any official release the
same way: download it from the
[GitHub releases page](https://github.com/aminch/bma800xl/releases) and use
it as `bma800xl-update.zip`. The card needs its `bma800xl-manifest.txt`,
which every install or update with the updater puts there.

These versions don't include the updater, so after installing one you can't
use the updater again: to update, copy a newer release to the card by hand,
as described in [INSTALLATION.md](INSTALLATION.md#upgrading). The web UI
warns about this when you drop such a zip and after it is uploaded, and the
update screen asks you to **Continue** before it lists the files.

## Backups

- Original kernels are kept in `/backup/kernel/v<old version>/`, for every
  version you update from.
- Every other file that was replaced is kept in `/backup/v<old version>/`.
  Only the most recent of these folders is kept.
- `/backup/v<old version>/update-report.txt` lists what was replaced, added
  and kept. The web UI's Update page shows it too.

To go back by hand, put the card in a computer and follow
`/backup/v<old version>/RESTORE.txt`: copy that folder's files, and the
matching `/backup/kernel/v<old version>/` kernels, back to the top of the
card.

## If the power goes off

- While the screen says *Checking* or *Extracting*, nothing on the card has
  changed yet. The update screen comes back next time.
- While it says *Installing*, BMA800XL finishes the update on the next start
  and then restarts.
- Don't switch off during an update. If the card no longer starts, restore
  the backup by hand as described above.
