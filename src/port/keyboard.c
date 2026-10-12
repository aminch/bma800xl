/*
 * keyboard.c - keyboard, gamepad and joystick input for BMA800XL.
 *
 * Translates USB HID keyboards and gamepads into what atari800 expects:
 * AKEY_* key codes from PLATFORM_Keyboard(), the console keys in
 * INPUT_key_consol, and joystick state from PLATFORM_PORT/PLATFORM_TRIG.
 * The behaviour follows atari800's SDL port so its documentation applies.
 *
 * This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "akey.h"
#include "atari.h"
#include "binload.h"
#include "colours.h"
#include "input.h"
#include "platform.h"
#include "ui.h"
#include "ui_basic.h"

#include "../host.h"
#include "bma800xl.h"

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

/* Keyboard layouts, as in BMC64 (src/keyboard/layout_*.c): the character
   each key types, as a Unicode code point (Latin-1), unshifted, shifted,
   with AltGr (right Alt) and with Shift+AltGr. 0 means none. Letters are
   not listed; they type a-z. Characters the Atari has no key for (å, £...)
   only type in the menus. */
typedef struct {
	unsigned char usage;
	unsigned short normal;
	unsigned short shifted;
	unsigned short altgr;
	unsigned short shifted_altgr;
} layout_key_t;

typedef struct {
	const char *name;
	const layout_key_t *keys;
	int has_altgr;   /* right Alt is AltGr, not a key of its own */
} layout_t;

static const layout_key_t keys_us[] = {
	{K_1, '1', '!', 0, 0}, {0x1f, '2', '@', 0, 0}, {0x20, '3', '#', 0, 0},
	{0x21, '4', '$', 0, 0}, {0x22, '5', '%', 0, 0}, {0x23, '6', '^', 0, 0},
	{0x24, '7', '&', 0, 0}, {0x25, '8', '*', 0, 0}, {K_9, '9', '(', 0, 0},
	{K_0, '0', ')', 0, 0}, {K_MINUS, '-', '_', 0, 0}, {K_EQUAL, '=', '+', 0, 0},
	{K_LEFTBRACKET, '[', '{', 0, 0}, {K_RIGHTBRACKET, ']', '}', 0, 0},
	{K_BACKSLASH, '\\', '|', 0, 0}, {K_SEMICOLON, ';', ':', 0, 0},
	{K_QUOTE, '\'', '"', 0, 0}, {K_GRAVE, '`', '~', 0, 0},
	{K_COMMA, ',', '<', 0, 0}, {K_PERIOD, '.', '>', 0, 0},
	{K_SLASH, '/', '?', 0, 0},
	/* Not in BMC64's table: the extra keys of an ISO keyboard. */
	{K_NONUS_HASH, '\\', '|', 0, 0}, {K_NONUS_BACKSLASH, '\\', '|', 0, 0},
	{0, 0, 0, 0, 0}
};

static const layout_key_t keys_uk[] = {
	{K_1, '1', '!', 0, 0}, {0x1f, '2', '"', 0, 0}, {0x20, '3', 0xa3, 0, 0},
	{0x21, '4', '$', 0, 0}, {0x22, '5', '%', 0, 0}, {0x23, '6', '^', 0, 0},
	{0x24, '7', '&', 0, 0}, {0x25, '8', '*', 0, 0}, {K_9, '9', '(', 0, 0},
	{K_0, '0', ')', 0, 0}, {K_MINUS, '-', '_', 0, 0}, {K_EQUAL, '=', '+', 0, 0},
	{K_LEFTBRACKET, '[', '{', 0, 0}, {K_RIGHTBRACKET, ']', '}', 0, 0},
	{K_BACKSLASH, '#', '~', 0, 0}, {K_NONUS_HASH, '#', '~', 0, 0},
	{K_SEMICOLON, ';', ':', 0, 0}, {K_QUOTE, '\'', '@', 0, 0},
	{K_GRAVE, '`', 0xac, 0, 0}, {K_NONUS_BACKSLASH, '\\', '|', 0, 0},
	{K_COMMA, ',', '<', 0, 0}, {K_PERIOD, '.', '>', 0, 0},
	{K_SLASH, '/', '?', 0, 0},
	{0, 0, 0, 0, 0}
};

