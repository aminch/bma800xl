/*
 * host.h - services the Circle kernel provides to the emulator.
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
 * The emulator (atari800 and the port in src/port/) is C and runs on core 1.
 * Everything it needs from the hardware goes through these functions, which
 * are implemented in src/kernel.cpp.
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
/* Changes a setting, which then wins over cmdline.txt; host_options_save()
   writes the settings to bma800xl.cfg on the SD card. Both return 0 on
   failure. */
int host_option_set(const char *name, const char *value);
int host_options_save(void);

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

/* Independent gameplay buttons. Bits 0-7 retain A/B/X/Y/L/R/Select/Start;
   bits 8-11 are L2/R2/L3/R3. Home stays reserved in host_pad_state().
   Generic devices retain their remaining USB button numbers. */
#define HOST_PAD_BUTTONS 32
#define HOST_PAD_AXES 16
struct host_pad_axis {
	int value, minimum, maximum;
};
struct host_pad_input {
	struct host_pad_axis axes[HOST_PAD_AXES];
	unsigned axis_mask;
	int dpad; /* HOST_JOY_* direction bits, without analogue axes. */
	int has_dpad;
};
struct host_pad_info {
	char name[64];
	char usb_id[24];
	uint32_t button_mask;
	unsigned generation;
	int known;
	unsigned axis_mask;
	int has_dpad;
};
int host_pad_info(int pad, struct host_pad_info *info);
uint32_t host_pad_buttons(int pad);
/* Copies a connected pad's separate axis and D-pad/hat state. */
int host_pad_input(int pad, struct host_pad_input *input);

/* Our generic USB layout, without combining shoulders and triggers. */
static inline uint32_t host_pad_generic_buttons(uint32_t buttons)
{
	return (buttons & ~UINT32_C(0x13c0))
	     | ((buttons & UINT32_C(0x00c0)) << 2)
	     | ((buttons & UINT32_C(0x0300)) >> 2);
}

/* State of the joystick wired to GPIO port PORT (0 or 1), as HOST_JOY_* bits.
   Always 0 when GPIO joysticks are disabled. */
int host_gpio_joystick(int port);

/* Mouse buttons. */
#define HOST_MOUSE_LEFT   0x01
#define HOST_MOUSE_RIGHT  0x02
#define HOST_MOUSE_MIDDLE 0x04

/* USB mice: the movement since the last call (x to the right, y down, in
   the mice's own counts) and the buttons held down, as HOST_MOUSE_* bits.
   All mice are added together. */
unsigned host_mouse(int *dx, int *dy);

/* ---- System -------------------------------------------------------------- */

void host_reboot(void);
void host_power_off(void);
/* The Pi's IP address as text, when the network is up. Returns 0 if not. */
int host_network_address(char *address, unsigned size);
/* Network state for people: "Wi-Fi 192.168.1.50", "Off", "Ethernet:
   waiting for address"... */
void host_network_status(char *text, unsigned size);
/* A Wi-Fi network found by host_wifi_scan(). signal is in dBm. */
struct host_wifi_ap {
	char ssid[33];
	int signal;
	int secure;
};
/* Scans for Wi-Fi networks for a few seconds. Returns how many it found
   (up to MAX), or -1 if Wi-Fi isn't running (it starts only when it is the
   selected network). */
int host_wifi_scan(struct host_wifi_ap *aps, int max);
/* ---- Modem (the R: device's TCP connection) ------------------------------ */

#define HOST_MODEM_IDLE       0  /* no call: never dialled, failed or hung up */
#define HOST_MODEM_DIALLING   1
#define HOST_MODEM_CONNECTED  2

/* Starts a TCP call to HOST (a name or an address) on PORT, hanging up any
   call first. Returns at once, with the state HOST_MODEM_DIALLING, or 0 if
   the network isn't running. */
int host_modem_dial(const char *host, unsigned port);
void host_modem_hang_up(void);
/* HOST_MODEM_*. A call that fails or that the other end closes goes back
   to HOST_MODEM_IDLE. */
int host_modem_state(void);
/* Queues up to LENGTH bytes to send; returns how many it took. */
unsigned host_modem_send(const uint8_t *data, unsigned length);
/* Takes up to SIZE received bytes; returns how many. */
unsigned host_modem_receive(uint8_t *data, unsigned size);

/* "Raspberry Pi 3 Model B+" */
const char *host_model_name(void);
/* The display's size in pixels. */
void host_display_size(int *width, int *height);
/* Asks for the display to refresh at FPS frames a second, where the host
   can set the video mode (the Pi 4 and 5 with video_driver=kms). Returns
   TRUE if the mode was changed; the refresh rate should then be measured
   again. On the Pi 5 the "resolution" setting (auto, 1920x1080 or
   1280x720) also chooses the mode's size. */
int host_display_match_rate(double fps);
/* TRUE if the "resolution" setting applies (the Pi 5 with
   video_driver=kms). */
int host_display_resolution_choice(void);
/* Writes a line of text to the log. */
void host_log(const char *message);

/* Where the log goes (enable_logging=off|uart|file). */
#define HOST_LOG_OFF  0
#define HOST_LOG_UART 1  /* serial port, GPIO14/15 at 115200 */
#define HOST_LOG_FILE 2  /* serial port and /bma800xl.log on the SD card */
int host_log_destination(void);
/* Changes it at once. Returns 0 if the log file cannot be opened. */
int host_set_log_destination(int destination);

#ifdef __cplusplus
}
#endif

#endif /* BMA800XL_HOST_H_ */
