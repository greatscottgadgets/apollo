/*
 * Code for dispatching Apollo vendor requests.
 *
 * Currently, we support only a vendor-request based protocol, as we're trying to
 * keep code size small for a potential switch to a SAMD11. This likely means we
 * want to avoid the overhead of the libgreat comms API.
 *
 * This file is part of Apollo.
 *
 * Copyright (c) 2019-2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <apollo_board.h>
#include <tusb.h>

#include "led.h"
#include "jtag.h"
#include "fpga.h"
//#include "selftest.h"
#include "debug_spi.h"
#include "usb_switch.h"
#include "fpga_adv.h"
#include "board_rev.h"

#define USB_API_MAJOR 1
#define USB_API_MINOR 2


// Supported vendor requests.
enum {
	VENDOR_REQUEST_GET_ID                  = 0xa0,
	VENDOR_REQUEST_SET_LED_PATTERN         = 0xa1,
	VENDOR_REQUEST_GET_FIRMWARE_VERSION    = 0xa2,
	VENDOR_REQUEST_GET_USB_API_VERSION     = 0xa3,
	VENDOR_REQUEST_GET_ADC_READING         = 0xa4,

	//
	// JTAG requests.
	//
	VENDOR_REQUEST_JTAG_START              = 0xbf,
	VENDOR_REQUEST_JTAG_STOP               = 0xbe,

	VENDOR_REQUEST_JTAG_CLEAR_OUT_BUFFER   = 0xb0,
	VENDOR_REQUEST_JTAG_SET_OUT_BUFFER     = 0xb1,
	VENDOR_REQUEST_JTAG_GET_IN_BUFFER      = 0xb2,
	VENDOR_REQUEST_JTAG_SCAN               = 0xb3,
	VENDOR_REQUEST_JTAG_RUN_CLOCK          = 0xb4,
	VENDOR_REQUEST_JTAG_GOTO_STATE         = 0xb5,
	VENDOR_REQUEST_JTAG_GET_STATE          = 0xb6,
	VENDOR_REQUEST_JTAG_BULK_SCAN          = 0xb7,

	// General programming requests.
	VENDOR_REQUEST_TRIGGER_RECONFIGURATION = 0xc0,
	VENDOR_REQUEST_FORCE_FPGA_OFFLINE      = 0xc1,
	VENDOR_REQUEST_ALLOW_FPGA_TAKEOVER_USB = 0xc2,


	//
	// Debug SPI requests
	//
	VENDOR_REQUEST_DEBUG_SPI_SEND          = 0x50,
	VENDOR_REQUEST_DEBUG_SPI_READ_RESPONSE = 0x51,
	VENDOR_REQUEST_FLASH_SPI_SEND          = 0x52,
	VENDOR_REQUEST_TAKE_FLASH_LINES        = 0x53,
	VENDOR_REQUEST_RELEASE_FLASH_LINES     = 0x54,

	//
	// Self-test requests.
	//
	VENDOR_REQUEST_GET_RAIL_VOLTAGE        = 0xe0,

	//
	// Microsoft WCID descriptor request
	//
	VENDOR_REQUEST_GET_MS_DESCRIPTOR       = 0xee,
};

// Microsoft OS 1.0 Compat ID Feature descriptor
uint8_t msft_10_compat_id[] = {
	// Header: length, bcdVersion, wIndex, bCount, reserved[7]
	U32_TO_U8S_LE(0x0028),
	U16_TO_U8S_LE(0x0100),
	U16_TO_U8S_LE(0x0004),
	0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	// Compatible ID: bFirstInterfaceNumber, reserved[1], compatibleID[8], subCompatibleID[8], reserved[6]
	0x02,
	0x01, 
	'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};

// Microsoft OS 1.0 Extended Properties Feature descriptor
uint8_t msft_10_ext_props[] = {
	U32_TO_U8S_LE(0x0000008E),  // Length 142 bytes
	U16_TO_U8S_LE(0x0100),      // Version
	U16_TO_U8S_LE(0x0005),      // Extended Properties Feature Descriptor index
	U16_TO_U8S_LE(0x0001),      // Number of sections

	U32_TO_U8S_LE(0x00000084),  // Size of the property section
	U32_TO_U8S_LE(0x00000001),  // Property data type (1 = Unicode REG_SZ)

	U16_TO_U8S_LE(0x0028),      // Property name length (40 bytes)
	// Property Name ("DeviceInterfaceGUID") 
	'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0, 'I', 0, 'n', 0,
	't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0, 'G', 0,
	'U', 0, 'I', 0, 'D', 0, 0, 0,

	U32_TO_U8S_LE(0x0000004E),  // Property data length (78 bytes)
	// Property Name "{88BAE032-5A81-49f0-BC3D-A4FF138216D6}" from winusb.inf
	'{', 0, '8', 0, '8', 0, 'b', 0, 'a', 0, 'e', 0, '0', 0, '3', 0,
	'2', 0, '-', 0, '5', 0, 'a', 0, '8', 0, '1', 0, '-', 0, '4', 0,
	'9', 0, 'f', 0, '0', 0, '-', 0, 'b', 0, 'c', 0, '3', 0, 'd', 0,
	'-', 0, 'a', 0, '4', 0, 'f', 0, 'f', 0, '1', 0, '3', 0, '8', 0,
	'2', 0, '1', 0, '6', 0, 'd', 0, '6', 0, '}', 0, 0, 0,
};

/**
 * Request Microsoft Windows Compatible ID descriptor.
*/
bool handle_get_ms_descriptor(uint8_t rhport, tusb_control_request_t const* request)
{
	if (request->wIndex == 0x0004) {
		return tud_control_xfer(rhport, request, msft_10_compat_id, sizeof(msft_10_compat_id));
	} else if (request->wIndex == 0x0005) {
		return tud_control_xfer(rhport, request, msft_10_ext_props, sizeof(msft_10_ext_props));
	} else {
		return false;
	}
}


