#
# This file is part of LUNA.
#
# Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
# SPDX-License-Identifier: BSD-3-Clause

import os
import time
import usb.core

from .jtag  import JTAGChain
from .spi   import DebugSPIConnection
from .ila   import ApolloILAFrontend
from .ecp5  import ECP5_JTAGProgrammer, ECP5_JTAGDebugSPIConnection, ECP5_JTAGRegisters
from .intel import IntelJTAGProgrammer

from .onboard_jtag import *

class DebuggerNotFound(IOError):
    pass


def create_ila_frontend(ila, *, use_cs_multiplexing=False):
    """ Convenience method that instantiates an Apollo debug session and creates an ILA frontend from it.

    Parameters:
        ila -- The SyncSerialILA object we'll be connecting to.
    """
    debugger = ApolloDebugger()
    return ApolloILAFrontend(debugger, ila=ila, use_inverted_cs=use_cs_multiplexing)



class ApolloDebugger:
    """ Class representing a link to an Apollo Debug Module. """

    # VID/PID pairs for Apollo and gateware.
    APOLLO_USB_IDS = [(0x1d50, 0x615c)]
    LUNA_USB_IDS   = [(0x1d50, 0x615b)]

    # Add pid.codes VID/PID pairs with PID from 0x0001 to 0x0010
    for i in range(16):
        LUNA_USB_IDS += [(0x1209, i+1)]

    # If we have a LUNA_USB_IDS variable, we can use it to find the LUNA device.
    if os.getenv("LUNA_USB_IDS"):
        LUNA_USB_IDS += [tuple([int(x, 16) for x in os.getenv("LUNA_USB_IDS").split(":")])]

    REQUEST_SET_LED_PATTERN    = 0xa1
    REQUEST_RECONFIGURE        = 0xc0
    REQUEST_FORCE_FPGA_OFFLINE = 0xc1
    REQUEST_HONOR_FPGA_ADV     = 0xc2

    LED_PATTERN_IDLE = 500
    LED_PATTERN_UPLOAD = 50


    # External boards (non-LUNA boards) are indicated with a Major revision of 0xFF.
    # Their minor revision then encodes the board type.
    EXTERNAL_BOARD_MAJOR = 0xFF
    EXTERNAL_BOARD_NAMES = {
        0: "Daisho [rev 31-Oct-2014]",
        1: "Xil.se Pergola FPGA",
        2: "Adafruit QT Py ATSAMD21E18",
    }

    EXTERNAL_BOARD_PROGRAMMERS = {
        0: IntelJTAGProgrammer,
        2: ECP5_JTAGProgrammer,
    }


    # LUNA subdevices (specialized variants of the LUNA board) use a major of 0xFE.
    SUBDEVICE_MAJORS = {
        0xFE: "Amalthea"
    }


    def __init__(self):
        """ Sets up a connection to the debugger. """

        # Try to create a connection to our Apollo debug firmware.
        device = self._find_device(self.APOLLO_USB_IDS)

        # If Apollo VID/PID is not found, try to find a gateware VID/PID with a valid Apollo stub
        # interface. If found, request the gateware to liberate the USB port. In devices with a 
        # shared port, this effectively hands off the USB port to Apollo.
        if device is None:

            # First, find the candidate device...
            fpga_device = self._find_device(self.LUNA_USB_IDS, custom_match=self._device_has_stub_iface)
            if fpga_device is None:
                raise DebuggerNotFound("No Apollo or valid LUNA device found. "
                    "The LUNA_USB_IDS environment variable can be used to add custom VID:PID pairs.")

            # ... and now request a USB handoff to Apollo
            try:
                self._request_handoff(fpga_device)
            except usb.USBError as e:
                raise DebuggerNotFound(f"Handoff request failed: {e.strerror}")

            # Wait for Apollo to enumerate and try again
            time.sleep(2) 
            device = self._find_device(self.APOLLO_USB_IDS)
            if device is None:
                raise DebuggerNotFound("Handoff was requested, but Apollo is not available")

        self.device = device
        self.major, self.minor = self.get_hardware_revision()

        # Create a basic JTAG chain interface, for debugging convenience.
        self.jtag  = JTAGChain(self)

        # Try to create an SPI-over-JTAG tunnel, if this board supports it.
        self.spi, self.registers = self.create_jtag_spi(self.jtag)

        # If it doesn't, use a hard SPI for JTAG-SPI.
        if self.spi is None:
            self.spi   = DebugSPIConnection(self)
            self.registers = self.spi

    @classmethod
    def _request_handoff(cls, device):
        """ Requests the gateware to liberate the USB port. """
        # Find the Apollo stub interface first
        stub_if = cls._device_has_stub_iface(device, return_iface=True)
        if stub_if is None:
            raise DebuggerNotFound("No Apollo stub interface found")

        # Send the request
        intf_number = stub_if.bInterfaceNumber
        REQUEST_APOLLO_ADV_STOP = 0xF0
        request_type = usb.ENDPOINT_OUT | usb.RECIP_INTERFACE | usb.TYPE_VENDOR
        device.ctrl_transfer(request_type, REQUEST_APOLLO_ADV_STOP, wIndex=intf_number, timeout=5000)

    @staticmethod
    def _find_device(ids, custom_match=None):
        for vid, pid in ids:
            device = usb.core.find(idVendor=vid, idProduct=pid, custom_match=custom_match)
            if device is not None:
                return device
        return None

    @staticmethod
    def _device_has_stub_iface(device, return_iface=False):
        """ Checks if a device has an Apollo stub interface present.

        Optionally return the interface itself.
        """
        for cfg in device:
            stub_if = usb.util.find_descriptor(cfg, bInterfaceClass=0xFF, bInterfaceSubClass=0x00)
            if stub_if is not None:
                return stub_if if return_iface else True
        return None if return_iface else False

    def detect_connected_version(self):
        """ Attempts to determine the revision of the connected hardware.

        Returns the relevant hardware's revision number, as (major, minor).
        """

        # Extract the major and minor from the device's USB descriptor.
        minor = self.device.bcdDevice & 0xFF
        major = self.device.bcdDevice >> 8

        return major, minor


    def get_fpga_type(self):
        """ Returns a string indicating the type of FPGA populated on the connected LUNA board.

        The returned format is the same as used in a nMigen platform file; and can be used to override
        a platform's device type.
        """

        with self.jtag as jtag:

            # First, we'll detect all devices on our JTAG chain.
            jtag_devices = jtag.enumerate()
            if not jtag_devices:
                raise IOError("Could not detect an FPGA via JTAG!")

            # ... and grab its device identifier.
            first_device = jtag_devices[0]
            if not hasattr(first_device, 'DEVICE'):
                raise IOError("First JTAG device in chain does not provide an FPGA type. Is this a proper board?")

            return first_device.DEVICE


    @property
    def serial_number(self):
        """ Returns the device's serial number, as a string. """
        return self.device.serial_number


    def get_hardware_revision(self):
        """ Returns the (major, minor) of the attached hardware revision. """

        minor = self.device.bcdDevice & 0xFF
        major = self.device.bcdDevice >> 8
        return major, minor


    def get_hardware_name(self):
        """ Returns a string describing this piece of hardware. """

        # If this is a non-LUNA board, we'll look up its name in our table.
        if self.major == self.EXTERNAL_BOARD_MAJOR:
            return self.EXTERNAL_BOARD_NAMES[self.minor]

        # If this is a non-LUNA board, we'll look up its name in our table.
        if self.major in self.SUBDEVICE_MAJORS:
            product_name = self.SUBDEVICE_MAJORS[self.major]
            major        = 0 # For now?
        else:
            product_name = "LUNA"
            major        = self.major

        # Otherwise, identify it by its revision number.
        return f"{product_name} r{major}.{self.minor}"


    def get_compatibility_string(self):
        """ Returns 'LUNA' for a LUNA board; or 'LUNA-compatible' for supported external board."""

        if self.major == self.EXTERNAL_BOARD_MAJOR:
            return 'LUNA-compatible'
        elif self.major in self.SUBDEVICE_MAJORS:
            return self.SUBDEVICE_MAJORS[self.major]

        return 'LUNA'


    def create_jtag_programmer(self, jtag_chain):
        """ Returns the JTAG programmer for the given device. """

        # If this is an external programmer, return its programmer type.
        if self.major == self.EXTERNAL_BOARD_MAJOR:
            programmer = self.EXTERNAL_BOARD_PROGRAMMERS[self.minor]

        # Otherwise, it should be an ECP5.
        else:
            programmer = ECP5_JTAGProgrammer

        return programmer(jtag_chain)



    def create_jtag_spi(self, jtag_chain):
        """ Returns a JTAG-over-SPI connection for the given device. """

        # If this is an external programmer, we don't yet know how to create a JTAG-SPI 
        # interface for it. For now, assume we can't.
        if self.major == self.EXTERNAL_BOARD_MAJOR:
            return None, None

        # Use a real debug SPI on r0.1 and r0.2.
        elif self.major == 0 and self.minor < 3:
            return None, None

        # Otherwise, if we have a revision greater than r0.2, our SPI should be via JTAG.
        else:
            return ECP5_JTAGDebugSPIConnection(jtag_chain, self), ECP5_JTAGRegisters(jtag_chain)



    def out_request(self, number, value=0, index=0, data=None, timeout=500):
        """ Helper that issues an OUT control request to the debugger. """

        request_type = usb.ENDPOINT_OUT | usb.RECIP_DEVICE | usb.TYPE_VENDOR
        return self.device.ctrl_transfer(request_type, number, value, index, data, timeout=timeout)


    def in_request(self, number, value=0, index=0, length=0, timeout=500):
        """ Helper that issues an IN control request to the debugger. """

        request_type = usb.ENDPOINT_IN | usb.RECIP_DEVICE | usb.TYPE_VENDOR
        result = self.device.ctrl_transfer(request_type, number, value, index, length, timeout=timeout)

        return bytes(result)


    def set_led_pattern(self, number):
        self.out_request(self.REQUEST_SET_LED_PATTERN, number)


    def soft_reset(self):
        """ Resets the target (FPGA/etc) connected to the debug controller. """
        self.out_request(self.REQUEST_RECONFIGURE)


    def force_fpga_offline(self):
        """ Resets the target (FPGA/etc) connected to the debug controller. """
        self.out_request(self.REQUEST_FORCE_FPGA_OFFLINE)

    def honor_fpga_adv(self):
        """ Tell Apollo to honor requests from FPGA_ADV again. Useful after reconfiguration. """
        self.out_request(self.REQUEST_HONOR_FPGA_ADV)

    def close(self):
        """ Closes the USB device so it can be reused, possibly by another ApolloDebugger """

        usb.util.dispose_resources(self.device)
