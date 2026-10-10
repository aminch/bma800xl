/*
 * host.h - services the Circle kernel provides to the emulator.
 *
 * The emulator (atari800 and the port in src/port/) is C and runs on core 1.
 * Everything it needs from the hardware goes through these functions, which
 * are implemented in src/kernel.cpp.
 *
 * This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMA800XL_HOST_H_
#define BMA800XL_HOST_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Options (cmdline.txt) ---------------------------------------------- */

const char *host_option(const char *name, const char *default_value);
int host_option_int(const char *name, int default_value);
double host_option_double(const char *name, double default_value);

/* ---- Time ---------------------------------------------------------------- */

/* Seconds since boot. */
double host_time(void);
void host_sleep_us(unsigned us);
/* Lets Circle's tasks (VCHIQ, USB) run. Call regularly. */
void host_yield(void);

/* ---- Video --------------------------------------------------------------- */

/* Allocates the emulator's 8 bit indexed frame buffer. */
int host_video_init(int width, int height, uint8_t **pixels, int *pitch);
/* Area of the frame buffer that is shown, and its display aspect ratio
   (width / height). */
void host_video_set_view(int x, int y, int w, int h, double aspect);
/* Sets palette entry INDEX to 0xRRGGBB. Takes effect at host_video_update_palette. */
void host_video_set_palette(int index, uint32_t rgb);
void host_video_update_palette(void);
/* Shows the frame buffer. With WAIT_VSYNC, returns after the next vertical
   blank, which paces the emulation to the display. */
void host_video_present(int wait_vsync);
/* Refresh rate of the display in Hz. */
double host_video_refresh_rate(void);

/* ---- Sound --------------------------------------------------------------- */

/* Opens audio output; returns the sample rate in use (0 on failure). */
unsigned host_sound_open(unsigned freq, int channels, unsigned latency_frames);
void host_sound_close(void);
/* Frames that can be written without exceeding the latency. */
unsigned host_sound_free_frames(void);
void host_sound_write(const int16_t *samples, unsigned frames);

/* ---- Input --------------------------------------------------------------- */

/* Pops the next keyboard event: USB HID usage code and pressed (1) or
   released (0). Returns 0 when there are no more events. */
int host_key_event(int *usage, int *pressed);
/* Is the key with this USB HID usage code down? */
int host_key_down(int usage);
/* USB HID modifier byte (bit 0 left ctrl ... bit 7 right GUI). */
unsigned host_key_modifiers(void);

#define HOST_JOY_UP     0x01
#define HOST_JOY_DOWN   0x02
#define HOST_JOY_LEFT   0x04
#define HOST_JOY_RIGHT  0x08
#define HOST_JOY_FIRE   0x10

#define HOST_PADS 4

/* Gamepad buttons, in a layout common to all pads. */
#define HOST_PAD_A       0x0001  /* bottom face button */
#define HOST_PAD_B       0x0002  /* right face button */
#define HOST_PAD_X       0x0004  /* left face button */
#define HOST_PAD_Y       0x0008  /* top face button */
#define HOST_PAD_L       0x0010
#define HOST_PAD_R       0x0020
#define HOST_PAD_SELECT  0x0040
#define HOST_PAD_START   0x0080
#define HOST_PAD_HOME    0x0100
#define HOST_PAD_UP      0x1000
#define HOST_PAD_DOWN    0x2000
#define HOST_PAD_LEFT    0x4000
#define HOST_PAD_RIGHT   0x8000

/* State of USB gamepad PAD (0..HOST_PADS-1) as HOST_PAD_* bits, or -1 if no
   pad is connected there. */
int host_pad_state(int pad);

/* State of the joystick wired to GPIO port PORT (0 or 1), as HOST_JOY_* bits.
   Always 0 when GPIO joysticks are disabled. */
int host_gpio_joystick(int port);

/* ---- System -------------------------------------------------------------- */

void host_reboot(void);
/* Shows a line of text on the boot log (serial and, early on, the screen). */
void host_log(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* BMA800XL_HOST_H_ */
