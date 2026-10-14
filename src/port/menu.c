/*
 * menu.c - the BMA800XL menu.
 *
 * BMA800XL's own settings (network, controllers, video, sound, power) and
 * information, shown with atari800's menu code so it looks and works like
 * atari800's menu, which is one item away. F1, the gamepad's Home button
 * or Select+Start open it. Settings are saved in bma800xl.cfg.
 *
 * This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "akey.h"
#include "atari.h"
#include "input.h"
#include "memory.h"
#include "platform.h"
#include "sound.h"
#include "ui.h"
#include "util.h"

#include "../host.h"
#include "bma800xl.h"

#ifndef BMA800XL_VERSION
#define BMA800XL_VERSION "dev"
#endif

/* The version with the git commit, when the build gives one. */
#ifdef BMA800XL_GIT
#define BMA800XL_FULL_VERSION BMA800XL_VERSION " (" BMA800XL_GIT ")"
#else
#define BMA800XL_FULL_VERSION BMA800XL_VERSION
#endif

#define WPA_CONFIG "wpa_supplicant.conf"

/* Something changed that only takes effect after a reboot. */
static int reboot_needed = FALSE;

/* Reboots or shuts down now, closing atari800's files first. */
static void power(int off)
{
	bma_request_power(off);
	BMA_MenuRequests();
}

/* ---- Settings ------------------------------------------------------------ */

static void save_setting(const char *name, const char *value)
{
	if (!host_option_set(name, value) || !host_options_save())
		UI_driver->fMessage("Cannot save bma800xl.cfg", 1);
}

static void save_setting_int(const char *name, int value)
{
	char text[16];
	snprintf(text, sizeof(text), "%d", value);
	save_setting(name, text);
}

static int setting_on(const char *name, int default_value)
{
	const char *value = host_option(name, NULL);
	if (value == NULL || *value == '\0')
		return default_value;
	return strcmp(value, "1") == 0 || strcmp(value, "yes") == 0
	    || strcmp(value, "true") == 0 || strcmp(value, "on") == 0;
}

static UWORD check_flags(int on)
{
	return UI_ITEM_CHECK | (on ? UI_ITEM_CHECKED : 0);
}

/* Shows CHOICES (NULL-terminated) as a popup; returns the index chosen, or
   -1 if cancelled. */
static int choose(const char *title, const char *const choices[], int current)
{
	static UI_tMenuItem items[24];
	int n;
	for (n = 0; choices[n] != NULL && n < 23; n++) {
		items[n].flags = UI_ITEM_ACTION;
		items[n].retval = n;
		items[n].prefix = NULL;
		items[n].item = (char *) choices[n];
		items[n].suffix = NULL;
	}
	items[n].flags = UI_ITEM_END;
	return UI_driver->fSelect(title, UI_SELECT_POPUP, current, items, NULL);
}

/* Index of VALUE in VALUES (NULL-terminated), or 0. */
static int value_index(const char *const values[], const char *value)
{
	int i;
	for (i = 0; value != NULL && values[i] != NULL; i++)
		if (strcmp(values[i], value) == 0)
			return i;
	return 0;
}

/* ---- Information ------------------------------------------------------------ */

static const char *machine_name(void)
{
	switch (Atari800_machine_type) {
	case Atari800_MACHINE_800:
		return "Atari 400/800";
	case Atari800_MACHINE_5200:
		return "Atari 5200";
	default:
		if (Atari800_builtin_game)
			return "Atari XEGS";
		if (MEMORY_ram_size == 128)
			return "Atari 130XE";
		if (MEMORY_ram_size > 128)
			return "Atari 320XE";
		if (Atari800_keyboard_leds)
			return "Atari 1200XL";
		return "Atari 800XL";
	}
}

static void machine_line(char *text, unsigned size)
{
	int w, h;
	host_display_size(&w, &h);
	snprintf(text, size, "%s %s, %dx%d %.0fHz", machine_name(),
	         Atari800_tv_mode == Atari800_TV_PAL ? "PAL" : "NTSC",
	         w, h, BMA_RefreshRate());
}

