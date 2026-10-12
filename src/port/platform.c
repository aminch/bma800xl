/*
 * platform.c - atari800 platform layer for BMA800XL.
 *
 * Implements the PLATFORM_* interface (see atari800's platform.h) on top of
 * the Circle kernel services in src/host.h, and runs the emulator's main
 * loop. Keyboard and joystick input are in keyboard.c.
 *
 * Timing: when the display refreshes at (close to) the emulated machine's
 * frame rate, every emulated frame is shown on its own display refresh and
 * the emulator waits for vertical blank instead of a timer. Scrolling is
 * then perfectly smooth. The sound is generated at a rate scaled by the
 * small difference between the two frame rates so the audio buffer neither
 * fills up nor runs dry. Otherwise atari800's own timer based pacing is used.
 *
 * This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "afile.h"
#include "akey.h"
#include "atari.h"
#include "colours.h"
#include "cpu.h"
#include "input.h"
#include "log.h"
#include "memory.h"
#include "monitor.h"
#include "platform.h"
#include "screen.h"
#include "sio.h"
#include "sound.h"
#include "ui.h"
#include "ui_basic.h"
#include "util.h"

#include "../host.h"
#include "bma800xl.h"

/* On-screen message (see below). */
static int message_frames;
static void draw_message(void);

/* ---- Display --------------------------------------------------------------- */

/* How much of the Atari's 384x240 screen is shown. */
enum {
	VIEW_PLAYFIELD = 0,   /* 320x192, no border */
	VIEW_NORMAL = 1,      /* 336x240, atari800's default */
	VIEW_FULL = 2         /* 384x240, everything */
};

static int view_mode = VIEW_NORMAL;
static uint8_t *fb_pixels = NULL;
static int fb_pitch = 0;
static int draw_direct = FALSE;     /* atari800 draws straight into fb_pixels */

static double refresh_rate = 0.0;   /* measured display refresh, Hz */
static int vsync_lock = FALSE;      /* pace emulation by the display */
static int vsync_tv_mode = -1;      /* TV mode vsync_lock was decided for */

static void apply_view(void)
{
	int x, y, w, h;
	/* Pixel aspect ratio: atari800's 336x240 view fills a 4:3 screen. */
	double par = (4.0 / 3.0) / (336.0 / 240.0);
	double aspect;

	switch (view_mode) {
	case VIEW_PLAYFIELD:
		x = 32; y = 24; w = 320; h = 192;
		break;
	case VIEW_FULL:
		x = 0; y = 0; w = Screen_WIDTH; h = Screen_HEIGHT;
		break;
	case VIEW_NORMAL:
	default:
		x = 24; y = 0; w = 336; h = Screen_HEIGHT;
		break;
	}
	aspect = w * par / h * host_option_double("aspect", 1.0);
	host_video_set_view(x, y, w, h, aspect);
}

/* border=normal|none|full */
static void read_border_setting(void)
{
	const char *border = host_option("border", "normal");
	if (strcmp(border, "none") == 0)
		view_mode = VIEW_PLAYFIELD;
	else if (strcmp(border, "full") == 0)
		view_mode = VIEW_FULL;
	else
		view_mode = VIEW_NORMAL;
}

static double atari_fps(void)
{
	return Atari800_tv_mode == Atari800_TV_PAL ? Atari800_FPS_PAL : Atari800_FPS_NTSC;
}

/* atari800's crash menu ("The Atari computer has crashed") is only on the
   screen. The CPU opens it through UI_Run, which calls the UI driver's fInit
   first: that is where the crash is written to the log, with the registers,
   the code around it and the disks in use. */
static UI_tDriver crash_logging_driver;

static void crash_logging_init(void)
{
	if (UI_crash_code >= 0) {
		char msg[96];
		int len, i;
		snprintf(msg, sizeof(msg), "Atari crashed: code $%02X (CIM) at $%04X",
		         UI_crash_code, UI_crash_address);
		host_log(msg);
		snprintf(msg, sizeof(msg), "  A=%02X X=%02X Y=%02X S=%02X P=%02X",
		         CPU_regA, CPU_regX, CPU_regY, CPU_regS, CPU_regP);
		host_log(msg);
		len = snprintf(msg, sizeof(msg), "  $%04X:", (UWORD) (UI_crash_address - 8));
		for (i = -8; i < 8; i++)
			len += snprintf(msg + len, sizeof(msg) - len, " %02X",
			                MEMORY_SafeGetByte((UWORD) (UI_crash_address + i)));
		host_log(msg);
		for (i = 0; i < SIO_MAX_DRIVES; i++) {
			if (SIO_drive_status[i] == SIO_OFF || SIO_drive_status[i] == SIO_NO_DISK)
				continue;
			snprintf(msg, sizeof(msg), "  D%d: %.80s", i + 1, SIO_filename[i]);
			host_log(msg);
		}
	}
	UI_BASIC_driver.fInit();
}

