/*
 * menu.h - internal interfaces for BMA800XL menu screens.
 *
 * BMA800XL - A bare metal Atari 800XL emulator for Raspberry Pi
 * Copyright (C) 2026 aminch <https://github.com/aminch>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Shared helpers use atari800's ui.h menu interface.
 */

#ifndef BMA800XL_PORT_MENU_H_
#define BMA800XL_PORT_MENU_H_

#include "atari.h"

void BMA_MenuSaveSetting(const char *name, const char *value);
void BMA_MenuSaveSettingInt(const char *name, int value);
int BMA_MenuSettingOn(const char *name, int default_value);
UWORD BMA_MenuCheckFlags(int on);
int BMA_MenuChoose(const char *title, const char *const choices[], int current);
int BMA_MenuValueIndex(const char *const values[], const char *value);

/* Returns TRUE if a controller setting requires a reboot. */
int BMA_MenuControllers(void);

#endif
