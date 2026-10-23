/*
 * gamepad_test.c - gamepad mappings, profiles and keyboard integration.
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
 * Exercises the port's host interface and atari800's AKEY/UI contracts.
 */

#include <assert.h>
#include <limits.h>
#include "../../src/port/gamepad.c"
#include "../../src/port/keyboard.c"

int Atari800_machine_type = Atari800_MACHINE_XLXE;
int Atari800_f_keys, INPUT_cx85, INPUT_key_shift, INPUT_key_consol;
int BINLOAD_pause_loading, BINLOAD_wait_active;
int UI_is_active, UI_alt_function = -1;

static int pads[HOST_PADS], keys[256], save_ok = 1, set_ok = 1, logs;
static char saved[HOST_PADS][72];
static char saved_extra[HOST_PADS][3][72];
static char saved_input[HOST_PADS][72];
static char saved_mode[HOST_PADS][16];
static struct host_pad_input pad_inputs[HOST_PADS];
static uint32_t extra_buttons[HOST_PADS];
static char usb_ids[HOST_PADS][24];
static int known_pads[HOST_PADS];
static unsigned generations[HOST_PADS];
static uint32_t button_masks[HOST_PADS];
static int event_usage, event_pressed, event_ready;
static int fail_group = -1;

const char *host_option(const char *name, const char *fallback)
{
	int port, group;
	for (port = 0; port < HOST_PADS; port++) {
		char name_input[24];
		snprintf(name_input, sizeof(name_input), "joy%d_input", port + 1);
		if (strcmp(name, name_input) == 0 && saved_input[port][0])
			return saved_input[port];
		snprintf(name_input, sizeof(name_input), "joy%d_mode", port + 1);
		if (strcmp(name, name_input) == 0 && saved_mode[port][0])
			return saved_mode[port];
		for (group = 0; group < 4; group++) {
			char key[24];
			const char *value = group == 0 ? saved[port] : saved_extra[port][group - 1];
			binding_setting_name(port, group, key, sizeof(key));
			if (strcmp(name, key) == 0 && value[0])
				return value;
		}
	}
	return fallback;
}

int host_option_int(const char *name, int fallback)
{
	return fallback;
}

int host_option_set(const char *name, const char *value)
{
	int port, group = 1;
	char *target;
	if (!set_ok)
		return 0;
	for (port = 0; port < HOST_PADS; port++) {
		char name_input[24];
		snprintf(name_input, sizeof(name_input), "joy%d_input", port + 1);
		if (strcmp(name, name_input) == 0) {
			assert(strlen(value) < sizeof(saved_input[port]));
			strcpy(saved_input[port], value);
			return 1;
		}
		snprintf(name_input, sizeof(name_input), "joy%d_mode", port + 1);
		if (strcmp(name, name_input) == 0) {
			assert(strlen(value) < sizeof(saved_mode[port]));
			strcpy(saved_mode[port], value);
			return 1;
		}
	}
	assert(sscanf(name, "joy%d_buttons%d", &port, &group) >= 1);
	assert(port >= 1 && port <= HOST_PADS);
	assert(group >= 1 && group <= 4);
	if (group == fail_group)
		return 0;
	assert(strlen(value) < sizeof(saved[0]));
	target = group == 1 ? saved[port - 1] : saved_extra[port - 1][group - 2];
	strcpy(target, value);
	return 1;
}

int host_options_save(void) { return save_ok; }
void host_log(const char *message) { assert(message[0]); logs++; }
int host_pad_state(int slot) { return slot >= 0 && slot < HOST_PADS ? pads[slot] : -1; }
uint32_t host_pad_buttons(int slot)
{
	return host_pad_state(slot) >= 0 ? (pads[slot] & 0xff) | extra_buttons[slot] : 0;
}
int host_pad_input(int slot, struct host_pad_input *input)
{
	if (host_pad_state(slot) < 0)
		return 0;
	*input = pad_inputs[slot];
	input->dpad |= (pads[slot] >> 12) & 0xf;
	return 1;
}
int host_pad_info(int slot, struct host_pad_info *info)
{
	if (host_pad_state(slot) < 0)
		return 0;
	memset(info, 0, sizeof(*info));
	snprintf(info->name, sizeof(info->name), "Test Pad %d", slot + 1);
	snprintf(info->usb_id, sizeof(info->usb_id), "%s", usb_ids[slot]);
	info->known = known_pads[slot];
	info->button_mask = button_masks[slot];
	info->generation = generations[slot];
	info->axis_mask = pad_inputs[slot].axis_mask;
	info->has_dpad = pad_inputs[slot].has_dpad;
	return 1;
}
int host_gpio_joystick(int port) { return 0; }
int host_key_down(int usage) { return keys[usage]; }
int host_key_event(int *usage, int *pressed)
{
	if (!event_ready)
		return 0;
	*usage = event_usage;
	*pressed = event_pressed;
	event_ready = 0;
	return 1;
}
int BMA_MenuRequests(void) { return 0; }
void bma_request_power(int power) { assert(!"unexpected power request"); }

