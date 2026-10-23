/*
 * gamepad.c - gamepad assignment, mappings and Atari input for BMA800XL.
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
 * Button actions follow atari800's sdl/input.h and ui.c interfaces.
 * Direction choices and diagonal sectors follow its SDL2 controller menu.
 * Paddles follow its SDL2 paddle mode (Atari_POT, PLATFORM_PORT in
 * sdl/input.c): two axes give the port's two POT values, and the paddle
 * triggers are the port's left and right direction lines.
 * USB reports use BMA800XL's host.h; defaults are in gamepad_profiles.c.
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "akey.h"
#include "input.h"
#include "ui.h"
#include "ui_basic.h"

#include "../host.h"
#include "../usb_hid_keys.h"
#include "bma800xl.h"
#include "gamepad.h"

static int gamepad_in_menu(void)
{
	return UI_is_active || UI_BASIC_in_kbui;
}

/* The key a held Home (or Select+Start) stands for; 0 when not held. */
static int menu_combo_key = 0;

/* Pad buttons that just turned into an Atari console key press. */
static int pad_consol = INPUT_CONSOL_NONE;

/* Which USB gamepad drives each Atari joystick port, or -1. Some USB devices
   (the 8BitDo Retro Keyboard's receiver, for one) also appear as a gamepad
   that never sends anything, so pads are not tied to ports by their USB
   number: by default a pad takes the first free port the first time it is
   used. joy1_pad=N (and joy2_pad...) fixes gamepad N to a port, and
   joy1_pad=-1 leaves the port without a gamepad; 0 is automatic. */
static int port_pad[HOST_PADS] = {-1, -1, -1, -1};
static int port_mode[HOST_PADS] = {0, 0, 0, 0};
static int pad_prev[HOST_PADS] = {-1, -1, -1, -1};

static const char *const pad_action_names[BMA_PAD_ACTIONS] = {
	"None", "Fire", "Option", "Select", "Start", "Help", "Break",
	"Open Menu", "On-screen Keyboard", "Warm Reset", "Cold Reset", "Turbo",
	"Run Program", "Disk Menu", "Cartridge Menu", "Tape Menu",
	"Save State", "Load State", "Quick Save", "Quick Load",
	"Second Paddle Fire"
};
static int pad_bindings[HOST_PADS][BMA_PAD_BUTTONS];
static unsigned action_held[HOST_PADS], action_pending[HOST_PADS];
static const struct bma_gamepad_profile *port_profile[HOST_PADS];
static const struct bma_pad_input_settings default_input = {
	BMA_PAD_SOURCE_AUTO, -1, 50, BMA_PAD_DIAGONALS_ORIGINAL
};
static struct bma_pad_input_settings pad_input_settings[HOST_PADS];
static int port_paddles[HOST_PADS];

static int menu_watch;
static int menu_connected[HOST_PADS], menu_ports[HOST_PADS];
static struct host_pad_info menu_info[HOST_PADS];

int BMA_GamepadMenuChanged(void)
{
	int pad, changed = FALSE;
	if (!menu_watch)
		return FALSE;
	for (pad = 0; pad < HOST_PADS; pad++) {
		struct host_pad_info info;
		int connected = host_pad_info(pad, &info);
		if (connected != menu_connected[pad] || port_pad[pad] != menu_ports[pad])
			changed = TRUE;
		if (connected) {
			if (info.generation != menu_info[pad].generation
			    || info.button_mask != menu_info[pad].button_mask
			    || info.known != menu_info[pad].known
			    || info.axis_mask != menu_info[pad].axis_mask
			    || info.has_dpad != menu_info[pad].has_dpad
			    || strcmp(info.name, menu_info[pad].name) != 0
			    || strcmp(info.usb_id, menu_info[pad].usb_id) != 0)
				changed = TRUE;
			menu_info[pad] = info;
		}
		menu_connected[pad] = connected;
		menu_ports[pad] = port_pad[pad];
	}
	return changed;
}

void BMA_GamepadMenuWatch(int enabled)
{
	menu_watch = enabled;
	if (enabled)
		BMA_GamepadMenuChanged();
}

