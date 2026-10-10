#
# atari800.mk - the atari800 sources BMA800XL builds.
#
# These come unmodified from the third_party/atari800 submodule. The list
# matches what atari800's own build compiles for a typical target, minus
# the host specific front ends (SDL, X11, curses, ...), the 80 column cards,
# NTSC/PAL display filters and recording features, which need a host video
# mode or libraries the bare metal port does not have.
#
# When updating atari800, check its src/Makefile.am for added or removed
# files (see docs/UPDATING.md).
#

A8_SRCS = \
	acidtest.c \
	afile.c \
	antic.c \
	artifact.c \
	atari.c \
	binload.c \
	cartridge.c \
	cartridge_info.c \
	cassette.c \
	cfg.c \
	colours.c \
	colours_external.c \
	colours_ntsc.c \
	colours_pal.c \
	compfile.c \
	cpu.c \
	crc32.c \
	cycle_map.c \
	devices.c \
	esc.c \
	gtia.c \
	img_tape.c \
	input.c \
	log.c \
	memory.c \
	monitor.c \
	mzpokeysnd.c \
	pbi.c \
	pbi_bb.c \
	pbi_mio.c \
	pbi_scsi.c \
	pia.c \
	pokey.c \
	pokeysnd.c \
	remez.c \
	rtime.c \
	screen.c \
	sio.c \
	sound.c \
	statesav.c \
	sysrom.c \
	ui.c \
	ui_basic.c \
	util.c \
	roms/altirra_5200_charset.c \
	roms/altirra_5200_os.c \
	roms/altirra_basic.c \
	roms/altirraos_800.c \
	roms/altirraos_xl.c
