/*
 * gamepad_profiles.c - controller names, axes, labels and button defaults.
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
 * Retains BMA800XL's existing M30 USB profile and Atari button defaults.
 * Standard button names follow Circle's circle/usb/usbgamepad.h interface.
 * M30 button labels follow the owner's on-device button-detection results.
 * Keyboard bindings use USB HID Keyboard/Keypad usage codes.
 */

#include <stddef.h>
#include <string.h>

#include "gamepad_profiles.h"
#include "usb_hid_keys.h"

static const char *const standard_names[BMA_PAD_BUTTONS] = {
	"A", "B", "X", "Y", "L", "R", "Select", "Start",
	"L2", "R2", "Left Stick", "Right Stick", "Home", "Touchpad"
};

static const int standard_bindings[BMA_PAD_BUTTONS] = {
	BMA_PAD_FIRE, BMA_PAD_KEY_BASE + K_SPACE, BMA_PAD_OPTION,
	BMA_PAD_KEY_BASE + K_RETURN, BMA_PAD_MENU, BMA_PAD_PADDLE_FIRE,
	BMA_PAD_SELECT, BMA_PAD_START
};

/* Host button slots move USB buttons 9/10 before USB buttons 7/8. */
static const char *const m30_button_names[BMA_PAD_BUTTONS] = {
	"Y", "B", "A", "X", "L2 (Z)", "R2 (C)", "Select", "Start", "L", "R"
};

static const int m30_bindings[BMA_PAD_BUTTONS] = {
	BMA_PAD_KEY_BASE + K_RETURN, BMA_PAD_KEY_BASE + K_SPACE,
	BMA_PAD_FIRE, BMA_PAD_OPTION, BMA_PAD_NONE, BMA_PAD_NONE,
	BMA_PAD_SELECT, BMA_PAD_START, BMA_PAD_MENU, BMA_PAD_PADDLE_FIRE
};

static const unsigned char m30_button_order[BMA_PAD_BUTTONS] = {
	2, 1, 3, 0, 8, 9, 6, 7, 4, 5, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};

static const struct bma_gamepad_profile generic_profile = {
	NULL, NULL, 0, 1, NULL, standard_bindings, NULL, 0
};
static const struct bma_gamepad_profile standard_profile = {
	NULL, NULL, 0, 1, standard_names, standard_bindings, NULL, 0
};
static const struct bma_gamepad_profile device_profiles[] = {
	{"venca3-24", "8BitDo M30 2.4G", 3, 4, m30_button_names, m30_bindings,
	 m30_button_order, 2}
};

const struct bma_gamepad_profile *BMA_GamepadProfile(const char *usb_id, int known)
{
	unsigned i;
	if (usb_id != NULL)
		for (i = 0; i < sizeof(device_profiles) / sizeof(device_profiles[0]); i++)
			if (strcmp(usb_id, device_profiles[i].usb_id) == 0)
				return &device_profiles[i];
	return known ? &standard_profile : &generic_profile;
}