static const struct bma_gamepad_profile *pad_profile(int pad)
{
	struct host_pad_info info;
	return host_pad_info(pad, &info) ? BMA_GamepadProfile(info.usb_id, info.known)
	                               : BMA_GamepadProfile(NULL, FALSE);
}

void BMA_GetPadInputSettings(int port, struct bma_pad_input_settings *settings)
{
	*settings = pad_input_settings[port];
}

static int input_settings_valid(const struct bma_pad_input_settings *settings)
{
	return settings->source >= BMA_PAD_SOURCE_BOTH && settings->source <= BMA_PAD_SOURCE_DPAD
	    && settings->axis >= -1 && settings->axis < HOST_PAD_AXES - 1
	    && settings->deadzone >= 0 && settings->deadzone <= 95
	    && settings->diagonals >= BMA_PAD_DIAGONALS_ORIGINAL
	    && settings->diagonals <= BMA_PAD_DIAGONALS_WIDE;
}

static void load_input_settings(int port)
{
	char name[24], *end;
	const char *value;
	int values[4], i;
	long number;
	struct bma_pad_input_settings settings;
	snprintf(name, sizeof(name), "joy%d_input", port + 1);
	value = host_option(name, "default");
	pad_input_settings[port] = default_input;
	if (strcmp(value, "default") == 0)
		return;
	for (i = 0; i < 4; i++) {
		errno = 0;
		number = strtol(value, &end, 10);
		if (end == value || errno || number < -1 || number > 95
		    || (i < 3 ? *end != ',' : *end != '\0'))
			break;
		values[i] = (int) number;
		value = end + (i < 3);
	}
	if (i == 4) {
		settings.source = values[0];
		settings.axis = values[1];
		settings.deadzone = values[2];
		settings.diagonals = values[3];
		if (input_settings_valid(&settings)) {
			pad_input_settings[port] = settings;
			return;
		}
	}
	{
		char message[64];
		snprintf(message, sizeof(message), "Invalid joy%d_input; using defaults", port + 1);
		host_log(message);
	}
}

int BMA_SavePadInputSettings(int port, const struct bma_pad_input_settings *settings)
{
	char name[24], value[72], previous[72];
	if (!input_settings_valid(settings)) {
		host_log("Cannot save invalid gamepad input settings");
		return FALSE;
	}
	snprintf(name, sizeof(name), "joy%d_input", port + 1);
	snprintf(previous, sizeof(previous), "%s", host_option(name, "default"));
	snprintf(value, sizeof(value), "%d,%d,%d,%d", settings->source, settings->axis,
	         settings->deadzone, settings->diagonals);
	if (!host_option_set(name, value))
		return FALSE;
	if (!host_options_save()) {
		if (!host_option_set(name, previous))
			host_log("Cannot restore gamepad input setting after save failure");
		return FALSE;
	}
	pad_input_settings[port] = *settings;
	return TRUE;
}

int BMA_PadPaddles(int port)
{
	return port >= 0 && port < HOST_PADS && port_paddles[port];
}

static void load_paddles(int port)
{
	char name[24];
	const char *value;
	snprintf(name, sizeof(name), "joy%d_mode", port + 1);
	value = host_option(name, "joystick");
	port_paddles[port] = strcmp(value, "paddles") == 0;
	if (!port_paddles[port] && strcmp(value, "joystick") != 0) {
		char message[64];
		snprintf(message, sizeof(message), "Invalid joy%d_mode; using joystick", port + 1);
		host_log(message);
	}
}

int BMA_SavePadPaddles(int port, int paddles)
{
	char name[24], previous[72];
	snprintf(name, sizeof(name), "joy%d_mode", port + 1);
	snprintf(previous, sizeof(previous), "%s", host_option(name, "joystick"));
	if (!host_option_set(name, paddles ? "paddles" : "joystick"))
		return FALSE;
	if (!host_options_save()) {
		if (!host_option_set(name, previous))
			host_log("Cannot restore joystick mode after save failure");
		return FALSE;
	}
	port_paddles[port] = paddles != 0;
	return TRUE;
}

