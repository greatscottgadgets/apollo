/**
 * button handler
 *
 * Copyright (c) 2023-2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "button.h"
#include "usb_switch.h"
#include "fpga.h"
#include "apollo_board.h"
#include <hal/include/hal_gpio.h>

static inline void delay(int cycles)
{
        while (cycles-- != 0)
                __NOP();
}


/**
 * Detect button press.
 */
bool button_pressed(void)
{
#ifdef BOARD_HAS_PROGRAM_BUTTON

#ifdef BOARD_HAS_SHARED_BUTTON
	bool level = gpio_get_pin_level(PROGRAM_BUTTON);
	gpio_set_pin_direction(PROGRAM_BUTTON, GPIO_DIRECTION_IN);
	gpio_set_pin_pull_mode(PROGRAM_BUTTON, GPIO_PULL_UP);
	delay(50);
	bool pressed = (gpio_get_pin_level(PROGRAM_BUTTON) == false);
	gpio_set_pin_direction(PROGRAM_BUTTON, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(PROGRAM_BUTTON, level);
	return pressed;
#else
	return (gpio_get_pin_level(PROGRAM_BUTTON) == false);
#endif

#else
	return false;
#endif
}


/**
 * Handle button events.
 */
void button_task(void)
{
	if (button_pressed()) {
		force_fpga_offline();
		take_over_usb();
	}
}
