/**
 * Code for basic FPGA interfacing.
 *
 * This file is part of LUNA.
 *
 * Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FPGA_H__
#define __FPGA_H__

/**
 * Sets up the I/O pins needed to configure the FPGA.
 */
void fpga_io_init(void);

/*
 * Allows or disallows the FPGA from configuring. When disallowed,
 * initialization (erasing of configuration memory) takes place, but the FPGA
 * does not proceed to the configuration phase.
 */
void permit_fpga_configuration(bool enable);

/**
 * Requests that the FPGA clear its configuration and try to reconfigure.
 */
void trigger_fpga_reconfiguration(void);

/**
 * Requests that we force the FPGA to be held in an unconfigured state.
 */
void force_fpga_offline(void);

#endif