/* The axis pair a port uses: its setting, or the device profile's. */
static void port_axes(int port, int pad, int *axis_x, int *axis_y)
{
	const struct bma_pad_input_settings *settings = &pad_input_settings[port];
	const struct bma_gamepad_profile *profile = pad_profile(pad);
	*axis_x = settings->axis < 0 ? profile->axis_x : settings->axis;
	*axis_y = settings->axis < 0 ? profile->axis_y : settings->axis + 1;
}

static int axis_position(const struct host_pad_axis *axis, int deadzone, int *active)
{
	int64_t range = (int64_t) axis->maximum - axis->minimum;
	int64_t centre = ((int64_t) axis->minimum + axis->maximum) / 2;
	int64_t delta = (int64_t) axis->value - centre;
	int64_t threshold;
	*active = FALSE;
	if (range <= 0)
		return 0;
	if (delta < (int64_t) axis->minimum - centre)
		delta = (int64_t) axis->minimum - centre;
	if (delta > (int64_t) axis->maximum - centre)
		delta = (int64_t) axis->maximum - centre;
	threshold = range * deadzone / 200;
	*active = delta < -threshold || delta > threshold;
	{
		int scaled = (int) (delta * 65536 / range);
		return scaled != 0 ? scaled : delta > 0 ? 1 : delta < 0 ? -1 : 0;
	}
}

static int pad_directions(int port, int pad)
{
	struct host_pad_input input;
	const struct bma_pad_input_settings *settings = &pad_input_settings[port];
	int axis_x, axis_y;
	int x = 0, y = 0, active_x = FALSE, active_y = FALSE, state = 0;
	int ax, ay, diagonal;
	port_axes(port, pad, &axis_x, &axis_y);
	if (!host_pad_input(pad, &input))
		return 0;
	if (settings->source == BMA_PAD_SOURCE_DPAD)
		return input.dpad;
	if (input.axis_mask & (1u << axis_x))
		x = axis_position(&input.axes[axis_x], settings->deadzone, &active_x);
	if (input.axis_mask & (1u << axis_y))
		y = axis_position(&input.axes[axis_y], settings->deadzone, &active_y);
	if (settings->diagonals != BMA_PAD_DIAGONALS_ORIGINAL && (active_x || active_y)) {
		ax = abs(x);
		ay = abs(y);
		/* Diagonal sectors span 30 degrees (narrow) or 45 degrees (wide). */
		diagonal = settings->diagonals == BMA_PAD_DIAGONALS_NARROW ? 577350 : 414214;
		if (settings->diagonals != BMA_PAD_DIAGONALS_NONE
		    && (int64_t) (ax < ay ? ax : ay) * 1000000
		       >= (int64_t) (ax > ay ? ax : ay) * diagonal) {
			active_x = x != 0;
			active_y = y != 0;
		}
		else {
			active_x = ax >= ay && x != 0;
			active_y = ay > ax;
		}
	}
	if (active_x) state |= x < 0 ? HOST_JOY_LEFT : HOST_JOY_RIGHT;
	if (active_y) state |= y < 0 ? HOST_JOY_UP : HOST_JOY_DOWN;
	if (settings->source == BMA_PAD_SOURCE_BOTH || settings->source == BMA_PAD_SOURCE_AUTO)
		state |= input.dpad;
	return state;
}

/* POT value (1-228) of paddle PADDLE (0 or 1) on Atari joystick PORT, or -1
   when the port is not in paddle mode or its axis is missing. The axis's
   minimum gives 228 and its maximum 1, as a paddle turned fully left and
   fully right. */
int BMA_PaddlePosition(int port, int paddle)
{
	struct host_pad_input input;
	const struct host_pad_axis *axis;
	int pad, axis_x, axis_y, number;
	int64_t range, value;
	if (!BMA_PadPaddles(port) || paddle < 0 || paddle > 1)
		return -1;
	pad = port_pad[port];
	if (!host_pad_input(pad, &input))
		return -1;
	port_axes(port, pad, &axis_x, &axis_y);
	number = paddle == 0 ? axis_x : axis_y;
	if (number < 0 || number >= HOST_PAD_AXES || !(input.axis_mask & (1u << number)))
		return -1;
	axis = &input.axes[number];
	range = (int64_t) axis->maximum - axis->minimum;
	if (range <= 0)
		return -1;
	value = (int64_t) axis->value - axis->minimum;
	if (value < 0)
		value = 0;
	if (value > range)
		value = range;
	return 228 - (int) ((value * 227 + range / 2) / range);
}

