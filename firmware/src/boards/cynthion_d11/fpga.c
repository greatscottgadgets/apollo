/**
 * Code for basic FPGA interfacing.
 *
 * This file is part of LUNA.
 *
 * Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bsp/board_api.h>
#include <hal/include/hal_gpio.h>

#include "apollo_board.h"
#include "jtag.h"

/**
 * Sets up the I/O pins needed to configure the FPGA.
 */
void fpga_io_init(void)
{
	// By default, keep PROGRAM_N from being driven.
	gpio_set_pin_level(FPGA_PROGRAM, true);
	gpio_set_pin_direction(FPGA_PROGRAM, GPIO_DIRECTION_IN);
}


/**
 * Requests that the FPGA clear its configuration and try to reconfigure.
 */
void trigger_fpga_reconfiguration(void)
{
	jtag_init();
	jtag_go_to_state(STATE_TEST_LOGIC_RESET);
	jtag_wait_time(2);
	jtag_deinit();

	gpio_set_pin_direction(FPGA_PROGRAM, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(FPGA_PROGRAM, false);

	board_delay(1);

	gpio_set_pin_level(FPGA_PROGRAM, true);
	gpio_set_pin_direction(FPGA_PROGRAM, GPIO_DIRECTION_IN);
}


/**
 * Requests that we hold the FPGA in an unconfigured state.
 */
void force_fpga_offline(void)
{
	gpio_set_pin_direction(FPGA_PROGRAM, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(FPGA_PROGRAM, false);
}
