/**
 * Code for basic FPGA interfacing.
 *
 * Copyright (c) 2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bsp/board_api.h>

#include "jtag.h"


extern uint8_t jtag_out_buffer[256];


/*
 * ECP5 opcode that enables offline configuration mode
 */
#define ISC_ENABLE 0xC6

/**
 * Requests that we hold the FPGA in an unconfigured state.
 */
void force_fpga_offline(void)
{
	jtag_init();
	jtag_go_to_state(STATE_TEST_LOGIC_RESET);
	jtag_go_to_state(STATE_SHIFT_IR);
	jtag_out_buffer[0] = ISC_ENABLE;
	jtag_scan(8, true, false);
	jtag_go_to_state(STATE_PAUSE_IR);
	jtag_go_to_state(STATE_SHIFT_DR);
	jtag_out_buffer[0] = 0;
	jtag_scan(8, true, false);
	jtag_go_to_state(STATE_PAUSE_DR);
	jtag_go_to_state(STATE_RUN_TEST_IDLE);
	jtag_wait_time(2);
	jtag_deinit();
}