/* Norwegian. The dead keys (¨ ^ ` on the keys right of P and of +) type
   their character at once. */
static const layout_key_t keys_no[] = {
	{K_1, '1', '!', 0, 0}, {0x1f, '2', '"', '@', 0},
	{0x20, '3', '#', 0xa3, 0}, {0x21, '4', 0xa4, '$', 0},
	{0x22, '5', '%', 0, 0}, {0x23, '6', '&', 0, 0},
	{0x24, '7', '/', '{', 0}, {0x25, '8', '(', '[', 0},
	{K_9, '9', ')', ']', 0}, {K_0, '0', '=', '}', 0},
	{K_MINUS, '+', '?', '\\', 0}, {K_EQUAL, '\\', '`', 0, 0},
	{K_LEFTBRACKET, 0xe5, 0xc5, 0, 0}, {K_RIGHTBRACKET, 0xa8, '^', 0, 0},
	{K_BACKSLASH, '\'', '*', 0, 0}, {K_NONUS_HASH, '\'', '*', 0, 0},
	{K_SEMICOLON, 0xf8, 0xd8, 0, 0}, {K_QUOTE, 0xe6, 0xc6, 0, 0},
	{K_GRAVE, '|', 0xa7, 0, 0}, {K_COMMA, ',', ';', 0, 0},
	{K_PERIOD, '.', ':', 0, 0}, {K_SLASH, '-', '_', 0, 0},
	{K_NONUS_BACKSLASH, '<', '>', 0, 0},
	{0, 0, 0, 0, 0}
};

static const layout_t layouts[] = {
	{"us", keys_us, FALSE},
	{"uk", keys_uk, FALSE},
	{"gb", keys_uk, FALSE},
	{"no", keys_no, TRUE},
	{NULL, NULL, FALSE}
};

static const layout_t *layout = &layouts[0];

/* Joystick emulation on the keyboard. */
int BMA_keypad_joystick = TRUE;   /* numeric keypad, fire on keypad 0/5 */
int BMA_arrow_joystick = FALSE;   /* arrow keys, fire on right Ctrl/Alt */

/* Set while the on-screen keyboard is showing (declared in ui_basic.h). */
int UI_BASIC_in_kbui = FALSE;

/* Is a menu (or the on-screen keyboard) taking the keys? */
static int in_menu(void)
{
	return UI_is_active || UI_BASIC_in_kbui;
}

static int lastkey = 0;       /* HID usage of the last key event */
static int lastuni = 0;       /* character of the last key press */
static int key_pressed = 0;   /* is lastkey still held? */
static int key_control = 0;

/* Gamepads acting as keys (see pad_virtual_keys). */
static unsigned char virtual_down[256];

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

