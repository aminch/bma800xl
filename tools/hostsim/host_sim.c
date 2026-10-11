/*
 * host_sim.c - runs the BMA800XL emulator port on a PC for testing.
 *
 * Implements src/host.h with a simulated display, sound output and input,
 * so atari800 and the port in src/port/ can be tested without a Raspberry
 * Pi. Time is virtual: each display refresh advances the clock by one frame
 * of a 50 Hz (or REFRESH Hz) display, so tests run as fast as the PC allows.
 *
 * A script on standard input drives the test, one command per line:
 *
 *   <frame> key <usage> <0|1>     press or release a key (USB HID usage)
 *   <frame> type <text>            type text (US layout, one key per frame)
 *   <frame> pad <n> <bits>         set gamepad n to HOST_PAD_* bits
 *   <frame> shot <file.ppm>        save the visible screen
 *   <frame> audio                  print audio statistics
 *   <frame> webui <reboot|coldstart>  a request from the web UI
 *   <frame> quit                   end the test
 *
 * This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/host.h"

/* From src/port/platform.c, called by the web UI on the Pi. */
void bma_request_power(int power_off);
void bma_request_coldstart(void);

/* ---- Options ---------------------------------------------------------------- */

static const char *option(const char *name)
{
	static char buf[1024];
	const char *cmdline = getenv("BMA_CMDLINE");
	char *p, *save = NULL;
	size_t n = strlen(name);
	if (cmdline == NULL)
		return NULL;
	strncpy(buf, cmdline, sizeof(buf) - 1);
	for (p = strtok_r(buf, " ", &save); p; p = strtok_r(NULL, " ", &save)) {
		if (strncmp(p, name, n) == 0 && p[n] == '=')
			return p + n + 1;
	}
	return NULL;
}

const char *host_option(const char *name, const char *default_value)
{
	const char *v = option(name);
	static char ret[8][256];
	static int slot;
	if (v == NULL)
		return default_value;
	slot = (slot + 1) % 8;
	strncpy(ret[slot], v, 255);
	return ret[slot];
}

int host_option_int(const char *name, int default_value)
{
	const char *v = option(name);
	return v ? atoi(v) : default_value;
}

double host_option_double(const char *name, double default_value)
{
	const char *v = option(name);
	return v ? atof(v) : default_value;
}

/* ---- Time ----------------------------------------------------------------- */

static double now = 0.0;
static double refresh = 50.0;
static double refresh_later = 0.0;  /* refresh rate after start up */
static long frame = 0;
static long presents = 0;

static void tick(void);

double host_time(void) { return now; }
void host_sleep_us(unsigned us) { now += us / 1e6; tick(); }
void host_yield(void) { now += 1e-7; }

/* ---- Sound ---------------------------------------------------------------- */

#define HW_RATE 44100
static int snd_open = 0;
static int snd_channels = 1;
static unsigned snd_capacity = 0;
static double snd_buffered = 0;   /* frames queued at the "VideoCore" */
static double snd_last = 0.0;
static long snd_written = 0, snd_underruns = 0, snd_writes = 0;
static double snd_min_fill = 1e9, snd_max_fill = 0;

static void sound_advance(void)
{
	double played;
	if (!snd_open)
		return;
	played = (now - snd_last) * HW_RATE;
	snd_last = now;
	if (played > snd_buffered) {
		if (snd_written > HW_RATE) { /* ignore start up */
			snd_underruns++;
			fprintf(stderr, "[sim] frame %ld: audio underrun (%.0f frames short)\n",
			        frame, played - snd_buffered);
		}
		snd_buffered = 0;
	} else {
		snd_buffered -= played;
	}
}

unsigned host_sound_open(unsigned freq, int channels, unsigned latency_frames)
{
	(void) freq;
	snd_open = 1;
	snd_channels = channels;
	snd_capacity = latency_frames;
	snd_last = now;
	fprintf(stderr, "[sim] sound open: %d channel(s), %u frames\n", channels, latency_frames);
	return HW_RATE;
}

void host_sound_close(void) {}

unsigned host_sound_free_frames(void)
{
	sound_advance();
	/* BMA_SOUND_HALF=1 offers only half the buffer, which keeps atari800's
	   own sound buffer fuller, as the VideoCore's late reports do on a Pi. */
	double cap = getenv("BMA_SOUND_HALF") ? snd_capacity / 2.0 : snd_capacity;
	return snd_buffered >= cap ? 0 : (unsigned) (cap - snd_buffered);
}