static void network_line(char *text, unsigned size)
{
	char status[40];
	host_network_status(status, sizeof(status));
	snprintf(text, size, "Network: %s", status);
}

/* fInfoScreen wants lines ended by '\0' and the text ended by '\n'. */
static void info_screen(const char *title, const char *const lines[])
{
	static char text[2048];
	char *p = text;
	int i;
	for (i = 0; lines[i] != NULL; i++) {
		size_t len = strlen(lines[i]);
		if (p + len + 2 >= text + sizeof(text))
			break;
		memcpy(p, lines[i], len + 1);
		p += len + 1;
	}
	*p = '\n';
	UI_driver->fInfoScreen(title, text);
}

static void about(void)
{
	char version[40], model[40], machine[64], network[64];
	const char *lines[24];
	int n = 0;
	snprintf(version, sizeof(version), "Version %s", BMA800XL_FULL_VERSION);
	snprintf(model, sizeof(model), "%.38s", host_model_name());
	machine_line(machine, sizeof(machine));
	network_line(network, sizeof(network));
	lines[n++] = "BMA800XL";
	lines[n++] = "Bare metal Atari 800XL emulator";
	lines[n++] = version;
	lines[n++] = "";
	lines[n++] = "atari800 " PACKAGE_VERSION;
	lines[n++] = "  atari800.github.io";
	lines[n++] = "Circle by Rene Stange";
	lines[n++] = "Display, sound, network";
	lines[n++] = " and WebUI from BMC64";
	lines[n++] = "";
	lines[n++] = model;
	lines[n++] = machine;
	lines[n++] = network;
	lines[n++] = "";
	lines[n++] = "github.com/aminch/bma800xl";
	lines[n] = NULL;
	info_screen("About BMA800XL", lines);
}

static void licence(void)
{
	static const char *const lines[] = {
		"BMA800XL is free software: you can",
		"redistribute it and/or modify it under",
		"the terms of the GNU General Public",
		"License, version 3 or (at your option)",
		"any later version.",
		"",
		"It is distributed in the hope that it",
		"will be useful, but WITHOUT ANY",
		"WARRANTY; without even the implied",
		"warranty of MERCHANTABILITY or FITNESS",
		"FOR A PARTICULAR PURPOSE.",
		"",
		"See LICENSE.txt on the SD card. It",
		"includes atari800 (GPL 2 or later),",
		"Circle (GPL 3), BMC64 code (Apache 2.0",
		"/ GPL 3) and Altirra's free ROMs.",
		NULL
	};
	info_screen("Licence", lines);
}

/* ---- Network ---------------------------------------------------------------- */

/* The first network in wpa_supplicant.conf, as the web UI and BMC64 write it. */
static char wifi_ssid[33], wifi_psk[64], wifi_country[3];

static void copy_quoted(char *dst, size_t size, const char *src)
{
	const char *end;
	if (*src == '"')
		src++;
	end = strchr(src, '"');
	if (end == NULL)
		end = src + strcspn(src, "\r\n");
	if ((size_t) (end - src) >= size)
		end = src + size - 1;
	memcpy(dst, src, end - src);
	dst[end - src] = '\0';
}

/* The menus show strings as UTF-8, with bytes that aren't valid UTF-8 taken
   as Latin-1 (src/patches/atari800_ui_basic_utf8.diff). Returns how many
   bytes the character at s takes, by the same rules. */
static int char_bytes(const char *s)
{
	const unsigned char *p = (const unsigned char *) s;
	int len, i;
	if (p[0] >= 0xc2 && p[0] <= 0xdf)
		len = 2;
	else if (p[0] >= 0xe0 && p[0] <= 0xef)
		len = 3;
	else if (p[0] >= 0xf0 && p[0] <= 0xf4)
		len = 4;
	else
		return 1;
	for (i = 1; i < len; i++)
		if ((p[i] & 0xc0) != 0x80)
			return 1;
	/* overlong forms, surrogates and code points past U+10FFFF */
	if ((p[0] == 0xe0 && p[1] < 0xa0) || (p[0] == 0xed && p[1] >= 0xa0)
	    || (p[0] == 0xf0 && p[1] < 0x90) || (p[0] == 0xf4 && p[1] >= 0x90))
		return 1;
	return len;
}