/* Times a few vertical blanks to find the display's refresh rate. */
static void measure_refresh_rate(void)
{
	enum { SKIP = 3, COUNT = 12 };
	double start = 0.0;
	int i;
	for (i = 0; i < SKIP + COUNT; i++) {
		host_video_present(TRUE);
		if (i == SKIP - 1)
			start = host_time();
	}
	refresh_rate = COUNT / (host_time() - start);
	{
		char msg[64];
		snprintf(msg, sizeof(msg), "Display refresh rate: %.3f Hz", refresh_rate);
		host_log(msg);
	}
}

/* The refresh rate measured at start up is only roughly right. While the
   emulation runs in step with the display, keep timing vertical blanks over
   a long window; the sound rate is then corrected with the better figure. */
static double vbl_window_start = 0.0;
static double vbl_last = 0.0;
static long vbl_count = 0;
static int refresh_refined = FALSE;

static void track_refresh_rate(void)
{
	double t = host_time();
	double period = 1.0 / refresh_rate;

	if (vbl_count == 0 || t - vbl_last > 1.5 * period || t - vbl_last < 0.5 * period) {
		/* A missed or doubled vertical blank: start a new window. */
		vbl_window_start = t;
		vbl_count = 0;
	}
	vbl_last = t;
	vbl_count++;
}

/* Returns TRUE when a better refresh rate is known and the sound rate should
   be recalculated. */
static int refine_refresh_rate(void)
{
	double measured;
	if (vbl_count < 200)
		return FALSE;
	measured = (vbl_count - 1) / (vbl_last - vbl_window_start);
	/* First refinement after about four seconds; later ones only when the
	   sound is paused anyway (in the menus) and the figure has moved. */
	if (refresh_refined && !(UI_is_active && fabs(measured / refresh_rate - 1.0) > 2e-5))
		return FALSE;
	if (refresh_refined && vbl_count < 1000)
		return FALSE;
	Log_print("BMA800XL: display refresh rate %.4f Hz", measured);
	refresh_rate = measured;
	refresh_refined = TRUE;
	return TRUE;
}

static void update_sync_mode(void)
{
	const char *mode = host_option("vsync", "auto");
	double ratio = refresh_rate / atari_fps();

	if (strcmp(mode, "off") == 0 || strcmp(mode, "0") == 0)
		vsync_lock = FALSE;
	else if (strcmp(mode, "on") == 0 || strcmp(mode, "1") == 0)
		vsync_lock = TRUE;
	else
		/* A 50 Hz display for PAL or 60 Hz for NTSC; anything else would
		   run the machine noticeably too fast or slow. */
		vsync_lock = ratio > 0.98 && ratio < 1.02;
	vsync_tv_mode = Atari800_tv_mode;

	Log_print("%s: display %.2f Hz, Atari %.2f Hz, %s",
	          "BMA800XL", refresh_rate, atari_fps(),
	          vsync_lock ? "synchronised to display" : "timer paced");
}

void PLATFORM_PaletteUpdate(void)
{
	int i;
	for (i = 0; i < 256; i++)
		host_video_set_palette(i, (uint32_t) Colours_table[i] & 0xffffff);
	host_video_update_palette();
}

/* ---- Sound ------------------------------------------------------------------ */

static int sound_paused = TRUE;
static int sound_channels = 1;
static unsigned sound_hw_rate = 0;
static const int16_t silence[2048 * 2];

static void feed_silence(void);

