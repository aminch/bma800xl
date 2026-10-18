/*
 * menu_controllers.c - BMA800XL controller settings and joystick test.
 *
 * Split from BMA800XL's menu.c. Button assignment choices follow
 * atari800's SDL2 controller menu (src/ui.c); drawing uses its ui.h interface.
 * Input and device profiles are supplied by gamepad.h.
 *
 * This file is part of BMA800XL. See LICENSE for licensing details.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screen.h"
#include "ui.h"
#include "ui_basic.h"
#include "util.h"

#include "../host.h"
#include "bma800xl.h"
#include "menu.h"

/* ---- Controllers -------------------------------------------------------------- */

static void joystick_test(void)
{
	enum { T_BACK = 1 };
	static char line[4][40];
	static UI_tMenuItem items[] = {
		UI_MENU_LABEL("Move a joystick or press fire."),
		UI_MENU_LABEL(""),
		UI_MENU_LABEL(line[0]),
		UI_MENU_LABEL(line[1]),
		UI_MENU_LABEL(line[2]),
		UI_MENU_LABEL(line[3]),
		UI_MENU_LABEL(""),
		UI_MENU_LABEL("Leave with Esc, or B on a gamepad."),
		UI_MENU_ACTION(T_BACK, "Back"),
		UI_MENU_END
	};

	BMA_joystick_test = TRUE;
	for (;;) {
		int port, option;
		for (port = 0; port < 4; port++) {
			int s = BMA_JoystickState(port);
			int pad = BMA_PortPad(port);
			char source[24];
			if (pad >= 0)
				snprintf(source, sizeof(source), "Gamepad %d", pad + 1);
			else
				strcpy(source, "");
			snprintf(line[port], sizeof(line[port]), "%d %-10s %s %s %s %s %s",
			         port + 1, source,
			         s & HOST_JOY_UP ? "Up" : "--",
			         s & HOST_JOY_DOWN ? "Dn" : "--",
			         s & HOST_JOY_LEFT ? "Lt" : "--",
			         s & HOST_JOY_RIGHT ? "Rt" : "--",
			         s & HOST_JOY_FIRE ? "Fire" : "----");
		}
		/* The input code sends Tab (-2 here) when a joystick changes. */
		option = UI_driver->fSelect("Joystick Test", 0, T_BACK, items, NULL);
		if (option != -2)
			break;
	}
	BMA_joystick_test = FALSE;
}

static void binding_name(int binding, char *text, unsigned size)
{
	if (binding < BMA_PAD_ACTIONS)
		Util_strlcpy(text, BMA_PadActionName(binding), size);
	else if (binding == BMA_PAD_KEY_BASE + 0x2c)
		Util_strlcpy(text, "Space", size);
	else if (binding == BMA_PAD_KEY_BASE + 0x28)
		Util_strlcpy(text, "Return", size);
	else if (binding == BMA_PAD_KEY_BASE + 0x29)
		Util_strlcpy(text, "Escape", size);
	else if (binding >= BMA_PAD_KEY_BASE + 4 && binding <= BMA_PAD_KEY_BASE + 29)
		snprintf(text, size, "Key %c", 'A' + binding - BMA_PAD_KEY_BASE - 4);
	else
		snprintf(text, size, "Keyboard HID $%02X", binding - BMA_PAD_KEY_BASE);
}

