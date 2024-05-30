/**
 * Code for basic FPGA interfacing.
 *
 * Copyright (c) 2020-2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bsp/board_api.h>
#include <hal/include/hal_gpio.h>

#include "apollo_board.h"
#include "jtag.h"
#include "fpga.h"
#include "board_rev.h"


/*
 * Allows or disallows the FPGA from configuring. When disallowed,
 * initialization (erasing of configuration memory) takes place, but the FPGA
 * does not proceed to the configuration phase.
 */
void permit_fpga_configuration(bool enable)
{
    if (get_board_revision() < CYNTHION_REV_1_3) return;

    gpio_set_pin_level(FPGA_INITN, enable);
    gpio_set_pin_direction(FPGA_INITN, GPIO_DIRECTION_OUT);

    /*
    * Delay a bit (in case the FPGA is already initializing) because
    * TN-02039 says that PROGRAMN should not have a falling edge during
    * initialization.
    */
    board_delay(1);
}


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
	gpio_set_pin_direction(FPGA_PROGRAM, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(FPGA_PROGRAM, false);

	board_delay(1);

	gpio_set_pin_level(FPGA_PROGRAM, true);
	gpio_set_pin_direction(FPGA_PROGRAM, GPIO_DIRECTION_IN);

	// Update internal state.
	fpga_set_online(true);
}