static void reset(void)
{
	int port;
	memset(saved, 0, sizeof(saved));
	memset(saved_extra, 0, sizeof(saved_extra));
	memset(saved_input, 0, sizeof(saved_input));
	memset(saved_mode, 0, sizeof(saved_mode));
	memset(pad_inputs, 0, sizeof(pad_inputs));
	memset(extra_buttons, 0, sizeof(extra_buttons));
	memset(usb_ids, 0, sizeof(usb_ids));
	memset(keys, 0, sizeof(keys));
	memset(action_pending, 0, sizeof(action_pending));
	lastkey = key_pressed = menu_combo_key = event_ready = 0;
	UI_is_active = 0;
	Atari800_machine_type = Atari800_MACHINE_XLXE;
	BINLOAD_wait_active = 0;
	save_ok = set_ok = 1;
	fail_group = -1;
	for (port = 0; port < HOST_PADS; port++) {
		known_pads[port] = 1;
		generations[port] = 1;
		button_masks[port] = 0xfff;
		pad_inputs[port].has_dpad = 1;
		pads[port] = 0;
		port_pad[port] = -1;
		port_mode[port] = 0;
		pad_prev[port] = -1;
	}
	BMA_KeyboardInit();
	for (port = 0; port < HOST_PADS; port++)
		port_pad[port] = port;
}

static void release(void)
{
	memset(pads, 0, sizeof(pads));
	memset(extra_buttons, 0, sizeof(extra_buttons));
	for (int i = 0; i < 10; i++)
		PLATFORM_Keyboard();
	assert(INPUT_key_consol == INPUT_CONSOL_NONE);
}

static void defaults(void)
{
	reset();
	pads[0] = HOST_PAD_A | HOST_PAD_UP;
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	assert(BMA_JoystickState(0) == (HOST_JOY_FIRE | HOST_JOY_UP));
	pads[0] = HOST_PAD_B;
	assert(PLATFORM_Keyboard() == AKEY_SPACE);
	assert(PLATFORM_Keyboard() == AKEY_SPACE);
	release();
	pads[1] = HOST_PAD_Y;
	assert(PLATFORM_Keyboard() == AKEY_RETURN);
	release();
	pads[2] = HOST_PAD_X | HOST_PAD_START;
	PLATFORM_Keyboard();
	assert(INPUT_key_consol == (INPUT_CONSOL_NONE & ~INPUT_CONSOL_OPTION & ~INPUT_CONSOL_START));
	release();
	Atari800_machine_type = Atari800_MACHINE_5200;
	pads[0] = HOST_PAD_START;
	assert(PLATFORM_Keyboard() == AKEY_5200_START);
	release();
	pads[0] = HOST_PAD_B;
	assert(PLATFORM_Keyboard() == AKEY_NONE);
}

static void mappings(void)
{
	int bindings[BMA_PAD_BUTTONS];
	reset();
	BMA_DefaultPadBindings(0, bindings);
	bindings[0] = BMA_PAD_NONE;
	bindings[1] = BMA_PAD_FIRE;
	bindings[4] = 1000 + K_LETTER('j');
	bindings[5] = BMA_PAD_OPTION;
	assert(BMA_SavePadBindings(1, bindings));
	assert(strcmp(saved[1], "0,1,2,1040,1013,2,3,4") == 0);
	pads[1] = HOST_PAD_A;
	assert(BMA_JoystickState(1) == 0);
	pads[1] = HOST_PAD_B;
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	assert(BMA_JoystickState(1) == HOST_JOY_FIRE);
	pads[0] = HOST_PAD_A;
	assert(BMA_JoystickState(0) == HOST_JOY_FIRE);
	pads[1] = HOST_PAD_L | HOST_PAD_R;
	assert(PLATFORM_Keyboard() == AKEY_j);
	assert(!(INPUT_key_consol & INPUT_CONSOL_OPTION));
	pads[1] = -1;
	for (int i = 0; i < 10; i++)
		PLATFORM_Keyboard();
	assert(!virtual_down[K_LETTER('j')]);
	assert(INPUT_key_consol == INPUT_CONSOL_NONE);
	assert(BMA_JoystickState(1) == 0);
	BMA_InputSettingsChanged();
	assert(BMA_PadBinding(1, 4) == 1000 + K_LETTER('j'));
	assert(BMA_PadBinding(0, 0) == BMA_PAD_FIRE);
	port_pad[1] = 2;
	pads[2] = HOST_PAD_B;
	assert(BMA_JoystickState(1) == HOST_JOY_FIRE);
}