static void update_pad_ports(void)
{
	int pad, port;
	for (pad = 0; pad < HOST_PADS; pad++) {
		int s = host_pad_state(pad);
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
}

/* Gamepad USB slot driving Atari joystick PORT, or -1. */
int BMA_PortPad(int port)
{
	return port >= 0 && port < HOST_PADS ? port_pad[port] : -1;
}

/* Reads the keyboard and joystick settings; the menu calls it again after
   changing one. */
void BMA_InputSettingsChanged(void)
{
	const char *name = host_option("keyboard", "us");
	const layout_t *l;
	layout = &layouts[0];
	for (l = layouts; l->name != NULL; l++) {
		if (strcmp(name, l->name) == 0)
			layout = l;
	}
	BMA_keypad_joystick = host_option_int("keypad_joystick", TRUE);
	BMA_arrow_joystick = host_option_int("arrow_joystick", FALSE);
	init_pad_ports();
}

void BMA_KeyboardInit(void)
{
	memset(virtual_down, 0, sizeof(virtual_down));
	BMA_InputSettingsChanged();
}

static int key_is_down(int usage)
{
	return host_key_down(usage) || virtual_down[usage];
}

static int shift_down(void)
{
	return key_is_down(K_LSHIFT) || key_is_down(K_RSHIFT);
}

/* Is right Alt held as AltGr? */
static int altgr_down(void)
{
	return layout->has_altgr && key_is_down(K_RALT);
}

/* The character (Unicode) key USAGE types, or 0. */
static int usage_to_char(int usage, int shift, int altgr)
{
	const layout_key_t *k;
	for (k = layout->keys; k->usage != 0; k++) {
		if (k->usage == usage) {
			if (altgr)
				return shift ? k->shifted_altgr : k->altgr;
			return shift ? k->shifted : k->normal;
		}
	}
	if (altgr)
		return 0;
	if (usage >= K_A && usage <= K_Z)
		return (shift ? 'A' : 'a') + usage - K_A;
	if (usage == K_SPACE)
		return ' ';
	return 0;
}

/* The key left of 1 is the Atari key, unless the layout puts a character
   there that the Atari has (| on a Norwegian keyboard). */
static int grave_is_atari_key(void)
{
	return BMA_CharToAkey(usage_to_char(K_GRAVE, FALSE, FALSE), 0) == AKEY_NONE;
}

/* Keys used by keyboard joysticks are not passed to the emulation; some
   games pause on any key press. */
static int is_joystick_key(int usage)
{
	if (BMA_keypad_joystick) {
		switch (usage) {
		case K_KP_1: case K_KP_2: case K_KP_3: case K_KP_4: case K_KP_5:
		case K_KP_6: case K_KP_7: case K_KP_8: case K_KP_9: case K_KP_0:
			return TRUE;
		}
	}
	if (BMA_arrow_joystick) {
		switch (usage) {
		case K_UP: case K_DOWN: case K_LEFT: case K_RIGHT: case K_RCTRL:
			return TRUE;
		case K_RALT:
			return !layout->has_altgr;
		}
	}
	return FALSE;
}

static int is_modifier_or_console(int usage)
{
	return (usage >= K_LCTRL && usage <= K_RGUI) || usage == KBD_OPTION
	    || usage == KBD_SELECT || usage == KBD_START;
}

/* Gamepads produce key presses for the parts of the Atari that are not on a
   joystick, and drive the menus while the UI is showing. */
static void pad_virtual_keys(unsigned char *down)
{
	int pad;
	int all = 0;
	for (pad = 0; pad < HOST_PADS; pad++) {
		int s = host_pad_state(pad);
		if (s > 0)
			all |= s;
	}

	pad_consol = INPUT_CONSOL_NONE;
	memset(down, 0, 256);

	/* Home, or Select and Start together, open the menu (or leave it). The
	   press keeps the meaning it had when it started: otherwise the Home
	   that opened the menu would turn into Escape inside it while held. */
	if ((all & HOST_PAD_HOME)
	    || (all & (HOST_PAD_SELECT | HOST_PAD_START)) == (HOST_PAD_SELECT | HOST_PAD_START)) {
		if (menu_combo_key == 0)
			menu_combo_key = in_menu() ? K_ESCAPE : KBD_UI;
		down[menu_combo_key] = 1;
		return;
	}
	menu_combo_key = 0;

	/* The joystick test shows the pad: only B and Start leave it. */
	if (BMA_joystick_test) {
		if (all & (HOST_PAD_B | HOST_PAD_START)) down[K_ESCAPE] = 1;
		return;
	}

	if (in_menu()) {
		if (all & HOST_PAD_UP) down[K_UP] = 1;
		if (all & HOST_PAD_DOWN) down[K_DOWN] = 1;
		if (all & HOST_PAD_LEFT) down[K_LEFT] = 1;
		if (all & HOST_PAD_RIGHT) down[K_RIGHT] = 1;
		if (all & HOST_PAD_A) down[K_RETURN] = 1;
		if (all & (HOST_PAD_B | HOST_PAD_START)) down[K_ESCAPE] = 1;
		return;
	}

	if (all & HOST_PAD_B) down[K_SPACE] = 1;
	if (all & HOST_PAD_Y) down[K_RETURN] = 1;
	if (all & HOST_PAD_X) pad_consol &= ~INPUT_CONSOL_OPTION;
	if (all & HOST_PAD_SELECT) pad_consol &= ~INPUT_CONSOL_SELECT;
	if (all & HOST_PAD_START) pad_consol &= ~INPUT_CONSOL_START;
}

/* Next key event, real or from a gamepad. Returns 0 when there is none. */
static int next_key_event(int *usage, int *pressed)
{
	unsigned char down[256];
	int i;

	if (host_key_event(usage, pressed))
		return 1;

	pad_virtual_keys(down);
	for (i = 0; i < 256; i++) {
		if (down[i] != virtual_down[i]) {
			virtual_down[i] = down[i];
			*usage = i;
			*pressed = down[i];
			return 1;
		}
	}
	return 0;
}

static int read_keyboard(void);
static int joystick_signature(void);

/* Set while the BMA800XL menu's joystick test is showing. */
int BMA_joystick_test = FALSE;

int PLATFORM_Keyboard(void)
{
	static int last_signature = -1;
	int code = read_keyboard();

	/* In the joystick test, a change on any joystick sends Tab, which makes
	   atari800's menu return so the test can show the new state. */
	if (BMA_joystick_test && code == AKEY_NONE) {
		int signature = joystick_signature();
		if (signature != last_signature) {
			last_signature = signature;
			return AKEY_TAB;
		}
	} else if (!BMA_joystick_test) {
		last_signature = -1;
	}
	return code;
}

static int read_keyboard(void)
{
	int shiftctrl = 0;
	int usage, pressed;
	int event_found = FALSE;

	update_pad_ports();

	/* Something from the web UI is waiting: back out of the menus. */
	if (in_menu() && BMA_MenuRequests())
		return AKEY_ESCAPE;

	/* Caps Lock gives a single key press. */
	if (lastkey == K_CAPSLOCK && key_pressed) {
		key_pressed = 0;
		lastkey = 0;
	}

	while (!event_found && next_key_event(&usage, &pressed)) {
		if (is_modifier_or_console(usage))
			continue;
		/* The power key: Shut Down. The Pi 400's keyboard sends it for
		   Fn+F10 while the Pi is running (Raspberry Pi, "Have you tried
		   turning it off and on again?", raspberrypi.com news, 2021). */
		if (usage == K_POWER) {
			if (pressed)
				bma_request_power(1);
			continue;
		}
		event_found = TRUE;
		lastkey = usage;
		key_pressed = pressed;
		lastuni = pressed ? usage_to_char(usage, shift_down(), altgr_down()) : 0;
	}

	/* SHIFT and CONTROL */
	INPUT_key_shift = shift_down();
	if (INPUT_key_shift)
		shiftctrl ^= AKEY_SHFT;
	key_control = key_is_down(K_LCTRL) || (key_is_down(K_RCTRL) && !BMA_arrow_joystick);
	if (key_control)
		shiftctrl ^= AKEY_CTRL;

	BINLOAD_pause_loading = FALSE;

	/* OPTION / SELECT / START */
	INPUT_key_consol = pad_consol;
	if (key_is_down(KBD_OPTION))
		INPUT_key_consol &= ~INPUT_CONSOL_OPTION;
	if (key_is_down(KBD_SELECT))
		INPUT_key_consol &= ~INPUT_CONSOL_SELECT;
	if (key_is_down(KBD_START) || (pad_consol & INPUT_CONSOL_START) == 0) {
		INPUT_key_consol &= ~INPUT_CONSOL_START;
		/* The 5200's START is a key on its keypad. */
		if (Atari800_machine_type == Atari800_MACHINE_5200 && !in_menu())
			return AKEY_5200_START;
	}

	if (!key_pressed)
		return AKEY_NONE;

	UI_alt_function = -1;
	if (key_is_down(K_LALT)) {
		switch (lastkey) {
		case K_LETTER('r'): UI_alt_function = UI_MENU_RUN; break;
		case K_LETTER('y'): UI_alt_function = UI_MENU_SYSTEM; break;
		case K_LETTER('o'): UI_alt_function = UI_MENU_SOUND; break;
		case K_LETTER('a'): UI_alt_function = UI_MENU_ABOUT; break;
		case K_LETTER('s'): UI_alt_function = UI_MENU_SAVESTATE; break;
		case K_LETTER('d'): UI_alt_function = UI_MENU_DISK; break;
		case K_LETTER('l'): UI_alt_function = UI_MENU_LOADSTATE; break;
		case K_LETTER('c'): UI_alt_function = UI_MENU_CARTRIDGE; break;
		case K_LETTER('t'): UI_alt_function = UI_MENU_CASSETTE; break;
		case K_LETTER('j'): UI_alt_function = UI_MENU_CONTROLLER; break;
		default: break;
		}
	}

	switch (lastkey) {
	case KBD_RESET:
		key_pressed = 0;
		return INPUT_key_shift ? AKEY_COLDSTART : AKEY_WARMSTART;
	case KBD_UI:
		key_pressed = 0;
		return AKEY_UI;
	case KBD_HELP:
		return AKEY_HELP ^ shiftctrl;
	case KBD_BREAK:
	case K_PAUSE:
		if (BINLOAD_wait_active) {
			BINLOAD_pause_loading = TRUE;
			return AKEY_NONE;
		}
		return AKEY_BREAK;
	case KBD_OSK:
		key_pressed = 0;
		return AKEY_KEYB;
	case KBD_TURBO:
		key_pressed = 0;
		return AKEY_TURBO;
	default:
		break;
	}

	if (UI_alt_function != -1) {
		key_pressed = 0;
		return AKEY_UI;
	}

	if (!in_menu() && is_joystick_key(lastkey)) {
		key_pressed = 0;
		return AKEY_NONE;
	}

	if (Atari800_machine_type == Atari800_MACHINE_5200 && !in_menu()) {
		switch (lastuni) {
		case 'p': return AKEY_5200_PAUSE;
		case 'r': return AKEY_5200_RESET;
		case '0': return AKEY_5200_0;
		case '1': return AKEY_5200_1;
		case '2': return AKEY_5200_2;
		case '3': return AKEY_5200_3;
		case '4': return AKEY_5200_4;
		case '5': return AKEY_5200_5;
		case '6': return AKEY_5200_6;
		case '7': return AKEY_5200_7;
		case '8': return AKEY_5200_8;
		case '9': return AKEY_5200_9;
		case '#': case '=': return AKEY_5200_HASH;
		case '*': return AKEY_5200_ASTERISK;
		}
		return AKEY_NONE;
	}

	switch (lastkey) {
	case K_GRAVE:
		if (!grave_is_atari_key())
			break;
		/* fall through */
	case K_LGUI:
		return AKEY_ATARI ^ shiftctrl;
	case K_END:
	case K_PAGEDOWN:
		return AKEY_F2 | AKEY_SHFT;
	case K_PAGEUP:
		return AKEY_F1 | AKEY_SHFT;
	case K_HOME:
		return key_control ? AKEY_LESS | shiftctrl : AKEY_CLEAR;
	case K_CAPSLOCK:
		return AKEY_CAPSTOGGLE | shiftctrl;
	case K_SPACE:
		return AKEY_SPACE ^ shiftctrl;
	case K_BACKSPACE:
		return AKEY_BACKSPACE | shiftctrl;
	case K_RETURN:
	case K_KP_ENTER:
		return AKEY_RETURN ^ shiftctrl;
	case K_LEFT:
		return (!in_menu() && Atari800_f_keys ? AKEY_F3 : (INPUT_key_shift ? AKEY_PLUS : AKEY_LEFT)) ^ shiftctrl;
	case K_RIGHT:
		return (!in_menu() && Atari800_f_keys ? AKEY_F4 : (INPUT_key_shift ? AKEY_ASTERISK : AKEY_RIGHT)) ^ shiftctrl;
	case K_UP:
		return (!in_menu() && Atari800_f_keys ? AKEY_F1 : (INPUT_key_shift ? AKEY_MINUS : AKEY_UP)) ^ shiftctrl;
	case K_DOWN:
		return (!in_menu() && Atari800_f_keys ? AKEY_F2 : (INPUT_key_shift ? AKEY_EQUAL : AKEY_DOWN)) ^ shiftctrl;
	case K_ESCAPE:
		return AKEY_ESCAPE ^ shiftctrl;
	case K_TAB:
		return AKEY_TAB ^ shiftctrl;
	case K_DELETE:
		if (INPUT_key_shift)
			return AKEY_DELETE_LINE | shiftctrl;
		return AKEY_DELETE_CHAR;
	case K_INSERT:
		if (INPUT_key_shift)
			return AKEY_INSERT_LINE | shiftctrl;
		return AKEY_INSERT_CHAR;
	}

	if (INPUT_cx85) {
		switch (lastkey) {
		case K_KP_1: return AKEY_CX85_1;
		case K_KP_2: return AKEY_CX85_2;
		case K_KP_3: return AKEY_CX85_3;
		case K_KP_4: return AKEY_CX85_4;
		case K_KP_5: return AKEY_CX85_5;
		case K_KP_6: return AKEY_CX85_6;
		case K_KP_7: return AKEY_CX85_7;
		case K_KP_8: return AKEY_CX85_8;
		case K_KP_9: return AKEY_CX85_9;
		case K_KP_0: return AKEY_CX85_0;
		case K_KP_PERIOD: return AKEY_CX85_PERIOD;
		case K_KP_MINUS: return AKEY_CX85_MINUS;
		case K_KP_ENTER: return AKEY_CX85_PLUS_ENTER;
		case K_KP_DIVIDE: return key_control ? AKEY_CX85_ESCAPE : AKEY_CX85_NO;
		case K_KP_MULTIPLY: return AKEY_CX85_DELETE;
		case K_KP_PLUS: return AKEY_CX85_YES;
		}
	}

	/* CTRL-0 to CTRL-9 and other control characters */
	if (key_control) {
		switch (lastkey) {
		case K_PERIOD: return AKEY_FULLSTOP | shiftctrl;
		case K_COMMA: return AKEY_COMMA | shiftctrl;
		case K_SEMICOLON: return AKEY_SEMICOLON | shiftctrl;
		case K_SLASH: return AKEY_SLASH | shiftctrl;
		case K_BACKSLASH: return AKEY_ESCAPE | shiftctrl;
		case K_0: return AKEY_CTRL_0 | shiftctrl;
		case K_1: return AKEY_CTRL_1 | shiftctrl;
		case 0x1f: return AKEY_CTRL_2 | shiftctrl;
		case 0x20: return AKEY_CTRL_3 | shiftctrl;
		case 0x21: return AKEY_CTRL_4 | shiftctrl;
		case 0x22: return AKEY_CTRL_5 | shiftctrl;
		case 0x23: return AKEY_CTRL_6 | shiftctrl;
		case 0x24: return AKEY_CTRL_7 | shiftctrl;
		case 0x25: return AKEY_CTRL_8 | shiftctrl;
		case K_9: return AKEY_CTRL_9 | shiftctrl;
		}
	}

	/* Characters the Atari has no key for (å) can still be typed in the
	   menus. */
	if (in_menu() && lastuni >= 0xa0 && lastuni <= 0xff)
		return UI_BASIC_KEY_CHAR(lastuni);

	return BMA_CharToAkey(lastuni, shiftctrl);
}

/* Atari key code for character C typed with SHIFTCTRL (AKEY_SHFT and/or
   AKEY_CTRL) held. */
int BMA_CharToAkey(int c, int shiftctrl)
{
	/* Letters keep their case from the Shift key, not Caps Lock. */
	if (c >= 'A' && c <= 'Z')
		c += 'a' - 'A';
	if (c >= 'a' && c <= 'z') {
		static const int letters[26] = {
			AKEY_a, AKEY_b, AKEY_c, AKEY_d, AKEY_e, AKEY_f, AKEY_g, AKEY_h,
			AKEY_i, AKEY_j, AKEY_k, AKEY_l, AKEY_m, AKEY_n, AKEY_o, AKEY_p,
			AKEY_q, AKEY_r, AKEY_s, AKEY_t, AKEY_u, AKEY_v, AKEY_w, AKEY_x,
			AKEY_y, AKEY_z
		};
		return letters[c - 'a'] | shiftctrl;
	}
	switch (c) {
	case ':': return AKEY_COLON;
	case '!': return AKEY_EXCLAMATION;
	case '@': return AKEY_AT;
	case '#': return AKEY_HASH;
	case '$': return AKEY_DOLLAR;
	case '%': return AKEY_PERCENT;
	case '^': return AKEY_CARET;
	case '&': return AKEY_AMPERSAND;
	case '*': return AKEY_ASTERISK;
	case '(': return AKEY_PARENLEFT;
	case ')': return AKEY_PARENRIGHT;
	case '+': return AKEY_PLUS;
	case '_': return AKEY_UNDERSCORE;
	case '"': return AKEY_DBLQUOTE;
	case '?': return AKEY_QUESTION;
	case '<': return AKEY_LESS;
	case '>': return AKEY_GREATER;
	case ';': return AKEY_SEMICOLON;
	case '0': return AKEY_0;
	case '1': return AKEY_1;
	case '2': return AKEY_2;
	case '3': return AKEY_3;
	case '4': return AKEY_4;
	case '5': return AKEY_5;
	case '6': return AKEY_6;
	case '7': return AKEY_7;
	case '8': return AKEY_8;
	case '9': return AKEY_9;
	case ',': return AKEY_COMMA;
	case '.': return AKEY_FULLSTOP;
	case '=': return AKEY_EQUAL;
	case '-': return AKEY_MINUS;
	case '\'': return AKEY_QUOTE;
	case '/': return AKEY_SLASH;
	case '\\': return AKEY_BACKSLASH;
	case '[': return AKEY_BRACKETLEFT;
	case ']': return AKEY_BRACKETRIGHT;
	case '|': return AKEY_BAR;
	}
	return AKEY_NONE;
}

/* ---- Joysticks ----------------------------------------------------------- */

/* HOST_JOY_* bits for Atari joystick port PORT (0..3). */
static int joystick_state(int port)
{
	int state = 0;
	int pad = port_pad[port] >= 0 ? host_pad_state(port_pad[port]) : -1;

	if (pad > 0) {
		if (pad & HOST_PAD_UP) state |= HOST_JOY_UP;
		if (pad & HOST_PAD_DOWN) state |= HOST_JOY_DOWN;
		if (pad & HOST_PAD_LEFT) state |= HOST_JOY_LEFT;
		if (pad & HOST_PAD_RIGHT) state |= HOST_JOY_RIGHT;
		if (pad & HOST_PAD_A) state |= HOST_JOY_FIRE;
	}

	if (port < 2)
		state |= host_gpio_joystick(port);

	if (port == 0 && BMA_keypad_joystick) {
		if (host_key_down(K_KP_8) || host_key_down(K_KP_7) || host_key_down(K_KP_9))
			state |= HOST_JOY_UP;
		if (host_key_down(K_KP_2) || host_key_down(K_KP_1) || host_key_down(K_KP_3))
			state |= HOST_JOY_DOWN;
		if (host_key_down(K_KP_4) || host_key_down(K_KP_7) || host_key_down(K_KP_1))
			state |= HOST_JOY_LEFT;
		if (host_key_down(K_KP_6) || host_key_down(K_KP_9) || host_key_down(K_KP_3))
			state |= HOST_JOY_RIGHT;
		if (host_key_down(K_KP_0) || host_key_down(K_KP_5))
			state |= HOST_JOY_FIRE;
	}

	if (port == 0 && BMA_arrow_joystick) {
		if (host_key_down(K_UP)) state |= HOST_JOY_UP;
		if (host_key_down(K_DOWN)) state |= HOST_JOY_DOWN;
		if (host_key_down(K_LEFT)) state |= HOST_JOY_LEFT;
		if (host_key_down(K_RIGHT)) state |= HOST_JOY_RIGHT;
		/* Right Alt is AltGr on some layouts. */
		if (host_key_down(K_RCTRL) || (host_key_down(K_RALT) && !layout->has_altgr))
			state |= HOST_JOY_FIRE;
	}

	return state;
}

/* HOST_JOY_* bits the Atari sees on joystick PORT (0..3). */
int BMA_JoystickState(int port)
{
	return port >= 0 && port < 4 ? joystick_state(port) : 0;
}

/* All four joysticks' state in one number, to notice changes. */
static int joystick_signature(void)
{
	int port, signature = 0;
	for (port = 0; port < 4; port++)
		signature |= joystick_state(port) << (5 * port);
	return signature;
}

static int stick_bits(int state)
{
	int stick = INPUT_STICK_CENTRE;
	if (state & HOST_JOY_UP) stick &= ~0x01;
	if (state & HOST_JOY_DOWN) stick &= ~0x02;
	if (state & HOST_JOY_LEFT) stick &= ~0x04;
	if (state & HOST_JOY_RIGHT) stick &= ~0x08;
	return stick;
}

int PLATFORM_PORT(int num)
{
	if (num < 0 || num > 1)
		return 0xff;
	return stick_bits(joystick_state(2 * num))
	     | (stick_bits(joystick_state(2 * num + 1)) << 4);
}

int PLATFORM_TRIG(int num)
{
	if (num < 0 || num > 3)
		return 1;
	return (joystick_state(num) & HOST_JOY_FIRE) ? 0 : 1;
}
