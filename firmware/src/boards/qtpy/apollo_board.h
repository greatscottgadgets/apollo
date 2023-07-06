/**
 * Apollo board definitions for Adafruit QT py
 *
 * Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
 * Copyright (c) 2021 Matt Johnston <matt@ucc.asn.au>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __APOLLO_BOARD_H__
#define __APOLLO_BOARD_H__

#include <sam.h>
#include <hal/include/hal_gpio.h>
#include <stdbool.h>

/**
 * TODO: the qtpy board has a single RGB neopixel.
 */
typedef enum {
	LED_A,
	LED_B,
	LED_C,
	LED_D,
	LED_E,
} led_t;

/**
 * GPIO pin numbers.
 */

// NOTE: there is assocated pinmux config in uart.c and spi.c
// that must be kept in sync with these pins, and SERCOM selection.

enum {
	// Each of the JTAG pins. SERCOM2
	TMS_GPIO = PIN_PA02, // A0 qtpy board edge
	TDI_GPIO = PIN_PA10, // MOSI
	TDO_GPIO = PIN_PA09, // MISO
	TCK_GPIO = PIN_PA11, // SCK

	// Connected to orangecrab pins 0 and 1. SERCOM0
	UART_RX = PIN_PA04, // A2 qtpy
	UART_TX = PIN_PA06, // A6 qtpy

	// Connected to orangecrab RSTFPGA_RESET, ecp5 PROGRAMN
	PIN_PROG = PIN_PA03, // A1 qtpy
};

#endif
