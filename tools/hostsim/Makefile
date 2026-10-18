#
# Builds the PC test harness for the emulator port (see host_sim.c).
#
#   make -C tools/hostsim
#   tools/hostsim/run_tests.sh
#

ROOT := ../..
A8_DIR := $(ROOT)/third_party/atari800/src
include $(ROOT)/src/port/atari800.mk

A8_VERSION := $(shell grep -m1 '^AC_INIT' $(ROOT)/third_party/atari800/configure.ac | cut -d, -f2 | tr -d ' []')

CC ?= cc
CFLAGS = -O2 -g -I $(ROOT)/src/port -I $(A8_DIR) -DA8_VERSION=\"$(A8_VERSION)\" -w

OUT := build
OBJS := $(addprefix $(OUT)/a8/,$(A8_SRCS:.c=.o)) \
        $(OUT)/port/platform.o $(OUT)/port/keyboard.o $(OUT)/port/gamepad.o \
        $(OUT)/port/menu.o $(OUT)/port/menu_controllers.o $(OUT)/port/modem.o \
        $(OUT)/gamepad_profiles.o $(OUT)/host_sim.o

all: $(OUT)/bma800xl_sim

$(OUT)/mouse_test: mouse_test.c $(A8_DIR)/input.c $(ROOT)/src/port/config.h
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -Wall -ffunction-sections -fdata-sections -o $@ $< -Wl,--gc-sections

test-mouse: $(OUT)/mouse_test
	./$(OUT)/mouse_test

GAMEPAD_HEADERS := $(ROOT)/src/port/gamepad.h $(ROOT)/src/gamepad_profiles.h $(ROOT)/src/usb_hid_keys.h

$(OUT)/gamepad_test: gamepad_test.c $(ROOT)/src/port/keyboard.c $(ROOT)/src/port/gamepad.c $(ROOT)/src/gamepad_profiles.c $(ROOT)/src/port/bma800xl.h $(ROOT)/src/host.h $(GAMEPAD_HEADERS)
	@mkdir -p $(OUT)
	$(CC) $(filter-out -w,$(CFLAGS)) -Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections -o $@ $< $(ROOT)/src/gamepad_profiles.c -Wl,--gc-sections

test-gamepad: $(OUT)/gamepad_test
	./$(OUT)/gamepad_test

$(OUT)/bma800xl_sim: $(OBJS)
	$(CC) -o $@ $(OBJS) -lm

$(OUT)/a8/%.o: $(A8_DIR)/%.c $(ROOT)/src/port/config.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/port/%.o: $(ROOT)/src/port/%.c $(ROOT)/src/port/config.h $(ROOT)/src/port/bma800xl.h $(ROOT)/src/host.h $(GAMEPAD_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -c -o $@ $<

$(OUT)/port/menu.o $(OUT)/port/menu_controllers.o: $(ROOT)/src/port/menu.h

$(OUT)/gamepad_profiles.o: $(ROOT)/src/gamepad_profiles.c $(GAMEPAD_HEADERS)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -Wall -c -o $@ $<

$(OUT)/host_sim.o: host_sim.c $(ROOT)/src/host.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -c -o $@ $<

clean:
	rm -rf $(OUT)

.PHONY: all clean test-mouse test-gamepad