static void actions(void)
{
	static const int expected[] = {
		AKEY_UI, AKEY_KEYB, AKEY_WARMSTART, AKEY_COLDSTART, AKEY_TURBO,
		AKEY_UI, AKEY_UI, AKEY_UI, AKEY_UI, AKEY_UI, AKEY_UI, AKEY_UI, AKEY_UI
	};
	static const int menus[] = {
		-1, -1, -1, -1, -1, UI_MENU_RUN, UI_MENU_DISK, UI_MENU_CARTRIDGE,
		UI_MENU_CASSETTE, UI_MENU_SAVESTATE, UI_MENU_LOADSTATE,
		UI_MENU_QUICKSAVESTATE, UI_MENU_QUICKLOADSTATE
	};
	int bindings[BMA_PAD_BUTTONS], action, i;
	reset();
	BMA_DefaultPadBindings(0, bindings);
	for (action = BMA_PAD_MENU; action <= BMA_PAD_QUICKLOAD; action++) {
		bindings[4] = action;
		assert(BMA_SavePadBindings(0, bindings));
		pads[0] = HOST_PAD_L;
		assert(PLATFORM_Keyboard() == expected[action - BMA_PAD_MENU]);
		assert(UI_alt_function == menus[action - BMA_PAD_MENU]);
		for (i = 0; i < 10; i++)
			assert(PLATFORM_Keyboard() == AKEY_NONE);
		release();
	}
	bindings[4] = BMA_PAD_TURBO;
	assert(BMA_SavePadBindings(0, bindings));
	pads[0] = HOST_PAD_B | HOST_PAD_L;
	assert(PLATFORM_Keyboard() == AKEY_TURBO);
	assert(PLATFORM_Keyboard() == AKEY_SPACE);
	release();
	bindings[4] = BMA_PAD_BREAK;
	assert(BMA_SavePadBindings(0, bindings));
	BINLOAD_wait_active = 1;
	pads[0] = HOST_PAD_L;
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	assert(BINLOAD_pause_loading);
	release();
}

static void keyboard_bindings(void)
{
	int bindings[BMA_PAD_BUTTONS];
	reset();
	BMA_DefaultPadBindings(0, bindings);
	bindings[4] = BMA_PAD_KEY_BASE + K_LSHIFT;
	bindings[5] = BMA_PAD_KEY_BASE + K_LETTER('j');
	assert(BMA_SavePadBindings(0, bindings));
	pads[0] = HOST_PAD_L | HOST_PAD_R;
	for (int i = 0; i < 3; i++)
		PLATFORM_Keyboard();
	assert(PLATFORM_Keyboard() == (AKEY_j | AKEY_SHFT));
	release();
	assert(!INPUT_key_shift);
	bindings[4] = BMA_PAD_KEY_BASE + K_KP_8;
	bindings[5] = BMA_PAD_KEY_BASE + K_KP_0;
	assert(BMA_SavePadBindings(0, bindings));
	pads[0] = HOST_PAD_L | HOST_PAD_R;
	for (int i = 0; i < 3; i++)
		PLATFORM_Keyboard();
	assert(BMA_JoystickState(0) == (HOST_JOY_UP | HOST_JOY_FIRE));
	release();
	assert(BMA_JoystickState(0) == 0);
}

static void independent_buttons(void)
{
	int bindings[BMA_PAD_BUTTONS];
	char label[32];
	reset();
	strcpy(saved[0], "1,1044,2,1040,11,2,3,4");
	BMA_InputSettingsChanged();
	assert(BMA_PadBinding(0, 8) == BMA_PAD_TURBO);
	assert(BMA_PadBinding(0, 9) == BMA_PAD_OPTION);
	BMA_DefaultPadBindings(0, bindings);
	bindings[4] = BMA_PAD_TURBO;
	bindings[8] = BMA_PAD_FIRE;
	bindings[31] = BMA_PAD_OPTION;
	assert(BMA_SavePadBindings(0, bindings));
	extra_buttons[0] = 1u << 8;
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	assert(BMA_JoystickState(0) == HOST_JOY_FIRE);
	release();
	pads[0] = HOST_PAD_L;
	assert(PLATFORM_Keyboard() == AKEY_TURBO);
	assert(BMA_JoystickState(0) == 0);
	release();
	extra_buttons[0] = UINT32_C(1) << 31;
	PLATFORM_Keyboard();
	assert(!(INPUT_key_consol & INPUT_CONSOL_OPTION));
	BMA_InputSettingsChanged();
	assert(BMA_PadBinding(0, 31) == BMA_PAD_OPTION);
	BMA_PadButtonLabel(0, 8, label, sizeof(label));
	assert(strcmp(label, "L2") == 0);
	assert(host_pad_generic_buttons(1u << 6) == (1u << 8));
	assert(host_pad_generic_buttons(1u << 8) == (1u << 6));
	assert(host_pad_generic_buttons(1u << 12) == 0);
	reset();
	port_pad[0] = -1;
	pad_prev[0] = 0;
	extra_buttons[0] = 1u << 10;
	PLATFORM_Keyboard();
	assert(BMA_PortPad(0) == 0);
}

static void menu_refresh(void)
{
	reset();
	UI_is_active = 1;
	BMA_GamepadMenuWatch(TRUE);
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	pads[1] = -1;
	assert(PLATFORM_Keyboard() == UI_BASIC_KEY_REFRESH);
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	pads[1] = 0;
	assert(PLATFORM_Keyboard() == UI_BASIC_KEY_REFRESH);
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	generations[1]++;
	assert(PLATFORM_Keyboard() == UI_BASIC_KEY_REFRESH);
	button_masks[1] |= 1u << 14;
	assert(PLATFORM_Keyboard() == UI_BASIC_KEY_REFRESH);
	strcpy(usb_ids[1], "venca3-24");
	assert(PLATFORM_Keyboard() == UI_BASIC_KEY_REFRESH);
	port_pad[0] = -1;
	assert(PLATFORM_Keyboard() == UI_BASIC_KEY_REFRESH);
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	BMA_GamepadMenuWatch(FALSE);
	pads[1] = -1;
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	BMA_GamepadMenuWatch(TRUE);
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	BMA_GamepadMenuWatch(FALSE);
}

