/*
 * mouse_test.c - CX22 pulse tests against atari800's input.c.
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
 * Uses INPUT_Frame and INPUT_Scanline from third_party/atari800/src/input.c,
 * observing the CX22 direction and clock bits documented there.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../../third_party/atari800/src/input.c"

int Atari800_machine_type = Atari800_MACHINE_XLXE;
int Atari800_tv_mode = Atari800_TV_PAL;
int Atari800_nframes, Atari800_keyboard_detached, CASSETTE_press_space;
UBYTE CPU_IRQ;
UBYTE POKEY_IRQEN, POKEY_IRQST, POKEY_KBCODE, POKEY_SKSTAT;
UBYTE POKEY_POT_input[8], GTIA_TRIG[4], PIA_PORT_input[2];
UBYTE ANTIC_PENH_input, ANTIC_PENV_input;

int PLATFORM_PORT(int port)
{
	(void) port;
	return 0xff;
}

int PLATFORM_TRIG(int port)
{
	(void) port;
	return 1;
}

int Atari_POT(int pot)
{
	(void) pot;
	return 228;
}

static int previous, pulses_x, pulses_y, wrong_direction;
static int signed_x, signed_y;
static int clock_tick, last_pulse_x, largest_gap_x, smallest_gap_x;
static int failures;

static void sample(int dx, int dy)
{
	int bits = (PIA_PORT_input[0] >> (4 * INPUT_mouse_port)) & 15;
	int changed = bits ^ previous;
	if (changed & 2) {
		pulses_x++;
		signed_x += (bits & 1) ? 1 : -1;
		if (last_pulse_x >= 0) {
			int gap = clock_tick - last_pulse_x;
			if (gap > largest_gap_x) largest_gap_x = gap;
			if (gap < smallest_gap_x) smallest_gap_x = gap;
		}
		last_pulse_x = clock_tick;
		if (!dx || (bits & 1) != (dx > 0))
			wrong_direction++;
	}
	if (changed & 8) {
		pulses_y++;
		signed_y += (bits & 4) ? 1 : -1;
		if (!dy || ((bits >> 2) & 1) != (dy > 0))
			wrong_direction++;
	}
	previous = bits;
}

static void frame(int dx, int dy)
{
	int line;
	INPUT_mouse_delta_x = dx;
	INPUT_mouse_delta_y = dy;
	INPUT_Frame();
	sample(dx, dy);
	for (line = 0; line < Atari800_tv_mode; line++) {
		clock_tick++;
		INPUT_Scanline();
		sample(dx, dy);
	}
}

static void reset(void)
{
	mouse_x = mouse_y = mouse_move_x = mouse_move_y = 0;
	INPUT_mouse_mode = INPUT_MOUSE_TRAK;
	INPUT_CenterMousePointer();
	mouse_last_right = mouse_last_down = 0;
	INPUT_mouse_buttons = 0;
	previous = 0;
	frame(0, 0);
	pulses_x = pulses_y = wrong_direction = 0;
	signed_x = signed_y = clock_tick = largest_gap_x = 0;
	last_pulse_x = -1;
	smallest_gap_x = Atari800_tv_mode;
}

static void check(const char *name, int expected_x, int expected_y)
{
	if (pulses_x != expected_x || pulses_y != expected_y || wrong_direction) {
		fprintf(stderr, "FAIL %s: pulses %d,%d (expected %d,%d), "
		        "%d wrong-axis/direction pulses\n",
		        name, pulses_x, pulses_y, expected_x, expected_y, wrong_direction);
		failures++;
	}
}

static void check_controls(void)
{
	int i;
	reset();
	frame(1, 1);
	INPUT_mouse_mode = INPUT_MOUSE_OFF;
	frame(0, 0);
	INPUT_mouse_mode = INPUT_MOUSE_TRAK;
	frame(0, 0);
	previous = (PIA_PORT_input[0] >> (4 * INPUT_mouse_port)) & 15;
	pulses_x = pulses_y = wrong_direction = 0;
	for (i = 0; i < 31; i++)
		frame(1, 1);
	check("mode change clears fractions", 31 * INPUT_mouse_speed / 32, 31 * INPUT_mouse_speed / 32);
	reset();
	INPUT_mouse_buttons = 1;
	frame(0, 0);
	if (GTIA_TRIG[INPUT_mouse_port] != 0) {
		fprintf(stderr, "FAIL trackball fire press\n");
		failures++;
	}
	INPUT_mouse_buttons = 0;
	frame(0, 0);
	if (GTIA_TRIG[INPUT_mouse_port] != 1) {
		fprintf(stderr, "FAIL trackball fire release\n");
		failures++;
	}
}

static uint32_t legacy_signature(int mode)
{
	uint32_t hash = 2166136261u;
	int i, line;
	mouse_x = mouse_y = mouse_move_x = mouse_move_y = 0;
	mouse_last_right = mouse_last_down = 0;
	INPUT_mouse_mode = mode;
	INPUT_mouse_speed = 3;
	INPUT_mouse_port = 0;
	for (i = 0; i < 100; i++) {
		INPUT_mouse_delta_x = i % 41 - 20;
		INPUT_mouse_delta_y = i % 23 - 11;
		INPUT_Frame();
		hash = (hash ^ PIA_PORT_input[0]) * 16777619u;
		for (line = 0; line < Atari800_tv_mode; line++) {
			INPUT_Scanline();
			hash = (hash ^ PIA_PORT_input[0]) * 16777619u;
		}
	}
	return hash;
}

int main(void)
{
	int tv, port, speed, sign, i;
	uint32_t st = legacy_signature(INPUT_MOUSE_ST);
	uint32_t amiga = legacy_signature(INPUT_MOUSE_AMIGA);
	if (st != 0x319359cbu || amiga != 0x1cde3455u) {
		fprintf(stderr, "FAIL legacy ST/Amiga waveforms: %08x %08x\n", st, amiga);
		failures++;
	}
	for (tv = 0; tv < 2; tv++) {
		Atari800_tv_mode = tv ? Atari800_TV_NTSC : Atari800_TV_PAL;
		for (port = 0; port < 2; port++) {
			INPUT_mouse_port = port;
			for (speed = 1; speed <= 9; speed++) {
				INPUT_mouse_speed = speed;
				check_controls();
				for (sign = -1; sign <= 1; sign += 2) {
					reset();
					for (i = 0; i < 32; i++)
						frame(sign, 0);
					check("fractional horizontal motion", speed, 0);
					reset();
					for (i = 0; i < 32; i++)
						frame(10 * sign, sign);
					check("shallow diagonal", 10 * speed, speed);
					reset();
					for (i = 0; i < 32; i++)
						frame(sign, 10 * sign);
					check("steep diagonal", speed, 10 * speed);
					reset();
					for (i = 0; i < 32; i++)
						frame(127 * sign, 0);
					check("fast horizontal motion", 127 * speed, 0);
					reset();
					for (i = 0; i < 32; i++)
						frame(254 * sign, 127 * sign);
					check("two reports per frame", 254 * speed, 127 * speed);
					pulses_x = pulses_y = wrong_direction = 0;
					for (i = 0; i < 10; i++)
						frame(0, 0);
					check("stationary mouse", 0, 0);
					reset();
					for (i = 0; i < 32; i++)
						frame(sign, -sign);
					for (i = 0; i < 32; i++)
						frame(-sign, sign);
					check("reversing both axes", 2 * speed, 2 * speed);
				}
				reset();
				for (i = 0; i < 20; i++)
					frame(32, 0);
				check("uniform horizontal motion", 20 * speed, 0);
				if (smallest_gap_x < Atari800_tv_mode / speed ||
				    largest_gap_x > (Atari800_tv_mode + speed - 1) / speed) {
					fprintf(stderr, "FAIL pulse spacing: %d..%d scanlines at speed %d\n",
					        smallest_gap_x, largest_gap_x, speed);
					failures++;
				}
				{
					uint32_t seed = 1;
					int fraction_x = 0, fraction_y = 0;
					reset();
					for (i = 0; i < 1000; i++) {
						int dx, dy, expected_x, expected_y;
						seed = seed * 1664525u + 1013904223u;
						dx = (int) (seed % 321) - 160;
						seed = seed * 1664525u + 1013904223u;
						dy = (int) (seed % 321) - 160;
						fraction_x += dx * speed;
						fraction_y += dy * speed;
						expected_x = fraction_x / 32;
						expected_y = fraction_y / 32;
						fraction_x %= 32;
						fraction_y %= 32;
						signed_x = signed_y = 0;
						frame(dx, dy);
						if (signed_x != expected_x || signed_y != expected_y) {
							fprintf(stderr, "FAIL changing direction: got %d,%d expected %d,%d\n",
							        signed_x, signed_y, expected_x, expected_y);
							failures++;
							break;
						}
					}
				}
			}
		}
	}
	if (failures) {
		fprintf(stderr, "%d mouse tests failed\n", failures);
		return EXIT_FAILURE;
	}
	puts("PASS CX22 pulse counts, spacing, direction, stopping and fire "
	     "(PAL/NTSC, ports 1/2, speeds 1-9); legacy ST/Amiga waveforms unchanged");
	return EXIT_SUCCESS;
}
