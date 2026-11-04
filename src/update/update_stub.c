//
// update_stub.c
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
// Modified for BMA800XL from BMC64's src/update/update_stub.c: this notice,
// and also built into the PC test harness.
//
// Built instead of the updater when updater.cfg has updater = off (or there is
// no updater.cfg), and in the PC test harness (tools/hostsim): the two entry
// points do nothing, so bma800xl-update.zip is ignored.
//

#include "update.h"

void update_boot_check(void) {}

void update_run_if_pending(void) {}