static void profiles(void)
{
	const struct bma_gamepad_profile *m30 = BMA_GamepadProfile("venca3-24", 0);
	const struct bma_gamepad_profile *standard = BMA_GamepadProfile("", 1);
	const struct bma_gamepad_profile *generic = BMA_GamepadProfile("", 0);
	const char *const m30_labels[] = {
		"Y", "B", "A", "X", "L2 (Z)", "R2 (C)", "Select", "Start", "L", "R"
	};
	const char *const m30_menu_labels[] = {
		"A", "B", "X", "Y", "L", "R", "Select", "Start", "L2 (Z)", "R2 (C)"
	};
	const int m30_defaults[BMA_PAD_BUTTONS] = {
		BMA_PAD_KEY_BASE + K_RETURN, BMA_PAD_KEY_BASE + K_SPACE,
		BMA_PAD_FIRE, BMA_PAD_OPTION, BMA_PAD_NONE, BMA_PAD_NONE,
		BMA_PAD_SELECT, BMA_PAD_START, BMA_PAD_MENU, BMA_PAD_PADDLE_FIRE
	};
	int bindings[BMA_PAD_BUTTONS], button, position;
	uint32_t seen = 0;
	char label[32];
	assert(m30 != generic && m30 != standard);
	assert(strcmp(m30->name, "8BitDo M30 2.4G") == 0);
	assert(m30->axis_x == 3 && m30->axis_y == 4);
	assert(generic->axis_x == 0 && generic->axis_y == 1);
	assert(m30->button_names != NULL && generic->button_names == NULL);
	assert(strcmp(standard->button_names[8], "L2") == 0);
	reset();
	PLATFORM_Keyboard();
	assert(port_profile[0] == standard);
	for (position = 0; position < BMA_PAD_BUTTONS; position++)
		assert(BMA_PadButtonIndex(0, position) == position);
	strcpy(usb_ids[0], "venca3-24");
	known_pads[0] = 0;
	PLATFORM_Keyboard();
	assert(port_profile[0] == m30);
	for (button = 0; button < BMA_PAD_BUTTONS; button++)
		assert(BMA_PadBinding(0, button) == m30_defaults[button]);
	for (button = 0; button < 10; button++) {
		BMA_PadButtonLabel(0, button, label, sizeof(label));
		assert(strcmp(label, m30_labels[button]) == 0);
	}
	for (position = 0; position < BMA_PAD_BUTTONS; position++) {
		button = BMA_PadButtonIndex(0, position);
		assert(button >= 0 && button < BMA_PAD_BUTTONS);
		assert(!(seen & (UINT32_C(1) << button)));
		seen |= UINT32_C(1) << button;
		if (position < 10) {
			BMA_PadButtonLabel(0, button, label, sizeof(label));
			assert(strcmp(label, m30_menu_labels[position]) == 0);
			assert(BMA_PadBinding(0, button)
			       == (position < 8 ? standard->bindings[position] : BMA_PAD_NONE));
		}
	}
	assert(seen == UINT32_MAX);
	pads[0] = 1u << 2;
	assert(BMA_JoystickState(0) == HOST_JOY_FIRE);
	assert(PLATFORM_TRIG(0) == 0);
	release();
	pads[0] = 1u << 1;
	assert(PLATFORM_Keyboard() == AKEY_SPACE);
	assert(PLATFORM_TRIG(0) == 1);
	release();
	pads[0] = 1u << 3;
	PLATFORM_Keyboard();
	assert(!(INPUT_key_consol & INPUT_CONSOL_OPTION));
	release();
	pads[0] = 1u << 0;
	assert(PLATFORM_Keyboard() == AKEY_RETURN);
	release();
	BMA_PadButtonLabel(0, 10, label, sizeof(label));
	assert(strcmp(label, "Button 11") == 0);
	BMA_DefaultPadBindings(0, bindings);
	bindings[0] = BMA_PAD_KEY_BASE + K_SPACE;
	bindings[8] = BMA_PAD_FIRE;
	assert(BMA_SavePadBindings(0, bindings));
	usb_ids[0][0] = '\0';
	known_pads[0] = 1;
	PLATFORM_Keyboard();
	assert(port_profile[0] == standard);
	assert(BMA_PadBinding(0, 0) == BMA_PAD_KEY_BASE + K_SPACE);
	assert(BMA_PadBinding(0, 8) == BMA_PAD_FIRE);
	assert(BMA_ResetPadBindings(0));
	assert(strcmp(saved[0], "default") == 0);
	for (button = 0; button < 3; button++)
		assert(strcmp(saved_extra[0][button], "default") == 0);
	strcpy(usb_ids[0], "venca3-24");
	known_pads[0] = 0;
	PLATFORM_Keyboard();
	assert(port_profile[0] == m30);
	for (button = 0; button < BMA_PAD_BUTTONS; button++)
		assert(BMA_PadBinding(0, button) == m30->bindings[button]);
	save_ok = 0;
	assert(!BMA_SavePadBindings(0, bindings));
	assert(strcmp(saved[0], "default") == 0);
	assert(!BMA_ResetPadBindings(0));
	save_ok = 1;
	usb_ids[0][0] = '\0';
	PLATFORM_Keyboard();
	assert(port_profile[0] == generic);
	assert(BMA_PadBinding(0, 0) == BMA_PAD_FIRE);
	BMA_PadButtonLabel(0, 8, label, sizeof(label));
	assert(strcmp(label, "Button 7") == 0);
	for (position = 0; position < BMA_PAD_BUTTONS; position++) {
		char expected[32];
		button = BMA_PadButtonIndex(0, position);
		BMA_PadButtonLabel(0, button, label, sizeof(label));
		snprintf(expected, sizeof(expected), "Button %d", position + 1);
		assert(strcmp(label, expected) == 0);
	}
}

