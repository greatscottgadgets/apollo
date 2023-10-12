/*
 * button handler
 *
 * Copyright (c) 2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <stdbool.h>


/**
 * Detect button press.
 */
bool button_pressed(void);


/**
 * Handle button events.
 */
void button_task(void);


#endif
