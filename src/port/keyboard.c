/*
 * keyboard.c - keyboard translation, keyboard joysticks and USB mouse input.
 *
 * Translates USB HID keyboards into what atari800 expects:
 * AKEY_* key codes from PLATFORM_Keyboard(), the console keys in
 * INPUT_key_consol, joystick state from PLATFORM_PORT/PLATFORM_TRIG, and
 * USB mice into INPUT_mouse_* for the device chosen in atari800's
 * Controller Configuration menu.
 * The behaviour follows atari800's SDL port so its documentation applies.
 * Gamepad input is supplied by gamepad.c through the same key path.
 *
 * This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "config.h"

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
#include "../usb_hid_keys.h"
#include "bma800xl.h"

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

/* Gamepads acting as keys (see BMA_GamepadKeys). */
static unsigned char virtual_down[256];

void BMA_KeyboardClear(void)
{
	lastkey = key_pressed = 0;
}

void BMA_KeyboardConsumeEscape(void)
{
	virtual_down[K_ESCAPE] = 1;
	BMA_KeyboardClear();
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
	BMA_GamepadSettingsChanged();
}

void BMA_KeyboardInit(void)
{
	memset(virtual_down, 0, sizeof(virtual_down));
	BMA_GamepadInit();
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

/* Next key event, real or from a gamepad. Returns 0 when there is none. */
static int next_key_event(int *usage, int *pressed)
{
	unsigned char down[256];
	int i;

	BMA_GamepadKeys(down);
	if (host_key_event(usage, pressed))
		return 1;
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
	int action;

	/* Menu requests from the keyboard take priority over gamepad actions. */
	if (code == AKEY_NONE)
		UI_alt_function = -1;
	action = BMA_GamepadAction(code >= 0 || code == AKEY_NONE || code == AKEY_BREAK);
	if (action != AKEY_NONE)
		code = action;

	if (code == AKEY_NONE && BMA_GamepadMenuChanged())
		return UI_BASIC_KEY_REFRESH;

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

int BMA_CapturePadKey(void)
{
	int usage, pressed, result = -1, pad;
	int previous[HOST_PADS];
	for (pad = 0; pad < HOST_PADS; pad++)
		previous[pad] = host_pad_state(pad);
	UI_driver->fMessage("Press a keyboard key (Esc cancels)", 0);
	for (;;) {
		if (BMA_MenuRequests()) {
			result = -1;
			break;
		}
		for (pad = 0; pad < HOST_PADS; pad++) {
			int state = host_pad_state(pad);
			int before = previous[pad] > 0 ? previous[pad] : 0;
			if (state > 0 && (state & ~before & (HOST_PAD_B | HOST_PAD_START | HOST_PAD_HOME))) {
				result = -1;
				virtual_down[K_ESCAPE] = 1;
				goto done;
			}
			previous[pad] = state;
		}
		if (host_key_event(&usage, &pressed) && pressed) {
			if (usage == K_POWER)
				bma_request_power(1);
			else if (usage == K_ESCAPE && result < 0)
				break;
			else if (result < 0 && BMA_PadBindingValid(BMA_PAD_KEY_BASE + usage))
				result = BMA_PAD_KEY_BASE + usage;
		}
		if (result >= BMA_PAD_KEY_BASE && !host_key_down(result - BMA_PAD_KEY_BASE))
			break;
		host_sleep_us(1000);
	}
done:
	lastkey = 0;
	key_pressed = 0;
	return result;
}

static int read_keyboard(void)
{
	int shiftctrl = 0;
	int usage, pressed;
	int event_found = FALSE;

	BMA_GamepadUpdate();

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
	INPUT_key_consol = BMA_GamepadConsole();
	if (key_is_down(KBD_OPTION))
		INPUT_key_consol &= ~INPUT_CONSOL_OPTION;
	if (key_is_down(KBD_SELECT))
		INPUT_key_consol &= ~INPUT_CONSOL_SELECT;
	if (key_is_down(KBD_START) || (BMA_GamepadConsole() & INPUT_CONSOL_START) == 0) {
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
	int state = BMA_GamepadJoystick(port);

	if (port < 2)
		state |= host_gpio_joystick(port);

	if (port == 0 && BMA_keypad_joystick) {
		if (key_is_down(K_KP_8) || key_is_down(K_KP_7) || key_is_down(K_KP_9))
			state |= HOST_JOY_UP;
		if (key_is_down(K_KP_2) || key_is_down(K_KP_1) || key_is_down(K_KP_3))
			state |= HOST_JOY_DOWN;
		if (key_is_down(K_KP_4) || key_is_down(K_KP_7) || key_is_down(K_KP_1))
			state |= HOST_JOY_LEFT;
		if (key_is_down(K_KP_6) || key_is_down(K_KP_9) || key_is_down(K_KP_3))
			state |= HOST_JOY_RIGHT;
		if (key_is_down(K_KP_0) || key_is_down(K_KP_5))
			state |= HOST_JOY_FIRE;
	}

	if (port == 0 && BMA_arrow_joystick) {
		if (key_is_down(K_UP)) state |= HOST_JOY_UP;
		if (key_is_down(K_DOWN)) state |= HOST_JOY_DOWN;
		if (key_is_down(K_LEFT)) state |= HOST_JOY_LEFT;
		if (key_is_down(K_RIGHT)) state |= HOST_JOY_RIGHT;
		/* Right Alt is AltGr on some layouts. */
		if (key_is_down(K_RCTRL) || (key_is_down(K_RALT) && !layout->has_altgr))
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

/* ---- Mouse --------------------------------------------------------------- */

/* Once a frame, before Atari800_Frame(): the mouse's movement and buttons
   for INPUT_Frame(), as SDL_INPUT_Mouse() in atari800's sdl/input.c. */
void BMA_ReadMouse(void)
{
	static double last_read;
	double now = host_time();
	unsigned buttons = host_mouse(&INPUT_mouse_delta_x, &INPUT_mouse_delta_y);

	/* Movement made while no frames ran (in a menu) is dropped. */
	if (now - last_read > 0.1) {
		INPUT_mouse_delta_x = 0;
		INPUT_mouse_delta_y = 0;
	}
	last_read = now;

	INPUT_mouse_buttons =
		((buttons & HOST_MOUSE_LEFT) ? 1 : 0) |
		((buttons & HOST_MOUSE_RIGHT) ? 2 : 0) |
		((buttons & HOST_MOUSE_MIDDLE) ? 4 : 0);

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
