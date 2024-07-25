#
# This file is part of Apollo.
#
# Copyright (c) 2020-2024 Great Scott Gadgets <info@greatscottgadgets.com>
# SPDX-License-Identifier: BSD-3-Clause

import os
import time
import usb.core
import platform
import errno
import sys

from .jtag  import JTAGChain
from .spi   import DebugSPIConnection
from .ila   import ApolloILAFrontend
from .ecp5  import ECP5_JTAGProgrammer, ECP5_JTAGDebugSPIConnection, ECP5_JTAGRegisters
from .intel import IntelJTAGProgrammer

from .onboard_jtag import *

import importlib.metadata
__version__ = importlib.metadata.version(__package__)


class DebuggerNotFound(IOError):
    pass


class USBAPIRequirement(Exception):
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
    APOLLO_USB_IDS = [(0x1d50, 0x615c), (0x1209, 0x0010)]
    LUNA_USB_IDS   = [(0x1d50, 0x615b)]

    # Add pid.codes test VID/PID pairs with PID from 0x0001 to 0x0005 used in
    # LUNA example gateware.
    for i in range(5):
        LUNA_USB_IDS += [(0x1209, i+1)]

    # Add pid.codes test VID/PID used by flash bridge.
    LUNA_USB_IDS += [(0x1209, 0x000f)]

    # If we have a LUNA_USB_IDS variable, we can use it to find the LUNA device.
    if os.getenv("LUNA_USB_IDS"):
        LUNA_USB_IDS += [tuple([int(x, 16) for x in os.getenv("LUNA_USB_IDS").split(":")])]

    REQUEST_GET_ID                  = 0xa0
    REQUEST_SET_LED_PATTERN         = 0xa1
    REQUEST_GET_FIRMWARE_VERSION    = 0xa2
    REQUEST_GET_USB_API_VERSION     = 0xa3
    REQUEST_GET_ADC_READING         = 0xa4
    REQUEST_RECONFIGURE             = 0xc0
    REQUEST_FORCE_FPGA_OFFLINE      = 0xc1
    REQUEST_ALLOW_FPGA_TAKEOVER_USB = 0xc2

    LED_PATTERN_IDLE = 500
    LED_PATTERN_UPLOAD = 50


    # External boards (non-Cynthion boards) are indicated with a Major revision of 0xFF.
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


    # Cynthion subdevices (specialized variants Cynthion) use a major of 0xFE.
    SUBDEVICE_MAJORS = {
        0xFE: "Amalthea"
    }

    backend = None


    def __init__(self, force_offline=False, device=None):
        """ Sets up a connection to the debugger. """

        # Try to create a connection to our Apollo debug firmware.
        if device is None:
            device = self._find_device(self.APOLLO_USB_IDS, custom_match=self._device_has_apollo_id)

        # If Apollo VID/PID is not found, try to find a gateware VID/PID with a valid Apollo stub
        # interface. If found, request the gateware to liberate the USB port. In devices with a
        # shared port, this effectively hands off the USB port to Apollo.
        if device is None:

            # First, find the candidate device...
            fpga_device = self._find_device(self.LUNA_USB_IDS, custom_match=self._device_has_stub_iface)
            if fpga_device is None:
                raise DebuggerNotFound("No Apollo debugger or stub interface found.")
            elif not force_offline:
                raise DebuggerNotFound("Apollo stub interface found but not requested to be forced offline.")

            # ... and now request a USB handoff to Apollo
            try:
                self._request_handoff(fpga_device)
            except usb.USBError as e:
                raise DebuggerNotFound(f"Handoff request failed: {e.strerror}")

            # Wait for Apollo to enumerate and try again
            device = self._find_device(self.APOLLO_USB_IDS, custom_match=self._device_has_apollo_id, timeout=5000)
            if device is None:
                raise DebuggerNotFound("Handoff was requested, but Apollo debugger not found.")

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

        # In Windows, we first need to claim the target interface.
        if platform.system() == "Windows":
            usb.util.claim_interface(device, stub_if)

        # Send the request
        intf_number = stub_if.bInterfaceNumber
        REQUEST_APOLLO_ADV_STOP = 0xF0
        request_type = usb.ENDPOINT_OUT | usb.RECIP_INTERFACE | usb.TYPE_VENDOR
        device.ctrl_transfer(request_type, REQUEST_APOLLO_ADV_STOP, wIndex=intf_number, timeout=5000)

    @classmethod
    def _init_backend(cls):
        """Initialize the USB backend."""
        if cls.backend is None:
            import usb.backend.libusb1

            # In Windows, we need to specify the libusb library location to create a backend.
            if platform.system() == "Windows":
                # Determine the path to libusb-1.0.dll.
                try:
                    from importlib_resources import files # <= 3.8
                except:
                    from importlib.resources import files # >= 3.9
                libusb_dll = os.path.join(files("usb1"), "libusb-1.0.dll")

                # Create a backend by explicitly passing the path to libusb_dll.
                cls.backend = usb.backend.libusb1.get_backend(find_library=lambda x: libusb_dll)
            else:
                # On other systems we can just use the default backend.
                cls.backend = usb.backend.libusb1.get_backend()

    @classmethod
    def _find_device(cls, ids, custom_match=None, timeout=0):
        """Find a USB device matching a list of VID/PIDs."""
        cls._init_backend()
        wait = 0
        while True:
            for vid, pid in ids:
                device = usb.core.find(backend=cls.backend, idVendor=vid, idProduct=pid, custom_match=custom_match)
                if device is not None:
                    return device
            # Should we wait and try again?
            if wait >= timeout:
                break
            time.sleep(0.5)
            wait += 500
        return None

    @classmethod
    def _find_all_devices(cls, ids, custom_match=None):
        """Find all USB devices matching a list of VID/PIDs."""
        cls._init_backend()
        devices = []
        candidates = []

        for vid, pid in ids:
            candidates.extend(usb.core.find(True, cls.backend, idVendor=vid, idProduct=pid))

        for device in candidates:
            try:
                if custom_match is None or custom_match(device):
                    devices.append(device)
            except (usb.USBError, NotImplementedError):
                # A permissions error or NotImplementedError is likely on
                # Windows if the device is not the expected type of device.
                # Other typical errors are transient conditions shortly after
                # device enumeration of a device that is not yet ready.
                pass
        return devices

    @staticmethod
    def _device_has_stub_iface(device, return_iface=False):
        """Check if a device has an Apollo stub interface present.

        Optionally return the interface itself.
        """
        for cfg in device:
            stub_if = usb.util.find_descriptor(cfg, bInterfaceClass=0xFF, bInterfaceSubClass=0x00)
            if stub_if is not None:
                return stub_if if return_iface else True
        return None if return_iface else False

    @staticmethod
    def _device_has_apollo_id(device):
        """Check if a device identifies itself as an Apollo debugger."""
        request_type = usb.ENDPOINT_IN | usb.RECIP_DEVICE | usb.TYPE_VENDOR
        try:
            response = device.ctrl_transfer(request_type, ApolloDebugger.REQUEST_GET_ID, data_or_wLength=256, timeout=500)
        except usb.USBError as e:
            if e.errno == errno.EPIPE:
                # A pipe error occurs when the device does not implement a
                # vendor request with a number matching REQUEST_GET_ID. This is
                # the expected result if the device is running Saturn-V.
                return False
            else:
                raise
        finally:
            usb.util.dispose_resources(device)
        apollo_id = bytes(response).decode('utf-8').split('\x00')[0]
        return True if "Apollo" in apollo_id else False

    def detect_connected_version(self):
        """ Attempts to determine the revision of the connected hardware.

        Returns the relevant hardware's revision number, as (major, minor).
        """

        # Extract the major and minor from the device's USB descriptor.
        minor = self.device.bcdDevice & 0xFF
        major = self.device.bcdDevice >> 8

        return major, minor


    def get_fpga_type(self):
        """ Returns a string indicating the type of FPGA populated on the connected Apollo board.

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

        # If this is a non-Cynthion board, we'll look up its name in our table.
        if self.major == self.EXTERNAL_BOARD_MAJOR:
            return self.EXTERNAL_BOARD_NAMES[self.minor]

        # If this is a non-Cynthion board, we'll look up its name in our table.
        if self.major in self.SUBDEVICE_MAJORS:
            product_name = self.SUBDEVICE_MAJORS[self.major]
            major        = 0 # For now?
        else:
            product_name = "Cynthion"
            major        = self.major

        # Otherwise, identify it by its revision number.
        return f"{product_name} r{major}.{self.minor}"


    def get_compatibility_string(self):
        """ Returns 'Cynthion' for a Cynthion board; or 'Apollo' for supported external board."""

        if self.major == self.EXTERNAL_BOARD_MAJOR:
            return 'Apollo'
        elif self.major in self.SUBDEVICE_MAJORS:
            return self.SUBDEVICE_MAJORS[self.major]

        return 'Cynthion'


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

    def allow_fpga_takeover_usb(self):
        """ Request Apollo to allow FPGA takeover of the USB port. Useful after reconfiguration. """
        self.out_request(self.REQUEST_ALLOW_FPGA_TAKEOVER_USB)

    def close(self):
        """ Closes the USB device so it can be reused, possibly by another ApolloDebugger """

        usb.util.dispose_resources(self.device)


    def get_firmware_version(self):
        """Return attached device's firmware version string."""
        c_string = self.in_request(self.REQUEST_GET_FIRMWARE_VERSION, length=256)
        return c_string.decode('utf-8').split('\x00')[0]

    def get_usb_api_version(self):
        """Return attached device's USB API version as a (major, minor) tuple."""
        raw_api_version = self.in_request(self.REQUEST_GET_USB_API_VERSION, length=2)
        api_major = int(raw_api_version[0])
        api_minor = int(raw_api_version[1])
        return (api_major, api_minor)

    def get_usb_api_version_string(self):
        """Return attached device's USB API version as a string."""
        (api_major, api_minor) = self.get_usb_api_version()
        return (f"{api_major}.{api_minor}")

    def require_usb_api_version(self, minimum):
        """Verify that the attached device meets a minimum USB API version requirement."""
        (dev_major, dev_minor) = self.get_usb_api_version()
        (req_major, req_minor) = minimum
        if dev_major < req_major or (dev_major == req_major and dev_minor < req_minor):
            raise USBAPIRequirement(f"Apollo debugger USB API {self.get_usb_api_version_string()} "
                    f"does not meet minimum required {minimum}. Please upgrade firmware.")

    def get_adc_reading(self):
        """Return raw ADC reading from attached device."""
        self.require_usb_api_version((1, 2))
        reading_bytes = self.in_request(self.REQUEST_GET_ADC_READING, length=2)
        return (reading_bytes[0] << 8) + reading_bytes[1]

    @classmethod
    def print_info(cls, ids=None, stub_ids=None, force_offline=False, timeout=5000, out=print):
        """ Print information about Apollo and all connected Apollo devices.

        Return True if any connected device is found.
        """
        out(f"Apollo version: {__version__}")
        out(f"Python version: {sys.version}\n")

        found_device = False
        if ids is None:
            ids = cls.APOLLO_USB_IDS
        if stub_ids is None:
            stub_ids = cls.LUNA_USB_IDS

        # Look for devices with stub interfaces.
        stub_devs = cls._find_all_devices(stub_ids, cls._device_has_stub_iface)

        for device in stub_devs:
            found_device = True
            out("Found Apollo stub interface!")
            out(f"\tBitstream: {device.product} ({device.manufacturer})")
            out(f"\tVendor ID: {device.idVendor:04x}")
            out(f"\tProduct ID: {device.idProduct:04x}")
            out(f"\tbcdDevice: {device.bcdDevice:04x}")
            out(f"\tBitstream serial number: {device.serial_number}")
            out("")

        count = 0
        if force_offline:
            apollo_devs = cls._find_all_devices(ids, cls._device_has_apollo_id)
            count = len(apollo_devs) + len(stub_devs)
            for device in apollo_devs:
                usb.util.dispose_resources(device)
            if count > 0:
                out(f"Forcing offline.\n")
            for device in stub_devs:
                try:
                    cls._request_handoff(device)
                except usb.USBError as e:
                    raise DebuggerNotFound(f"Handoff request failed: {e.strerror}")

        # Look for Apollo debuggers.
        start = time.time()
        while True:
            apollo_devs = cls._find_all_devices(ids, cls._device_has_apollo_id)
            if len(apollo_devs) >= count:
                break;
            if ((time.time() - start) * 1000) >= timeout:
                raise DebuggerNotFound("Handoff was requested, but Apollo debugger not found.")
            time.sleep(0.1)

        for device in apollo_devs:
            found_device = True
            debugger = ApolloDebugger(device=device)
            out(f"Found {debugger.get_compatibility_string()} device!")
            out(f"\tHardware: {debugger.get_hardware_name()}")
            out(f"\tManufacturer: {device.manufacturer}")
            out(f"\tProduct: {device.product}")
            out(f"\tSerial number: {device.serial_number}")
            out(f"\tVendor ID: {device.idVendor:04x}")
            out(f"\tProduct ID: {device.idProduct:04x}")
            out(f"\tbcdDevice: {device.bcdDevice:04x}")
            try:
                out(f"\tADC reading: {debugger.get_adc_reading()}")
            except USBAPIRequirement:
                pass
            out(f"\tFirmware version: {debugger.get_firmware_version()}")
            out(f"\tUSB API version: {debugger.get_usb_api_version_string()}")
            if force_offline:
                debugger.force_fpga_offline()
                with debugger.jtag as jtag:
                    programmer = debugger.create_jtag_programmer(jtag)
                    flash_uid = programmer.read_flash_uid()
                    out(f"\tFlash UID: {flash_uid:016x}")
            out("")

        return found_device