void host_sound_write(const int16_t *samples, unsigned frames)
{
	(void) samples;
	sound_advance();
	if (snd_written > HW_RATE * 2) {
		if (snd_buffered < snd_min_fill) snd_min_fill = snd_buffered;
	}
	snd_buffered += frames;
	snd_written += frames;
	snd_writes++;
	if (snd_written > HW_RATE * 2) {
		if (snd_buffered > snd_max_fill) snd_max_fill = snd_buffered;
	}
}

/* ---- Video ---------------------------------------------------------------- */

static uint8_t *fb;
static int fb_w, fb_h;
static uint32_t palette[256];
static int view_x, view_y, view_w, view_h;

int host_video_init(int width, int height, uint8_t **pixels, int *pitch)
{
	fb_w = width;
	fb_h = height;
	fb = calloc(width, height);
	*pixels = fb;
	*pitch = width;
	return 0;
}

void host_video_set_view(int x, int y, int w, int h, double aspect)
{
	view_x = x; view_y = y; view_w = w; view_h = h;
	fprintf(stderr, "[sim] view %dx%d at %d,%d aspect %.3f\n", w, h, x, y, aspect);
}

void host_video_set_palette(int index, uint32_t rgb) { palette[index] = rgb; }
void host_video_update_palette(void) {}

static void screenshot(const char *name)
{
	FILE *f = fopen(name, "wb");
	int x, y;
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", view_w, view_h);
	for (y = view_y; y < view_y + view_h; y++)
		for (x = view_x; x < view_x + view_w; x++) {
			uint32_t c = palette[fb[y * fb_w + x]];
			fputc((c >> 16) & 0xff, f);
			fputc((c >> 8) & 0xff, f);
			fputc(c & 0xff, f);
		}
	fclose(f);
	fprintf(stderr, "[sim] frame %ld: saved %s\n", frame, name);
}

/* ---- Input ---------------------------------------------------------------- */

#define QSIZE 1024
static int q_usage[QSIZE], q_pressed[QSIZE];
static int q_head, q_tail;
static unsigned char key_state[256];
static int pads[HOST_PADS] = {-1, -1, -1, -1};

static void push_key(int usage, int pressed)
{
	q_usage[q_head] = usage;
	q_pressed[q_head] = pressed;
	q_head = (q_head + 1) % QSIZE;
}

int host_key_event(int *usage, int *pressed)
{
	tick();
	if (q_tail == q_head)
		return 0;
	*usage = q_usage[q_tail];
	*pressed = q_pressed[q_tail];
	key_state[*usage] = (unsigned char) *pressed;
	q_tail = (q_tail + 1) % QSIZE;
	return 1;
}

int host_key_down(int usage) { return key_state[usage & 0xff]; }

unsigned host_key_modifiers(void)
{
	unsigned m = 0;
	int i;
	for (i = 0; i < 8; i++)
		if (key_state[0xe0 + i])
			m |= 1u << i;
	return m;
}

int host_pad_state(int pad) { return pad >= 0 && pad < HOST_PADS ? pads[pad] : -1; }
int host_gpio_joystick(int port) { (void) port; return 0; }

/* US layout: character to (usage, shift). */
static int char_usage(int c, int *shift)
{
	static const char *row = "1234567890";
	static const char *shifted_row = "!@#$%^&*()";
	static const struct { char c, s; int u; } sym[] = {
		{'-', '_', 0x2d}, {'=', '+', 0x2e}, {'[', '{', 0x2f}, {']', '}', 0x30},
		{'\\', '|', 0x31}, {';', ':', 0x33}, {'\'', '"', 0x34}, {',', '<', 0x36},
		{'.', '>', 0x37}, {'/', '?', 0x38}, {0, 0, 0}
	};
	const char *p;
	int i;
	*shift = 0;
	if (c >= 'a' && c <= 'z') return 0x04 + c - 'a';
	if (c >= 'A' && c <= 'Z') { *shift = 1; return 0x04 + c - 'A'; }
	if (c == ' ') return 0x2c;
	if (c == '\n') return 0x28;
	if ((p = strchr(row, c)) != NULL) return 0x1e + (int) (p - row);
	if ((p = strchr(shifted_row, c)) != NULL) { *shift = 1; return 0x1e + (int) (p - shifted_row); }
	for (i = 0; sym[i].c; i++) {
		if (sym[i].c == c) return sym[i].u;
		if (sym[i].s == c) { *shift = 1; return sym[i].u; }
	}
	return 0;
}

/* Pending typed text: one key down per frame, released the frame after. */
static char typing[1024];
static int typing_pos = -1, typing_up = 0;

