/*
 * LED control abstraciton code.
 *
 * This file is part of LUNA.
 *
 * Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <tusb.h>
#include <bsp/board_api.h>


#include "led.h"


/** Store the current LED blink pattern. */
static led_pattern_t led_pattern = LED_IDLE;


/**
 * Sets the active LED pattern.
 */
void led_set_pattern(led_pattern_t pattern)
{
	led_pattern = pattern;
	leds_off();
}


/**
 * Sets up each of the LEDs for use.
 */
void led_init(void)
{
	uint8_t pins[] = { LED_A, };

	// Default each LED to an output and _off_.
	for (unsigned i = 0; i < LED_COUNT; ++i) {
		gpio_set_pin_direction(pins[i], GPIO_DIRECTION_OUT);
		gpio_set_pin_level(pins[i], true);
	}
}


/**
 * Turns the provided LED on.
 */
void led_on(led_t led)
{
	gpio_set_pin_level(led, false);
}


/**
 * Turns the provided LED off.
 */
void led_off(led_t led)
{
	gpio_set_pin_level(led, true);
}


/**
 * Toggles the provided LED.
 */
void led_toggle(led_t led)
{
	gpio_toggle_pin_level(led);
}


/**
 * Sets whether a given led is on.
 */
void led_set(led_t led, bool on)
{
	gpio_set_pin_level(led, !on);
}


/**
 * Turns off all of the device's LEDs.
 */
void leds_off(void)
{
  led_t leds[] = {LED_A};

  for (unsigned i = 0; i < LED_COUNT; ++i) {
    led_off(leds[i]);
  }
}


/**
 * Turns on the given LED.
 */
static void display_led_number(uint8_t number)
{
  led_t leds[] = {LED_A};

  if (number < LED_COUNT) {
    led_on(leds[number]);
  }
}


/**
 * Task that handles LED updates.
 */
void led_task(void)
{
	static uint32_t start_ms = 0;

	// Blink every interval ms
	if ( board_millis() - start_ms < led_pattern) {
		return; // not enough time
	}

	start_ms += led_pattern;
	led_toggle(LED_A);
}
