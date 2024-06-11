/**
 * switch control for USB port shared by Apollo and FPGA
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2023-2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "usb_switch.h"
#include "apollo_board.h"

#include <tusb.h>
#include <bsp/board_api.h>

enum {
	SWITCH_UNKNOWN = 0,
	SWITCH_MCU     = 1,
	SWITCH_FPGA    = 2,
};

#ifdef BOARD_HAS_SHARED_USB
static int switch_state = SWITCH_UNKNOWN;
#endif

/**
 * Hand off shared USB port to FPGA.
 */
void hand_off_usb(void)
{
#ifdef BOARD_HAS_SHARED_USB
	if (switch_state == SWITCH_FPGA) return;

	// Disable internal pull-up resistor on D+/D- pins for a moment to force a disconnection
	tud_disconnect();
	board_delay(100);

#ifdef BOARD_HAS_USB_SWITCH
	gpio_set_pin_level(USB_SWITCH, false);
	gpio_set_pin_direction(USB_SWITCH, GPIO_DIRECTION_OUT);
#else
	gpio_set_pin_pull_mode(PHY_RESET, GPIO_PULL_DOWN);
	gpio_set_pin_direction(PHY_RESET, GPIO_DIRECTION_IN);
#endif

	switch_state = SWITCH_FPGA;
#endif
}


/**
 * Take control of USB port from FPGA.
 */
void take_over_usb(void)
{
#ifdef BOARD_HAS_SHARED_USB
	if (switch_state == SWITCH_MCU) return;

#ifdef BOARD_HAS_USB_SWITCH
	gpio_set_pin_level(USB_SWITCH, true);
	gpio_set_pin_direction(USB_SWITCH, GPIO_DIRECTION_OUT);
#else
	gpio_set_pin_level(PHY_RESET, false);
	gpio_set_pin_direction(PHY_RESET, GPIO_DIRECTION_OUT);
#endif

	// Disable internal pull-up resistor on D+/D- pins for a moment to force a disconnection
	tud_disconnect();
	board_delay(100);
	tud_connect();

	switch_state = SWITCH_MCU;
#endif
}


/**
 * True if the USB switch handed over the port to the FPGA. 
 */
bool fpga_controls_usb_port(void)
{
#ifdef BOARD_HAS_SHARED_USB
	return switch_state == SWITCH_FPGA;
#else
	return false;
#endif
}
