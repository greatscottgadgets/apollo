/**
 * DFU Runtime Support
 *
 * This file provides support for automatically rebooting into the DFU bootloader.
 *
 * Copyright (c) 2023-2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * Copyright (c) 2024 Markus Blechschmidt <marble@computer-in.love>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/bootrom.h"
#include "tusb.h"

/**
 * Handler for DFU_DETACH events, which should cause us to reboot into the bootloader.
 */
void tud_dfu_runtime_reboot_to_dfu_cb(void)
{
	reset_usb_boot(0, 0);
	while(1);
}