/* atari800's paddle input (input.h): POTs 0-7, two to a joystick port. */
int Atari_POT(int pot)
{
	int value = BMA_PaddlePosition(pot / 2, pot & 1);
	return value < 0 ? 228 : value;
}

const char *BMA_PadButtonName(int button)
{
	const char *name = BMA_GamepadProfile(NULL, TRUE)->button_names[button];
	return name != NULL ? name : "Button";
}

int BMA_PadButtonIndex(int pad, int position)
{
	struct host_pad_info info;
	const struct bma_gamepad_profile *profile = pad_profile(pad);
	if (profile->button_order != NULL)
		return profile->button_order[position];
	if (host_pad_info(pad, &info) && !info.known) {
		if (position == 6 || position == 7)
			return position + 2;
		if (position == 8 || position == 9)
			return position - 2;
	}
	return position;
}

void BMA_PadButtonLabel(int pad, int button, char *text, unsigned size)
{
	struct host_pad_info info;
	const struct bma_gamepad_profile *profile = pad_profile(pad);
	if (profile->button_names != NULL && profile->button_names[button] != NULL)
		snprintf(text, size, "%s", profile->button_names[button]);
	else if (host_pad_info(pad, &info) && !info.known) {
		int raw = button;
		if (button == 6 || button == 7)
			raw += 2;
		else if (button == 8 || button == 9)
			raw -= 2;
		snprintf(text, size, "Button %d", raw + 1);
	}
	else if (button < 14)
		snprintf(text, size, "%s", BMA_PadButtonName(button));
	else
		snprintf(text, size, "Button %d", button + 1);
}

const char *BMA_PadActionName(int action)
{
	return pad_action_names[action];
}

void BMA_DefaultPadBindings(int port, int *bindings)
{
	memcpy(bindings, pad_profile(port_pad[port])->bindings, sizeof(pad_bindings[port]));
}

int BMA_PadBinding(int port, int button)
{
	return pad_bindings[port][button];
}

int BMA_PadBindingValid(int binding)
{
	int usage;
	if (binding < BMA_PAD_KEY_BASE)
		return binding >= 0 && binding < BMA_PAD_ACTIONS;
	usage = binding - BMA_PAD_KEY_BASE;
	return (usage >= K_A && usage <= 0x81 && usage != K_POWER)
	    || (usage >= K_LCTRL && usage <= K_RGUI);
}

static void binding_setting_name(int port, int group, char *name, unsigned size)
{
	if (group == 0)
		snprintf(name, size, "joy%d_buttons", port + 1);
	else
		snprintf(name, size, "joy%d_buttons%d", port + 1, group + 1);
}

static void encode_bindings(const int *bindings, char *value, unsigned size)
{
	int i, length = 0;
	for (i = 0; i < 8; i++)
		length += snprintf(value + length, size - length, i ? ",%d" : "%d", bindings[i]);
}

static void load_port_bindings(int port);

static int save_pad_bindings(int port, const int *bindings)
{
	char name[24], value[72], previous[BMA_PAD_BUTTONS / 8][72];
	int i, group;
	for (i = 0; bindings != NULL && i < BMA_PAD_BUTTONS; i++) {
		if (!BMA_PadBindingValid(bindings[i]) || (i == 12 && bindings[i] != BMA_PAD_NONE)) {
			host_log("Cannot save invalid gamepad binding");
			return FALSE;
		}
	}
	for (group = 0; group < BMA_PAD_BUTTONS / 8; group++) {
		binding_setting_name(port, group, name, sizeof(name));
		snprintf(previous[group], sizeof(previous[group]), "%s", host_option(name, "default"));
	}
	for (group = 0; group < BMA_PAD_BUTTONS / 8; group++) {
		binding_setting_name(port, group, name, sizeof(name));
		if (bindings != NULL)
			encode_bindings(bindings + group * 8, value, sizeof(value));
		else
			strcpy(value, "default");
		if (!host_option_set(name, value))
			break;
	}
	if (group != BMA_PAD_BUTTONS / 8 || !host_options_save()) {
		for (i = 0; i < group; i++) {
			binding_setting_name(port, i, name, sizeof(name));
			if (!host_option_set(name, previous[i]))
				host_log("Cannot restore gamepad button setting after save failure");
		}
		return FALSE;
	}
	load_port_bindings(port);
	action_pending[port] = 0;
	action_held[port] = host_pad_buttons(port_pad[port]);
	return TRUE;
}

