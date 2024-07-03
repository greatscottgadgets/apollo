#!/usr/bin/env python3
#
# This file is part of Apollo.
#
# Copyright (c) 2020-2024 Great Scott Gadgets <info@greatscottgadgets.com>
# SPDX-License-Identifier: BSD-3-Clause

from __future__ import print_function
from operator import invert

import os
import sys
import ast
import time
import errno
import logging
import argparse
from collections import namedtuple
import xdg.BaseDirectory
from functools import partial

from apollo_fpga import ApolloDebugger
from apollo_fpga.jtag import JTAGChain, JTAGPatternError
from apollo_fpga.ecp5 import ECP5_JTAGProgrammer, ECP5FlashBridgeProgrammer
from apollo_fpga.onboard_jtag import *


#
# Common JEDEC manufacturer IDs for SPI flash chips.
#
JEDEC_MANUFACTURERS = {
    0x01: "AMD/Spansion/Cypress",
    0x04: "Fujitsu",
    0x1C: "eON",
    0x1F: "Atmel/Microchip",
    0x20: "Micron/Numonyx/ST",
    0x37: "AMIC",
    0x62: "SANYO",
    0x89: "Intel",
    0x8C: "ESMT",
    0xA1: "Fudan",
    0xAD: "Hyundai",
    0xBF: "SST",
    0xC2: "Micronix",
    0xC8: "Gigadevice",
    0xD5: "ISSI",
    0xEF: "Winbond",
    0xE0: 'Paragon',
}

#
# Common JEDEC device IDs. Prefixed with their manufacturer for easy / unique lookup.
#
JEDEC_PARTS = {
    0xEF3015: "W25X16L",
    0xEF3014: "W25X80L",
    0xEF3013: "W25X40L",
    0xEF3012: "W25X20L",
    0xEF3011: "W25X10L",
    0xEF4015: "W25Q16DV",
    0xEF4016: "W25Q32DV",
    0xEF4017: "W25Q64DV",
    0xEF4018: "W25Q128DV",
    0xC22515: "MX25L1635E",
    0xC22017: "MX25L6405D",
    0xC22016: "MX25L3205D",
    0xC22015: "MX25L1605D",
    0xC22014: "MX25L8005",
    0xC22013: "MX25L4005",
    0xC22010: "MX25L512E",
    0x204011: "M45PE10",
    0x202014: "M25P80",
    0x1f4501: "AT24DF081",
    0x1C3114: "EN25F80",
    0xE04014: "PN25F08",
}


def print_device_info(device, args):
    """ Command that prints information about devices connected to the scan chain to the console. """

    logging.info(f"Detected a {device.get_compatibility_string()} device!")
    logging.info(f"\tHardware: {device.get_hardware_name()}")
    logging.info(f"\tSerial number: {device.serial_number}")
    logging.info(f"\tFirmware version: {device.get_firmware_version()}")
    logging.info(f"\tUSB API version: {device.get_usb_api_version_string()}")
    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)
        flash_uid = programmer.read_flash_uid()
    logging.info(f"\tFlash UID: {flash_uid:016x}")

def print_chain_info(device, args):
    """ Command that prints information about devices connected to the scan chain to the console. """

    with device.jtag as jtag:
        logging.info("Scanning for connected devices...")
        detected_devices = jtag.enumerate()

        # If devices exist on the scan chain, print their information.
        if detected_devices:
            logging.info("{} device{} detected on the scan chain:\n".format(
                        len(detected_devices), 's' if len(detected_devices) > 1 else ''))

            for device in detected_devices:
                logging.info("    {:08x} -- {}".format(device.idcode(), device.description()))


            logging.info('')

        else:
            logging.info("No devices found.\n")


def play_svf_file(device, args):
    """ Command that prints the relevant flash chip's information to the console. """

    if not args.file:
        logging.error("You must provide an SVF filename to play!\n")
        sys.exit(-1)

    with device.jtag as jtag:
        try:
            jtag.play_svf_file(args.file)
        except JTAGPatternError:
            # Our SVF player has already logged the error to stderr.
            logging.error("")


def configure_fpga(device, args):
    """ Command that prints information about devices connected to the scan chain to the console. """

    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)

        with open(args.file, "rb") as f:
            bitstream = f.read()

        programmer.configure(bitstream)

    # Let the LUNA gateware take over in devices with shared USB port
    device.allow_fpga_takeover_usb()


def ensure_unconfigured(device):
    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)
        programmer.unconfigure()


def erase_flash(device, args):
    ensure_unconfigured(device)

    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)
        programmer.erase_flash()


