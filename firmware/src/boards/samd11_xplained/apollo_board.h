/**
 * Apollo board definitions for SAMD11 Xplained hardware.
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2020-2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __APOLLO_BOARD_H__
#define __APOLLO_BOARD_H__

#include <sam.h>
#include <hal/include/hal_gpio.h>
#include <stdbool.h>

/* pid.codes test VID/PID */
#define USB_VID 0x1209
#define USB_PID 0x0010


/**
 * GPIO pins for each of the microcontroller LEDs.
 */
typedef enum {
	LED_STATUS = PIN_PA16,

	LED_COUNT = 1
} led_t;



/**
 * Pin locations for the debug SPI connection.
 * Used when using bitbang mode for the debug SPI.
 */


enum {
	PIN_SCK      = PIN_PA10,

	PIN_SDI      = PIN_PA11,
	PIN_SDO      = PIN_PA22,

	PIN_FPGA_CS  = PIN_PA23,
};



/**
 * GPIO pin numbers.
 */
enum {
	// Each of the JTAG pins.
	TCK_GPIO = PIN_PA07,
	TDO_GPIO = PIN_PA04,
	TDI_GPIO = PIN_PA06,
	TMS_GPIO = PIN_PA05,

};

#endif
