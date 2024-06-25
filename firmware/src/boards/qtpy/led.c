/*
 * LED control abstraciton code.
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2020-2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <tusb.h>
#include <sam.h>
#include <bsp/board_api.h>
#include <hal/include/hal_gpio.h>


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
	// uint8_t pins[] = { LED_A, LED_B, LED_C, LED_D, LED_E };

	// Default each LED to an output and _off_.
}


/**
 * Turns the provided LED on.
 */
void led_on(led_t led)
{
}


/**
 * Turns the provided LED off.
 */
void led_off(led_t led)
{
}


/**
 * Toggles the provided LED.
 */
void led_toggle(led_t led)
{
}


/**
 * Sets whether a given led is on.
 */
void led_set(led_t led, bool on)
{
}


/**
 * Turns off all of the device's LEDs.
 */
void leds_off(void)
{
  led_t leds[] = {LED_A, LED_B, LED_C, LED_D, LED_E};

  for (unsigned i = 0; i < 5; ++i) {
    led_off(leds[i]);
  }
}


/**
 * Turns on the given LED.
 */
static void display_led_number(uint8_t number)
{
  led_t leds[] = {LED_A, LED_B, LED_C, LED_D, LED_E};

  if (number < 5) {
    led_on(leds[number]);
  }
}


/**
 * Task that handles LED updates.
 */
void led_task(void)
{
  static uint32_t start_ms = 0;
  static uint8_t active_led = 0;
  static bool count_up = true;

  // Blink every interval ms
  if ( board_millis() - start_ms < led_pattern) return; // not enough time
  start_ms += led_pattern;

  switch (led_pattern) {

    // Standard blink pattern for when the device is idle.
    // Indicates that the device's JTAG lines are un-pulled.
    case LED_IDLE:
      led_toggle(LED_E);
      break;

    // Blink patterns for when the device is being used for JTAG
    // operation. When these are on, the uC is driving the JTAG lines,
    // so the JTAG header probably shouldn't used to drive the lines.
    case LED_JTAG_CONNECTED:
    case LED_JTAG_UPLOADING:

      // Sweep back and forth.
      if (active_led == 0xFF) {
        count_up = true;
      }
      if (active_led == 4) {
        count_up = false;
      }
      active_led = count_up ? active_led + 1  : active_led - 1;

      leds_off();
      display_led_number(active_led);
      display_led_number(active_led + 1);

      break;

    // Blink patterns for when the device is being used for SPI flash access.
    // When these are displayed,
    case LED_FLASH_CONNECTED:

      if (active_led == 5) {
        active_led = 0;
      }

      leds_off();
      display_led_number(active_led++);

      break;

  }
}