/* Copies the first width characters (not bytes) of src, padded with spaces
   to width characters if pad is set. size must allow 4 bytes a character. */
static void copy_chars(char *dst, size_t size, const char *src, int width, int pad)
{
	char *end = dst + size - 1;
	for (; width > 0 && *src != '\0'; width--) {
		int n = char_bytes(src);
		if (dst + n > end)
			break;
		memcpy(dst, src, n);
		dst += n;
		src += n;
	}
	for (; pad && width > 0 && dst < end; width--)
		*dst++ = ' ';
	*dst = '\0';
}

static void read_wifi_config(void)
{
	char path[FILENAME_MAX], line[160];
	FILE *f;
	wifi_ssid[0] = wifi_psk[0] = '\0';
	strcpy(wifi_country, "US");
	Util_catpath(path, BMA_SdRoot(), WPA_CONFIG);
	f = fopen(path, "r");
	if (f == NULL)
		return;
	while (fgets(line, sizeof(line), f) != NULL) {
		char *p = line + strspn(line, " \t");
		if (strncmp(p, "country=", 8) == 0)
			copy_quoted(wifi_country, sizeof(wifi_country), p + 8);
		else if (strncmp(p, "ssid=", 5) == 0 && wifi_ssid[0] == '\0')
			copy_quoted(wifi_ssid, sizeof(wifi_ssid), p + 5);
		else if (strncmp(p, "psk=", 4) == 0 && wifi_psk[0] == '\0')
			copy_quoted(wifi_psk, sizeof(wifi_psk), p + 4);
	}
	fclose(f);
}

static int write_wifi_config(void)
{
	char path[FILENAME_MAX];
	FILE *f;
	Util_catpath(path, BMA_SdRoot(), WPA_CONFIG);
	f = fopen(path, "w");
	if (f == NULL)
		return FALSE;
	fprintf(f, "country=%s\n\nnetwork={\n\tssid=\"%s\"\n", wifi_country, wifi_ssid);
	if (wifi_psk[0] != '\0')
		fprintf(f, "\tpsk=\"%s\"\n\tkey_mgmt=WPA-PSK\n", wifi_psk);
	else
		fprintf(f, "\tkey_mgmt=NONE\n");
	fprintf(f, "}\n");
	return fclose(f) == 0;
}

static void edit_wifi(const char *title, char *value, int size)
{
	char text[64];
	Util_strlcpy(text, value, sizeof(text));
	if (UI_driver->fEditString(title, text, size)) {
		Util_strlcpy(value, text, size);
		if (write_wifi_config())
			reboot_needed = TRUE;
		else
			UI_driver->fMessage("Cannot write " WPA_CONFIG, 1);
	}
}

static int compare_signal(const void *a, const void *b)
{
	return ((const struct host_wifi_ap *) b)->signal - ((const struct host_wifi_ap *) a)->signal;
}

/* Wi-Fi is off: offers to switch to it and reboot, after which a scan
   works. */
static void offer_wifi_reboot(void)
{
	enum { W_YES, W_NO };
	static UI_tMenuItem items[] = {
		UI_MENU_LABEL("Scanning needs Wi-Fi switched on,"),
		UI_MENU_LABEL("which takes a reboot. Then come back"),
		UI_MENU_LABEL("here and scan again."),
		UI_MENU_LABEL(""),
		UI_MENU_ACTION(W_YES, "Switch to Wi-Fi and Reboot"),
		UI_MENU_ACTION(W_NO, "Cancel"),
		UI_MENU_END
	};
	if (UI_driver->fSelect("Wi-Fi is Off", 0, W_YES, items, NULL) == W_YES) {
		save_setting("network", "wifi");
		power(0);
	}
}

/* Scans for Wi-Fi networks and lets one be chosen, which becomes the Wi-Fi
   network. Returns 1 if it was chosen, 2 if it also needs a password, or 0
   if nothing changed. */
