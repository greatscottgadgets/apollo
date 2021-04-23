#
# Apollo debug controller firmware
#

# Ensure that a APOLLO_BOARD is selected.
ifeq ($(APOLLO_BOARD), )
	BOARD:=$(error You need to specify an APOLLO_BOARD as a make variable (e.g. APOLLO_BOARD=luna)!)
endif

# If the board is specified as 'luna' without a processor, we'll try to emulate the behavior
# of LUNA's Apollo as much as we can.
ifeq ($(APOLLO_BOARD), luna)
	BOARD := luna_d11

	# These should default to the latest revision; but can be set on the command line.
	BOARD_REVISION_MAJOR ?= 0
	BOARD_REVISION_MINOR ?= 3

	# On r0.1 or r0.2 boards, we want to target the SAMD21 / luna_d11 configuration.
	ifeq ($(BOARD_REVISION_MAJOR), 0)
		ifeq ($(BOARD_REVISION_MINOR), 1)
			BOARD := luna_d21
		endif
		ifeq ($(BOARD_REVISION_MINOR), 2)
			BOARD := luna_d21
		endif
	endif
else
	BOARD := $(APOLLO_BOARD)
endif


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
include $(TINYUSB_PATH)/tools/top.mk
include $(TINYUSB_PATH)/examples/make.mk

# If we're using intercept-build, override our compiler back to intercept-cc.
ifeq ($(USE_INTERCEPT_CC), yes)
	CC := intercept-cc
endif

CFLAGS += \
	-Wno-unused-parameter \
	-fstrict-volatile-bitfields \
	-D_BOARD_REVISION_MAJOR_=$(BOARD_REVISION_MAJOR) \
	-D_BOARD_REVISION_MINOR_=$(BOARD_REVISION_MINOR) \
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
dfu: _build/$(BOARD)/$(BOARD)-firmware.bin
	dfu-util -a 0 -d 1d50:615c -D $< || dfu-util -a 0 -d 16d0:05a5 -D $<


# Flashing using the Black Magic Probe,
BMP_SERIAL ?= /dev/ttyACM0
bmp: _build/$(BOARD)/$(BOARD)-firmware.elf
	arm-none-eabi-gdb -nx --batch \
	-ex 'target extended-remote $(BMP_SERIAL)' \
	-ex 'monitor swdp_scan' \
	-ex 'attach 1' \
	-ex 'load' \
	-ex 'kill' \
	$<


info:
	echo Building for $(BOARD).
