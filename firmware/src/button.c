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
#include "fpga_adv.h"
#include <bsp/board_api.h>
#include <hal/include/hal_gpio.h>

/**
 * Detect button press.
 */
bool button_pressed(void)
{
#ifdef BOARD_HAS_PROGRAM_BUTTON
	const unsigned int BUTTON_TIME_WINDOW = 200; //ms
	static uint32_t last_pressed = 0;
	static bool prev_btn_read;

	bool curr_btn_read = (gpio_get_pin_level(PROGRAM_BUTTON) == false);

	// Only detect falling edges.
	bool pressed = (prev_btn_read && !curr_btn_read);

	// Avoid additional presses within a time window.
	if (pressed) {
		pressed = (board_millis() - last_pressed >= BUTTON_TIME_WINDOW);
		last_pressed = board_millis();
	}

	prev_btn_read = curr_btn_read;
	return pressed;
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
		if (fpga_is_online()) {
			// Force the FPGA offline and take control of the USB port.
			force_fpga_offline();
			take_over_usb();
		} else {
			// Reconfigure FPGA from flash and permit hand off the USB port to the FPGA.
			permit_fpga_configuration(true);
			trigger_fpga_reconfiguration();
			honor_fpga_adv();
		}
	}
}