int BMA_SavePadBindings(int port, const int *bindings)
{
	return save_pad_bindings(port, bindings);
}

int BMA_ResetPadBindings(int port)
{
	return save_pad_bindings(port, NULL);
}

static int decode_bindings(const char *value, int group, int *bindings)
{
	int i;
	for (i = 0; i < 8; i++) {
		char *end;
		long binding;
		errno = 0;
		binding = strtol(value, &end, 10);
		if (end == value || errno || binding < 0 || binding > BMA_PAD_KEY_BASE + K_RGUI
		    || !BMA_PadBindingValid((int) binding)
		    || (group * 8 + i == 12 && binding != BMA_PAD_NONE)
		    || (i < 7 ? *end != ',' : *end != '\0'))
			return FALSE;
		bindings[i] = (int) binding;
		value = end + (i < 7);
	}
	return TRUE;
}

static void load_port_bindings(int port)
{
	int group, legacy = FALSE;
	char name[24];
	const char *value;
	int bindings[8];
	port_profile[port] = pad_profile(port_pad[port]);
	BMA_DefaultPadBindings(port, pad_bindings[port]);
	for (group = 0; group < BMA_PAD_BUTTONS / 8; group++) {
		if (group == 1 && legacy) {
			/* Earlier mappings combined L/L2 and R/R2. */
			pad_bindings[port][8] = pad_bindings[port][4];
			pad_bindings[port][9] = pad_bindings[port][5];
		}
		binding_setting_name(port, group, name, sizeof(name));
		value = host_option(name, NULL);
		if (value == NULL || strcmp(value, "default") == 0)
			continue;
		if (decode_bindings(value, group, bindings)) {
			if (group == 0)
				legacy = TRUE;
			memcpy(pad_bindings[port] + group * 8, bindings, sizeof(bindings));
		}
		else {
			char message[80];
			snprintf(message, sizeof(message), "Invalid %s; using default gamepad buttons", name);
			host_log(message);
		}
	}
}

static int pad_menu_pressed(int state)
{
	return state >= 0 && ((state & HOST_PAD_HOME)
	    || (state & (HOST_PAD_SELECT | HOST_PAD_START)) == (HOST_PAD_SELECT | HOST_PAD_START));
}

static int pad_port(int pad)
{
	int port;
	for (port = 0; port < HOST_PADS; port++)
		if (port_pad[port] == pad)
			return port;
	return -1;
}

/* Reads the joyN_pad settings. Automatic ports keep the pad they have,
   unless that pad is now fixed to another port. */
static void init_pad_ports(void)
{
	static const char *const names[HOST_PADS] = {"joy1_pad", "joy2_pad", "joy3_pad", "joy4_pad"};
	int port, other;
	for (port = 0; port < HOST_PADS; port++) {
		int mode = host_option_int(names[port], 0);
		if (mode < -1 || mode > HOST_PADS)
			mode = 0;
		port_mode[port] = mode;
		if (mode != 0)
			port_pad[port] = mode > 0 ? mode - 1 : -1;
	}
	for (port = 0; port < HOST_PADS; port++) {
		if (port_mode[port] != 0 || port_pad[port] < 0)
			continue;
		for (other = 0; other < HOST_PADS; other++)
			if (port_mode[other] > 0 && port_mode[other] - 1 == port_pad[port])
				port_pad[port] = -1;
	}
}

