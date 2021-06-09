#!/usr/bin/env python3
#
# This file is part of LUNA
#
# Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
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

from apollo_fpga import ApolloDebugger
from apollo_fpga.jtag import JTAGChain, JTAGPatternError
from apollo_fpga.ecp5 import ECP5_JTAGProgrammer
from apollo_fpga.onboard_jtag import *


COMMAND_HELP_TEXT = \
"""configure  -- Uploads a bitstream to the device's FPGA over JTAG.
program       -- Programs the target bitstream onto the attached FPGA.
force-offline -- Forces the board's FPGA offline; useful for recovering a "bricked" JTAG connection.
jtag-scan     -- Prints information about devices on the onboard JTAG chain.
flash-info    -- Prints information about the FPGA's attached configuration flash.
flash-erase   -- Erases the contents of the FPGA's flash memory.
flash-program -- Programs the target bitstream onto the attached FPGA.
svf           -- Plays a given SVF file over JTAG.
spi           -- Sends the given list of bytes over debug-SPI, and returns the response.
spi-inv       -- Sends the given list of bytes over SPI with inverted CS.
spi-reg       -- Reads or writes to a provided register over the debug-SPI.
jtag-spi      -- Sends the given list of bytes over SPI-over-JTAG, and returns the response.
jtag-reg      -- Reads or writes to a provided register of JTAG-tunneled debug SPI.
"""

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
    logging.info(f"\tSerial number: {device.serial_number}\n")


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

    if not args.argument:
        logging.error("You must provide an SVF filename to play!\n")
        sys.exit(-1)

    with device.jtag as jtag:
        try:
            jtag.play_svf_file(args.argument)
        except JTAGPatternError:
            # Our SVF player has already logged the error to stderr.
            logging.error("")


def configure_fpga(device, args):
    """ Command that prints information about devices connected to the scan chain to the console. """

    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)

        with open(args.argument, "rb") as f:
            bitstream = f.read()

        programmer.configure(bitstream)


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
    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)

        with open(args.argument, "rb") as f:
            bitstream = f.read()

        programmer.flash(bitstream)

    device.soft_reset()

def read_back_flash(device, args):

    # XXX abstract this?
    length = ast.literal_eval(args.value) if args.value else (4 * 1024 * 1024)

    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)

        with open(args.argument, "wb") as f:
            bitstream = programmer.read_flash(length)
            f.write(bitstream)

    device.soft_reset()



def print_flash_info(device, args):
    """ Command that prints information about the currently connected FPGA's configuration flash. """
    ensure_unconfigured(device)

    with device.jtag as jtag:
        programmer = device.create_jtag_programmer(jtag)
        manufacturer, device = programmer.read_flash_id()

        if manufacturer == 0xFF:
            logging.info("No flash detected.")
            return

        logging.info("")
        logging.info(f"Detected an FPGA-connected SPI configuration flash!")

        try:
            logging.info(f"\tManufacturer: {JEDEC_MANUFACTURERS[manufacturer]} ({manufacturer:02x})")
        except KeyError:
            logging.info(f"\tUnknown manufacturer ({manufacturer:02x}).")

        try:
            logging.info(f"\tDevice: {JEDEC_PARTS[device]} ({device:06x})")
        except KeyError:
            logging.info(f"\tUnknown device ({device:06x}).")

        logging.info("")


def reconfigure_fpga(device, args):
    """ Command that requests the attached ECP5 reconfigure itself from its SPI flash. """
    device.soft_reset()


def force_fpga_offline(device, args):
    """ Command that requests the attached ECP5 be held unconfigured. """
    device.force_fpga_offline()
    logging.warning("\nWARNING: Forced the FPGA into an unconfigured state!\n")
    logging.warning("Configuration will not work properly until you run 'apollo reconfigure' or reset the device.")
    logging.warning("Flashing the FPGA's configuration SPI flash will still work as intended.\n\n")


def _do_debug_spi(device, spi, args, *, invert_cs):

    # Try to figure out what data the user wants to send.
    data_raw = ast.literal_eval(args.argument)
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
    device.set_led_pattern(int(args.argument))

def debug_spi_inv(device, args):
    debug_spi(device, args, invert_cs=True)


def _do_debug_spi_register(device, spi, args):

    # Try to figure out what data the user wants to send.
    address = int(args.argument, 0)
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


def main():

    commands = {
        # Info queries
        'info':          print_device_info,
        'jtag-scan':     print_chain_info,
        'flash-info':    print_flash_info,

        # Flash commands
        'flash-erase':   erase_flash,
        'flash':         program_flash,
        'flash-program': program_flash,
        'flash-read':    read_back_flash,

        # JTAG commands
        'svf':           play_svf_file,
        'configure':     configure_fpga,
        'reconfigure':   reconfigure_fpga,
        'force-offline': force_fpga_offline,

        # SPI debug exchanges
        'spi':           debug_spi,
        'spi-inv':       debug_spi_inv,
        'spi-reg':       debug_spi_register,

        # JTAG-SPI debug exchanges.
        'jtag-spi':      jtag_debug_spi,
        'jtag-reg':      jtag_debug_spi_register,

        # Misc
        'leds':          set_led_pattern,

    }


    # Set up a simple argument parser.
    parser = argparse.ArgumentParser(description="Apollo FPGA Configuration / Debug tool",
            formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('command', metavar='command:', choices=commands, help=COMMAND_HELP_TEXT)
    parser.add_argument('argument', metavar="[argument]", nargs='?',
                        help='the argument to the given command; often a filename')
    parser.add_argument('value', metavar="[value]", nargs='?',
                        help='the value to a register write command, or the length for flash read')

    args = parser.parse_args()
    device = ApolloDebugger()

    # Set up python's logging to act as a simple print, for now.
    logging.basicConfig(level=logging.INFO, format="%(message)-s")

    # Execute the relevant command.
    command = commands[args.command]
    command(device, args)


if __name__ == '__main__':
    main()
