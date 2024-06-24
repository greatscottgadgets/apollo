/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Great Scott Gadgets <info@greatscottgadgets.com>
 * Copyright (c) 2019 Katherine J. Temkin <kate@ktemkin.com>
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#include "tusb.h"
#include "board_rev.h"
#include "usb/usb_protocol.h"

enum
{
	STRING_INDEX_LANGUAGE      = 0,
	STRING_INDEX_MANUFACTURER  = 1,
	STRING_INDEX_PRODUCT       = 2,
	STRING_INDEX_SERIAL_NUMBER = 3,
	STRING_INDEX_MICROSOFT     = 0xee,
};

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t desc_device =
{
	.bLength            = sizeof(tusb_desc_device_t),
	.bDescriptorType    = TUSB_DESC_DEVICE,
	.bcdUSB             = 0x0200,

	// We use bDeviceClass = 0 to indicate that we're a composite device.
	// Another option is to use the Interface Association Descriptor (IAD) method, 
	// but this requires extra descriptors.
	.bDeviceClass       = 0,
	.bDeviceSubClass    = 0,
	.bDeviceProtocol    = 0,

	.bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

	// These are a unique VID/PID for development LUNA boards.
	.idVendor           = 0x1d50,
	.idProduct          = 0x615c,

	.iManufacturer      = STRING_INDEX_MANUFACTURER,
	.iProduct           = STRING_INDEX_PRODUCT,
	.iSerialNumber      = STRING_INDEX_SERIAL_NUMBER,

	.bNumConfigurations = 0x01
};

/**
 * Return pointer to device descriptor.
 * Invoked by GET DEVICE DESCRIPTOR request.
 */
uint8_t const * tud_descriptor_device_cb(void)
{
	desc_device.bcdDevice = get_board_revision();
	return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum
{
	ITF_NUM_CDC = 0,
	ITF_NUM_CDC_DATA,
	ITF_NUM_DFU_RT,
	ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_DFU_RT_DESC_LEN)


uint8_t const desc_configuration[] =
{
	// Interface count, string index, total length, attribute, power in mA
	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

	// Interface number, string index, EP notification address and size, EP data address (out, in) and size.
	TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 0, 0x81, 8, 0x02, 0x83, 64),

	// Interface descriptor for the DFU runtime interface.
	TUD_DFU_RT_DESCRIPTOR(ITF_NUM_DFU_RT, 0, 0x0d, 500, 4096),
};


/**
 * Return pointer to configuration descriptor.
 * Invoked by GET CONFIGURATION DESCRIPTOR request.
 */
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
	(void) index; // for multiple configurations
	return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

#define STRING_DESC_LEN(x)    (2 + ((x) * 2))
#define STRING_DESC_MAX_CHARS 31
#define SERIAL_NUMBER_CHARS   26

typedef struct {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bString[STRING_DESC_MAX_CHARS];
} desc_string_t __attribute__((aligned(2)));

static desc_string_t desc_string;

static char serial_string[SERIAL_NUMBER_CHARS + 1];

/**
 * Return the microcontroller's unique ID in Base32.
 */
static inline char *get_serial_number_string(void)
{
	int count = 0;

	// Documented in section 9.3.3 of D21 datasheet, page 32 (rev G), but no header file,
	// these are not contiguous addresses.
	uint32_t ser[5];
	ser[0] = *(uint32_t *)0x0080A00C;
	ser[1] = *(uint32_t *)0x0080A040;
	ser[2] = *(uint32_t *)0x0080A044;
	ser[3] = *(uint32_t *)0x0080A048;
	ser[4] = 0;

	uint8_t *tmp = (uint8_t *)ser;

	// ... and convert our serial number into Base32.
	int buffer = tmp[0];
	int next = 1;
	int bits_left = 8;

	for (unsigned i = 0; i < SERIAL_NUMBER_CHARS; ++i) {
		if (bits_left < 5) {
			buffer <<= 8;
			buffer |= tmp[next++] & 0xff;
			bits_left += 8;
		}
		bits_left -= 5;
		int index = (buffer >> bits_left) & 0x1f;
		serial_string[count++] = index + (index < 26 ? 'A' : '2' - 26);  // RFC 4648 Base32
	}
	serial_string[count] = 0;

	return serial_string;
}

/**
 * Return pointer to string descriptor.
 * Invoked by GET STRING DESCRIPTOR request.
 */
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	uint8_t chr_count;
	const char* str;

	desc_string.bDescriptorType = TUSB_DESC_STRING;

	switch (index) {
	case STRING_INDEX_LANGUAGE:
		desc_string.bLength = STRING_DESC_LEN(1);
		desc_string.bString[0] = USB_LANGID_EN_US;
		return (uint16_t const *) &desc_string;
	case STRING_INDEX_MANUFACTURER:
		str = get_manufacturer_string();
		break;
	case STRING_INDEX_PRODUCT:
		str = get_product_string();
		break;
	case STRING_INDEX_SERIAL_NUMBER:
		str = get_serial_number_string();
		break;
	case STRING_INDEX_MICROSOFT:
		// Microsoft OS 1.0 String Descriptor
		str = "MSFT100\xee";
		break;
	default:
		return NULL;
	}

	// Cap at max chars.
	chr_count = strlen(str);
	if (chr_count > STRING_DESC_MAX_CHARS) chr_count = STRING_DESC_MAX_CHARS;

	// Encode string as UTF-16.
	for (uint8_t i=0; i<chr_count; i++) {
		desc_string.bString[i] = str[i];
	}

	desc_string.bLength = STRING_DESC_LEN(chr_count);

	return (uint16_t const *) &desc_string;
}
