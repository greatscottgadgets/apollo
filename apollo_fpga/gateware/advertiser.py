#
# This file is part of Apollo.
#
# Copyright (c) 2023 Great Scott Gadgets <info@greatscottgadgets.com>
# SPDX-License-Identifier: BSD-3-Clause

""" Controllers for communicating with Apollo through the FPGA_ADV pin """

from amaranth                       import Elaboratable, Module, Signal, Mux

from luna.gateware.usb.usb2.request import USBRequestHandler
from usb_protocol.types             import USBRequestType, USBRequestRecipient


class ApolloAdvertiser(Elaboratable):
    """ Gateware that implements an announcement signal for Apollo using the FPGA_ADV pin.

    Used to tell Apollo that the gateware wants to use the CONTROL port.
    Apollo will keep the port switch connected to the FPGA after a reset as long as this 
    signal is being received and the port takeover is allowed.

    I/O ports:
        I: stop -- Advertisement signal is stopped if this line is asserted.
    """
    def __init__(self, pad=None, clk_freq_hz=None):
        self.pad         = pad
        self.clk_freq_hz = clk_freq_hz
        self.stop        = Signal()

    def default_request_handler(self, if_number):
        return ApolloAdvertiserRequestHandler(if_number, self.stop)

    def elaborate(self, platform):
        m = Module()

        # Handle default values.
        if self.pad is None:
            self.pad = platform.request("int")
        if self.clk_freq_hz is None:
            self.clk_freq_hz = platform.DEFAULT_CLOCK_FREQUENCIES_MHZ["sync"] * 1e6

        # Generate clock with 20ms period.
        half_period = int(self.clk_freq_hz * 10e-3)
        timer       = Signal(range(half_period))
        clk         = Signal()
        m.d.sync   += timer.eq(Mux(timer == half_period-1, 0, timer+1))
        with m.If((timer == 0) & (~self.stop)):
            m.d.sync += clk.eq(~clk)

        # Drive the FPGA_ADV pin with the generated clock signal.
        m.d.comb += self.pad.o.eq(clk)
        
        return m


class ApolloAdvertiserRequestHandler(USBRequestHandler):
    """ Request handler for ApolloAdvertiser. 
    
    Implements default vendor requests related to ApolloAdvertiser.
    """

    """ The bInterfaceProtocol version supported by this request handler. """
    PROTOCOL_VERSION = 0x00

    REQUEST_APOLLO_ADV_STOP = 0xF0

    def __init__(self, if_number, stop_pin):
        super().__init__()
        self.if_number = if_number
        self.stop_pin  = stop_pin

    def elaborate(self, platform):
        m = Module()

        interface         = self.interface
        setup             = self.interface.setup

        #
        # Vendor request handlers.

        with m.If((setup.type == USBRequestType.VENDOR) & \
                  (setup.recipient == USBRequestRecipient.INTERFACE) & \
                  (setup.index == self.if_number)):
            
            with m.If(setup.request == self.REQUEST_APOLLO_ADV_STOP):

                # Notify that we want to manage this request
                m.d.comb += interface.claim.eq(1)

                # Once the receive is complete, respond with an ACK.
                with m.If(interface.rx_ready_for_response):
                    m.d.comb += interface.handshakes_out.ack.eq(1)

                # If we reach the status stage, send a ZLP.
                with m.If(interface.status_requested):
                    m.d.comb += self.send_zlp()
                    m.d.usb += self.stop_pin.eq(1)

        return m
