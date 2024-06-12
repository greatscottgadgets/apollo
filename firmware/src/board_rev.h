/*
 * Board revision detection.
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>

#ifndef __BOARD_REV_H__
#define __BOARD_REV_H__

/**
 * Detect hardware revision using a board-specific method.
 */
void detect_hardware_revision(void);

/**
 * Returns the board revision in bcdDevice format.
 */
uint16_t get_board_revision(void);


#endif
