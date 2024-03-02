/**
 * button handler
 *
 * Copyright (c) 2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "button.h"
#include "usb_switch.h"
#include "fpga.h"
#include "apollo_board.h"


/**
 * Detect button press.
 */
bool button_pressed(void)
{
#ifdef BOARD_HAS_PROGRAM_BUTTON
	return (gpio_get_pin_level(PROGRAM_BUTTON) == false);
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
