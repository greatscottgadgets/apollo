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
#include <hal/include/hal_gpio.h>


/**
 * Hand off shared USB port to FPGA.
 */
void hand_off_usb(void)
{
#ifdef BOARD_HAS_USB_SWITCH
	gpio_set_pin_level(USB_SWITCH, false);
	gpio_set_pin_direction(USB_SWITCH, GPIO_DIRECTION_OUT);
#endif
}


/**
 * Take control of USB port from FPGA.
 */
void take_over_usb(void)
{
#ifdef BOARD_HAS_USB_SWITCH
	gpio_set_pin_level(USB_SWITCH, true);
	gpio_set_pin_direction(USB_SWITCH, GPIO_DIRECTION_OUT);
#endif
}