int PLATFORM_SoundSetup(Sound_setup_t *setup)
{
	unsigned buffer_ms = setup->buffer_ms ? setup->buffer_ms
	                     : (unsigned) host_option_int("audio_buffer_ms", 40);
	unsigned frames;

	if (buffer_ms < 10)
		buffer_ms = 10;
	if (setup->channels < 1)
		setup->channels = 1;
	if (setup->channels > 2)
		setup->channels = 2;
	setup->sample_size = 2;

	frames = 44100 * buffer_ms / 1000;
	sound_hw_rate = host_sound_open(44100, setup->channels, frames);
	if (sound_hw_rate == 0)
		return FALSE;
	sound_channels = setup->channels;

	/* When the frame rate follows the display, generate audio at the rate
	   that keeps up with the display (see the top of this file). */
	setup->freq = sound_hw_rate;
	if (vsync_lock)
		setup->freq = (unsigned) (sound_hw_rate * atari_fps() / refresh_rate + 0.5);
	setup->buffer_frames = setup->freq * buffer_ms / 1000;
	sound_paused = TRUE;
	return TRUE;
}

void PLATFORM_SoundExit(void)
{
	host_sound_close();
}

void PLATFORM_SoundPause(void)
{
	sound_paused = TRUE;
	feed_silence();
}

void PLATFORM_SoundContinue(void)
{
	sound_paused = FALSE;
}

unsigned int PLATFORM_SoundAvailable(void)
{
	/* Let the VideoCore tell us how much it has played. */
	host_yield();
	return host_sound_free_frames() * sound_channels * 2;
}

void PLATFORM_SoundWrite(UBYTE const *buffer, unsigned int size)
{
	host_sound_write((const int16_t *) buffer, size / (sound_channels * 2));
}

/* Keeps the audio output fed with silence while emulation is paused, so it
   restarts cleanly. */
static void feed_silence(void)
{
	unsigned frames;
	if (sound_hw_rate == 0)
		return;
	frames = host_sound_free_frames();
	if (frames > 2048)
		frames = 2048;
	if (frames > 0)
		host_sound_write(silence, frames);
}

/* ---- Time ------------------------------------------------------------------- */

/* atari800's clock: real time plus the sleeps it asked for that were
   skipped. Atari800_Sync() moves its target on by one Atari frame (which
   its sound speed adjustment can stretch by 5%) every frame, and only
   catches up when it falls behind, never when it gets ahead. With the
   display doing the pacing its sleeps are skipped, so against real time it
   would drift ahead, by a minute or more in a long session, and the first
   sleep in a menu would then wait for all of it. Counting the skipped
   sleeps keeps the two together. */
static double skipped_sleep = 0.0;
#define MAX_SLEEP 0.25

double PLATFORM_Time(void)
{
	return host_time() + skipped_sleep;
}

void PLATFORM_Sleep(double s)
{
	/* With the emulation locked to the display, the wait for vertical blank
	   does the pacing; atari800's timer would only make frames late. */
	if (s <= 0.0 || (vsync_lock && !UI_is_active && !Atari800_turbo)) {
		if (s > 0.0)
			skipped_sleep += s;
		host_yield();
		return;
	}
	/* No wait atari800 asks for is ever this long (a frame is 20 ms). If it
	   does, its clock has got ahead of real time somehow: move it on rather
	   than leave the menus deaf until real time catches up. */
	if (s > MAX_SLEEP) {
		char msg[64];
		snprintf(msg, sizeof(msg), "atari800's clock was %.2f s ahead; moved on", s);
		host_log(msg);
		skipped_sleep += s - MAX_SLEEP;
		s = MAX_SLEEP;
	}
	host_sleep_us((unsigned) (s * 1000000.0));
	/* Menus wait here between key presses without drawing frames. */
	if (sound_paused)
		feed_silence();
}

/* ---- Screen ----------------------------------------------------------------- */

/* Reopens the sound output after the sound rate has changed. */
static void restart_sound(void)
{
	if (Sound_enabled) {
		Sound_Setup();
		if (!UI_is_active)
			Sound_Continue();
	}
}

void PLATFORM_DisplayScreen(void)
{
	int wait;

	if (Atari800_tv_mode != vsync_tv_mode) {
		/* PAL/NTSC changed: the frame rate and the sound rate follow, and
		   the display too where the host can change its mode. */
		if (host_display_match_rate(atari_fps())) {
			measure_refresh_rate();
			refresh_refined = FALSE;
			vbl_count = 0;
		}
		update_sync_mode();
		restart_sound();
	} else if (vsync_lock && refine_refresh_rate()) {
		restart_sound();
	}

	if (message_frames > 0 && !UI_is_active) {
		message_frames--;
		draw_message();
	}

	if (!draw_direct) {
		const UBYTE *src = (const UBYTE *) Screen_atari;
		uint8_t *dst = fb_pixels;
		int y;
		for (y = 0; y < Screen_HEIGHT; y++) {
			memcpy(dst, src, Screen_WIDTH);
			src += Screen_WIDTH;
			dst += fb_pitch;
		}
	}

	if (sound_paused)
		feed_silence();

	wait = !Atari800_turbo && (vsync_lock || UI_is_active);
	host_video_present(wait);
	if (wait)
		track_refresh_rate();
}

