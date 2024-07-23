/*
 * Board revision detection.
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "board_rev.h"

/**
 * Detect hardware revision using a board-specific method.
 */
__attribute__((weak)) void detect_hardware_revision(void)
{
}

/**
 * Returns the board revision in bcdDevice format.
 */
__attribute__((weak)) uint16_t get_board_revision(void)
{
    return (_BOARD_REVISION_MAJOR_ << 8) | _BOARD_REVISION_MINOR_;
}

/**
 * Return the manufacturer string.
 */
__attribute__((weak)) const char *get_manufacturer_string(void)
{
	return "Apollo Project";
}

/**
 * Return the product string.
 */
__attribute__((weak)) const char *get_product_string(void)
{
	return "Apollo Debugger";
}

/**
 * Return the raw ADC value.
 */
__attribute__((weak)) uint16_t get_adc_reading(void)
{
    return 0;
}
