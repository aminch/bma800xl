/*
 * modem.c - the R: device: an Atari 850 serial port with a modem on it
 * that dials TCP addresses, for calling BBSes over the network.
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
 * Takes the place of atari800's rdevice.c (not built): the same RDevice_*
 * entry points, which atari800's devices.c installs as the R: handler when
 * "R: device" is on in the Emulator Settings. The connection itself is the
 * Circle kernel's (host_modem_* in src/host.h).
 *
 * The modem understands Hayes commands, upper or lower case, ending with
 * Return:
 *   ATD host[:port] or ATDT/ATDP/ATDI host [port]  call (port 23 if none)
 *   ATH  hang up             ATO  back on line after +++
 *   ATZ  hang up and reset   ATE0/ATE1  echo off/on
 * Anything else is accepted with OK. A key pressed while dialling cancels
 * the call. +++ with a second's pause before and after returns to
 * commands without hanging up. Opening "R:host" for writing also calls
 * host, on the port in aux2 (23 if 0).
 *
 * Sources:
 *  - atari800 rdevice.c (Tom Hunt, Chris Martin, the Atari800 development
 *    team, GPL 2 or later): the handler's entry points and what they return;
 *    STATUS's DVSTAT bytes (746-749; 747 = 0xfc when not in concurrent
 *    mode); XIO 34 (DTR off hangs up), 38 (translation, line feeds) and 40
 *    (concurrent mode when aux1 >= 12); "R:host" in OPEN; ATDI.
 *  - Atari 850 Interface Module Operator's Manual: translation (XIO 38
 *    aux1 bits 4-5: light, heavy, none; bit 6 adds a line feed after each
 *    CR): light changes EOL to CR going out and CR to EOL coming in, and
 *    clears bit 7; heavy also drops incoming characters outside 32-124.
 *  - Hayes AT command set: the commands, result codes and the +++ escape
 *    with its one second guard time.
 *  - RFC 854 and RFC 855 (Telnet): IAC (255) commands, doubled IAC for a
 *    data byte of 255, option negotiation (WILL 251, WONT 252, DO 253,
 *    DONT 254, SB 250 ... IAC SE 240); RFC 856 (BINARY, 0), RFC 857 (ECHO,
 *    1), RFC 858 (SUPPRESS-GO-AHEAD, 3) are accepted, others refused;
 *    RFC 1143: replies only when an option changes.
 *  - atari800 esc.c and cpu.c: an escape code runs with CPU_regPC just past
 *    its two bytes; moving it back two runs it again, so READ waits for a
 *    character while the emulator keeps going. BRKKEY (17) is 0 after the
 *    Break key, which the OS's handlers report as error 128 and reset.
 */

#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "cpu.h"
#include "devices.h"
#include "log.h"
#include "memory.h"
#include "rdevice.h"

#include "../host.h"

#define DVSTAT 746
#define BRKKEY 17

#define EOL 0x9b
#define CR 0x0d
#define LF 0x0a

#define XLATE_LIGHT 0
#define XLATE_HEAVY 1
#define XLATE_NONE 2

#define IAC 255
#define DONT 254
#define DO 253
#define WONT 252
#define WILL 251
#define SB 250
#define SE 240

/* Seconds of quiet before and after +++. */
#define GUARD_TIME 1.0

/* What the Atari reads: modem messages and received characters,
   translated. */
#define IN_SIZE 2048
#define IN_RESERVE 64
static UBYTE in_buf[IN_SIZE];
static unsigned in_head, in_tail;

static int concurrent;
static int translation = XLATE_LIGHT;
static int append_lf;

static int echo = 1;
static int online;          /* characters go to the line, not the modem */
static int last_state = HOST_MODEM_IDLE;
static char command[80];
static unsigned command_len;
static int plus_count;
static double last_sent;

static int telnet_state;
static UBYTE telnet_verb;
static UBYTE telnet_us[256];   /* options we do */
static UBYTE telnet_him[256];  /* options the other end does */

