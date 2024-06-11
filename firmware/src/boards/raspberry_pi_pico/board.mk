
# This is an external board, so its identity is determined by its revision number.
# MAJOR = external board
# MINOR = generic Apollo board
BOARD_REVISION_MAJOR := 255
BOARD_REVISION_MINOR := 3

CMAKE_DEFSYM +=	\
	-DBOARD_REVISION_MAJOR=$(BOARD_REVISION_MAJOR) \
	-DBOARD_REVISION_MINOR=$(BOARD_REVISION_MINOR) \
	-DVERSION_STRING="$(VERSION_STRING)"

ifeq "$(PICO_SDK_PATH)" ""
CMAKE_DEFSYM += -DPICO_SDK_FETCH_FROM_GIT=1
else
CMAKE_DEFSYM += -DPICO_SDK_PATH=$(PICO_SDK_PATH)
endif
