#
# atari800.mk - the atari800 sources BMA800XL builds.
#
# These come from the third_party/atari800 submodule. The list
# matches what atari800's own build compiles for a typical target, minus
# the host specific front ends (SDL, X11, curses, ...), the 80 column cards,
# NTSC/PAL display filters and recording features, which need a host video
# mode or libraries the bare metal port does not have.
#
# When updating atari800, check its src/Makefile.am for added or removed
# files (see docs/UPDATING.md).
#
# The submodule itself is never modified. The few changes BMA800XL needs
# are patches, src/patches/atari800_*.diff (made with git format-patch in
# the atari800 repository, so they can be sent upstream). The files they
# change are copied into $(A8_PATCHED_DIR) and patched there, and are
# compiled from there instead of from $(A8_DIR). The including Makefile sets
# ROOT, A8_DIR and A8_PATCHED_DIR.
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

A8_PATCHES := $(sort $(wildcard $(ROOT)/src/patches/atari800_*.diff))
# The files the patches change, relative to atari800's src/
A8_PATCHED := $(sort $(shell sed -n 's|^+++ b/src/||p' /dev/null $(A8_PATCHES)))
A8_PATCHED_STAMP := $(A8_PATCHED_DIR)/.patched

# The stamp lists the patches applied; when that changes (a patch added or
# removed) everything is patched and compiled again.
A8_PATCH_LIST := $(notdir $(A8_PATCHES))
ifneq ($(strip $(A8_PATCH_LIST)),$(strip $(shell cat $(A8_PATCHED_STAMP) 2>/dev/null)))
.PHONY: $(A8_PATCHED_STAMP)
endif

# (this file must not change the including Makefile's default goal)
A8_SAVED_GOAL := $(.DEFAULT_GOAL)
$(A8_PATCHED_STAMP): $(A8_PATCHES) $(addprefix $(A8_DIR)/,$(A8_PATCHED))
	@rm -rf $(A8_PATCHED_DIR)
	@mkdir -p $(A8_PATCHED_DIR)
	@$(if $(A8_PATCHED),cd $(A8_DIR) && cp --parents $(A8_PATCHED) $(abspath $(A8_PATCHED_DIR)))
	@for p in $(A8_PATCHES); do \
		echo "  PATCH $$(basename $$p)"; \
		patch -s -p2 -d $(A8_PATCHED_DIR) < $$p || exit 1; \
	done
	@echo "$(A8_PATCH_LIST)" > $@
.DEFAULT_GOAL := $(A8_SAVED_GOAL)
