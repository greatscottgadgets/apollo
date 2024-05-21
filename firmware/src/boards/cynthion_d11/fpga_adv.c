/**
 * FPGA advertisement pin handling code.
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2023 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdbool.h>
#include "fpga_adv.h"
#include "usb_switch.h"
#include "apollo_board.h"
#include <hal/include/hal_gpio.h>

#include <bsp/board_api.h>
#include <hpl/pm/hpl_pm_base.h>
#include <hpl/gclk/hpl_gclk_base.h>
#include <peripheral_clk_config.h>

#ifdef BOARD_HAS_USB_SWITCH

// Store the timestamp of the last physical port advertisement
#define TIMEOUT 100UL
static uint32_t last_phy_adv = 0;

// Switching the shared USB port to the FPGA is allowed.
static bool fpga_usb_allowed = false;

// Create a reference to our SERCOM object.
typedef Sercom sercom_t;
static sercom_t *sercom = SERCOM1;

static void fpga_adv_byte_received_cb(uint8_t byte, int parity_error);

#endif

/**
 * Initialize FPGA_ADV receive-only serial port
 */
void fpga_adv_init(void)
{
#ifdef BOARD_HAS_USB_SWITCH
	gpio_set_pin_direction(FPGA_ADV, GPIO_DIRECTION_IN);
	gpio_set_pin_pull_mode(FPGA_ADV, GPIO_PULL_UP);

	// Disable the SERCOM before configuring it, to 1) ensure we're not transacting
	// during configuration; and 2) as many of the registers are R/O when the SERCOM is enabled.
	while(sercom->USART.SYNCBUSY.bit.ENABLE);
	sercom->USART.CTRLA.bit.ENABLE = 0;

	// Software reset the SERCOM to restore initial values.
	while(sercom->USART.SYNCBUSY.bit.SWRST);
	sercom->USART.CTRLA.bit.SWRST = 1;

	// The SWRST bit becomes accessible again once the software reset is
	// complete -- we'll use this to wait for the reset to be finshed.
	while(sercom->USART.SYNCBUSY.bit.SWRST);

	// Ensure we can work with the full SERCOM.
	while(sercom->USART.SYNCBUSY.bit.SWRST || sercom->USART.SYNCBUSY.bit.ENABLE);

	// Pinmux the relevant pins to be used for the SERCOM.
	gpio_set_pin_function(PIN_PA09, MUX_PA09C_SERCOM1_PAD3);

	// Set up clocking for the SERCOM peripheral.
	_pm_enable_bus_clock(PM_BUS_APBC, SERCOM1);
	_gclk_enable_channel(SERCOM1_GCLK_ID_CORE, GCLK_CLKCTRL_GEN_GCLK0_Val);

	// Configure the SERCOM for UART mode.
	sercom->USART.CTRLA.reg =
		SERCOM_USART_CTRLA_DORD     |          // LSB first
		SERCOM_USART_CTRLA_RXPO(3)  |          // RX on PA09 (PAD[3])
		SERCOM_USART_CTRLA_SAMPR(0) |          // use 16x oversampling
		SERCOM_USART_CTRLA_FORM(1)	|          // enable parity
		SERCOM_USART_CTRLA_RUNSTDBY |          // don't autosuspend the clock
		SERCOM_USART_CTRLA_MODE_USART_INT_CLK; // use internal clock

	// Configure our baud divisor.
	const uint32_t baudrate = 9600;
	const uint32_t baud = (((uint64_t)CONF_CPU_FREQUENCY << 16) - ((uint64_t)baudrate << 20)) / CONF_CPU_FREQUENCY;
	sercom->USART.BAUD.reg = baud;

	// Configure TX/RX and framing.
	sercom->USART.CTRLB.reg =
			SERCOM_USART_CTRLB_CHSIZE(0) | // 8-bit words
			SERCOM_USART_CTRLB_RXEN;       // Enable RX.

	// Wait for our changes to apply.
	while (sercom->USART.SYNCBUSY.bit.CTRLB);

	// Enable our receive interrupt, as we want to asynchronously dump data into
	// the UART console.
	sercom->USART.INTENSET.reg = SERCOM_USART_INTENSET_RXC;

	// Enable the UART IRQ.
	NVIC_EnableIRQ(SERCOM1_IRQn);

	// Finally, enable the SERCOM.
	sercom->USART.CTRLA.bit.ENABLE = 1;
	while(sercom->USART.SYNCBUSY.bit.ENABLE);

	// Set timestamp to ensure we don't erroneously detect an initial advertisement.
	last_phy_adv = board_millis() - TIMEOUT;
#endif
}

/**
 * Task for things related with the advertisement pin
 */
void fpga_adv_task(void)
{
#ifdef BOARD_HAS_USB_SWITCH
    // Take over USB after timeout
	if (fpga_requesting_port() == false) {
		take_over_usb();
	} else if (fpga_usb_allowed) {
		hand_off_usb();
	}
#endif
}

/**
 * Allow FPGA takeover of the USB port
 */
void allow_fpga_takeover_usb(bool allow)
{
	fpga_usb_allowed = allow;
}

/**
 * True if we received an advertisement message within the last time window.
 */
bool fpga_requesting_port(void)
{
#ifdef BOARD_HAS_USB_SWITCH
	return board_millis() - last_phy_adv < TIMEOUT;
#else
	return false;
#endif
}


#ifdef BOARD_HAS_USB_SWITCH
/**
 * FPGA_ADV interrupt handler.
 */
void SERCOM1_Handler(void)
{
	// If we've just received a character, handle it.
	if (sercom->USART.INTFLAG.bit.RXC)
	{
		// Read the relevant character, which marks this interrupt as serviced.
		uint16_t byte = sercom->USART.DATA.reg;
		fpga_adv_byte_received_cb(byte, sercom->USART.STATUS.bit.PERR);
	}
}

static void fpga_adv_byte_received_cb(uint8_t byte, int parity_error) {
	if (parity_error) {
		return;
	}

	if (byte == 'A') {
		last_phy_adv = board_millis();
	}
}
#endif
