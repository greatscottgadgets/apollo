/**
 * FPGA advertisement pin handling code.
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2024 Great Scott Gadgets <info@greatscottgadgets.com>
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

// Switching the shared USB port to the FPGA is allowed.
static bool fpga_usb_allowed = false;

// Duration of the time window (in milliseconds).
#define WINDOW_PERIOD_MS 200UL

// Store the timestamp of the last time window update.
static uint32_t last_update = 0;

// Counter of edges detected within the last time window.
static uint32_t window_edges = 0;

// Counter of edges detected since the last time window update.
static volatile uint32_t edge_counter = 0;

#endif

/**
 * Initialize FPGA_ADV receive-only pin
 */
void fpga_adv_init(void)
{
#ifdef BOARD_HAS_USB_SWITCH
	// Enable the APB clock for EIC (External Interrupt Controller).
	_pm_enable_bus_clock(PM_BUS_APBA, EIC);

	// Configure GCLK for EIC.
	_gclk_enable_channel(GCLK_CLKCTRL_ID_EIC_Val, GCLK_CLKCTRL_GEN_GCLK0_Val);
	while (GCLK->STATUS.bit.SYNCBUSY);

	// Configure FPGA_ADV as an input with function A (external interrupt).
	gpio_set_pin_direction(FPGA_ADV, GPIO_DIRECTION_IN);
	gpio_set_pin_pull_mode(FPGA_ADV, GPIO_PULL_UP);
	gpio_set_pin_function(FPGA_ADV, MUX_PA09A_EIC_EXTINT7);

	// Disable EIC.
	EIC->CTRL.bit.ENABLE = 0;
	while (EIC->STATUS.bit.SYNCBUSY);

	// Configure EIC to trigger on rising edge.
	EIC->CONFIG[0].reg &= ~EIC_CONFIG_SENSE7_Msk;
	EIC->CONFIG[0].reg |= EIC_CONFIG_SENSE7_RISE;

	// Enable External Interrupt.
	EIC->INTENSET.reg = EIC_INTENSET_EXTINT(1 << 7);

	// Enable EIC.
	EIC->CTRL.bit.ENABLE = 1;
	while (EIC->STATUS.bit.SYNCBUSY);

	// Enable IRQ.
	NVIC_EnableIRQ(EIC_IRQn);
#endif
}

/**
 * Task for things related with the advertisement pin
 */
void fpga_adv_task(void)
{
#ifdef BOARD_HAS_USB_SWITCH
	// Wait for the defined time window.
	if (board_millis() - last_update < WINDOW_PERIOD_MS) return;

	// Update edge counts inside time window.
	window_edges = edge_counter;
	edge_counter = 0;
	last_update  = board_millis();

    // Take over USB if the FPGA is not requesting the port.
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
	// True iff the number of edge counts surpasses the defined threshold.
	return window_edges > 2;
#else
	return false;
#endif
}


#ifdef BOARD_HAS_USB_SWITCH
/**
 * FPGA_ADV interrupt handler.
 */
void EIC_Handler(void) {
  // Clear the interrupt flag.
  EIC->INTFLAG.reg = EIC_INTFLAG_EXTINT(1 << 7);

  // Increment our edge counter.
  edge_counter++;
}
#endif