static int button_action(int pad, int button, int current)
{
	static const char *const categories[] = {"Action", "Atari Key", "Keyboard Key", "None", NULL};
	static const char *const keyboard[] = {"Press Keyboard Key...", "Escape Key", NULL};
	static const int actions[] = {
		BMA_PAD_FIRE, BMA_PAD_MENU, BMA_PAD_KEYBOARD, BMA_PAD_WARMSTART,
		BMA_PAD_COLDSTART, BMA_PAD_TURBO, BMA_PAD_RUN, BMA_PAD_DISK,
		BMA_PAD_CARTRIDGE, BMA_PAD_TAPE, BMA_PAD_SAVE, BMA_PAD_LOAD,
		BMA_PAD_QUICKSAVE, BMA_PAD_QUICKLOAD
	};
	static const int atari_keys[] = {
		BMA_PAD_START, BMA_PAD_SELECT, BMA_PAD_OPTION, BMA_PAD_HELP, BMA_PAD_BREAK
	};
	const char *choices[sizeof(actions) / sizeof(actions[0]) + 1];
	char label[24], binding[24], message[64];
	const int *values;
	int category, count, i, selected = 0;
	BMA_PadButtonLabel(pad, button, label, sizeof(label));
	binding_name(current, binding, sizeof(binding));
	snprintf(message, sizeof(message), "%s: %s", label, binding);
	UI_driver->fMessage(message, 0);
	category = current == BMA_PAD_NONE ? 3 : current >= BMA_PAD_KEY_BASE ? 2
	         : current >= BMA_PAD_OPTION && current <= BMA_PAD_BREAK ? 1 : 0;
	category = BMA_MenuChoose("Button Assignment", categories, category);
	if (category < 0)
		return -1;
	if (category == 3)
		return BMA_PAD_NONE;
	if (category == 2) {
		i = BMA_MenuChoose("Keyboard Key", keyboard, current == BMA_PAD_KEY_BASE + 0x29);
		return i < 0 ? -1 : i == 1 ? BMA_PAD_KEY_BASE + 0x29 : BMA_CapturePadKey();
	}
	values = category == 0 ? actions : atari_keys;
	count = category == 0 ? sizeof(actions) / sizeof(actions[0])
	                      : sizeof(atari_keys) / sizeof(atari_keys[0]);
	for (i = 0; i < count; i++) {
		choices[i] = BMA_PadActionName(values[i]);
		if (values[i] == current)
			selected = i;
	}
	choices[count] = NULL;
	i = BMA_MenuChoose(category == 0 ? "Action" : "Atari Key", choices, selected);
	return i < 0 ? -1 : values[i];
}

static int controller_select(const char *title, int flags, int *option,
                             const UI_tMenuItem *items)
{
	int selection_type = 0;
	*option = UI_driver->fSelect(title, flags, *option, items, &selection_type);
	BMA_GamepadMenuWatch(FALSE);
	return *option >= 0 && selection_type == UI_BASIC_USER_REFRESH;
}

static void button_mapping(int port)
{
	UI_tMenuItem items[BMA_PAD_BUTTONS + 3];
	char names[BMA_PAD_BUTTONS][24], labels[BMA_PAD_BUTTONS][24], title[40];
	int option = BMA_PAD_BUTTONS, button, position;
	snprintf(title, sizeof(title), "Joystick %d Buttons", port + 1);

	for (;;) {
		struct host_pad_info info;
		int bindings[BMA_PAD_BUTTONS], binding, saved;
		int pad = BMA_PortPad(port), n = 1;
		BMA_GamepadMenuWatch(TRUE);
		int connected = host_pad_info(pad, &info);
		uint32_t mask = connected ? info.button_mask : 0xfff;
		memset(items, 0, sizeof(items));
		items[0].flags = UI_ITEM_ACTION;
		items[0].retval = BMA_PAD_BUTTONS;
		items[0].item = "Select to Start Button Detection...";
		for (button = 0; button < BMA_PAD_BUTTONS; button++) {
			bindings[button] = BMA_PadBinding(port, button);
			binding_name(bindings[button], names[button], sizeof(names[button]));
		}
		for (position = 0; position < BMA_PAD_BUTTONS; position++) {
			button = BMA_PadButtonIndex(pad, position);
			if (!(mask & (UINT32_C(1) << button)) || button == 12)
				continue;
			BMA_PadButtonLabel(pad, button, labels[button], sizeof(labels[button]));
			items[n].flags = UI_ITEM_SUBMENU;
			items[n].retval = button;
			items[n].item = labels[button];
			items[n++].suffix = names[button];
		}
		items[n].flags = UI_ITEM_ACTION;
		items[n].retval = BMA_PAD_BUTTONS + 1;
		items[n++].item = "Restore Defaults";
		items[n].flags = UI_ITEM_END;
		if (controller_select(title, 0, &option, items))
			continue;
		if (option < 0)
			return;
		if (option == BMA_PAD_BUTTONS + 1)
			saved = BMA_ResetPadBindings(port);
		else {
			button = option == BMA_PAD_BUTTONS ? BMA_CapturePadButton(port) : option;
			if (button < 0)
				continue;
			for (n = 0; n < BMA_PAD_BUTTONS; n++)
				bindings[n] = BMA_PadBinding(port, n);
			binding = button_action(BMA_PortPad(port), button, bindings[button]);
			if (binding < 0)
				continue;
			bindings[button] = binding;
			saved = BMA_SavePadBindings(port, bindings);
		}
		if (!saved)
			UI_driver->fMessage("Cannot save gamepad buttons", 1);
	}
}

