/**
 * FPGA advertisement pin handling code.
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdbool.h>

/**
 * Initialize FPGA_ADV receive-only serial port
 */
void fpga_adv_init(void)
{
}

/**
 * Task for things related with the advertisement pin
 */
void fpga_adv_task(void)
{
}

/**
 * Allow FPGA takeover of the USB port
 */
void allow_fpga_takeover_usb(void)
{
}

/**
 * True if we received an advertisement message within the last time window.
 */
bool fpga_requesting_port(void)
{
    return false;
}