/* ---- Characters to the Atari --------------------------------------------- */

static unsigned in_used(void)
{
	return in_head - in_tail;
}

static void in_put(UBYTE c)
{
	if (translation != XLATE_NONE) {
		c &= 0x7f;
		if (c == CR)
			c = EOL;
		else if (translation == XLATE_HEAVY && (c < 32 || c > 124))
			return;
	}
	if (in_used() < IN_SIZE) {
		in_buf[in_head % IN_SIZE] = c;
		in_head++;
	}
}

static void in_text(const char *text)
{
	while (*text != '\0')
		in_put((UBYTE) *text++);
}

static void result(const char *text)
{
	in_text("\r\n");
	in_text(text);
	in_text("\r\n");
}

/* ---- Telnet -------------------------------------------------------------- */

static void telnet_reset(void)
{
	telnet_state = 0;
	memset(telnet_us, 0, sizeof(telnet_us));
	memset(telnet_him, 0, sizeof(telnet_him));
}

static void telnet_reply(UBYTE verb, UBYTE option)
{
	UBYTE reply[3];
	reply[0] = IAC;
	reply[1] = verb;
	reply[2] = option;
	host_modem_send(reply, 3);
}

static void telnet_option(UBYTE verb, UBYTE option)
{
	switch (verb) {
	case WILL:
		if (option == 0 || option == 1 || option == 3) {
			if (!telnet_him[option]) {
				telnet_him[option] = 1;
				telnet_reply(DO, option);
			}
		}
		else
			telnet_reply(DONT, option);
		break;
	case WONT:
		if (telnet_him[option]) {
			telnet_him[option] = 0;
			telnet_reply(DONT, option);
		}
		break;
	case DO:
		if (option == 0 || option == 3) {
			if (!telnet_us[option]) {
				telnet_us[option] = 1;
				telnet_reply(WILL, option);
			}
		}
		else
			telnet_reply(WONT, option);
		break;
	case DONT:
		if (telnet_us[option]) {
			telnet_us[option] = 0;
			telnet_reply(WONT, option);
		}
		break;
	}
}

/* A byte from the line. */
static void telnet_receive(UBYTE c)
{
	switch (telnet_state) {
	case 0:  /* data */
		if (c == IAC)
			telnet_state = 1;
		else
			in_put(c);
		break;
	case 1:  /* after IAC */
		if (c == IAC) {
			in_put(c);
			telnet_state = 0;
		}
		else if (c >= WILL) {
			telnet_verb = c;
			telnet_state = 2;
		}
		else if (c == SB)
			telnet_state = 3;
		else
			telnet_state = 0;
		break;
	case 2:  /* after IAC WILL/WONT/DO/DONT */
		telnet_option(telnet_verb, c);
		telnet_state = 0;
		break;
	case 3:  /* in a subnegotiation */
		if (c == IAC)
			telnet_state = 4;
		break;
	case 4:  /* IAC in a subnegotiation */
		telnet_state = c == SE ? 0 : 3;
		break;
	}
}

/* Sends a byte to the line. Returns 0 if there is no room for it. */
static int telnet_send(UBYTE c)
{
	static const UBYTE iac_iac[2] = { IAC, IAC };
	if (c == IAC)
		return host_modem_send(iac_iac, 2) != 0;
	return host_modem_send(&c, 1) != 0;
}

/* ---- The modem ----------------------------------------------------------- */

static void dial(const char *host, unsigned port)
{
	if (port == 0 || port > 65535)
		port = 23;
	if (!host_modem_dial(host, port)) {
		Log_print("R: cannot dial %s: the network isn't running", host);
		result("NO CARRIER");
		return;
	}
	last_state = HOST_MODEM_DIALLING;
}

static void hang_up(void)
{
	host_modem_hang_up();
	last_state = HOST_MODEM_IDLE;
	online = FALSE;
}

