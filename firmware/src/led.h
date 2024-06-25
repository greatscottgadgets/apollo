/*
 * LED control abstraciton code.
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2019-2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LED_H__
#define __LED_H__

#include <apollo_board.h>

/**
 * LED patterns.
 *
 * Values 0 to 31 will be interpreted as static bitmasks, and can be used
 * to turn on specific combinations of LEDs in a fixed pattern.
 *
 * Other values as defined in this enum will produce dynamic blink patterns,
 * with different semantic meanings.
 */
typedef enum {
  LED_IDLE = 500,
  LED_JTAG_CONNECTED = 150,
  LED_JTAG_UPLOADING = 50,

  LED_FLASH_CONNECTED = 130,
} led_pattern_t;



/**
 * Sets the active LED pattern.
 *
 * See @ref led_pattern_t for the meaning of the pattern argument.
 */
void led_set_pattern(led_pattern_t pattern);


/**
 * Sets up each of the LEDs for use.
 */
void led_init(void);


/**
 * Turns the provided LED on.
 */
void led_on(led_t led);


/**
 * Turns the provided LED off.
 */
void led_off(led_t led);


/**
 * Turns off all of the device's LEDs.
 */
void leds_off(void);


/**
 * Toggles the provided LED.
 */
void led_toggle(led_t led);


/**
 * Sets whether a given led is on.
 */
void led_set(led_t led, bool on);


/**
 * Task that handles LED updates.
 */
void led_task(void);

#endif