static void port_device_text(int port, char *text, unsigned size)
{
	char name[24];
	struct host_pad_info info;
	int pad = BMA_PortPad(port), mode;
	snprintf(name, sizeof(name), "joy%d_pad", port + 1);
	mode = host_option_int(name, 0);
	if (mode < 0)
		Util_strlcpy(text, "None", size);
	else if (host_pad_info(pad, &info))
		snprintf(text, size, mode == 0 ? "Auto (%s)" : "%s", info.name);
	else if (mode == 0)
		Util_strlcpy(text, "Automatic", size);
	else
		snprintf(text, size, "Gamepad %d (disconnected)", mode);
}

static void select_port_device(int port)
{
	char name[24], text[HOST_PADS][80];
	UI_tMenuItem items[HOST_PADS + 3];
	UBYTE *background = Util_malloc(Screen_WIDTH * Screen_HEIGHT);
	int slot, mode, selected;
	struct host_pad_info info[HOST_PADS], check;
	int connected[HOST_PADS];
	snprintf(name, sizeof(name), "joy%d_pad", port + 1);
	mode = host_option_int(name, 0);
	selected = mode < 0 ? HOST_PADS + 1 : mode;
	memcpy(background, Screen_atari, Screen_WIDTH * Screen_HEIGHT);
	for (;;) {
		int count = 1;
		BMA_GamepadMenuWatch(TRUE);
		memcpy(Screen_atari, background, Screen_WIDTH * Screen_HEIGHT);
		memset(items, 0, sizeof(items));
		items[0].flags = UI_ITEM_ACTION;
		items[0].item = "Automatic";
		for (slot = 0; slot < HOST_PADS; slot++) {
			connected[slot] = host_pad_info(slot, &info[slot]);
			if (!connected[slot] && mode != slot + 1)
				continue;
			snprintf(text[slot], sizeof(text[slot]), "%d: %.63s", slot + 1,
			         connected[slot] ? info[slot].name : "(disconnected)");
			items[count].flags = UI_ITEM_ACTION;
			items[count].retval = slot + 1;
			items[count++].item = text[slot];
		}
		items[count].flags = UI_ITEM_ACTION;
		items[count].retval = HOST_PADS + 1;
		items[count++].item = "None";
		items[count].flags = UI_ITEM_END;
		if (!controller_select("Device", UI_SELECT_POPUP, &selected, items))
			break;
	}
	free(background);
	if (selected < 0)
		return;
	if (selected == HOST_PADS + 1)
		selected = -1;
	if (selected == mode)
		return;
	slot = selected - 1;
	if (slot >= 0 && (!host_pad_info(slot, &check)
	    || !connected[slot] || check.generation != info[slot].generation)) {
		UI_driver->fMessage("Gamepad changed; choose again", 1);
		return;
	}
	BMA_MenuSaveSettingInt(name, selected);
	BMA_InputSettingsChanged();
}