/* ---- Files ------------------------------------------------------------------ */

/* atari800 writes to disk images without flushing them, and the SD card's
   file system caches what it can. Since the Pi can be switched off at any
   time, push everything out to the card once a second. Files that have not
   been written to cost almost nothing. */
static void sync_files(void)
{
	int fd;
	fflush(NULL);
	for (fd = 3; fd < 20; fd++)
		fsync(fd);
}

/* ---- Configuration ---------------------------------------------------------- */

void PLATFORM_ConfigInit(void)
{
	view_mode = VIEW_NORMAL;
	/* Start in BASIC, like a real 800XL. Hold Option (F2) while booting a
	   game that needs BASIC off, or turn it off in System Settings. */
	Atari800_disable_basic = FALSE;
}

int PLATFORM_Configure(char *option, char *parameters)
{
	/* Older versions kept these in atari800.cfg. They are BMA800XL
	   settings now, in bma800xl.cfg; skip them quietly. */
	(void) parameters;
	return strcmp(option, "BMA800XL_BORDER") == 0
	    || strcmp(option, "BMA800XL_KEYPAD_JOYSTICK") == 0
	    || strcmp(option, "BMA800XL_ARROW_JOYSTICK") == 0;
}

void PLATFORM_ConfigSave(FILE *fp)
{
	(void) fp;
}

/* ---- Requests from the web UI ----------------------------------------------- */

/* The web UI's server runs as a Circle task, which only runs when the
   emulator yields, so these are only ever set between emulation steps. They
   are acted on after the current frame, or, while a menu is open, by
   BMA_MenuRequests(). */
static volatile int request_power = -1;   /* 0 reboot, 1 power off */
static volatile int request_coldstart = FALSE;
static volatile int request_autostart = FALSE;
static char autostart_path[FILENAME_MAX];

void bma_request_power(int power_off)
{
	request_power = power_off ? 1 : 0;
}

void bma_request_coldstart(void)
{
	request_coldstart = TRUE;
}

/* Name kept from BMC64, whose web UI code calls it. */
void emu_autostart_interrupt(const char *path)
{
	Util_strlcpy(autostart_path, path, sizeof(autostart_path));
	request_autostart = TRUE;
}

static void handle_power_request(void)
{
	if (request_power >= 0) {
		int power_off = request_power;
		request_power = -1;
		/* Shuts atari800 down, which closes the disk images. */
		Atari800_Exit(FALSE);
		sync_files();
		if (power_off)
			host_power_off();
		else
			host_reboot();
	}
}

static void handle_requests(void)
{
	if (request_autostart) {
		request_autostart = FALSE;
		Log_print("Web UI: starting %s", autostart_path);
		if (AFILE_OpenFile(autostart_path, TRUE, 1, FALSE) == AFILE_ERROR)
			Log_print("Cannot start %s", autostart_path);
	}
	if (request_coldstart) {
		request_coldstart = FALSE;
		Log_print("Web UI: cold start");
		Atari800_Coldstart();
	}
	handle_power_request();
}

/* The main loop doesn't run while a menu is open. Reboot and power off
   happen at once; for anything else this returns TRUE, and the menus are
   closed (see PLATFORM_Keyboard) so the main loop can do it. */
int BMA_MenuRequests(void)
{
	handle_power_request();
	return request_coldstart || request_autostart;
}

/* ---- On-screen message ------------------------------------------------------ */

/* A small 5x7 font for the web UI's address, which atari800's own status
   line can't show (it has no '.' or ':'). Only the characters used. */
