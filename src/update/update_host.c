//
// update_host.c
//
// BMC64 - A bare metal Commodore emulator for Raspberry Pi
// Copyright (C) 2026 aminch <https://github.com/aminch>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// Modified for BMA800XL from BMC64's src/update/update_host.c: rewritten for
// BMA800XL. The view is drawn on atari800's screen (Screen_atari) with the
// Atari's character set and the colours of atari800's menus (as atari800's
// ui_basic.c draws them), and shown with PLATFORM_DisplayScreen; keys,
// gamepads and GPIO joysticks come from src/host.h.
//
// The updater's only connection to the rest of BMA800XL (see update_host.h).
//

#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "atari.h"
#include "memory.h"
#include "platform.h"
#include "screen.h"

#include "../host.h"
#include "../usb_hid_keys.h"
#include "update_host.h"

#ifndef BMA800XL_VERSION
#define BMA800XL_VERSION "dev"
#endif

// Atari palette colours of atari800's menus (ui_basic.c): light blue text
// on blue. The highlight bar is the text colour, and text drawn on it takes
// the background colour, so the selected row is shown in inverse as in the
// menus.
#define COL_BG 0x94
#define COL_FG 0x9a
#define COL_DIM 0x96
#define COL_NOTE 0x0e

// The 40x25 character area, centred in atari800's 336x240 view.
#define VIEW_X 24
#define VIEW_W 336
#define AREA_LEFT 32
#define AREA_TOP 20

static UBYTE charset[1024];
static int have_charset;

const char *uh_version(void) { return BMA800XL_VERSION; }

const char *uh_board_kernel(void) {
  // The kernel each board's config.txt section boots (see make_all.sh).
#if RASPPI == 1
  return "kernel.img";
#elif RASPPI == 2
  return "kernel7.img";
#elif RASPPI == 3
  return "kernel8-32.img";
#elif RASPPI == 4
  return "kernel8-rpi4.img";
#else
  return "kernel_2712.img";
#endif
}

// ---- input ----

// Gamepad and joystick directions and buttons, as one set of bits.
enum {
  JOY_UP = 0x01,
  JOY_DOWN = 0x02,
  JOY_LEFT = 0x04,
  JOY_RIGHT = 0x08,
  JOY_SELECT = 0x10,
  JOY_BACK = 0x20,
};

static unsigned joy_last;

static unsigned joy_state(void) {
  unsigned bits = 0;
  for (int pad = 0; pad < HOST_PADS; pad++) {
    int s = host_pad_state(pad);
    if (s < 0) {
      continue;
    }
    if (s & HOST_PAD_UP) bits |= JOY_UP;
    if (s & HOST_PAD_DOWN) bits |= JOY_DOWN;
    if (s & HOST_PAD_LEFT) bits |= JOY_LEFT;
    if (s & HOST_PAD_RIGHT) bits |= JOY_RIGHT;
    if (s & HOST_PAD_A) bits |= JOY_SELECT;
    if (s & HOST_PAD_B) bits |= JOY_BACK;
  }
  for (int port = 0; port < 2; port++) {
    int s = host_gpio_joystick(port);
    if (s & HOST_JOY_UP) bits |= JOY_UP;
    if (s & HOST_JOY_DOWN) bits |= JOY_DOWN;
    if (s & HOST_JOY_LEFT) bits |= JOY_LEFT;
    if (s & HOST_JOY_RIGHT) bits |= JOY_RIGHT;
    if (s & HOST_JOY_FIRE) bits |= JOY_SELECT;
  }
  return bits;
}

void uh_take_screen(int active) {
  if (!active) {
    // platform.c sets the emulator's own view again.
    return;
  }
  if (!have_charset) {
    MEMORY_GetCharset(charset);
    have_charset = 1;
  }
  // atari800's normal view, which shows all 25 rows (the playfield view of
  // border=none has only 24). Its pixel aspect ratio fills a 4:3 screen, as
  // platform.c's.
  host_video_set_view(VIEW_X, 0, VIEW_W, Screen_HEIGHT,
                      4.0 / 3.0 * host_option_double("aspect", 1.0));
  // Keys pressed while booting, and buttons already held, don't count.
  int usage, pressed;
  while (host_key_event(&usage, &pressed)) {
  }
  joy_last = joy_state();
}