static void type_step(void)
{
	int shift, u;
	if (typing_pos < 0)
		return;
	if (typing_up) {
		u = char_usage(typing[typing_pos], &shift);
		push_key(u, 0);
		if (shift) push_key(0xe1, 0);
		typing_up = 0;
		if (typing[++typing_pos] == '\0')
			typing_pos = -1;
		return;
	}
	u = char_usage(typing[typing_pos], &shift);
	if (shift) push_key(0xe1, 1);
	push_key(u, 1);
	typing_up = 1;
}

/* ---- Script ----------------------------------------------------------------- */

static char line[1100];
static int have_line = 0;
static long line_frame = -1;

static void run_script(void)
{
	for (;;) {
		char cmd[32];
		int n;
		if (!have_line) {
			if (!fgets(line, sizeof(line), stdin)) {
				line_frame = -1;
				return;
			}
			line[strcspn(line, "\r\n")] = '\0';
			if (line[0] == '#' || line[0] == '\0')
				continue;
			have_line = 1;
			line_frame = atol(line);
		}
		if (frame < line_frame)
			return;
		have_line = 0;
		if (sscanf(line, "%*ld %31s%n", cmd, &n) != 1)
			continue;
		if (strcmp(cmd, "key") == 0) {
			int usage, pressed;
			sscanf(line + n, "%i %d", &usage, &pressed);
			push_key(usage, pressed);
		} else if (strcmp(cmd, "type") == 0) {
			strncpy(typing, line + n + 1, sizeof(typing) - 2);
			/* "\n" in the script means Return */
			{
				char *p;
				while ((p = strstr(typing, "\\n")) != NULL) {
					*p = '\n';
					memmove(p + 1, p + 2, strlen(p + 2) + 1);
				}
			}
			typing_pos = 0;
			typing_up = 0;
		} else if (strcmp(cmd, "pad") == 0) {
			int pad, bits;
			sscanf(line + n, "%d %i", &pad, &bits);
			pads[pad] = bits;
		} else if (strcmp(cmd, "shot") == 0) {
			screenshot(line + n + 1);
		} else if (strcmp(cmd, "webui") == 0) {
			if (strstr(line + n, "reboot"))
				bma_request_power(0);
			else if (strstr(line + n, "coldstart"))
				bma_request_coldstart();
		} else if (strcmp(cmd, "audio") == 0) {
			fprintf(stderr, "[sim] frame %ld: audio written %ld frames in %ld writes, "
			        "queue before write min %.0f, after write max %.0f, underruns %ld\n",
			        frame, snd_written, snd_writes, snd_min_fill, snd_max_fill, snd_underruns);
			fprintf(stderr, "[sim] frame %ld: %ld frames presented\n", frame, presents);
		} else if (strcmp(cmd, "quit") == 0) {
			fprintf(stderr, "[sim] quit at frame %ld (%.2f s)\n", frame, now);
			exit(0);
		}
	}
}

/* The script runs on display frames counted from the virtual clock, so it
   keeps going while the emulator sleeps (in its menus, for example). */
static void tick(void)
{
	long f = (long) (now * refresh + 1e-6);
	while (frame < f) {
		frame++;
		type_step();
		run_script();
	}
	if (frame > 2000000)
		exit(1);
}

void host_video_present(int wait_vsync)
{
	presents++;
	if (refresh_later > 0 && frame == 30)
		refresh = refresh_later;
	if (wait_vsync) {
		/* Wait for the next vertical blank of the virtual display. */
		double period = 1.0 / refresh;
		now = ((long) (now / period + 1e-6) + 1) * period;
	} else {
		now += 1e-4;
	}
	tick();
}

/* ---- System ----------------------------------------------------------------- */

void host_reboot(void)
{
	fprintf(stderr, "[sim] reboot requested at frame %ld\n", frame);
	exit(0);
}

void host_power_off(void)
{
	fprintf(stderr, "[sim] power off requested at frame %ld\n", frame);
	exit(0);
}

int host_network_address(char *address, unsigned size)
{
	const char *ip = getenv("BMA_IP");
	if (ip == NULL || frame < 100)
		return 0;
	snprintf(address, size, "%s", ip);
	return 1;
}

void host_log(const char *message)
{
	fprintf(stderr, "[log] %s\n", message);
}

extern void bma800xl_main(void);

int main(void)
{
	const char *r = getenv("BMA_REFRESH");
	if (r)
		refresh = atof(r);
	r = getenv("BMA_REFRESH_LATER");
	if (r)
		refresh_later = atof(r);
	bma800xl_main();
	return 0;
}
