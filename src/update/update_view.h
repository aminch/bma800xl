//
// update_view.h
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
// From BMC64's src/update/update_view.h, unchanged apart from the BMA800XL
// name.
//
// The update view: a full-screen screen of its own (not built from the menus)
// where the user chooses Replace or Keep for each changed file.
//

#ifndef BMA800XL_UPDATE_VIEW_H
#define BMA800XL_UPDATE_VIEW_H

// Reads /bma800xl-update.zip, shows the view and applies the update if the user
// asks for it (then reboots). Returns if the user chose not to update now.
void uv_run(void);

#endif
