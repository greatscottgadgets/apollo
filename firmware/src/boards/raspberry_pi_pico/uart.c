/**
 * UART driver code.
 *
 * This file is part of LUNA.
 *
 * Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "hardware/uart.h"
#include "hardware/irq.h"

#include "apollo_board.h"


// Create a quick reference to our uart object.
static uart_inst_t *uart_inst = PICO_DEFAULT_UART;

// Keep track of whether our UART has been configured and is active.
bool uart_active = false;

// Declare interrupt handler
static void on_uart_rx();


/**
 * Pinmux the relevent pins so the can be used for SERCOM UART.
 */
static void _uart_configure_pinmux(bool use_for_uart)
{
	if (use_for_uart) {
		gpio_set_function(UART_TX, GPIO_FUNC_UART);
		gpio_set_function(UART_RX, GPIO_FUNC_UART);
	} else {
		gpio_set_function(UART_TX, GPIO_FUNC_NULL);
		gpio_set_function(UART_RX, GPIO_FUNC_NULL);
	}
}


/**
 * Configures the relevant UART's target's pins to be used for UART.
 */
void uart_configure_pinmux(void)
{
	_uart_configure_pinmux(true);
	uart_active = true;
}


/**
 * Releases the relevant pins from being used for UART, returning them
 * to use as GPIO.
 */
void uart_release_pinmux(void)
{
	_uart_configure_pinmux(false);
	uart_active = false;
}


/**
 * Configures the UART we'll use for our system console.
 * TODO: support more configuration (parity, stop, etc.)
 */
void uart_initialize(bool configure_pinmux, unsigned long baudrate)
{
	uart_deinit(uart_inst);

	if (configure_pinmux) {
		uart_configure_pinmux();
	}

    uart_init(uart_inst, baudrate);

	while(!uart_is_enabled(uart_inst));

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(uart_inst, false);

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = (uart_inst == uart0) ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(uart_inst, true, false);
}


/**
 * Callback issued when the UART recieves a new byte.
 */
__attribute__((weak)) void uart_byte_received_cb(uint8_t byte) {
    (void) byte; // unused
}


/**
 * UART interrupt handler.
 */
void on_uart_rx() {
    while (uart_is_readable(uart_inst)) {
        uint8_t ch = uart_getc(uart_inst);
		uart_byte_received_cb(ch);
    }
}


/**
 * @return True iff the UART can accept data.
 */
bool uart_ready_for_write(void)
{
    return uart_is_writable(uart_inst);
}


/**
 * Starts a write over the Apollo console UART.

 * Does not check for readiness; it is assumed the caller knows that the
 * UART is avaiable (e.g. by calling uart_ready_for_write).
 */
void uart_nonblocking_write(uint8_t byte)
{
	if(uart_ready_for_write()) {
        uart_putc_raw(uart_inst, byte);
    }
}


/**
 * Writes a byte over the Apollo console UART.
 *
 * @param byte The byte to be written.
 */
void uart_blocking_write(uint8_t byte)
{
	while(!uart_ready_for_write()) {}
    uart_putc_raw(uart_inst, byte);
}
