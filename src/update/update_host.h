//
// update_host.h
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
// Modified for BMA800XL from BMC64's src/update/update_host.h: BMA800XL's
// kernels.
//
// Everything the updater needs from BMA800XL, in one place. Implemented only
// in update_host.c, so the rest of src/update/ never includes BMA800XL
// headers and can be built and tested on a PC (tools/update/test/).
//

#ifndef BMA800XL_UPDATE_HOST_H
#define BMA800XL_UPDATE_HOST_H

#include <stdarg.h>

// Running BMA800XL version, e.g. "1.4.5".
const char *uh_version(void);

// Kernel file name of the board BMA800XL is running on: "kernel.img",
// "kernel7.img", "kernel8-32.img", "kernel8-rpi4.img" or "kernel_2712.img".
const char *uh_board_kernel(void);

// Take (1) or give back (0) the screen and input.
void uh_take_screen(int active);

// Drawing. uh_begin_frame() clears the screen and returns the top left of a
// 40x25 character area (8x8 pixel characters); 0 if nothing can be drawn.
int uh_begin_frame(int *left, int *top);
void uh_draw_text(const char *text, int x, int y, int color);
void uh_draw_rect(int x, int y, int w, int h, int color, int fill);
void uh_end_frame(void);

struct uh_colors {
  int bg, fg, hilite, border, dim, note;
};
void uh_colors(struct uh_colors *c);

// Input. Returns 1 and a key code (UH_KEY_*) if a key was pressed or
// released since the last call.
enum {
  UH_KEY_NONE,
  UH_KEY_UP,
  UH_KEY_DOWN,
  UH_KEY_LEFT,
  UH_KEY_RIGHT,
  UH_KEY_SELECT,   // Return, Space, joystick fire
  UH_KEY_BACK,     // Escape, F12
  UH_KEY_PAGE_UP,
  UH_KEY_PAGE_DOWN,
};
int uh_read_key(int *key, int *pressed);

// Keeps devices serviced while the view runs its own loop (Circle's tasks:
// USB, the network).
void uh_poll(void);

void uh_reboot(void);

void uh_log(const char *fmt, ...);

#endif
