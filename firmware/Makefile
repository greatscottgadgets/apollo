#
# Apollo debug controller firmware
#

VERSION_STRING ?= $(shell git describe --abbrev=7 --dirty --always --tags)

# Ensure that a APOLLO_BOARD is selected.
ifeq ($(APOLLO_BOARD), )
	BOARD:=$(error You need to specify an APOLLO_BOARD as a make variable (e.g. APOLLO_BOARD=cynthion)!)
endif

ifeq ($(APOLLO_BOARD), cynthion)
	BOARD := cynthion_d11
endif

# Allow Cynthion's old name "LUNA" for backward compatibility.
ifeq ($(APOLLO_BOARD), luna)
	BOARD := cynthion_d11
endif

ifeq ($(BOARD), cynthion_d11)
	# Enable revision autodetection by default if no revision is provided.
	ifeq ($(BOARD_REVISION_MAJOR),)
		ifeq ($(BOARD_REVISION_MINOR),)
			BOARD_REVISION_DETECT := 1
			BOARD_REVISION_MAJOR := 255
			BOARD_REVISION_MINOR := 255
		endif
	endif

	# On r0.1 or r0.2 boards, we target the SAMD21 configuration.
	ifeq ($(BOARD_REVISION_MAJOR), 0)
		ifeq ($(BOARD_REVISION_MINOR), 1)
			BOARD := cynthion_d21
		endif
		ifeq ($(BOARD_REVISION_MINOR), 2)
			BOARD := cynthion_d21
		endif
	endif
else
	BOARD := $(APOLLO_BOARD)
endif

# This should be set to 1 at this point if autodetection is enabled.
BOARD_REVISION_DETECT ?= 0

# Default to using a maximum possible HW version, which means "generic Apollo board".
BOARD_REVISION_MAJOR ?= 255
BOARD_REVISION_MINOR ?= 255

# Path to the TINYUSB library.
TINYUSB_PATH = ../lib/tinyusb

# Detect intercept-cc before we build.
ifeq ($(CC), intercept-cc)
	USE_INTERCEPT_CC := yes
endif

# Include the tinyusb basis BSP.
include $(TINYUSB_PATH)/examples/make.mk

# If we're using intercept-build, override our compiler back to intercept-cc.
ifeq ($(USE_INTERCEPT_CC), yes)
	CC := intercept-cc
endif

CFLAGS += \
	-Wno-unused-parameter \
	-Wno-cast-qual \
	-fstrict-volatile-bitfields \
	-D_BOARD_REVISION_DETECT_=$(BOARD_REVISION_DETECT) \
	-D_BOARD_REVISION_MAJOR_=$(BOARD_REVISION_MAJOR) \
	-D_BOARD_REVISION_MINOR_=$(BOARD_REVISION_MINOR) \
	-D VERSION_STRING=\"$(VERSION_STRING)\" \
	-g

INC += \
	src \
	src/boards/$(BOARD) \
	$(TOP)/hw \


# Include our per-board make definitions.
include src/boards/$(BOARD)/board.mk

# Build using all 'c' files in our source directories.
SOURCES += $(wildcard src/*.c src/boards/$(BOARD)/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(SOURCES))

# For now, take advantage of the example rules.
include $(TINYUSB_PATH)/examples/rules.mk

# Reset BUILD to fix clean target
BUILD := _build

# Flashing using Saturn-V.
dfu: _build/$(BOARD)/firmware.bin
	fwup-util --device 1d50:615c $< || fwup-util --device 1209:0010 $<


# Flashing using the Black Magic Probe,
BMP_SERIAL ?= /dev/ttyACM0
bmp: _build/$(BOARD)/firmware.elf
	arm-none-eabi-gdb -nx --batch \
	-ex 'target extended-remote $(BMP_SERIAL)' \
	-ex 'monitor swdp_scan' \
	-ex 'attach 1' \
	-ex 'load' \
	-ex 'kill' \
	$<


info:
	echo Building for $(BOARD).
