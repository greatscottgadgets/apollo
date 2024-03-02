/**
 * This file is part of LUNA.
 *
 * Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <tusb.h>
#include <apollo_board.h>
#include "spi.h"

#include <jtag.h>

extern uint8_t jtag_in_buffer[256];
extern uint8_t jtag_out_buffer[256];


/**
 * Hook that performs hardware-specific initialization.
 */
void jtag_platform_init(void)
{
	// Set up our SPI port for SPI-accelerated JTAG.
	spi_initialize(SPI_FPGA_JTAG, true, false, 1, 1, 1);
}


/**
 * Hook that performs hardware-specific deinitialization.
 */
void jtag_platform_deinit(void)
{
}
