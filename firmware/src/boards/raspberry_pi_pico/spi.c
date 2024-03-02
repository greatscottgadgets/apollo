/*
 * SPI driver code.
 *
 * This file is part of LUNA.
 *
 * Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bsp/board_api.h>

#include "hardware/spi.h"
#include "hardware/resets.h"
#include "hardware/clocks.h"

#include "apollo_board.h"
#include "spi.h"
#include "led.h"


static const char reverse_char[0x100] = {0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
                      0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
                      0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
                      0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
                      0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
                      0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
                      0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
                      0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
                      0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
                      0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
                      0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
                      0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
                      0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
                      0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
                      0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
                      0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF};

static volatile bool _lsb_first = false;


/**
 * Returns the SPI object associated with the given target.
 */
static spi_inst_t *spi_inst_for_target(spi_target_t target)
{
	switch (target) {
		case SPI_FPGA_JTAG:  return spi0;
		case SPI_FPGA_DEBUG: return NULL;
	}

	return NULL;
}


/**
 * Pinmux the relevent pins so the can be used for SPI.
 */
static void _spi_configure_pinmux(spi_target_t target, bool use_for_spi)
{
	switch (target) {

		// FPGA JTAG connection -- configure TDI, TCK, TDO
		case SPI_FPGA_JTAG:
			if (use_for_spi) {
				gpio_set_function(TDI_GPIO, GPIO_FUNC_SPI);
				gpio_set_function(TCK_GPIO, GPIO_FUNC_SPI);
				gpio_set_function(TDO_GPIO, GPIO_FUNC_SPI);
			} else {
				gpio_set_function(TDI_GPIO, GPIO_FUNC_SIO);
				gpio_set_function(TCK_GPIO, GPIO_FUNC_SIO);
				gpio_set_function(TDO_GPIO, GPIO_FUNC_SIO);
			}
			break;
		case SPI_FPGA_DEBUG:
			// not implemented
			break;
	}
}


/**
 * Configures the relevant SPI target's pins to be used for SPI.
 */
void spi_configure_pinmux(spi_target_t target)
{
	_spi_configure_pinmux(target, true);
}


/**
 * Returns the relevant SPI target's pins to being used for GPIO.
 */
void spi_release_pinmux(spi_target_t target)
{
	_spi_configure_pinmux(target, false);
}


/**
 * Configures the provided target to be used as an SPI port.
 */
void spi_initialize(spi_target_t target, bool lsb_first, bool configure_pinmux, uint8_t baud_divider,
	 uint8_t clock_polarity, uint8_t clock_phase)
{
	spi_inst_t *spi = spi_inst_for_target(target);
	_lsb_first = lsb_first;

	// Disable the SPI before configuring it.
	spi_deinit(spi);

	// Set up clocking for the SPI peripheral.
    spi_init(spi, 8 * 1000 * 1000 / (2*(baud_divider+1)));

	// Configure the SPI for master mode.
	spi_set_slave(spi, false);

    // Set SPI format
    spi_set_format( spi,   // SPI instance
                    8,      // Number of bits per transfer
                    clock_polarity,      // Polarity (CPOL)
                    clock_phase,      // Phase (CPHA)
                    SPI_MSB_FIRST);

	// Pinmux the relevant pins to be used for the SPI.
	if (configure_pinmux) {
		spi_configure_pinmux(target);
	}
}


/**
 * Synchronously send a single byte on the given SPI bus.
 * Does not manage the SSEL line.
 */
uint8_t spi_send_byte(spi_target_t target, uint8_t data)
{
	uint8_t dst;
	spi_inst_t *spi = spi_inst_for_target(target);

	if (_lsb_first) {
		data = reverse_char[data];
	}

	spi_write_read_blocking(spi, &data, &dst, 1);

	if (_lsb_first) {
		dst = reverse_char[dst];
	}

	return dst;
}


/**
 * Sends a block of data over the SPI bus.
 *
 * @param port The port on which to perform the SPI transaction.
 * @param data_to_send The data to be transferred over the SPI bus.
 * @param data_received Any data received during the SPI transaction.
 * @param length The total length of the data to be exchanged, in bytes.
 */
void spi_send(spi_target_t port, void *data_to_send, void *data_received, size_t length)
{
	uint8_t *to_send  = data_to_send;
	uint8_t *received = data_received;

	// TODO: use the FIFO to bulk send data
	for (unsigned i = 0; i < length; ++i) {
		received[i] = spi_send_byte(port, to_send[i]);
	}
}