int uh_read_key(int *key, int *pressed) {
  int usage;
  while (host_key_event(&usage, pressed)) {
    switch (usage) {
    case K_UP:
      *key = UH_KEY_UP;
      return 1;
    case K_DOWN:
      *key = UH_KEY_DOWN;
      return 1;
    case K_LEFT:
      *key = UH_KEY_LEFT;
      return 1;
    case K_RIGHT:
      *key = UH_KEY_RIGHT;
      return 1;
    case K_RETURN:
    case K_KP_ENTER:
    case K_SPACE:
      *key = UH_KEY_SELECT;
      return 1;
    case K_ESCAPE:
    case K_F12:
      *key = UH_KEY_BACK;
      return 1;
    case K_PAGEUP:
      *key = UH_KEY_PAGE_UP;
      return 1;
    case K_PAGEDOWN:
      *key = UH_KEY_PAGE_DOWN;
      return 1;
    default:
      break; // Ignore everything else.
    }
  }

  // One change of a gamepad or joystick at a time.
  static const struct {
    unsigned bit;
    int key;
  } joy_keys[] = {
      {JOY_UP, UH_KEY_UP},         {JOY_DOWN, UH_KEY_DOWN},
      {JOY_LEFT, UH_KEY_LEFT},     {JOY_RIGHT, UH_KEY_RIGHT},
      {JOY_SELECT, UH_KEY_SELECT}, {JOY_BACK, UH_KEY_BACK},
  };
  unsigned now = joy_state();
  unsigned changed = now ^ joy_last;
  for (unsigned i = 0; i < sizeof(joy_keys) / sizeof(joy_keys[0]); i++) {
    if (changed & joy_keys[i].bit) {
      joy_last ^= joy_keys[i].bit;
      *key = joy_keys[i].key;
      *pressed = (now & joy_keys[i].bit) != 0;
      return 1;
    }
  }
  return 0;
}

// ---- drawing ----

static UBYTE *pixel(int x, int y) {
  if (x < 0 || y < 0 || x >= Screen_WIDTH || y >= Screen_HEIGHT) {
    return NULL;
  }
  return (UBYTE *)Screen_atari + y * Screen_WIDTH + x;
}

int uh_begin_frame(int *left, int *top) {
  if (Screen_atari == NULL || !have_charset) {
    return 0;
  }
  memset(Screen_atari, 0x00, Screen_WIDTH * Screen_HEIGHT);
  *left = AREA_LEFT;
  *top = AREA_TOP;
  return 1;
}

void uh_draw_text(const char *text, int x, int y, int color) {
  for (; *text != '\0'; text++, x += 8) {
    unsigned ch = (unsigned char)*text;
    const UBYTE *glyph = charset + (ch < 0x80 ? ch : '?') * 8;
    for (int row = 0; row < 8; row++) {
      UBYTE bits = glyph[row];
      for (int col = 0; col < 8; col++, bits <<= 1) {
        UBYTE *p = pixel(x + col, y + row);
        if (p != NULL && (bits & 0x80)) {
          *p = *p == COL_FG ? COL_BG : (UBYTE)color;
        }
      }
    }
  }
}

void uh_draw_rect(int x, int y, int w, int h, int color, int fill) {
  for (int row = y; row < y + h; row++) {
    for (int col = x; col < x + w; col++) {
      if (fill || row == y || row == y + h - 1 || col == x || col == x + w - 1) {
        UBYTE *p = pixel(col, row);
        if (p != NULL) {
          *p = (UBYTE)color;
        }
      }
    }
  }
}

void uh_end_frame(void) { PLATFORM_DisplayScreen(); }

void uh_colors(struct uh_colors *c) {
  c->bg = COL_BG;
  c->fg = COL_FG;
  c->hilite = COL_FG;
  c->border = COL_FG;
  c->dim = COL_DIM;
  c->note = COL_NOTE;
}

// ---- system ----

void uh_poll(void) {
  // Lets Circle's tasks (USB, the network) run between frames.
  host_sleep_us(10000);
}

void uh_reboot(void) { host_reboot(); }

void uh_log(const char *fmt, ...) {
  char line[256];
  int n = snprintf(line, sizeof(line), "update: ");
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line + n, sizeof(line) - n, fmt, ap);
  va_end(ap);
  host_log(line);
}