static void menus(void)
{
	int bindings[BMA_PAD_BUTTONS];
	reset();
	BMA_DefaultPadBindings(0, bindings);
	bindings[0] = BMA_PAD_COLDSTART;
	bindings[6] = bindings[7] = BMA_PAD_TURBO;
	assert(BMA_SavePadBindings(0, bindings));
	UI_is_active = 1;
	pads[0] = HOST_PAD_A;
	assert(PLATFORM_Keyboard() == AKEY_RETURN);
	UI_is_active = 0;
	assert(PLATFORM_Keyboard() != AKEY_COLDSTART);
	release();
	pads[0] = HOST_PAD_SELECT | HOST_PAD_START;
	assert(PLATFORM_Keyboard() == AKEY_UI);
	UI_is_active = 1;
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	UI_is_active = 0;
	assert(PLATFORM_Keyboard() == AKEY_NONE);
	release();
	pads[0] = HOST_PAD_SELECT;
	pads[1] = HOST_PAD_START;
	assert(PLATFORM_Keyboard() == AKEY_TURBO);
	release();
	port_pad[3] = -1;
	pads[3] = HOST_PAD_HOME;
	assert(PLATFORM_Keyboard() == AKEY_UI);
}

static void m30_menus(void)
{
	unsigned char down[256];
	int bindings[BMA_PAD_BUTTONS];
	reset();
	strcpy(usb_ids[0], "venca3-24");
	known_pads[0] = 0;
	PLATFORM_Keyboard();
	BMA_DefaultPadBindings(0, bindings);
	bindings[2] = BMA_PAD_NONE;
	assert(BMA_SavePadBindings(0, bindings));
	UI_is_active = 1;
	pads[0] = 1u << 2;
	BMA_GamepadKeys(down);
	assert(down[K_RETURN] && !down[K_ESCAPE]);
	pads[0] = 1u << 0;
	BMA_GamepadKeys(down);
	assert(!down[K_RETURN] && !down[K_ESCAPE]);
	pads[0] = HOST_PAD_B;
	BMA_GamepadKeys(down);
	assert(down[K_ESCAPE] && !down[K_RETURN]);
	pads[0] = HOST_PAD_START;
	BMA_GamepadKeys(down);
	assert(down[K_ESCAPE] && !down[K_RETURN]);
	pads[0] = 0;
	pads[1] = HOST_PAD_A;
	BMA_GamepadKeys(down);
	assert(down[K_RETURN]);
	pads[1] = 0;
	port_pad[0] = -1;
	pads[0] = 1u << 2;
	BMA_GamepadKeys(down);
	assert(down[K_RETURN]);
	UI_is_active = 0;
	UI_BASIC_in_kbui = 1;
	BMA_GamepadKeys(down);
	assert(down[K_RETURN]);
	UI_BASIC_in_kbui = 0;
}

static void menu_open_defaults(void)
{
	int device, bindings[BMA_PAD_BUTTONS], button, i;
	for (device = 0; device < 3; device++) {
		reset();
		known_pads[0] = device == 0;
		if (device == 2)
			strcpy(usb_ids[0], "venca3-24");
		PLATFORM_Keyboard();
		button = device == 2 ? 8 : 4;
		assert(BMA_PadBinding(0, button) == BMA_PAD_MENU);
		extra_buttons[0] = UINT32_C(1) << button;
		assert(PLATFORM_Keyboard() == AKEY_UI);
		UI_is_active = 1;
		for (i = 0; i < 10; i++)
			assert(PLATFORM_Keyboard() == AKEY_NONE);
		release();
		extra_buttons[0] = UINT32_C(1) << button;
		assert(PLATFORM_Keyboard() == AKEY_NONE);
		release();
		UI_is_active = 0;
		BMA_DefaultPadBindings(0, bindings);
		bindings[button] = BMA_PAD_NONE;
		assert(BMA_SavePadBindings(0, bindings));
		extra_buttons[0] = UINT32_C(1) << button;
		assert(PLATFORM_Keyboard() == AKEY_NONE);
		UI_is_active = 1;
		assert(PLATFORM_Keyboard() == AKEY_NONE);
		release();
		bindings[3] = BMA_PAD_MENU;
		assert(BMA_SavePadBindings(0, bindings));
		extra_buttons[0] = UINT32_C(1) << 3;
		assert(PLATFORM_Keyboard() == AKEY_NONE);
		release();
		UI_is_active = 0;
		extra_buttons[0] = UINT32_C(1) << 3;
		assert(PLATFORM_Keyboard() == AKEY_UI);
		release();
		assert(BMA_ResetPadBindings(0));
		assert(BMA_PadBinding(0, button) == BMA_PAD_MENU);
		extra_buttons[0] = UINT32_C(1) << button;
		assert(PLATFORM_Keyboard() == AKEY_UI);
		release();
	}
}

