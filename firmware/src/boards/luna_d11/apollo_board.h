/**
 * Apollo board definitions for SAMD11 Xplained hardware.
 *
 * This file is part of LUNA.
 *
 * Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __APOLLO_BOARD_H__
#define __APOLLO_BOARD_H__

#include <sam.h>
#include <hal/include/hal_gpio.h>
#include <stdbool.h>

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
 * GPIO pin numbers.
 */
enum {
	// Each of the JTAG pins.
	TCK_GPIO = PIN_PA15,
	TDO_GPIO = PIN_PA10,
	TDI_GPIO = PIN_PA14,
	TMS_GPIO = PIN_PA11,
};


/**
 * List of pins used for FPGA interfacing.
 */
enum {
	PROGRAM_GPIO = PIN_PA08
};


#endif