void BMA_GamepadUpdate(void)
{
	int pad, port;
	for (pad = 0; pad < HOST_PADS; pad++) {
		int s = host_pad_state(pad);
		port = pad_port(pad);
		if (s >= 0) {
			s = (s & 0x1ff) != 0 || host_pad_buttons(pad) != 0;
			/* A paddle's position is never idle, so only its
			   buttons claim a port. */
			if (port >= 0) {
				if (!port_paddles[port])
					s |= pad_directions(port, pad) != 0;
			}
			else
				for (port = 0; port < HOST_PADS; port++)
					if (port_mode[port] == 0 && port_pad[port] < 0
					    && !port_paddles[port])
						s |= pad_directions(port, pad) != 0;
		}
		int prev = pad_prev[pad];
		pad_prev[pad] = s;

		port = pad_port(pad);
		if (s < 0) {
			/* Unplugged: free an automatic port. */
			if (port >= 0 && port_mode[port] == 0)
				port_pad[port] = -1;
			continue;
		}
		/* Claim a free automatic port when the pad goes from idle to
		   pressed. */
		if (prev != 0 || s == 0 || port >= 0)
			continue;
		for (port = 0; port < HOST_PADS; port++) {
			if (port_mode[port] == 0 && port_pad[port] < 0) {
				char msg[48];
				port_pad[port] = pad;
				snprintf(msg, sizeof(msg), "Gamepad %d is joystick %d", pad + 1, port + 1);
				host_log(msg);
				break;
			}
		}
	}
	for (port = 0; port < HOST_PADS; port++)
		if (port_profile[port] != pad_profile(port_pad[port]))
			load_port_bindings(port);
}

void BMA_GamepadSettingsChanged(void)
{
	int port;
	init_pad_ports();
	for (port = 0; port < HOST_PADS; port++) {
		load_port_bindings(port);
		load_input_settings(port);
		load_paddles(port);
	}
	memset(action_pending, 0, sizeof(action_pending));
}

void BMA_GamepadInit(void)
{
	int port;
	menu_watch = FALSE;
	menu_combo_key = 0;
	pad_consol = INPUT_CONSOL_NONE;
	memset(action_held, 0, sizeof(action_held));
	memset(action_pending, 0, sizeof(action_pending));
	for (port = 0; port < HOST_PADS; port++) {
		port_pad[port] = -1;
		pad_prev[port] = -1;
		port_profile[port] = NULL;
		pad_input_settings[port] = default_input;
		port_paddles[port] = FALSE;
	}
}

int BMA_GamepadConsole(void)
{
	return pad_consol;
}

/* Gamepad USB slot driving Atari joystick PORT, or -1. */
int BMA_PortPad(int port)
{
	return port >= 0 && port < HOST_PADS ? port_pad[port] : -1;
}


/* Gamepads produce key presses for the parts of the Atari that are not on a
   joystick, and drive the menus while the UI is showing. */