static void set_axis(int pad, int axis, int value, int minimum, int maximum)
{
	pad_inputs[pad].axis_mask |= 1u << axis;
	pad_inputs[pad].axes[axis].value = value;
	pad_inputs[pad].axes[axis].minimum = minimum;
	pad_inputs[pad].axes[axis].maximum = maximum;
}

static void direction_settings(void)
{
	struct bma_pad_input_settings settings, other;
	const char *invalid[] = {
		"", "0,-1,50", "0,-1,50,0,1", "4,-1,50,0", "-1,-1,50,0",
		"0,-2,50,0", "0,15,50,0", "0,-1,96,0", "0,-1,-1,0",
		"0,-1,50,4", "0,-1,50,x", "999999999999999999999999,0,50,0"
	};
	char previous[72];
	int i, before, sign_x, sign_y;
	reset();
	BMA_GetPadInputSettings(0, &settings);
	assert(settings.source == BMA_PAD_SOURCE_AUTO && settings.axis == -1);
	assert(settings.deadzone == 50 && settings.diagonals == BMA_PAD_DIAGONALS_ORIGINAL);
	set_axis(0, 0, 32767, -32768, 32767);
	set_axis(0, 1, 0, -32768, 32767);
	pad_inputs[0].dpad = HOST_JOY_UP;
	assert(BMA_JoystickState(0) == (HOST_JOY_RIGHT | HOST_JOY_UP));
	settings.source = BMA_PAD_SOURCE_AXES;
	assert(BMA_SavePadInputSettings(0, &settings));
	assert(BMA_JoystickState(0) == HOST_JOY_RIGHT);
	settings.source = BMA_PAD_SOURCE_DPAD;
	assert(BMA_SavePadInputSettings(0, &settings));
	assert(BMA_JoystickState(0) == HOST_JOY_UP);
	settings.source = BMA_PAD_SOURCE_AUTO;
	assert(BMA_SavePadInputSettings(0, &settings));
	assert(BMA_JoystickState(0) == (HOST_JOY_RIGHT | HOST_JOY_UP));
	pad_inputs[0].dpad = 0;
	assert(BMA_JoystickState(0) == HOST_JOY_RIGHT);
	pad_inputs[0].has_dpad = 0;
	assert(BMA_JoystickState(0) == HOST_JOY_RIGHT);

	settings.source = BMA_PAD_SOURCE_AXES;
	settings.axis = 2;
	assert(BMA_SavePadInputSettings(0, &settings));
	assert(BMA_JoystickState(0) == 0);
	set_axis(0, 2, -32768, -32768, 32767);
	set_axis(0, 3, 32767, -32768, 32767);
	assert(BMA_JoystickState(0) == (HOST_JOY_LEFT | HOST_JOY_DOWN));
	BMA_InputSettingsChanged();
	BMA_GetPadInputSettings(0, &other);
	assert(other.axis == 2 && other.source == BMA_PAD_SOURCE_AXES);
	BMA_GetPadInputSettings(1, &other);
	assert(other.axis == -1 && other.source == BMA_PAD_SOURCE_AUTO);
	port_pad[0] = -1;
	pad_prev[0] = 0;
	BMA_GamepadUpdate();
	assert(port_pad[0] == 0);

	settings.axis = 0;
	settings.deadzone = 20;
	assert(BMA_SavePadInputSettings(0, &settings));
	set_axis(0, 0, 10000, -32768, 32767);
	assert(BMA_JoystickState(0) == HOST_JOY_RIGHT);
	settings.deadzone = 50;
	assert(BMA_SavePadInputSettings(0, &settings));
	assert(BMA_JoystickState(0) == 0);
	settings.deadzone = 0;
	for (sign_x = -1; sign_x <= 1; sign_x += 2)
		for (sign_y = -1; sign_y <= 1; sign_y += 2) {
			int horizontal = sign_x < 0 ? HOST_JOY_LEFT : HOST_JOY_RIGHT;
			int vertical = sign_y < 0 ? HOST_JOY_UP : HOST_JOY_DOWN;
			set_axis(0, 0, sign_x * 25000, -32768, 32767);
			set_axis(0, 1, sign_y * 12500, -32768, 32767);
			settings.diagonals = BMA_PAD_DIAGONALS_NARROW;
			assert(BMA_SavePadInputSettings(0, &settings));
			assert(BMA_JoystickState(0) == horizontal);
			settings.diagonals = BMA_PAD_DIAGONALS_WIDE;
			assert(BMA_SavePadInputSettings(0, &settings));
			assert(BMA_JoystickState(0) == (horizontal | vertical));
			settings.diagonals = BMA_PAD_DIAGONALS_NONE;
			assert(BMA_SavePadInputSettings(0, &settings));
			assert(BMA_JoystickState(0) == horizontal);
			settings.diagonals = BMA_PAD_DIAGONALS_NARROW;
			set_axis(0, 1, sign_y * 25000, -32768, 32767);
			assert(BMA_SavePadInputSettings(0, &settings));
			assert(BMA_JoystickState(0) == (horizontal | vertical));
		}
	settings.source = BMA_PAD_SOURCE_DPAD;
	settings.diagonals = BMA_PAD_DIAGONALS_NONE;
	assert(BMA_SavePadInputSettings(0, &settings));
	pad_inputs[0].dpad = HOST_JOY_LEFT | HOST_JOY_DOWN;
	assert(BMA_JoystickState(0) == (HOST_JOY_LEFT | HOST_JOY_DOWN));
	pads[0] = -1;
	assert(BMA_JoystickState(0) == 0);
	pads[0] = 0;

	strcpy(previous, saved_input[0]);
	settings.deadzone = 75;
	save_ok = 0;
	assert(!BMA_SavePadInputSettings(0, &settings));
	assert(strcmp(previous, saved_input[0]) == 0);
	BMA_GetPadInputSettings(0, &other);
	assert(other.deadzone == 0);
	save_ok = 1;
	set_ok = 0;
	assert(!BMA_SavePadInputSettings(0, &settings));
	assert(strcmp(previous, saved_input[0]) == 0);
	set_ok = 1;
	settings.axis = HOST_PAD_AXES - 1;
	before = logs;
	assert(!BMA_SavePadInputSettings(0, &settings));
	assert(logs == before + 1);
	for (i = 1; i < (int)(sizeof(invalid) / sizeof(invalid[0])); i++) {
		strcpy(saved_input[0], invalid[i]);
		before = logs;
		BMA_GamepadSettingsChanged();
		assert(logs == before + 1);
		BMA_GetPadInputSettings(0, &other);
		assert(other.source == BMA_PAD_SOURCE_AUTO && other.axis == -1);
		assert(other.deadzone == 50 && other.diagonals == BMA_PAD_DIAGONALS_ORIGINAL);
	}
	reset();
	known_pads[0] = 0;
	strcpy(usb_ids[0], "venca3-24");
	set_axis(0, 3, 255, 0, 255);
	set_axis(0, 4, 127, 0, 255);
	assert(BMA_JoystickState(0) == HOST_JOY_RIGHT);
}

