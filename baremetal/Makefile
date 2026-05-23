#
# Circle bare-metal libretro runner for Raspberry Pi 5.
#

CIRCLEHOME ?= ../circle
TOOLCHAIN ?= C:/Users/dennis/Documents/GitHub/MINERVA_closed/fantasy-pi/toolchain/aarch64-none-elf

AARCH ?= 64
RASPPI ?= 5
PREFIX64 ?= $(TOOLCHAIN)/bin/aarch64-none-elf-
STDLIB_SUPPORT ?= 1

ROM_PATH ?= GAME.GB

OBJS = \
	kernel/main.o \
	kernel/kernel.o \
	kernel/memory.o \
	libretro/libretro_runner.o \
	libretro/builtin_pattern_core.o \
	platform/circle/circle_log.o \
	platform/circle/circle_timer.o \
	platform/circle/circle_video.o \
	platform/circle/circle_audio.o \
	platform/circle/circle_input.o \
	platform/circle/circle_fs.o

LIBS = \
	$(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	$(CIRCLEHOME)/lib/sound/libsound.a \
	$(CIRCLEHOME)/lib/usb/libusb.a \
	$(CIRCLEHOME)/lib/input/libinput.a \
	$(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
	$(CIRCLEHOME)/lib/fs/libfs.a \
	$(CIRCLEHOME)/lib/libcircle.a

EXTRAINCLUDE += \
	-I . \
	-I kernel \
	-I libretro \
	-I platform/circle \
	-I ../src/libretro-common/include

CFLAGS += -DDEPTH=16 -DRA_BAREMETAL_ROM_PATH=\"$(ROM_PATH)\"

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