def program_flash(device, args):
    if args.fast:
        return program_flash_fast(device, args)

    ensure_unconfigured(device)

    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)
        offset = ast.literal_eval(args.offset) if args.offset else 0

        with open(args.file, "rb") as f:
            bitstream = f.read()

        programmer.flash(bitstream, offset=offset)



def program_flash_fast(device, args):
    # check if flash-fast is supported
    try:
        from amaranth.build.run import LocalBuildProducts
        from luna.gateware.platform import get_appropriate_platform
        from apollo_fpga.gateware.flash_bridge import FlashBridge, FlashBridgeConnection
    except ImportError:
        logging.error("`flash --fast` requires the `luna` package in the Python environment.\n"
                      "Install `luna` or use `flash` instead.")
        sys.exit(-1)

    # get platform
    platform=get_appropriate_platform()

    # Retrieve a FlashBridge cached bitstream or build it
    plan = platform.build(FlashBridge(), do_build=False)
    cache_dir = os.path.join(
        xdg.BaseDirectory.save_cache_path('apollo'), 'build', plan.digest().hex()
    )
    if os.path.exists(cache_dir):
        products = LocalBuildProducts(cache_dir)
    else:
        products = plan.execute_local(cache_dir)

    # Configure flash bridge
    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)
        programmer.configure(products.get("top.bit"))

    # Let the LUNA gateware take over in devices with shared USB port
    device.allow_fpga_takeover_usb()

    # Wait for flash bridge enumeration
    time.sleep(2)

    # Program SPI flash memory using the configured bridge
    bridge = FlashBridgeConnection()
    programmer = ECP5FlashBridgeProgrammer(bridge=bridge)
    with open(args.file, "rb") as f:
        bitstream = f.read()
    programmer.flash(bitstream)


def program_flash_fast_deprecated(device, args):
    logging.warning("[WARNING] `flash-fast` is now deprecated. Please use `flash --fast` instead.")
    return program_flash_fast(device, args)


def read_back_flash(device, args):
    ensure_unconfigured(device)

    # XXX abstract this?
    length = ast.literal_eval(args.length) if args.length else (4 * 1024 * 1024)
    offset = ast.literal_eval(args.offset) if args.offset else 0
    if offset:
        length = min(length, 4 * 1024 * 1024 - offset)

    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)

        with open(args.file, "wb") as f:
            bitstream = programmer.read_flash(length, offset=offset)
            f.write(bitstream)

    device.soft_reset()



def print_flash_info(device, args):
    """ Command that prints information about the currently connected FPGA's configuration flash. """
    ensure_unconfigured(device)
    serial_number = device.serial_number

    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)
        manufacturer, device = programmer.read_flash_id()
        unique_id = programmer.read_flash_uid()

        if manufacturer == 0xFF:
            logging.info("No flash detected.")
            return

        logging.info(f"Device serial number: {serial_number}")
        logging.info("Detected an FPGA-connected SPI configuration flash!")

        try:
            logging.info(f"\tManufacturer: {JEDEC_MANUFACTURERS[manufacturer]} ({manufacturer:02x})")
        except KeyError:
            logging.info(f"\tUnknown manufacturer ({manufacturer:02x}).")

        try:
            logging.info(f"\tDevice: {JEDEC_PARTS[device]} ({device:06x})")
        except KeyError:
            logging.info(f"\tUnknown device ({device:06x}).")

        logging.info(f"\tUnique ID: {unique_id:016x}")

        logging.info("")


def reconfigure_fpga(device, args):
    """ Command that requests the attached ECP5 reconfigure itself from its SPI flash. """
    device.soft_reset()

    # Let the LUNA gateware take over in devices with shared USB port
    device.allow_fpga_takeover_usb()


def force_fpga_offline(device, args):
    """ Command that requests the attached ECP5 be held unconfigured. """
    device.force_fpga_offline()


def _do_debug_spi(device, spi, args, *, invert_cs):

    # Try to figure out what data the user wants to send.
    data_raw = ast.literal_eval(args.bytes)
    if isinstance(data_raw, int):
        data_raw = [data_raw]

    data_to_send = bytes(data_raw)
    response     = spi.transfer(data_to_send, invert_cs=invert_cs)

    print("response: {}".format(response))


def debug_spi(device, args, *, invert_cs=False):
    _do_debug_spi(device, device.spi, args, invert_cs=invert_cs)


def jtag_debug_spi(device, args):
    """ Command that issues data over a JTAG-over-SPI connection. """

    with device.jtag as jtag:
        spi, _ = device.create_jtag_spi(jtag)
        _do_debug_spi(device, spi, args, invert_cs=False)