void BMA_GamepadKeys(unsigned char *down)
{
	int pad, port, button, menu_pressed = FALSE;
	int all = 0;
	for (pad = 0; pad < HOST_PADS; pad++) {
		int s = host_pad_state(pad);
		if (s > 0)
			all |= s;
		if (pad_menu_pressed(s))
			menu_pressed = TRUE;
	}

	pad_consol = INPUT_CONSOL_NONE;
	memset(down, 0, 256);

	/* Home, or Select and Start together, open the menu (or leave it). The
	   press keeps the meaning it had when it started: otherwise the Home
	   that opened the menu would turn into Escape inside it while held. */
	if (menu_pressed) {
		if (menu_combo_key == 0)
			menu_combo_key = gamepad_in_menu() ? K_ESCAPE : KBD_UI;
		down[menu_combo_key] = 1;
		return;
	}
	menu_combo_key = 0;

	/* The joystick test shows the pad: only B and Start leave it. */
	if (BMA_joystick_test) {
		if (all & (HOST_PAD_B | HOST_PAD_START)) down[K_ESCAPE] = 1;
		return;
	}

	if (gamepad_in_menu()) {
		int gpio = host_gpio_joystick(0) | host_gpio_joystick(1);
		if (gpio & HOST_JOY_UP) down[K_UP] = 1;
		if (gpio & HOST_JOY_DOWN) down[K_DOWN] = 1;
		if (gpio & HOST_JOY_LEFT) down[K_LEFT] = 1;
		if (gpio & HOST_JOY_RIGHT) down[K_RIGHT] = 1;
		if (gpio & HOST_JOY_FIRE) down[K_RETURN] = 1;
		if (all & HOST_PAD_UP) down[K_UP] = 1;
		if (all & HOST_PAD_DOWN) down[K_DOWN] = 1;
		if (all & HOST_PAD_LEFT) down[K_LEFT] = 1;
		if (all & HOST_PAD_RIGHT) down[K_RIGHT] = 1;
		for (pad = 0; pad < HOST_PADS; pad++) {
			uint32_t buttons = host_pad_buttons(pad);
			if (buttons & (UINT32_C(1) << pad_profile(pad)->menu_confirm_button))
				down[K_RETURN] = 1;
		}
		if (all & (HOST_PAD_B | HOST_PAD_START)) down[K_ESCAPE] = 1;
		return;
	}

	for (port = 0; port < HOST_PADS; port++) {
		uint32_t state = host_pad_buttons(port_pad[port]);
		if (state == 0)
			continue;
		for (button = 0; button < BMA_PAD_BUTTONS; button++) {
			int binding = pad_bindings[port][button];
			if (!(state & (UINT32_C(1) << button)))
				continue;
			if (binding >= BMA_PAD_KEY_BASE)
				down[binding - BMA_PAD_KEY_BASE] = 1;
			else if (binding == BMA_PAD_OPTION)
				pad_consol &= ~INPUT_CONSOL_OPTION;
			else if (binding == BMA_PAD_SELECT)
				pad_consol &= ~INPUT_CONSOL_SELECT;
			else if (binding == BMA_PAD_START)
				pad_consol &= ~INPUT_CONSOL_START;
			else if (binding == BMA_PAD_HELP)
				down[KBD_HELP] = 1;
			else if (binding == BMA_PAD_BREAK)
				down[KBD_BREAK] = 1;
		}
	}
}

/* Emulator actions are edges; Atari keys and console keys remain held. */
int BMA_GamepadAction(int dispatch)
{
	int port, button, blocked = gamepad_in_menu();
	for (port = 0; port < HOST_PADS; port++)
		if (pad_menu_pressed(host_pad_state(port)))
			blocked = TRUE;
	for (port = 0; port < HOST_PADS; port++) {
		uint32_t held = host_pad_buttons(port_pad[port]);
		action_pending[port] &= held;
		if (blocked)
			action_pending[port] = 0;
		else
			action_pending[port] |= held & ~action_held[port];
		action_held[port] = held;
	}
	if (blocked || !dispatch)
		return AKEY_NONE;
	for (port = 0; port < HOST_PADS; port++) {
		for (button = 0; button < BMA_PAD_BUTTONS; button++) {
			int binding = pad_bindings[port][button];
			uint32_t mask = UINT32_C(1) << button;
			int pressed = (action_pending[port] & mask) != 0;
			action_pending[port] &= ~mask;
			if (!pressed)
				continue;
			switch (binding) {
			case BMA_PAD_MENU: return AKEY_UI;
			case BMA_PAD_KEYBOARD: return AKEY_KEYB;
			case BMA_PAD_WARMSTART: return AKEY_WARMSTART;
			case BMA_PAD_COLDSTART: return AKEY_COLDSTART;
			case BMA_PAD_TURBO: return AKEY_TURBO;
			case BMA_PAD_RUN: UI_alt_function = UI_MENU_RUN; break;
			case BMA_PAD_DISK: UI_alt_function = UI_MENU_DISK; break;
			case BMA_PAD_CARTRIDGE: UI_alt_function = UI_MENU_CARTRIDGE; break;
			case BMA_PAD_TAPE: UI_alt_function = UI_MENU_CASSETTE; break;
			case BMA_PAD_SAVE: UI_alt_function = UI_MENU_SAVESTATE; break;
			case BMA_PAD_LOAD: UI_alt_function = UI_MENU_LOADSTATE; break;
			case BMA_PAD_QUICKSAVE: UI_alt_function = UI_MENU_QUICKSAVESTATE; break;
			case BMA_PAD_QUICKLOAD: UI_alt_function = UI_MENU_QUICKLOADSTATE; break;
			default: continue;
			}
			return AKEY_UI;
		}
	}
	return AKEY_NONE;
}