/**
 * Simple request that's used to identify the running firmware; mostly a sanity check.
 */
bool handle_get_id_request(uint8_t rhport, tusb_control_request_t const* request)
{
	static char description[] = "Apollo Debug Module";
	return tud_control_xfer(rhport, request, description, sizeof(description));
}


/**
 * Request firmware version string.
 */
bool handle_get_firmware_version_request(uint8_t rhport, tusb_control_request_t const* request)
{
	static char version[] = VERSION_STRING;
	return tud_control_xfer(rhport, request, version, sizeof(version));
}


/**
 * Request USB API version.
 */
bool handle_get_usb_api_version_request(uint8_t rhport, tusb_control_request_t const* request)
{
	static char usb_api[2] = {USB_API_MAJOR, USB_API_MINOR};
	return tud_control_xfer(rhport, request, usb_api, sizeof(usb_api));
}


/**
 * Request raw ADC reading.
 */
bool handle_get_adc_reading_request(uint8_t rhport, tusb_control_request_t const* request)
{
	static char buf[2];
	uint16_t reading = get_adc_reading();
	buf[0] = reading >> 8;
	buf[1] = reading & 0xff;
	return tud_control_xfer(rhport, request, buf, sizeof(buf));
}


/**
 * Request that changes the active LED pattern.
 */
bool handle_set_led_pattern(uint8_t rhport, tusb_control_request_t const* request)
{
	led_set_pattern(request->wValue);
	return tud_control_xfer(rhport, request, NULL, 0);
}


/**
 * Request that changes the active LED pattern.
 */
bool handle_trigger_fpga_reconfiguration(uint8_t rhport, tusb_control_request_t const* request)
{
	trigger_fpga_reconfiguration();
	return tud_control_xfer(rhport, request, NULL, 0);
}


/**
 * Request that forces the FPGA offline, preventing bricking.
 */
bool handle_force_fpga_offline(uint8_t rhport, tusb_control_request_t const* request)
{
	force_fpga_offline();
	return tud_control_xfer(rhport, request, NULL, 0);
}


/**
 * Request Apollo to allow FPGA takeover of the USB port.
 */
bool handle_allow_fpga_takeover_usb(uint8_t rhport, tusb_control_request_t const* request)
{
	return tud_control_xfer(rhport, request, NULL, 0);
}

bool handle_allow_fpga_takeover_usb_finish(uint8_t rhport, tusb_control_request_t const* request)
{
	allow_fpga_takeover_usb(true);
	return true;
}



/**
 * Primary vendor request handler.
 */
