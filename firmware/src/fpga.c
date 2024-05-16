/**
 * Code for basic FPGA interfacing.
 *
 * Copyright (c) 2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bsp/board_api.h>

#include "jtag.h"
#include "fpga.h"
#include "fpga_adv.h"


extern uint8_t jtag_out_buffer[256];

bool fpga_online = false;

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

	// Update internal state.
	fpga_set_online(false);
}

/*
 * True after FPGA reconfiguration, false after forcing FPGA offline.
 */
bool fpga_is_online(void)
{
	return fpga_online;
}

/**
 * Update our understanding of the FPGA's state.
 */
void fpga_set_online(bool online)
{
	fpga_online = online;

	/*
	 * When the FPGA goes offline, stop allowing the FPGA to take over the
	 * shared USB port. After the FPGA comes back online, the host may
	 * request that it be allowed again, but we assume that it is
	 * disallowed until told otherwise.
	 */
	if (!online) {
		allow_fpga_takeover_usb(false);
	}
}