static int scan_wifi(void)
{
	enum { MAX_APS = 20 };
	static struct host_wifi_ap aps[MAX_APS];
	static char lines[MAX_APS][128];
	static UI_tMenuItem items[MAX_APS + 3];
	int n, i, chosen;

	UI_driver->fMessage("Scanning for Wi-Fi networks...", 0);
	n = host_wifi_scan(aps, MAX_APS);
	if (n < 0) {
		offer_wifi_reboot();
		return FALSE;
	}
	if (n == 0) {
		UI_driver->fMessage("No Wi-Fi networks found", 1);
		return FALSE;
	}
	qsort(aps, n, sizeof(aps[0]), compare_signal);

	items[0].flags = UI_ITEM_ACTION;
	items[0].retval = -1;
	items[0].prefix = NULL;
	items[0].item = "Network                   Lock Signal";
	items[0].suffix = NULL;
	for (i = 0; i < n; i++) {
		char name[25 * 4 + 1];
		copy_chars(name, sizeof(name), aps[i].ssid, 25, TRUE);
		snprintf(lines[i], sizeof(lines[i]), "%s %-4s %4ddBm",
		         name, aps[i].secure ? "Yes" : "", aps[i].signal);
		items[i + 1].flags = UI_ITEM_ACTION;
		items[i + 1].retval = i;
		items[i + 1].prefix = NULL;
		items[i + 1].item = lines[i];
		items[i + 1].suffix = NULL;
	}
	items[n + 1].flags = UI_ITEM_END;

	chosen = UI_driver->fSelect("Choose a Wi-Fi Network", 0, 0, items, NULL);
	if (chosen < 0 || chosen >= n)
		return FALSE;

	/* The password is entered on the Network page; an open network has
	   none, and the same network keeps the one it has. */
	if (!aps[chosen].secure)
		wifi_psk[0] = '\0';
	else if (strcmp(wifi_ssid, aps[chosen].ssid) != 0)
		wifi_psk[0] = '\0';
	Util_strlcpy(wifi_ssid, aps[chosen].ssid, sizeof(wifi_ssid));
	if (!write_wifi_config()) {
		UI_driver->fMessage("Cannot write " WPA_CONFIG, 1);
		return 0;
	}
	save_setting("network", "wifi");
	reboot_needed = TRUE;
	return aps[chosen].secure && wifi_psk[0] == '\0' ? 2 : 1;
}