static int button_capture_cancelled(void)
{
	int usage, pressed;
	if (BMA_MenuRequests())
		return TRUE;
	while (host_key_event(&usage, &pressed)) {
		if (pressed && usage == K_ESCAPE) {
			BMA_KeyboardClear();
			return TRUE;
		}
		if (pressed && usage == K_POWER)
			bma_request_power(1);
	}
	return FALSE;
}

int BMA_CapturePadButton(int port)
{
	uint32_t previous[HOST_PADS];
	struct host_pad_info initial[HOST_PADS], info;
	int connected[HOST_PADS], pad;
	if (port_mode[port] < 0) {
		UI_driver->fMessage("Select a gamepad or Automatic first", 1);
		return -1;
	}
	for (pad = 0; pad < HOST_PADS; pad++) {
		previous[pad] = host_pad_buttons(pad);
		connected[pad] = host_pad_info(pad, &initial[pad]);
	}
	UI_driver->fMessage("Press a gamepad button; Esc cancels", 0);
	for (;;) {
		if (button_capture_cancelled())
			return -1;
		for (pad = 0; pad < HOST_PADS; pad++) {
			uint32_t held, changed;
			int target = port_pad[port];
			if (pad != target && (target >= 0 || port_mode[port] != 0 || pad_port(pad) >= 0))
				continue;
			if (!host_pad_info(pad, &info)
			    || (connected[pad] && info.generation != initial[pad].generation)) {
				if (pad == target) {
					UI_driver->fMessage("Gamepad disconnected or changed", 1);
					return -1;
				}
				connected[pad] = 0;
				continue;
			}
			if (!connected[pad]) {
				connected[pad] = 1;
				initial[pad] = info;
				previous[pad] = host_pad_buttons(pad);
			}
			if (pad_menu_pressed(host_pad_state(pad))) {
				menu_combo_key = K_ESCAPE;
				BMA_KeyboardConsumeEscape();
				return -1;
			}
			held = host_pad_buttons(pad);
			changed = held & ~previous[pad];
			previous[pad] = held;
			if (changed && !(changed & (changed - 1))) {
				int button = 0;
				while (!(changed & (UINT32_C(1) << button)))
					button++;
				while (host_pad_buttons(pad) & changed) {
					if (button_capture_cancelled())
						return -1;
					if (pad_menu_pressed(host_pad_state(pad))) {
						menu_combo_key = K_ESCAPE;
						BMA_KeyboardConsumeEscape();
						return -1;
					}
					host_sleep_us(1000);
				}
				if (!host_pad_info(pad, &info)
				    || info.generation != initial[pad].generation) {
					UI_driver->fMessage("Gamepad disconnected or changed", 1);
					return -1;
				}
				port_pad[port] = pad;
				load_port_bindings(port);
				BMA_KeyboardClear();
				return button;
			}
			if (changed)
				UI_driver->fMessage("Press one button at a time", 0);
		}
		host_sleep_us(1000);
	}
}


/* In paddle mode Fire is the first paddle's trigger (the left direction
   line) and Second Paddle Fire the second's (the right one). */
int BMA_GamepadJoystick(int port)
{
	int paddles = BMA_PadPaddles(port);
	int state = paddles ? 0 : pad_directions(port, port_pad[port]);
	int pad = port_pad[port] >= 0 ? host_pad_state(port_pad[port]) : -1;
	uint32_t buttons = host_pad_buttons(port_pad[port]);

	if (pad >= 0 && !pad_menu_pressed(pad)) {
		int button;
		for (button = 0; button < BMA_PAD_BUTTONS; button++) {
			int binding = pad_bindings[port][button];
			if (!(buttons & (UINT32_C(1) << button)))
				continue;
			if (binding == BMA_PAD_FIRE)
				state |= paddles ? HOST_JOY_LEFT : HOST_JOY_FIRE;
			else if (binding == BMA_PAD_PADDLE_FIRE && paddles)
				state |= HOST_JOY_RIGHT;
		}
	}

	return state;
}
