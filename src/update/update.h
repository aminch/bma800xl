//
// update.h
//
// BMC64 - A bare metal Commodore emulator for Raspberry Pi
// Copyright (C) 2026 aminch <https://github.com/aminch>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// Modified for BMA800XL from BMC64's src/update/update.h: called from
// BMA800XL's kernel and emulator start up.
//
// The BMA800XL Updater: applies /bma800xl-update.zip. See docs/UPDATER.md for
// what the user sees.
//
// This is the only header the rest of BMA800XL includes.
//

#ifndef BMA800XL_UPDATE_H
#define BMA800XL_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

// Early boot on core 0, once the SD card is mounted and before the network and
// USB start. Finishes an update that was interrupted while files were being
// moved into place (and reboots if it did), then remembers whether
// /bma800xl-update.zip is waiting.
void update_boot_check(void);

// On the emulator's core, once atari800 has started and before the emulation
// runs. If an update is waiting, shows the update view. Returns when the user
// chooses not to update now; if the update is applied, BMA800XL reboots
// instead.
void update_run_if_pending(void);

#ifdef __cplusplus
}
#endif

#endif
