/*
 * usb_hid_keys.h - keyboard usages shared by keyboard and gamepad input.
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
 * Values follow the USB HID Usage Tables, Keyboard/Keypad page (0x07).
 */

#ifndef BMA800XL_USB_HID_KEYS_H_
#define BMA800XL_USB_HID_KEYS_H_

/* USB HID usage codes (keyboard page). */
enum {
	K_A = 0x04, K_Z = 0x1d,
	K_1 = 0x1e, K_9 = 0x26, K_0 = 0x27,
	K_RETURN = 0x28, K_ESCAPE = 0x29, K_BACKSPACE = 0x2a, K_TAB = 0x2b,
	K_SPACE = 0x2c, K_MINUS = 0x2d, K_EQUAL = 0x2e, K_LEFTBRACKET = 0x2f,
	K_RIGHTBRACKET = 0x30, K_BACKSLASH = 0x31, K_NONUS_HASH = 0x32,
	K_SEMICOLON = 0x33, K_QUOTE = 0x34, K_GRAVE = 0x35, K_COMMA = 0x36,
	K_PERIOD = 0x37, K_SLASH = 0x38, K_CAPSLOCK = 0x39,
	K_F1 = 0x3a, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10,
	K_F11, K_F12,
	K_PRINTSCREEN = 0x46, K_SCROLLLOCK = 0x47, K_PAUSE = 0x48,
	K_INSERT = 0x49, K_HOME = 0x4a, K_PAGEUP = 0x4b, K_DELETE = 0x4c,
	K_END = 0x4d, K_PAGEDOWN = 0x4e, K_RIGHT = 0x4f, K_LEFT = 0x50,
	K_DOWN = 0x51, K_UP = 0x52, K_NUMLOCK = 0x53,
	K_KP_DIVIDE = 0x54, K_KP_MULTIPLY = 0x55, K_KP_MINUS = 0x56,
	K_KP_PLUS = 0x57, K_KP_ENTER = 0x58, K_KP_1 = 0x59, K_KP_2, K_KP_3,
	K_KP_4, K_KP_5, K_KP_6, K_KP_7, K_KP_8, K_KP_9, K_KP_0 = 0x62,
	K_KP_PERIOD = 0x63, K_NONUS_BACKSLASH = 0x64, K_APPLICATION = 0x65,
	K_POWER = 0x66,
	K_LCTRL = 0xe0, K_LSHIFT = 0xe1, K_LALT = 0xe2, K_LGUI = 0xe3,
	K_RCTRL = 0xe4, K_RSHIFT = 0xe5, K_RALT = 0xe6, K_RGUI = 0xe7
};

/* Usage code of letter key C ('a'..'z'). */
#define K_LETTER(c) (K_A + (c) - 'a')

/* Key bindings, the same defaults as atari800's SDL port. */
#define KBD_UI      K_F1
#define KBD_OPTION  K_F2
#define KBD_SELECT  K_F3
#define KBD_START   K_F4
#define KBD_RESET   K_F5
#define KBD_HELP    K_F6
#define KBD_BREAK   K_F7
#define KBD_OSK     K_F11
#define KBD_TURBO   K_F12

#endif
