/*
 * gamepad_profiles.h - device defaults shared by the host and emulator port.
 *
 * Uses BMA800XL's existing button layout and Circle gamepad axis convention.
 * This file is part of BMA800XL. See LICENSE for licensing details.
 */

#ifndef BMA800XL_GAMEPAD_PROFILES_H_
#define BMA800XL_GAMEPAD_PROFILES_H_

#define BMA_PAD_BUTTONS 32
#define BMA_PAD_KEY_BASE 1000

enum {
	BMA_PAD_NONE, BMA_PAD_FIRE, BMA_PAD_OPTION, BMA_PAD_SELECT, BMA_PAD_START,
	BMA_PAD_HELP, BMA_PAD_BREAK, BMA_PAD_MENU, BMA_PAD_KEYBOARD,
	BMA_PAD_WARMSTART, BMA_PAD_COLDSTART, BMA_PAD_TURBO,
	BMA_PAD_RUN, BMA_PAD_DISK, BMA_PAD_CARTRIDGE, BMA_PAD_TAPE,
	BMA_PAD_SAVE, BMA_PAD_LOAD, BMA_PAD_QUICKSAVE, BMA_PAD_QUICKLOAD,
	BMA_PAD_PADDLE_FIRE, BMA_PAD_ACTIONS
};

struct bma_gamepad_profile {
	const char *usb_id;
	const char *name;
	int axis_x, axis_y;
	const char *const *button_names;
	const int *bindings;
	const unsigned char *button_order;
	int menu_confirm_button;
};

#ifdef __cplusplus
extern "C" {
#endif

const struct bma_gamepad_profile *BMA_GamepadProfile(const char *usb_id, int known);

#ifdef __cplusplus
}
#endif

#endif