static void axis_thresholds(void)
{
	int value, deadzone, active, normalized, range;
	struct host_pad_axis axis;
	for (range = 0; range < 2; range++)
		for (deadzone = 0; deadzone <= 95; deadzone += 5)
			for (value = 0; value <= 255; value++) {
				int centre, threshold;
				axis.minimum = range ? -128 : 0;
				axis.maximum = range ? 127 : 255;
				axis.value = value + axis.minimum;
				centre = (axis.minimum + axis.maximum) / 2;
				threshold = (axis.maximum - axis.minimum) * deadzone / 200;
				normalized = axis_position(&axis, deadzone, &active);
				assert(active == (axis.value < centre - threshold || axis.value > centre + threshold));
				assert(normalized == 0 || (normalized < 0) == (axis.value < centre));
			}
	axis.minimum = INT_MIN;
	axis.maximum = INT_MAX;
	axis.value = INT_MIN;
	assert(axis_position(&axis, 50, &active) < 0 && active);
	axis.value = INT_MAX;
	assert(axis_position(&axis, 50, &active) > 0 && active);
	axis.value = 1;
	assert(axis_position(&axis, 0, &active) > 0 && active);
	axis.maximum = axis.minimum;
	assert(axis_position(&axis, 50, &active) == 0 && !active);
}

