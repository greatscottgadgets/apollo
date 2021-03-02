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
program    -- Programs the target bitstream onto the attached FPGA.
jtag-scan  -- Prints information about devices on the onboard JTAG chain.
svf        -- Plays a given SVF file over JTAG.
spi        -- Sends the given list of bytes over debug-SPI, and returns the response.
spi-inv    -- Sends the given list of bytes over SPI with inverted CS.
spi-reg    -- Reads or writes to a provided register over the debug-SPI.
jtag-spi   -- Sends the given list of bytes over SPI-over-JTAG, and returns the response.
jtag-reg   -- Reads or writes to a provided register of JTAG-tunneled debug SPI.
"""


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


def reconfigure_ecp5(device, args):
    """ Command that requests the attached ECP5 reconfigure itself from its MSPI flash. """

    device.soft_reset()



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
        'info':        print_device_info,
        'jtag-scan':   print_chain_info,

        # JTAG commands
        'svf':         play_svf_file,
        'configure':   configure_fpga,
        'reconfigure': reconfigure_ecp5,

        # SPI debug exchanges
        'spi':         debug_spi,
        'spi-inv':     debug_spi_inv,
        'spi-reg':     debug_spi_register,

        # JTAG-SPI debug exchanges.
        'jtag-spi':    jtag_debug_spi,
        'jtag-reg':    jtag_debug_spi_register,

        # Misc
        'leds':        set_led_pattern,

    }


    # Set up a simple argument parser.
    parser = argparse.ArgumentParser(description="Apollo FPGA Configuration / Debug tool",
            formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('command', metavar='command:', choices=commands, help=COMMAND_HELP_TEXT)
    parser.add_argument('argument', metavar="[argument]", nargs='?',
                        help='the argument to the given command; often a filename')
    parser.add_argument('value', metavar="[value]", nargs='?',
                        help='the value to a register write command')

    args = parser.parse_args()
    device = ApolloDebugger()

    # Set up python's logging to act as a simple print, for now.
    logging.basicConfig(level=logging.INFO, format="%(message)-s")

    # Execute the relevant command.
    command = commands[args.command]
    command(device, args)


if __name__ == '__main__':
    main()
