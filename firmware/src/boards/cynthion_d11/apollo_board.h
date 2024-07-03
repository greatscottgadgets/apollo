/**
 * Apollo board definitions for Cynthion r0.3 and above
 *
 * Copyright (c) 2020-2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __APOLLO_BOARD_H__
#define __APOLLO_BOARD_H__

#include <sam.h>
#include <hal/include/hal_gpio.h>
#include <stdbool.h>

#define USB_VID 0x1d50
#define USB_PID 0x615c

#define BOARD_HAS_PROGRAM_BUTTON
#define BOARD_HAS_SHARED_USB

#if ((_BOARD_REVISION_MAJOR_ == 0) && (_BOARD_REVISION_MINOR_ < 6))
#define BOARD_HAS_SHARED_BUTTON
#else
#define BOARD_HAS_USB_SWITCH
#endif

/**
 * GPIO pins for each of the microcontroller LEDs.
 */
typedef enum {
	LED_A = PIN_PA16, // Blue
	LED_B = PIN_PA17, // Pink
	LED_C = PIN_PA22, // White
	LED_D = PIN_PA23, // Pink
	LED_E = PIN_PA27, // Blue

	LED_COUNT = 5
} led_t;


/**
 * GPIO pins for FPGA JTAG
 */
enum {
	// Each of the JTAG pins.
	TCK_GPIO = PIN_PA15,
	TDO_GPIO = PIN_PA10,
	TDI_GPIO = PIN_PA14,
	TMS_GPIO = PIN_PA11,
};


/**
 * Other GPIO pins
 */
enum {
	FPGA_PROGRAM   = PIN_PA08,
#ifdef BOARD_HAS_SHARED_BUTTON
	PROGRAM_BUTTON = PIN_PA16,
	PHY_RESET      = PIN_PA09,
#else
	PROGRAM_BUTTON = PIN_PA02,
	USB_SWITCH     = PIN_PA06,
	FPGA_ADV       = PIN_PA09,
#endif
	// FPGA sysCFG pins only in revs >= 1.3.
	FPGA_INITN     = PIN_PA03,
	FPGA_DONE      = PIN_PA04,
};


/**
 * Cynthion board revisions as bcdDevice values.
 */
enum {
    CYNTHION_REV_UNKNOWN = 0,
    CYNTHION_REV_0_6     = (0 << 8) | 6,
    CYNTHION_REV_0_7     = (0 << 8) | 7,
    CYNTHION_REV_1_0     = (1 << 8) | 0,
    CYNTHION_REV_1_1     = (1 << 8) | 1,
    CYNTHION_REV_1_2     = (1 << 8) | 2,
    CYNTHION_REV_1_3     = (1 << 8) | 3,
    CYNTHION_REV_1_4     = (1 << 8) | 4,
};


#endif