static void network_menu(void)
{
	enum { N_DEVICE, N_SCAN, N_SSID, N_PASSWORD, N_COUNTRY, N_WEBUI, N_PIN, N_TIMEZONE, N_REBOOT };
	static const char *const devices[] = {"off", "ethernet", "wifi", NULL};
	static const char *const device_names[] = {"Off", "Ethernet", "Wi-Fi", NULL};
	static char status[64], device[16], ssid[20 * 4 + 1], password[16], country[8], pin[12], tz[16];
	static UI_tMenuItem items[] = {
		UI_MENU_LABEL(status),
		UI_MENU_LABEL(""),
		UI_MENU_ACTION_PREFIX(N_DEVICE, "Network: ", device),
		UI_MENU_SUBMENU(N_SCAN, "Scan for Wi-Fi Networks"),
		UI_MENU_ACTION_PREFIX(N_SSID, "Wi-Fi Network: ", ssid),
		UI_MENU_ACTION_PREFIX(N_PASSWORD, "Wi-Fi Password: ", password),
		UI_MENU_ACTION_PREFIX(N_COUNTRY, "Wi-Fi Country: ", country),
		UI_MENU_CHECK(N_WEBUI, "Web UI"),
		UI_MENU_ACTION_PREFIX(N_PIN, "Web UI PIN: ", pin),
		UI_MENU_ACTION_PREFIX(N_TIMEZONE, "Time Zone: ", tz),
		UI_MENU_LABEL(""),
		UI_MENU_LABEL("Changes apply after a reboot."),
		UI_MENU_ACTION(N_REBOOT, "Reboot Now"),
		UI_MENU_END
	};
	int option = N_DEVICE;

	for (;;) {
		int device_index = value_index(devices, host_option("network", "off"));
		int minutes = host_option_int("timezone", 0);
		const char *webui_pin = host_option("webui_pin", "");

		read_wifi_config();
		network_line(status, sizeof(status));
		Util_strlcpy(device, device_names[device_index], sizeof(device));
		copy_chars(ssid, sizeof(ssid), wifi_ssid[0] ? wifi_ssid : "(none)", 20, FALSE);
		Util_strlcpy(password, wifi_psk[0] ? "********" : "(none)", sizeof(password));
		Util_strlcpy(country, wifi_country, sizeof(country));
		Util_strlcpy(pin, webui_pin[0] ? webui_pin : "(none)", sizeof(pin));
		if (minutes == 0)
			strcpy(tz, "UTC");
		else
			snprintf(tz, sizeof(tz), "UTC%+d", minutes / 60);
		items[7].flags = check_flags(setting_on("webui", TRUE));

		option = UI_driver->fSelect("Network", 0, option, items, NULL);
		switch (option) {
		case N_DEVICE: {
			int i = choose("Network", device_names, device_index);
			if (i >= 0 && i != device_index) {
				save_setting("network", devices[i]);
				reboot_needed = TRUE;
			}
			break;
		}
		case N_SCAN:
			/* Next, the password, if the network needs one. */
			if (scan_wifi() == 2)
				option = N_PASSWORD;
			break;
		case N_SSID:
			edit_wifi("Wi-Fi network name", wifi_ssid, sizeof(wifi_ssid));
			break;
		case N_PASSWORD:
			wifi_psk[0] = '\0';
			edit_wifi("Wi-Fi password (blank: none)", wifi_psk, sizeof(wifi_psk));
			break;
		case N_COUNTRY:
			edit_wifi("Two letter country code", wifi_country, sizeof(wifi_country));
			break;
		case N_WEBUI:
			save_setting("webui", setting_on("webui", TRUE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case N_PIN: {
			char text[9];
			Util_strlcpy(text, webui_pin, sizeof(text));
			if (UI_driver->fEditString("Web UI PIN (blank: none)", text, sizeof(text))) {
				save_setting("webui_pin", text);
				reboot_needed = TRUE;
			}
			break;
		}
		case N_TIMEZONE: {
			static char labels[27][8];
			static const char *zones[28];
			int h, current = 12;
			for (h = -12; h <= 14; h++) {
				if (h == 0)
					strcpy(labels[h + 12], "UTC");
				else
					snprintf(labels[h + 12], sizeof(labels[0]), "UTC%+d", h);
				zones[h + 12] = labels[h + 12];
				if (h * 60 == minutes)
					current = h + 12;
			}
			zones[27] = NULL;
			h = choose("Time Zone", zones, current);
			if (h >= 0 && h != current) {
				save_setting_int("timezone", (h - 12) * 60);
				reboot_needed = TRUE;
			}
			break;
		}
		case N_REBOOT:
			power(0);
			return;
		default:
			return;
		}
	}
}

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

static void controllers_menu(void)
{
	enum { C_PORT1, C_PORT2, C_PORT3, C_PORT4, C_KEYPAD, C_ARROWS, C_GPIO, C_LAYOUT, C_TEST };
	static const char *const port_names[] = {"joy1_pad", "joy2_pad", "joy3_pad", "joy4_pad"};
	static const char *const port_titles[] = {"Joystick 1", "Joystick 2", "Joystick 3", "Joystick 4"};
	static const char *const modes[] = {"Automatic", "Gamepad 1", "Gamepad 2",
	                                    "Gamepad 3", "Gamepad 4", "None", NULL};
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

	for (;;) {
		int port;
		for (port = 0; port < 4; port++) {
			int mode = host_option_int(port_names[port], 0);
			int pad = BMA_PortPad(port);
			if (mode == 0 && pad >= 0)
				snprintf(port_text[port], sizeof(port_text[port]), "Auto (Gamepad %d)", pad + 1);
			else if (mode == 0)
				strcpy(port_text[port], "Automatic");
			else if (mode > 0 && mode <= 4)
				Util_strlcpy(port_text[port], modes[mode], sizeof(port_text[port]));
			else
				strcpy(port_text[port], "None");
		}
		items[C_KEYPAD].flags = check_flags(setting_on("keypad_joystick", TRUE));
		items[C_ARROWS].flags = check_flags(setting_on("arrow_joystick", FALSE));
		items[C_GPIO].flags = check_flags(setting_on("gpio_joysticks", FALSE));
		Util_strlcpy(layout, layout_names[value_index(layouts, host_option("keyboard", "us"))],
		             sizeof(layout));

		option = UI_driver->fSelect("Controllers", 0, option, items, NULL);
		switch (option) {
		case C_PORT1:
		case C_PORT2:
		case C_PORT3:
		case C_PORT4: {
			int mode = host_option_int(port_names[option], 0);
			int current = mode < 0 ? 5 : mode;
			int i = choose(port_titles[option], modes, current);
			if (i >= 0 && i != current) {
				save_setting_int(port_names[option], i == 5 ? -1 : i);
				BMA_InputSettingsChanged();
			}
			break;
		}
		case C_KEYPAD:
			save_setting("keypad_joystick", setting_on("keypad_joystick", TRUE) ? "0" : "1");
			BMA_InputSettingsChanged();
			break;
		case C_ARROWS:
			save_setting("arrow_joystick", setting_on("arrow_joystick", FALSE) ? "0" : "1");
			BMA_InputSettingsChanged();
			break;
		case C_GPIO:
			save_setting("gpio_joysticks", setting_on("gpio_joysticks", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case C_LAYOUT: {
			int i = choose("Keyboard Layout", layout_names,
			               value_index(layouts, host_option("keyboard", "us")));
			if (i >= 0) {
				save_setting("keyboard", layouts[i]);
				BMA_InputSettingsChanged();
			}
			break;
		}
		case C_TEST:
			joystick_test();
			break;
		default:
			return;
		}
	}
}

/* ---- Video and sound ---------------------------------------------------------- */

static void video_menu(void)
{
	enum { V_LOCK, V_BORDER, V_RESOLUTION, V_SHADER, V_CURVATURE, V_SMOOTH };
	static const char *const locks[] = {"auto", "on", "off", NULL};
	static const char *const lock_names[] = {"Automatic", "On", "Off", NULL};
	static const char *const borders[] = {"normal", "none", "full", NULL};
	static const char *const border_names[] = {"Normal", "None", "Full", NULL};
	/* The Pi 5 only: its firmware ignores hdmi_mode. */
	static const char *const resolutions[] = {"auto", "1920x1080", "1280x720", NULL};
	static const char *const resolution_names[] = {"Auto", "1920x1080", "1280x720", NULL};
	static char display[40], lock[16], border[16], resolution[16];
	static UI_tMenuItem items[] = {
		UI_MENU_LABEL(display),
		UI_MENU_LABEL(""),
		UI_MENU_ACTION_PREFIX(V_LOCK, "Frames Locked to Display: ", lock),
		UI_MENU_ACTION_PREFIX(V_BORDER, "Border: ", border),
		UI_MENU_ACTION_PREFIX(V_RESOLUTION, "Resolution: ", resolution),
		UI_MENU_CHECK(V_SHADER, "CRT Shader (reboot)"),
		UI_MENU_CHECK(V_CURVATURE, "CRT Curvature (reboot)"),
		UI_MENU_CHECK(V_SMOOTH, "Smooth Scaling (reboot)"),
		UI_MENU_END
	};
	int option = V_LOCK;

	for (;;) {
		int w, h;
		int lock_index = value_index(locks, host_option("vsync", "auto"));
		int border_index = value_index(borders, host_option("border", "normal"));
		int resolution_index = value_index(resolutions, host_option("resolution", "auto"));
		host_display_size(&w, &h);
		snprintf(display, sizeof(display), "Display: %dx%d, %.2f Hz", w, h, BMA_RefreshRate());
		Util_strlcpy(lock, lock_names[lock_index], sizeof(lock));
		Util_strlcpy(border, border_names[border_index], sizeof(border));
		Util_strlcpy(resolution, resolution_names[resolution_index], sizeof(resolution));
		items[4].flags = host_display_resolution_choice() ? UI_ITEM_ACTION : UI_ITEM_HIDDEN;
		items[5].flags = check_flags(setting_on("crt_shader", FALSE));
		items[6].flags = check_flags(setting_on("crt_curvature", FALSE));
		items[7].flags = check_flags(setting_on("smooth", FALSE));

		option = UI_driver->fSelect("Video", 0, option, items, NULL);
		switch (option) {
		case V_LOCK: {
			int i = choose("Frames Locked to Display", lock_names, lock_index);
			if (i >= 0) {
				save_setting("vsync", locks[i]);
				BMA_VideoSettingsChanged();
			}
			break;
		}
		case V_BORDER: {
			int i = choose("Border", border_names, border_index);
			if (i >= 0) {
				save_setting("border", borders[i]);
				BMA_VideoSettingsChanged();
			}
			break;
		}
		case V_RESOLUTION: {
			int i = choose("Resolution", resolution_names, resolution_index);
			if (i >= 0 && i != resolution_index) {
				save_setting("resolution", resolutions[i]);
				BMA_DisplayModeChanged();
			}
			break;
		}
		case V_SHADER:
			save_setting("crt_shader", setting_on("crt_shader", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case V_CURVATURE:
			save_setting("crt_curvature", setting_on("crt_curvature", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case V_SMOOTH:
			save_setting("smooth", setting_on("smooth", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		default:
			return;
		}
	}
}

static void sound_menu(void)
{
	enum { S_OUTPUT, S_BUFFER };
	static const char *const outputs[] = {"auto", "hdmi", "headphones", NULL};
	static const char *const output_names[] = {"Automatic", "HDMI", "Headphones", NULL};
	static const char *const buffers[] = {"20", "40", "60", "80", "100", NULL};
	static const char *const buffer_names[] = {"20 ms", "40 ms", "60 ms", "80 ms", "100 ms", NULL};
	static char output[16], buffer[16];
	static UI_tMenuItem items[] = {
		UI_MENU_ACTION_PREFIX(S_OUTPUT, "Output (reboot): ", output),
		UI_MENU_ACTION_PREFIX(S_BUFFER, "Buffer (reboot): ", buffer),
		UI_MENU_LABEL(""),
		UI_MENU_LABEL("A longer buffer is safer, a"),
		UI_MENU_LABEL("shorter one has less delay."),
		UI_MENU_END
	};
	int option = S_OUTPUT;

	for (;;) {
		const char *out = host_option("audio_out", "auto");
		int output_index = strcmp(out, "analog") == 0 ? 2 : value_index(outputs, out);
		int buffer_index = value_index(buffers, host_option("audio_buffer_ms", "40"));
		if (host_option("audio_buffer_ms", NULL) == NULL)
			buffer_index = 1;
		Util_strlcpy(output, output_names[output_index], sizeof(output));
		Util_strlcpy(buffer, buffer_names[buffer_index], sizeof(buffer));

		option = UI_driver->fSelect("Sound", 0, option, items, NULL);
		switch (option) {
		case S_OUTPUT: {
			int i = choose("Sound Output", output_names, output_index);
			if (i >= 0 && i != output_index) {
				save_setting("audio_out", outputs[i]);
				reboot_needed = TRUE;
			}
			break;
		}
		case S_BUFFER: {
			int i = choose("Sound Buffer", buffer_names, buffer_index);
			if (i >= 0 && i != buffer_index) {
				save_setting("audio_buffer_ms", buffers[i]);
				reboot_needed = TRUE;
			}
			break;
		}
		default:
			return;
		}
	}
}

/* ---- Logging -------------------------------------------------------------------- */

/* Where the log goes: changed at once, and kept in bma800xl.cfg. */
static void logging_menu(void)
{
	static const char *const values[] = {"off", "uart", "file"};
	static const char *const names[] = {
		"Off",
		"UART (serial port GPIO14/15)",
		"File (bma800xl.log and serial)",
		NULL
	};
	int current = host_log_destination();
	int i = choose("Logging", names, current);
	if (i < 0 || i == current)
		return;
	if (!host_set_log_destination(i)) {
		UI_driver->fMessage("Cannot open bma800xl.log", 1);
		return;
	}
	save_setting("enable_logging", values[i]);
}

/* ---- Main menu ------------------------------------------------------------------ */

/* Returns TRUE to go on to atari800's menu. */
static int main_menu(void)
{
	enum { M_EMULATOR, M_ABOUT, M_LICENCE, M_NETWORK, M_CONTROLLERS, M_VIDEO, M_SOUND,
	       M_LOGGING, M_REBOOT, M_POWER_OFF, M_BACK };
	static const char *const logging_names[] = {"Off", "UART", "File", NULL};
	static char machine[64], network[64], logging[8];
	static UI_tMenuItem items[] = {
		UI_MENU_LABEL(machine),
		UI_MENU_ACTION(M_ABOUT, "About BMA800XL"),
		UI_MENU_ACTION(M_LICENCE, "Licence"),
		UI_MENU_LABEL(""),
		UI_MENU_LABEL(network),
		UI_MENU_SUBMENU(M_NETWORK, "Network"),
		UI_MENU_LABEL(""),
		UI_MENU_SUBMENU(M_EMULATOR, "Emulator Menu (atari800)"),
		UI_MENU_SUBMENU(M_CONTROLLERS, "Controllers"),
		UI_MENU_SUBMENU(M_VIDEO, "Video"),
		UI_MENU_SUBMENU(M_SOUND, "Sound"),
		UI_MENU_ACTION_PREFIX(M_LOGGING, "Logging: ", logging),
		UI_MENU_LABEL(""),
		UI_MENU_ACTION(M_REBOOT, "Reboot"),
		UI_MENU_ACTION(M_POWER_OFF, "Shut Down"),
		UI_MENU_ACTION_ACCEL(M_BACK, "Back to Atari", "Esc"),
		UI_MENU_END
	};
	static int option = M_EMULATOR;

	for (;;) {
		machine_line(machine, sizeof(machine));
		network_line(network, sizeof(network));
		Util_strlcpy(logging, logging_names[host_log_destination()], sizeof(logging));
		option = UI_driver->fSelect("BMA800XL " BMA800XL_VERSION, 0, option, items, NULL);
		switch (option) {
		case M_EMULATOR:
			return TRUE;
		case M_ABOUT:
			about();
			break;
		case M_LICENCE:
			licence();
			break;
		case M_NETWORK:
			network_menu();
			break;
		case M_CONTROLLERS:
			controllers_menu();
			break;
		case M_VIDEO:
			video_menu();
			break;
		case M_SOUND:
			sound_menu();
			break;
		case M_LOGGING:
			logging_menu();
			break;
		case M_REBOOT:
			power(0);
			return FALSE;
		case M_POWER_OFF:
			power(1);
			return FALSE;
		default:
			option = M_EMULATOR;
			return FALSE;
		}
		/* Alt+letter: one of atari800's menus. */
		if (UI_alt_function >= 0)
			return TRUE;
		/* A request from a submenu (Reboot Now) or the web UI. */
		if (BMA_MenuRequests())
			return FALSE;
	}
}

/* Offers a reboot when settings changed that need one. */
static void offer_reboot(void)
{
	enum { R_NOW, R_LATER };
	static UI_tMenuItem items[] = {
		UI_MENU_LABEL("Some changes take effect only"),
		UI_MENU_LABEL("after a reboot."),
		UI_MENU_LABEL(""),
		UI_MENU_ACTION(R_NOW, "Reboot Now"),
		UI_MENU_ACTION(R_LATER, "Later"),
		UI_MENU_END
	};
	if (!reboot_needed)
		return;
	reboot_needed = FALSE;
	if (UI_driver->fSelect("Reboot?", 0, R_NOW, items, NULL) == R_NOW)
		power(0);
}

int BMA_Menu(void)
{
	int emulator_menu;

	Sound_Pause();
	UI_is_active = TRUE;
	UI_driver->fInit();

	emulator_menu = main_menu() || UI_alt_function >= 0;
	if (!emulator_menu)
		offer_reboot();

	UI_is_active = FALSE;
	/* Don't let the key that closed the menu reach the Atari. */
	while (PLATFORM_Keyboard() != AKEY_NONE)
		Atari800_Sync();
	Sound_Continue();
	return emulator_menu;
}