/* ATD's dial string: [T|P|I] host, then :port or a space and port. */
static void dial_string(char *s)
{
	char *end;
	char *port;
	if (*s == 'T' || *s == 'P' || *s == 'I')
		s++;
	while (*s == ' ')
		s++;
	end = s + strlen(s);
	while (end > s && end[-1] == ' ')
		*--end = '\0';
	if (*s == '\0') {
		result("ERROR");
		return;
	}
	port = strchr(s, ':');
	if (port == NULL)
		port = strchr(s, ' ');
	if (port != NULL)
		*port++ = '\0';
	dial(s, port != NULL ? (unsigned) atoi(port) : 23);
}

static void run_command(void)
{
	char *s = command;
	unsigned i;

	for (i = 0; i < command_len; i++)
		command[i] = (char) toupper((unsigned char) command[i]);
	command[command_len] = '\0';
	command_len = 0;

	while (*s == ' ')
		s++;
	if (s[0] != 'A' || s[1] != 'T')
		return;
	s += 2;

	while (*s != '\0') {
		char c = *s++;
		switch (c) {
		case 'D':
			dial_string(s);
			return;
		case 'H':
			hang_up();
			break;
		case 'O':
			if (host_modem_state() != HOST_MODEM_CONNECTED) {
				result("NO CARRIER");
				return;
			}
			online = TRUE;
			plus_count = 0;
			result("CONNECT");
			return;
		case 'Z':
			hang_up();
			echo = 1;
			break;
		case 'E':
			echo = *s == '1';
			break;
		case 'A':
			result("ERROR");
			return;
		case '&':
		case '\\':
		case '%':
			/* &C1 and the like: the letter is not a command. */
			if (*s != '\0')
				s++;
			break;
		default:
			break;
		}
		/* A command's number or S register value. */
		while (isdigit((unsigned char) *s) || *s == '=' || *s == '?')
			s++;
	}
	result("OK");
}

/* A character the Atari sends while the modem takes commands. */
static void command_char(UBYTE c)
{
	if (c == LF)
		return;
	if (echo)
		in_put(c);
	if (c == CR || c == EOL) {
		run_command();
		return;
	}
	if (c == 0x08 || c == 0x7f || c == 0x7e) {
		if (command_len > 0)
			command_len--;
		return;
	}
	if (command_len < sizeof(command) - 1)
		command[command_len++] = (char) c;
}

/* A character from the Atari, after translation. Returns 0 if it couldn't
   be taken yet. */
static int out_char(UBYTE c)
{
	int state = host_modem_state();
	if (online && state == HOST_MODEM_CONNECTED) {
		double now = host_time();
		if (!telnet_send(c))
			return 0;
		if (c == '+' && (plus_count == 0 ? now - last_sent >= GUARD_TIME
		                                 : plus_count < 3 && now - last_sent < GUARD_TIME))
			plus_count++;
		else
			plus_count = 0;
		last_sent = now;
	}
	else if (state == HOST_MODEM_DIALLING)
		host_modem_hang_up();  /* reported as NO CARRIER */
	else
		command_char(c);
	return 1;
}

/* Brings the Atari's side up to date with the line. */
static void update(void)
{
	int state = host_modem_state();
	int drained = TRUE;

	for (;;) {
		UBYTE buf[256];
		unsigned used = in_used();
		unsigned room = used < IN_SIZE - IN_RESERVE ? IN_SIZE - IN_RESERVE - used : 0;
		unsigned i, n;
		if (room > sizeof(buf))
			room = sizeof(buf);
		if (room == 0) {
			drained = FALSE;
			break;
		}
		n = host_modem_receive(buf, room);
		if (n == 0)
			break;
		for (i = 0; i < n; i++)
			telnet_receive(buf[i]);
	}

	if (state == HOST_MODEM_CONNECTED && last_state != HOST_MODEM_CONNECTED) {
		telnet_reset();
		online = TRUE;
		plus_count = 0;
		last_sent = host_time();
		last_state = state;
		result("CONNECT");
	}
	else if (state == HOST_MODEM_IDLE && last_state != HOST_MODEM_IDLE && drained) {
		online = FALSE;
		last_state = state;
		result("NO CARRIER");
	}

	if (online && plus_count == 3 && host_time() - last_sent >= GUARD_TIME) {
		online = FALSE;
		plus_count = 0;
		result("OK");
	}
}