def set_led_pattern(device, args):
    device.set_led_pattern(int(args.pattern))

def debug_spi_inv(device, args):
    debug_spi(device, args, invert_cs=True)


def _do_debug_spi_register(device, spi, args):

    # Try to figure out what data the user wants to send.
    address = int(args.address, 0)
    if args.value:
        value = int(args.value, 0)
        is_write = True
    else:
        value = 0
        is_write = False

    try:
        response = spi.register_transaction(address, is_write=is_write, value=value)
        print("0x{:08x}".format(response))
    except IOError as e:
        logging.critical(f"{e}\n")


def debug_spi_register(device, args):
    _do_debug_spi_register(device, device.spi, args)

def jtag_debug_spi_register(device, args):
    _reg, reg = device.create_jtag_spi(device.jtag)
    _do_debug_spi_register(device, reg, args)


Command = namedtuple("Command", ("name", "alias", "args", "help", "handler"),
                     defaults=(None, [], [], None, None))

COMMANDS = [
    # Info queries
    Command("info", handler=print_device_info, args=[(("--force-offline",), dict(action='store_true'))],
            help="Print device info.", ),
    Command("jtag-scan", handler=print_chain_info,
            help="Prints information about devices on the onboard JTAG chain."),
    Command("flash-info", handler=print_flash_info, args=[(("--force-offline",), dict(action='store_true'))],
            help="Prints information about the FPGA's attached configuration flash."),

    # Flash commands
    Command("flash-erase", handler=erase_flash,
            help="Erases the contents of the FPGA's flash memory."),
    Command("flash-program", args=["file", "--offset", (("--fast",), dict(action='store_true'))],
            handler=program_flash, help="Programs the target bitstream onto the FPGA's configuration flash."),
    Command("flash-fast", args=["file", "--offset"], handler=program_flash_fast_deprecated,
            help="Programs a bitstream onto the FPGA's configuration flash using a SPI bridge."),
    Command("flash-read", args=["file", "--offset", "--length"], handler=read_back_flash,
            help="Reads the contents of the attached FPGA's configuration flash."),

    # JTAG commands
    Command("svf", args=["file"], handler=play_svf_file,
            help="Plays a given SVF file over JTAG."),
    Command("configure", args=["file"], handler=configure_fpga,
            help="Uploads a bitstream to the device's FPGA over JTAG."),
    Command("reconfigure", handler=reconfigure_fpga,
            help="Requests the attached ECP5 reconfigure itself from its SPI flash."),
    Command("force-offline", handler=force_fpga_offline,
            help="Forces the board's FPGA offline."),

    # SPI debug exchanges
    Command("spi", args=["bytes"], handler=debug_spi,
            help="Sends the given list of bytes over debug-SPI, and returns the response."),
    Command("spi-inv", args=["bytes"], handler=debug_spi_inv,
            help="Sends the given list of bytes over SPI with inverted CS."),
    Command("spi-reg", args=["address", "value"], handler=debug_spi_register,
            help="Reads or writes to a provided register over the debug-SPI."),

    # JTAG-SPI debug exchanges.
    Command("jtag-spi", args=["bytes"], handler=jtag_debug_spi,
            help="Sends the given list of bytes over SPI-over-JTAG, and returns the response."),
    Command("jtag-reg", args=["address", "value"], handler=jtag_debug_spi_register,
            help="Reads or writes to a provided register of JTAG-tunneled debug SPI."),

    # Misc
    Command("leds", args=["pattern"], handler=set_led_pattern,
            help="Sets the specified pattern for the Debug LEDs."),
]


def main():

    # Set up a simple argument parser.
    parser = argparse.ArgumentParser(description="Apollo FPGA Configuration / Debug tool",
            formatter_class=argparse.RawTextHelpFormatter)
    sub_parsers = parser.add_subparsers(dest="command", metavar="command")
    for command in COMMANDS:
        cmd_parser = sub_parsers.add_parser(command.name, aliases=command.alias, help=command.help)
        cmd_parser.set_defaults(func=command.handler)
        for arg in command.args:
            if isinstance(arg, tuple):
                cmd_parser.add_argument(*arg[0], **arg[1])
            else:
                cmd_parser.add_argument(arg)

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        return

    # Force the FPGA offline by default in most commands to force Apollo mode if needed.
    force_offline = args.force_offline if "force_offline" in args else True
    device = ApolloDebugger(force_offline=force_offline)

    # Set up python's logging to act as a simple print, for now.
    logging.basicConfig(level=logging.INFO, format="%(message)-s")

    # Execute the relevant command.
    args.func(device, args)


if __name__ == '__main__':
    main()
