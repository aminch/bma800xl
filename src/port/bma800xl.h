/*
 * bma800xl.h - shared declarations for the BMA800XL atari800 port.
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
 */

#ifndef BMA800XL_PORT_H_
#define BMA800XL_PORT_H_

#include "gamepad.h"

/* keyboard.c */
extern int BMA_keypad_joystick;
extern int BMA_arrow_joystick;
extern int BMA_joystick_test;
void BMA_KeyboardInit(void);
void BMA_InputSettingsChanged(void);
int BMA_JoystickState(int port);
void BMA_ReadMouse(void);
int BMA_CharToAkey(int c, int shiftctrl);
void BMA_KeyboardClear(void);
void BMA_KeyboardConsumeEscape(void);
int BMA_CapturePadKey(void);

/* platform.c */
void bma800xl_main(void);
const char *BMA_SdRoot(void);
double BMA_RefreshRate(void);
void BMA_VideoSettingsChanged(void);
void BMA_DisplayModeChanged(void);
void bma_request_power(int power_off);

/* menu.c: returns TRUE to go on to atari800's menu. */
int BMA_Menu(void);
int BMA_MenuRequests(void);

#endif /* BMA800XL_PORT_H_ */