/* ---- The R: handler (atari800's devices.c calls these) ------------------- */

static void ok(void)
{
	CPU_regY = 1;
	CPU_ClrN;
}

/* OPEN "R:host" for writing calls host (from rdevice.c). */
void RDevice_OPEN(void)
{
	UWORD address;
	char host[128];
	unsigned n = 0;

	ok();
	concurrent = FALSE;
	if (!(MEMORY_dGetByte(Devices_ICAX1Z) & 0x08))
		return;
	address = Devices_SkipDeviceName();
	if (address == 0)
		return;
	while (n < sizeof(host) - 1) {
		char c = (char) MEMORY_dGetByte(address++);
		if (c < '!' || c > '\x7e')
			break;
		host[n++] = (char) tolower((unsigned char) c);
	}
	host[n] = '\0';
	if (n > 0)
		dial(host, MEMORY_dGetByte(Devices_ICAX2Z));
}

/* Closing leaves the call up. */
void RDevice_CLOS(void)
{
	ok();
	concurrent = FALSE;
}

void RDevice_READ(void)
{
	update();
	if (in_used() == 0) {
		if (MEMORY_dGetByte(BRKKEY) == 0) {
			MEMORY_dPutByte(BRKKEY, 0xff);
			CPU_regY = 128;
			CPU_SetN;
			return;
		}
		/* Nothing yet: run this again. */
		CPU_regPC -= 2;
		return;
	}
	CPU_regA = in_buf[in_tail % IN_SIZE];
	in_tail++;
	ok();
}

void RDevice_WRIT(void)
{
	UBYTE c = CPU_regA;

	update();
	if (translation != XLATE_NONE) {
		if (c == EOL) {
			if (!out_char(CR)) {
				CPU_regPC -= 2;
				return;
			}
			if (append_lf)
				out_char(LF);
			ok();
			return;
		}
		c &= 0x7f;
	}
	if (!out_char(c)) {
		CPU_regPC -= 2;
		return;
	}
	ok();
}

void RDevice_STAT(void)
{
	update();
	MEMORY_dPutByte(DVSTAT, 0);
	if (concurrent) {
		unsigned n = in_used();
		if (n > 0xffff)
			n = 0xffff;
		MEMORY_dPutByte(DVSTAT + 1, (UBYTE) n);
		MEMORY_dPutByte(DVSTAT + 2, (UBYTE) (n >> 8));
	}
	else {
		MEMORY_dPutByte(DVSTAT + 1, 0xfc);
		MEMORY_dPutByte(DVSTAT + 2, 0);
	}
	MEMORY_dPutByte(DVSTAT + 3, 0);
	CPU_regA = 1;
	ok();
}

void RDevice_SPEC(void)
{
	int aux1 = MEMORY_dGetByte(Devices_ICAX1Z);

	switch (MEMORY_dGetByte(Devices_ICCOMZ)) {
	case 34:  /* DTR, RTS, XMT */
		if ((aux1 & 0xc0) == 0x80 && host_modem_state() != HOST_MODEM_IDLE) {
			host_modem_hang_up();  /* reported as NO CARRIER */
			online = FALSE;
		}
		break;
	case 38:  /* translation and parity */
		translation = (aux1 & 0x20) ? XLATE_NONE
		            : (aux1 & 0x10) ? XLATE_HEAVY : XLATE_LIGHT;
		append_lf = (aux1 & 0x40) != 0;
		break;
	case 40:  /* concurrent mode */
		concurrent = aux1 >= 12;
		break;
	default:  /* 32 (force short block), 36 (baud rate): nothing to do */
		break;
	}
	CPU_regA = 1;
	ok();
}

void RDevice_INIT(void)
{
	CPU_regA = 1;
	ok();
}

void RDevice_Exit(void)
{
	host_modem_hang_up();
}
