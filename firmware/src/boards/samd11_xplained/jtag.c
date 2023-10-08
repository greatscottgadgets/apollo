/**
 * Copyright (c) 2020-2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <tusb.h>
#include <apollo_board.h>
#include "spi.h"

#include <jtag.h>


/**
 * Hook that performs hardware-specific initialization.
 */
void jtag_platform_init(void)
{
	// Ensure the TDO GPIO is continuously sampled, rather
	// than sampled on-demand. This allows us to significantly
	// speak up TDO reads.
	PORT->Group[0].CTRL.reg = (1 << TDO_GPIO);

	// Set up our SPI port for SPI-accelerated JTAG.
	spi_init(SPI_FPGA_JTAG, true, false, 1, 1, 1);
}


/**
 * Hook that performs hardware-specific deinitialization.
 */
void jtag_platform_deinit(void)
{
}
