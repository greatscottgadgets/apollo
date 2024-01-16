/**
 * FPGA advertisement pin handling code.
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FPGA_ADV_H__
#define __FPGA_ADV_H__

/**
 * Initialize FPGA_ADV receive-only serial port
 */
void fpga_adv_init(void);

/**
 * Task for things related with the advertisement pin
 */
void fpga_adv_task(void);

/**
 * Honor requests from FPGA_ADV again
 */
void honor_fpga_adv(void);


#endif
