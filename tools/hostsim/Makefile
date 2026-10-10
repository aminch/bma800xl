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
        $(OUT)/port/platform.o $(OUT)/port/keyboard.o $(OUT)/host_sim.o

all: $(OUT)/bma800xl_sim

$(OUT)/bma800xl_sim: $(OBJS)
	$(CC) -o $@ $(OBJS) -lm

$(OUT)/a8/%.o: $(A8_DIR)/%.c $(ROOT)/src/port/config.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/port/%.o: $(ROOT)/src/port/%.c $(ROOT)/src/port/config.h $(ROOT)/src/host.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -c -o $@ $<

$(OUT)/host_sim.o: host_sim.c $(ROOT)/src/host.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -c -o $@ $<

clean:
	rm -rf $(OUT)

.PHONY: all clean
