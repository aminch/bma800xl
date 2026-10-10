/*
 * config.h - atari800 build configuration for BMA800XL (bare metal Pi).
 *
 * atari800 normally generates this file with autoconf. BMA800XL builds the
 * upstream sources unmodified with this hand-written replacement, so it
 * lists only what the bare metal port supports. When updating atari800,
 * compare against a freshly generated src/config.h (see docs/UPDATING.md).
 */

#ifndef BMA800XL_CONFIG_H_
#define BMA800XL_CONFIG_H_

/* Package */
#define PACKAGE "atari800"
#define PACKAGE_NAME "Atari800"
#define PACKAGE_TARNAME "atari800"
#define PACKAGE_BUGREPORT "pstehlik@sophics.cz"
#define PACKAGE_URL ""
#ifndef A8_VERSION
#define A8_VERSION "unknown"
#endif
#define PACKAGE_VERSION A8_VERSION
#define VERSION A8_VERSION
#define PACKAGE_STRING "Atari800 " A8_VERSION

/* Configuration file, relative to the root of the SD card. */
#define DEFAULT_CFG_NAME "atari800.cfg"

/* Target: bare metal Raspberry Pi (Circle + newlib) */
#define BMA800XL 1

/* Platform hooks implemented in src/port/platform.c */
#define SUPPORTS_PLATFORM_SLEEP 1
#define SUPPORTS_PLATFORM_TIME 1
#define SUPPORTS_PLATFORM_CONFIGINIT 1
#define SUPPORTS_PLATFORM_CONFIGURE 1
#define SUPPORTS_PLATFORM_CONFIGSAVE 1
#define SUPPORTS_PLATFORM_PALETTEUPDATE 1

/* Emulation features */
#define SOUND 1
#define STEREO_SOUND 1
#define CONSOLE_SOUND 1
#define INTERPOLATE_SOUND 1
#define NONLINEAR_MIXING 1
#define NEW_CYCLE_EXACT 1
#define CRASH_MENU 1
#define EMUOS_ALTIRRA 1
#define PBI_BB 1
#define PBI_MIO 1
#define USE_UI_BASIC_ONSCREEN_KEYBOARD 1

/* C library (newlib) */
#define STDC_HEADERS 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_TIME_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_DIRENT_H 1
#define HAVE_UINTPTR_T 1

#define HAVE_ATEXIT 1
#define HAVE_FDOPEN 1
#define HAVE_FFLUSH 1
#define HAVE_FLOOR 1
#define HAVE_FSTAT 1
#define HAVE_GETCWD 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_LOCALTIME 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMSET 1
#define HAVE_MKDIR 1
#define HAVE_MODF 1
#define HAVE_OPENDIR 1
#define HAVE_RENAME 1
#define HAVE_REWIND 1
#define HAVE_SNPRINTF 1
#define HAVE_STAT 1
#define HAVE_STRCASECMP 1
#define HAVE_STRCHR 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRNCPY 1
#define HAVE_STRRCHR 1
#define HAVE_STRSTR 1
#define HAVE_STRTOL 1
#define HAVE_TIME 1
#define HAVE_UNLINK 1
#define HAVE_VPRINTF 1
#define HAVE_VSNPRINTF 1

#endif /* BMA800XL_CONFIG_H_ */
