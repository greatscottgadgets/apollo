/*
 * Copyright (c) 2019-2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * Copyright (c) 2019 Katherine J. Temkin <kate@ktemkin.com>
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <tusb.h>
#include <bsp/board_api.h>
#include <apollo_board.h>

#include "led.h"
#include "jtag.h"
#include "fpga.h"
#include "console.h"
#include "debug_spi.h"
#include "usb_switch.h"
#include "button.h"



/**
 * Main round-robin 'scheduler' for the execution tasks.
 */
int main(void)
{
	board_init();
	tusb_init();

	fpga_io_init();
	led_init();
	debug_spi_init();

	if (button_pressed()) {
		/*
		 * Interrupted start-up: Force the FPGA offline and take
		 * control of the USB port.
		 */
                force_fpga_offline();
                take_over_usb();

		/*
		 * Now that the FPGA is being held offline, release the
		 * mechanism that prevented the FPGA from configuring itself at
		 * startup.
		 */
		permit_fpga_configuration(true);
	} else {
		/*
		 * Normal start-up: Reconfigure FPGA from flash and hand off
		 * the USB port to the FPGA. This effectively makes the RESET
		 * button reset both the microcontroller and the FPGA.
		 */
		permit_fpga_configuration(true);
		trigger_fpga_reconfiguration();
		hand_off_usb();
	}


	while (1) {
		tud_task(); // tinyusb device task
		console_task();
		heartbeat_task();
		button_task();
	}

	return 0;
}