static void paddles(void)
{
	struct bma_pad_input_settings settings;
	int bindings[BMA_PAD_BUTTONS], before;
	reset();
	set_axis(0, 0, -32768, -32768, 32767);
	set_axis(0, 1, 32767, -32768, 32767);
	assert(!BMA_PadPaddles(0));
	assert(Atari_POT(0) == 228 && Atari_POT(1) == 228);
	assert(BMA_JoystickState(0) == (HOST_JOY_LEFT | HOST_JOY_DOWN));

	/* Paddle mode: the axes give positions, not directions. */
	assert(BMA_SavePadPaddles(0, TRUE));
	assert(strcmp(saved_mode[0], "paddles") == 0);
	assert(BMA_PadPaddles(0) && !BMA_PadPaddles(1));
	assert(Atari_POT(0) == 228 && Atari_POT(1) == 1);
	assert(BMA_JoystickState(0) == 0);
	set_axis(0, 0, 0, -32768, 32767);
	assert(Atari_POT(0) == 114);
	set_axis(0, 0, 255, 0, 255);
	set_axis(0, 1, 0, 0, 255);
	assert(Atari_POT(0) == 1 && Atari_POT(1) == 228);
	set_axis(0, 0, 400, 0, 255);
	assert(Atari_POT(0) == 1);
	assert(Atari_POT(2) == 228 && Atari_POT(7) == 228);

	/* The axis pair setting chooses the paddle axes. */
	BMA_GetPadInputSettings(0, &settings);
	settings.axis = 2;
	assert(BMA_SavePadInputSettings(0, &settings));
	assert(Atari_POT(0) == 228 && Atari_POT(1) == 228);
	set_axis(0, 2, 0, 0, 255);
	set_axis(0, 3, 255, 0, 255);
	assert(Atari_POT(0) == 228 && Atari_POT(1) == 1);

	/* Fire and Second Paddle Fire are the left and right lines. */
	pads[0] = HOST_PAD_A;
	assert(BMA_JoystickState(0) == HOST_JOY_LEFT);
	assert(PLATFORM_TRIG(0) == 1);
	assert((PLATFORM_PORT(0) & 0x0f) == (INPUT_STICK_CENTRE & ~0x04));
	BMA_DefaultPadBindings(0, bindings);
	bindings[1] = BMA_PAD_PADDLE_FIRE;
	assert(BMA_SavePadBindings(0, bindings));
	pads[0] = HOST_PAD_A | HOST_PAD_B;
	assert(BMA_JoystickState(0) == (HOST_JOY_LEFT | HOST_JOY_RIGHT));
	pads[0] = 0;

	/* Saved, reloaded, and back to a joystick. */
	BMA_InputSettingsChanged();
	assert(BMA_PadPaddles(0));
	assert(BMA_SavePadPaddles(0, FALSE));
	assert(strcmp(saved_mode[0], "joystick") == 0);
	assert(Atari_POT(0) == 228);
	pads[0] = HOST_PAD_B;
	assert(BMA_JoystickState(0) == (HOST_JOY_LEFT | HOST_JOY_DOWN));
	pads[0] = 0;
	save_ok = 0;
	assert(!BMA_SavePadPaddles(0, TRUE));
	assert(!BMA_PadPaddles(0) && strcmp(saved_mode[0], "joystick") == 0);
	save_ok = 1;
	strcpy(saved_mode[0], "spinner");
	before = logs;
	BMA_InputSettingsChanged();
	assert(logs == before + 1 && !BMA_PadPaddles(0));

	/* Only a button claims an automatic port in paddle mode. */
	reset();
	strcpy(saved_mode[0], "paddles");
	BMA_InputSettingsChanged();
	port_pad[0] = -1;
	set_axis(0, 0, 255, 0, 255);
	BMA_GamepadUpdate();
	BMA_GamepadUpdate();
	assert(port_pad[0] == -1);
	pads[0] = HOST_PAD_A;
	BMA_GamepadUpdate();
	assert(port_pad[0] == 0);
	pads[0] = 0;
}

static void persistence(void)
{
	const char *invalid[] = {"", "1,2", "1,2,3,4,5,6,7,8,9",
		"1,2,3,4,5,6,7,900", "1,2,3,4,5,6,7,1102",
		"1,2,3,4,5,6,7,-1", "1,2,3,4,5,6,7,9999999999999999999999999",
		"1,2,3,4,5,6,7,x", "1,2,3,4,5,6,7,8junk"};
	int bindings[BMA_PAD_BUTTONS], i, before;
	reset();
	BMA_DefaultPadBindings(0, bindings);
	bindings[4] = BMA_PAD_TURBO;
	save_ok = 0;
	assert(!BMA_SavePadBindings(0, bindings));
	assert(BMA_PadBinding(0, 4) == BMA_PAD_MENU);
	BMA_InputSettingsChanged();
	assert(BMA_PadBinding(0, 4) == BMA_PAD_MENU);
	save_ok = 1;
	set_ok = 0;
	assert(!BMA_SavePadBindings(0, bindings));
	set_ok = 1;
	fail_group = 3;
	assert(!BMA_SavePadBindings(0, bindings));
	fail_group = -1;
	BMA_InputSettingsChanged();
	assert(BMA_PadBinding(0, 4) == BMA_PAD_MENU);
	bindings[4] = 999;
	before = logs;
	assert(!BMA_SavePadBindings(0, bindings));
	assert(logs == before + 1);
	for (i = 1; i < (int) (sizeof(invalid) / sizeof(invalid[0])); i++) {
		strcpy(saved[0], invalid[i]);
		before = logs;
		BMA_InputSettingsChanged();
		assert(logs == before + 1);
		assert(BMA_PadBinding(0, 0) == BMA_PAD_FIRE);
		assert(BMA_PadBinding(0, 4) == BMA_PAD_MENU);
	}
}

int main(void)
{
	defaults();
	mappings();
	actions();
	keyboard_bindings();
	independent_buttons();
	menu_refresh();
	profiles();
	menus();
	m30_menus();
	menu_open_defaults();
	direction_settings();
	axis_thresholds();
	paddles();
	persistence();
	puts("PASS per-device profiles, per-port overrides, independent buttons, legacy mappings, "
	     "held keys, action edges, menu controls, unplugging, paddles and settings validation");
	return 0;
}