static const struct { char c; const char *rows[7]; } msg_font[] = {
	{'0', {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
	{'1', {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
	{'2', {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
	{'3', {"####.", "....#", "....#", ".###.", "....#", "....#", "####."}},
	{'4', {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}},
	{'5', {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
	{'6', {".###.", "#....", "#....", "####.", "#...#", "#...#", ".###."}},
	{'7', {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
	{'8', {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
	{'9', {".###.", "#...#", "#...#", ".####", "....#", "....#", ".###."}},
	{'.', {".....", ".....", ".....", ".....", ".....", ".##..", ".##.."}},
	{':', {".....", ".##..", ".##..", ".....", ".##..", ".##..", "....."}},
	{'/', {"....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."}},
	{'B', {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}},
	{'E', {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
	{'H', {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
	{'I', {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
	{'P', {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}},
	{'T', {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
	{'U', {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
	{'W', {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"}},
};

static char message[48];

/* Draws MESSAGE centred near the bottom of the visible screen. */
static void draw_message(void)
{
	int len = (int) strlen(message);
	int width = len * 6 + 2;
	int x0 = (Screen_visible_x1 + Screen_visible_x2 - width) / 2;
	int y0 = Screen_visible_y2 - 14;
	UBYTE *screen = (UBYTE *) Screen_atari;
	int i, x, y;

	if (x0 < 0)
		x0 = 0;
	for (y = 0; y < 11; y++)
		memset(screen + (y0 + y) * Screen_WIDTH + x0, 0x00, width);
	for (i = 0; i < len; i++) {
		unsigned k;
		for (k = 0; k < sizeof(msg_font) / sizeof(msg_font[0]); k++)
			if (msg_font[k].c == message[i])
				break;
		if (k == sizeof(msg_font) / sizeof(msg_font[0]))
			continue; /* space or unknown */
		for (y = 0; y < 7; y++)
			for (x = 0; x < 5; x++)
				if (msg_font[k].rows[y][x] == '#')
					screen[(y0 + 2 + y) * Screen_WIDTH + x0 + 2 + i * 6 + x] = 0x0f;
	}
}

/* Shows the web UI's address on screen once the network is up. */
static void show_network_address(void)
{
	static int shown = FALSE;
	char ip[32];
	if (shown || !host_network_address(ip, sizeof(ip)))
		return;
	shown = TRUE;
	snprintf(message, sizeof(message), "WEB UI: HTTP://%s/", ip);
	message_frames = 15 * 50;
	Log_print("Web UI: http://%s/", ip);
}

/* ---- Start up and shut down ------------------------------------------------- */

int PLATFORM_Initialise(int *argc, char *argv[])
{
	if (host_video_init(Screen_WIDTH, Screen_HEIGHT, &fb_pixels, &fb_pitch) != 0) {
		Log_print("Cannot initialise the display");
		return FALSE;
	}
	/* Let atari800 draw straight into the frame buffer when the layout
	   matches. Screen_Initialise only allocates a screen if none is set. */
	if (fb_pitch == Screen_WIDTH && Screen_atari == NULL) {
		Screen_atari = (ULONG *) fb_pixels;
		draw_direct = TRUE;
	}
	memset(fb_pixels, 0, fb_pitch * Screen_HEIGHT);

	read_border_setting();
	apply_view();
	PLATFORM_PaletteUpdate();

	host_display_match_rate(atari_fps());
	measure_refresh_rate();
	update_sync_mode();

	BMA_KeyboardInit();

	crash_logging_driver = UI_BASIC_driver;
	crash_logging_driver.fInit = crash_logging_init;
	UI_driver = &crash_logging_driver;

	if (!Sound_Initialise(argc, argv))
		return FALSE;

	return TRUE;
}

int PLATFORM_Exit(int run_monitor)
{
	Log_flushlog();

	if (run_monitor) {
		/* There is no monitor console unless one is wired to the serial
		   port; keep the emulation running. */
		if (host_option_int("serial_monitor", 0))
			return MONITOR_Run();
		return TRUE;
	}

	return FALSE;
}

static void bma_on_exit(void)
{
	/* atari800 has shut down and closed its files: start again. */
	host_reboot();
}

/* Directory holding atari800.cfg and the ROM folder. Only the PC test
   harness changes it. */
static const char *sd_root(void)
{
	return host_option("root", "/");
}

/* ---- For the BMA800XL menu ---------------------------------------------------- */

const char *BMA_SdRoot(void)
{
	return sd_root();
}

double BMA_RefreshRate(void)
{
	return refresh_rate;
}

/* The frame lock or border setting changed. */
void BMA_VideoSettingsChanged(void)
{
	update_sync_mode();
	restart_sound();
	read_border_setting();
	apply_view();
}

/* Builds atari800's command line from cmdline.txt options. */
static int build_args(char **argv, int max)
{
	static char argv0[FILENAME_MAX];
	int argc = 0;
	const char *value;

	/* atari800 keeps atari800.cfg, and looks for ROMs, in the directory of
	   argv[0]: the root of the SD card. */
	Util_catpath(argv0, sd_root(), "atari800");
	argv[argc++] = argv0;

	value = host_option("machine", NULL);
	if (value != NULL) {
		if (strcmp(value, "400") == 0 || strcmp(value, "800") == 0)
			argv[argc++] = "-atari";
		else if (strcmp(value, "1200xl") == 0)
			argv[argc++] = "-1200";
		else if (strcmp(value, "xe") == 0 || strcmp(value, "130xe") == 0)
			argv[argc++] = "-xe";
		else if (strcmp(value, "320xe") == 0)
			argv[argc++] = "-320xe";
		else if (strcmp(value, "xegs") == 0)
			argv[argc++] = "-xegs";
		else if (strcmp(value, "5200") == 0)
			argv[argc++] = "-5200";
		else
			argv[argc++] = "-xl";
	}

	value = host_option("tv", NULL);
	if (value != NULL)
		argv[argc++] = strcmp(value, "ntsc") == 0 ? "-ntsc" : "-pal";

	value = host_option("basic", NULL);
	if (value != NULL)
		argv[argc++] = strcmp(value, "1") == 0 || strcmp(value, "on") == 0
		               ? "-basic" : "-nobasic";

	/* A disk, program or cartridge to start: autostart=/games/foo.atr */
	value = host_option("autostart", NULL);
	if (value != NULL && *value != '\0' && argc < max) {
		argv[argc++] = strdup(value);
	}

	argv[argc] = NULL;
	return argc;
}

void bma800xl_main(void)
{
	static char *argv[16];
	int argc = build_args(argv, 14);
	int frames_since_sync = 0;

	atexit(bma_on_exit);

	if (!Atari800_Initialise(&argc, argv)) {
		host_log("atari800 failed to start; see the serial log");
		for (;;)
			host_sleep_us(1000000);
	}

	/* The file browser starts in the SD card's root unless the
	   configuration says otherwise. */
	if (UI_n_atari_files_dir == 0) {
		Util_strlcpy(UI_atari_files_dir[0], sd_root(), FILENAME_MAX);
		UI_n_atari_files_dir = 1;
	}
	if (UI_n_saved_files_dir == 0) {
		Util_strlcpy(UI_saved_files_dir[0], sd_root(), FILENAME_MAX);
		UI_n_saved_files_dir = 1;
	}

	for (;;) {
		INPUT_key_code = PLATFORM_Keyboard();

		/* F1, Home or Select+Start open the BMA800XL menu, which leads on
		   to atari800's. Alt+letter shortcuts go straight to atari800's. */
		if (INPUT_key_code == AKEY_UI && UI_alt_function < 0) {
			if (!BMA_Menu())
				INPUT_key_code = AKEY_NONE;
		}

		if (INPUT_key_code == AKEY_KEYB) {
			/* The on-screen keyboard (F11): pick one key to press. */
			Sound_Pause();
			UI_BASIC_in_kbui = TRUE;
			INPUT_key_code = UI_BASIC_OnScreenKeyboard(NULL, Atari800_machine_type);
			UI_BASIC_in_kbui = FALSE;
			switch (INPUT_key_code) {
			case AKEY_OPTION:
				INPUT_key_consol &= ~INPUT_CONSOL_OPTION;
				break;
			case AKEY_SELECT:
				INPUT_key_consol &= ~INPUT_CONSOL_SELECT;
				break;
			case AKEY_START:
				INPUT_key_consol &= ~INPUT_CONSOL_START;
				break;
			}
			/* Don't let the key that closed it reach the emulation. */
			while (PLATFORM_Keyboard() != AKEY_NONE)
				Atari800_Sync();
			Sound_Continue();
		}

		Atari800_Frame();
		if (Atari800_display_screen)
			PLATFORM_DisplayScreen();
		else
			host_yield();

		if (++frames_since_sync >= 50) {
			frames_since_sync = 0;
			sync_files();
			show_network_address();
		}
		handle_requests();
	}
}