static void joystick_menu(int port)
{
	char text[80], title[32];
	UI_tMenuItem items[] = {
		UI_MENU_ACTION_PREFIX(0, "Device: ", text),
		UI_MENU_SUBMENU(1, "Button Mapping"),
		UI_MENU_END
	};
	int option = 0;
	snprintf(title, sizeof(title), "Joystick %d", port + 1);
	for (;;) {
		BMA_GamepadMenuWatch(TRUE);
		port_device_text(port, text, sizeof(text));
		if (controller_select(title, 0, &option, items))
			continue;
		if (option == 0) {
			select_port_device(port);
		}
		else if (option == 1)
			button_mapping(port);
		else
			return;
	}
}

int BMA_MenuControllers(void)
{
	enum { C_PORT1, C_PORT2, C_PORT3, C_PORT4, C_KEYPAD, C_ARROWS, C_GPIO, C_LAYOUT, C_TEST };
	static const char *const layouts[] = {"us", "uk", "no", NULL};
	static const char *const layout_names[] = {"US", "UK", "Norwegian", NULL};
	static char port_text[4][32], layout[16];
	static UI_tMenuItem items[] = {
		UI_MENU_ACTION_PREFIX(C_PORT1, "Joystick 1: ", port_text[0]),
		UI_MENU_ACTION_PREFIX(C_PORT2, "Joystick 2: ", port_text[1]),
		UI_MENU_ACTION_PREFIX(C_PORT3, "Joystick 3: ", port_text[2]),
		UI_MENU_ACTION_PREFIX(C_PORT4, "Joystick 4: ", port_text[3]),
		UI_MENU_CHECK(C_KEYPAD, "Keypad is Joystick 1"),
		UI_MENU_CHECK(C_ARROWS, "Arrow Keys are Joystick 1"),
		UI_MENU_CHECK(C_GPIO, "GPIO Joysticks (reboot)"),
		UI_MENU_ACTION_PREFIX(C_LAYOUT, "Keyboard Layout: ", layout),
		UI_MENU_LABEL(""),
		UI_MENU_SUBMENU(C_TEST, "Joystick Test"),
		UI_MENU_END
	};
	int option = C_PORT1;
	int reboot_needed = FALSE;

	for (;;) {
		int port;
		BMA_GamepadMenuWatch(TRUE);
		for (port = 0; port < 4; port++) {
			port_device_text(port, port_text[port], sizeof(port_text[port]));
		}
		items[C_KEYPAD].flags = BMA_MenuCheckFlags(BMA_MenuSettingOn("keypad_joystick", TRUE));
		items[C_ARROWS].flags = BMA_MenuCheckFlags(BMA_MenuSettingOn("arrow_joystick", FALSE));
		items[C_GPIO].flags = BMA_MenuCheckFlags(BMA_MenuSettingOn("gpio_joysticks", FALSE));
		Util_strlcpy(layout, layout_names[BMA_MenuValueIndex(layouts, host_option("keyboard", "us"))],
		             sizeof(layout));

		if (controller_select("Controllers", 0, &option, items))
			continue;
		switch (option) {
		case C_PORT1:
		case C_PORT2:
		case C_PORT3:
		case C_PORT4: {
			joystick_menu(option);
			break;
		}
		case C_KEYPAD:
			BMA_MenuSaveSetting("keypad_joystick", BMA_MenuSettingOn("keypad_joystick", TRUE) ? "0" : "1");
			BMA_InputSettingsChanged();
			break;
		case C_ARROWS:
			BMA_MenuSaveSetting("arrow_joystick", BMA_MenuSettingOn("arrow_joystick", FALSE) ? "0" : "1");
			BMA_InputSettingsChanged();
			break;
		case C_GPIO:
			BMA_MenuSaveSetting("gpio_joysticks", BMA_MenuSettingOn("gpio_joysticks", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case C_LAYOUT: {
			int i = BMA_MenuChoose("Keyboard Layout", layout_names,
			               BMA_MenuValueIndex(layouts, host_option("keyboard", "us")));
			if (i >= 0) {
				BMA_MenuSaveSetting("keyboard", layouts[i]);
				BMA_InputSettingsChanged();
			}
			break;
		}
		case C_TEST:
			joystick_test();
			break;
		default:
			return reboot_needed;
		}
	}
}
