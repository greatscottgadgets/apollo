/**
 * Code for basic FPGA interfacing.
 *
 * This file is part of LUNA.
 *
 * Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bsp/board_api.h>
#include <apollo_board.h>

#include "apollo_board.h"
#include "jtag.h"
#include "fpga.h"


/*
 * Allows or disallows the FPGA from configuring. When disallowed,
 * initialization (erasing of configuration memory) takes place, but the FPGA
 * does not proceed to the configuration phase.
 */
void permit_fpga_configuration(bool enable)
{
}

/**
 * Sets up the I/O pins needed to configure the FPGA.
 */
void fpga_io_init(void)
{
}


/**
 * Requests that the FPGA clear its configuration and try to reconfigure.
 */
void trigger_fpga_reconfiguration(void)
{
    /*
	 * If the JTAG TAP was left in certain states, pulsing PROGRAMN has no
	 * effect, so we reset the state first.
	 */
	jtag_init();
	jtag_go_to_state(STATE_TEST_LOGIC_RESET);
	jtag_wait_time(2);
	jtag_deinit();
	/*
	 * Now pulse PROGRAMN to instruct the FPGA to configure itself.
	 */
	gpio_set_pin_direction(PIN_PROG, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(PIN_PROG, false);

	board_delay(1);

	gpio_set_pin_level(PIN_PROG, true);
	gpio_set_pin_direction(PIN_PROG, GPIO_DIRECTION_IN);

	// Update internal state.
	fpga_set_online(true);
}