static bool handle_vendor_request_setup(uint8_t rhport, tusb_control_request_t const* request)
{
	switch(request->bRequest) {
		case VENDOR_REQUEST_GET_ADC_READING:
			return handle_get_adc_reading_request(rhport, request);
		case VENDOR_REQUEST_GET_ID:
			return handle_get_id_request(rhport, request);
		case VENDOR_REQUEST_GET_FIRMWARE_VERSION:
			return handle_get_firmware_version_request(rhport, request);
		case VENDOR_REQUEST_GET_USB_API_VERSION:
			return handle_get_usb_api_version_request(rhport, request);
		case VENDOR_REQUEST_TRIGGER_RECONFIGURATION:
			return handle_trigger_fpga_reconfiguration(rhport, request);
		case VENDOR_REQUEST_FORCE_FPGA_OFFLINE:
			return handle_force_fpga_offline(rhport, request);
		case VENDOR_REQUEST_ALLOW_FPGA_TAKEOVER_USB:
			return handle_allow_fpga_takeover_usb(rhport, request);

		// JTAG requests
		case VENDOR_REQUEST_JTAG_CLEAR_OUT_BUFFER:
			return handle_jtag_request_clear_out_buffer(rhport, request);
		case VENDOR_REQUEST_JTAG_SET_OUT_BUFFER:
			return handle_jtag_request_set_out_buffer(rhport, request);
		case VENDOR_REQUEST_JTAG_GET_IN_BUFFER:
			return handle_jtag_request_get_in_buffer(rhport, request);
		case VENDOR_REQUEST_JTAG_SCAN:
			return handle_jtag_request_scan(rhport, request);
		case VENDOR_REQUEST_JTAG_RUN_CLOCK:
			return handle_jtag_run_clock(rhport, request);
		case VENDOR_REQUEST_JTAG_START:
			return handle_jtag_start(rhport, request);
		case VENDOR_REQUEST_JTAG_GOTO_STATE:
			return handle_jtag_go_to_state(rhport, request);
		case VENDOR_REQUEST_JTAG_STOP:
			return handle_jtag_stop(rhport, request);
		case VENDOR_REQUEST_JTAG_GET_STATE:
			return handle_jtag_get_state(rhport, request);

		// LED control requests.
		case VENDOR_REQUEST_SET_LED_PATTERN:
			return handle_set_led_pattern(rhport, request);

		// Debug SPI requests.
#ifdef _BOARD_HAS_DEBUG_SPI
		case VENDOR_REQUEST_DEBUG_SPI_SEND:
			return handle_debug_spi_send(rhport, request);
		case VENDOR_REQUEST_DEBUG_SPI_READ_RESPONSE:
			return handle_debug_spi_get_response(rhport, request);
		case VENDOR_REQUEST_FLASH_SPI_SEND:
			return handle_flash_spi_send(rhport, request);
		case VENDOR_REQUEST_TAKE_FLASH_LINES:
			return handle_take_configuration_spi(rhport, request);
		case VENDOR_REQUEST_RELEASE_FLASH_LINES:
			return handle_release_configuration_spi(rhport, request);
#endif

		// Self-test requests.
		/*
		case VENDOR_REQUEST_GET_RAIL_VOLTAGE:
			return handle_get_rail_voltage(rhport, request);
		*/

		case VENDOR_REQUEST_GET_MS_DESCRIPTOR:
			return handle_get_ms_descriptor(rhport, request);

		default:
			return false;
	}

}

/**
 * Called when a vendor request is completed.
 *
 * This is used to complete any actions that need to happen once data is available, e.g.
 * during an IN transfer.
 */
static bool handle_vendor_request_complete(uint8_t rhport, tusb_control_request_t const * request)
{
	switch (request->bRequest) {
		case VENDOR_REQUEST_DEBUG_SPI_SEND:
			return handle_debug_spi_send_complete(rhport, request);
		case VENDOR_REQUEST_FLASH_SPI_SEND:
			return handle_flash_spi_send_complete(rhport, request);
		default:
			return true;
	}

}

/**
 * Called when a vendor request is finished.
 */
static bool handle_vendor_request_finish(uint8_t rhport, tusb_control_request_t const * request)
{
	switch (request->bRequest) {
		case VENDOR_REQUEST_ALLOW_FPGA_TAKEOVER_USB:
			return handle_allow_fpga_takeover_usb_finish(rhport, request);
		default:
			return true;
	}
}


bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
	switch (stage) {
		case CONTROL_STAGE_SETUP:
			return handle_vendor_request_setup(rhport, request);
		case CONTROL_STAGE_DATA:
			return handle_vendor_request_complete(rhport, request);
		case CONTROL_STAGE_ACK:
			return handle_vendor_request_finish(rhport, request);
		default:
			return true;

	}

}
