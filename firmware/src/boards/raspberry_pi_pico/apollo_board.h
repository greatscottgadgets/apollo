/**
 * Apollo board definitions for Adafruit QT Py RP2040
 *
 * This file is part of LUNA.
 *
 * Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
 * Copyright (c) 2022 Markus Blechschmidt <marble@hamburg.ccc.de>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __APOLLO_BOARD_H__
#define __APOLLO_BOARD_H__

#include <stdbool.h>

#include "boards/pico.h"
#include "bsp/rp2040/board.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"


#define __NOP() {asm volatile("nop");}


typedef unsigned int gpio_t;


typedef enum gpio_direction{
	GPIO_DIRECTION_IN = GPIO_IN,
	GPIO_DIRECTION_OUT = GPIO_OUT,
} gpio_direction_t;


typedef enum gpio_pull_mode {
	GPIO_PULL_OFF,
	GPIO_PULL_UP,
	GPIO_PULL_DOWN,
} gpio_pull_mode_t;


/**
 * GPIO pins for each of the microcontroller LEDs.
 */
typedef enum {
	LED_A = LED_PIN, // Green

	LED_COUNT = 1,
} led_t;

/**
 * GPIO pin numbers.
 */



enum {
	// // Each of the JTAG pins. SPI0
	TMS_GPIO = 5,
	TDI_GPIO = 3, // MOSI
	TDO_GPIO = 4, // MISO
	TCK_GPIO = 6, // SCK

	// // Connected to orangecrab pins 0 and 1. SERCOM0
	UART_RX = UART_RX_PIN,
	UART_TX = UART_TX_PIN,
};


static inline void gpio_set_pin_level(const gpio_t gpio_pin, bool level) {
	gpio_put(gpio_pin, level);
}


static inline bool gpio_get_pin_level(const gpio_t gpio_pin) {
	return gpio_get(gpio_pin);
}


static inline void gpio_toggle_pin_level(const gpio_t gpio_pin) {
	gpio_set_pin_level(gpio_pin, !gpio_get_pin_level(gpio_pin));
}


static inline void gpio_set_pin_direction(const gpio_t gpio_pin, const enum gpio_direction direction) {
	gpio_init(gpio_pin);
	gpio_set_dir(gpio_pin, direction);
}


static inline void gpio_set_pin_pull_mode(const gpio_t gpio_pin, const gpio_pull_mode_t pull_mode) {
	switch(pull_mode) {
		case GPIO_PULL_OFF: {
			gpio_disable_pulls(gpio_pin);
		} break;
		case GPIO_PULL_UP: {
			gpio_pull_up(gpio_pin);
		} break;
		case GPIO_PULL_DOWN: {
			gpio_pull_down(gpio_pin);
		} break;
		default: {

		} break;
	}
}


#endif
