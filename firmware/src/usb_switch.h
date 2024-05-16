/*
 * switch control for USB port shared by Apollo and FPGA
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __USB_SWITCH_H__
#define __USB_SWITCH_H__

#include <stdbool.h>

/**
 * Hand off shared USB port to FPGA.
 */
void hand_off_usb(void);

/**
 * Take control of USB port from FPGA.
 */
void take_over_usb(void);

/**
 * True if the USB switch handed over the port to the FPGA. 
 */
bool fpga_controls_usb_port(void);
#endif
