/**
 * switch control for USB port shared by Apollo and FPGA
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "usb_switch.h"
#include "apollo_board.h"
#include "led.h"
#include <hal/include/hal_gpio.h>

/**
 * Hand off shared USB port to FPGA.
 */
void hand_off_usb(void)
{
#if _BOARD_REVISION_MAJOR_ == 1
	gpio_set_pin_level(USB_SWITCH, false);
	gpio_set_pin_direction(USB_SWITCH, GPIO_DIRECTION_OUT);
	led_off(LED_D);
#else
	led_on(LED_D);
#endif
}

/**
 * Take control of USB port from FPGA.
 */
void take_over_usb(void)
{
#if _BOARD_REVISION_MAJOR_ == 1
	gpio_set_pin_level(USB_SWITCH, true);
	gpio_set_pin_direction(USB_SWITCH, GPIO_DIRECTION_OUT);
#endif
	led_on(LED_D);
}

/**
 * Handle switch control user request.
 */
void switch_control_task(void)
{
	gpio_set_pin_direction(PROGRAM_BUTTON, GPIO_DIRECTION_IN);
	if (gpio_get_pin_level(PROGRAM_BUTTON) == false) {
		take_over_usb();
	}
}
