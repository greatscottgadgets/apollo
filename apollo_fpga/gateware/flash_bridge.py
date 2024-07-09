#
# This file is part of Apollo.
#
# Copyright (c) 2023 Great Scott Gadgets <info@greatscottgadgets.com>
# SPDX-License-Identifier: BSD-3-Clause

import usb.core

from amaranth                          import Signal, Elaboratable, Module, Cat, C
from amaranth.lib.fifo                 import AsyncFIFO

from luna.gateware.interface.flash     import ECP5ConfigurationFlashInterface
from luna.gateware.interface.spi       import SPIBus
from luna.gateware.stream              import StreamInterface
from luna.gateware.usb.usb2.request    import USBRequestHandler
from luna.gateware.usb.request.windows import MicrosoftOS10DescriptorCollection
from luna.gateware.usb.request.windows import MicrosoftOS10RequestHandler
from luna.usb2                         import USBDevice, USBStreamInEndpoint, USBStreamOutEndpoint

from usb_protocol.types                import USBRequestType, USBRequestRecipient
from usb_protocol.emitters             import DeviceDescriptorCollection
from usb_protocol.emitters.descriptors.standard import get_string_descriptor

from apollo_fpga                       import ApolloDebugger
from .advertiser                       import ApolloAdvertiser, ApolloAdvertiserRequestHandler

VENDOR_ID  = 0x1209
PRODUCT_ID = 0x000F

BULK_ENDPOINT_NUMBER = 1
MAX_BULK_PACKET_SIZE = 512


class SPIStreamController(Elaboratable):
    """ Class that drives a SPI bus with data from input stream packets.
    Data received from the device is returned as another packet."""

    def __init__(self):
        self.period = 4  # powers of two only
        self.bus    = SPIBus()
        self.input  = StreamInterface()
        self.output = StreamInterface()

    def elaborate(self, platform):
        m = Module()

        # Counter for clock generation
        cycles = Signal(range(self.period))

        # Generate strobes for clock edges
        sck_fall = Signal()
        sck_rise = Signal()
        sck_d    = Signal()
        m.d.sync += sck_d.eq(self.bus.sck)
        m.d.comb += [
            sck_fall.eq( sck_d & ~self.bus.sck),  # falling edge
            sck_rise.eq(~sck_d &  self.bus.sck),  # rising edge
        ]

        # I/O shift registers, bit counter and last flag
        shreg_o = Signal(8)
        shreg_i = Signal(8)
        count_o = Signal(range(8))
        last    = Signal()

        m.d.comb += [
            self.bus.sdi        .eq(shreg_o[-1]),
            self.output.payload .eq(shreg_i),
        ]

        with m.FSM() as fsm:
            m.d.comb += self.bus.cs.eq(~fsm.ongoing('IDLE'))

            with m.State("IDLE"):
                m.d.comb += [
                    self.input.ready    .eq(1),
                    self.bus.sck        .eq(0),
                ]
                with m.If(self.input.valid):
                    m.next = 'SHIFT'

            with m.State("WAIT"):
                m.d.comb += [
                    self.input.ready    .eq(1),
                    self.bus.sck        .eq(0),
                ]
                with m.If(self.input.valid):
                    m.next = 'SHIFT'

            with m.State("SHIFT"):
                m.d.comb += [
                    self.input.ready    .eq(sck_fall & (count_o == 0) & ~last),
                    self.bus.sck        .eq(cycles[-1])
                ]
                m.d.sync += cycles.eq(cycles + 1)

                # Read logic, latch on rising edge
                m.d.sync += self.output.valid.eq(0)
                with m.If(sck_rise):
                    m.d.sync += [
                        shreg_i             .eq(Cat(self.bus.sdo, shreg_i[:-1])),
                        self.output.valid   .eq(count_o == 0),
                        self.output.last    .eq(last),
                    ]

                # Write logic, setup on falling edge
                with m.If(sck_fall):
                    m.d.sync += [
                        shreg_o             .eq(Cat(C(0,1), shreg_o[:-1])),
                        count_o             .eq(count_o - 1),
                    ]
                    with m.If(count_o == 0):
                        with m.If(last):
                            m.next = 'END'
                        with m.Elif(~self.input.valid):
                            m.next = 'WAIT'

            with m.State("END"):
                m.d.comb += [
                    self.input.ready    .eq(0),
                    self.bus.sck        .eq(0),
                ]
                m.d.sync += [
                    last    .eq(0),
                    cycles  .eq(0),
                ]
                m.next = 'IDLE'

        with m.If(self.input.valid & self.input.ready):
            m.d.sync += [
                shreg_o     .eq(self.input.payload),
                last        .eq(self.input.last),
                count_o     .eq(7),
            ]

        return m


class FlashBridgeRequestHandler(USBRequestHandler):
    """ Request handler that can trigger a FPGA reconfiguration. """

    REQUEST_TRIGGER_RECONF = 0

    def __init__(self, if_number):
        super().__init__()
        self.if_number = if_number

    def elaborate(self, platform):
        m = Module()

        interface         = self.interface
        setup             = self.interface.setup

        #
        # Vendor request handlers.

        self_prog = platform.request("self_program", dir="o").o

        with m.If((setup.type == USBRequestType.VENDOR) & \
                  (setup.recipient == USBRequestRecipient.INTERFACE) & \
                  (setup.index == self.if_number)):

            with m.Switch(setup.request):

                with m.Case(self.REQUEST_TRIGGER_RECONF):

                    m.d.comb += interface.claim.eq(1)

                    # Once the receive is complete, respond with an ACK.
                    with m.If(interface.rx_ready_for_response):
                        m.d.comb += interface.handshakes_out.ack.eq(1)

                    # If we reach the status stage, send a ZLP.
                    with m.If(interface.status_requested):
                        m.d.comb += self.send_zlp()
                        m.d.usb += self_prog.eq(1)

        return m


class FlashBridgeSubmodule(Elaboratable):
    """ Implements gateware for the USB<->SPI bridge. Intended to use as a submodule
        See example in FlashBridge """

    def __init__(self, endpoint):
        # Endpoint number for the in/out stream endpoints
        self.endpoint = endpoint

        # Define endpoints
        self.endpoint_out = USBStreamOutEndpoint(
            endpoint_number=endpoint,
            max_packet_size=MAX_BULK_PACKET_SIZE,
        )
        self.endpoint_in = USBStreamInEndpoint(
            endpoint_number=endpoint,
            max_packet_size=MAX_BULK_PACKET_SIZE
        )

    def elaborate(self, platform):
        m = Module()

        stream_in  = self.endpoint_in.stream
        stream_out = self.endpoint_out.stream

        # Use two small asynchronous FIFOs for crossing clock domains
        spi     = SPIStreamController()
        spi_bus = ECP5ConfigurationFlashInterface(bus=platform.request('spi_flash'), use_cs=True)
        tx_fifo = AsyncFIFO(width=8+1, depth=8, w_domain="usb", r_domain="sync")
        rx_fifo = AsyncFIFO(width=8+1, depth=8, w_domain="sync", r_domain="usb")

        m.submodules += spi
        m.submodules += spi_bus
        m.submodules += tx_fifo
        m.submodules += rx_fifo

        m.d.comb += [
            # Connect output from USB host to transmission FIFO
            tx_fifo.w_data      .eq(Cat(stream_out.payload, stream_out.last)),
            tx_fifo.w_en        .eq(stream_out.valid),
            stream_out.ready    .eq(tx_fifo.w_rdy),

            # Connect transmission FIFO to the SPI controller
            Cat(spi.input.payload, spi.input.last).eq(tx_fifo.r_data),
            spi.input.valid     .eq(tx_fifo.r_rdy),
            tx_fifo.r_en        .eq(spi.input.ready),

            # Connect output from SPI controller to reception FIFO
            rx_fifo.w_data      .eq(Cat(spi.output.payload, spi.output.last)),
            rx_fifo.w_en        .eq(spi.output.valid),
            spi.output.ready    .eq(1),  # ignore rx_fifo.w_rdy

            # Connect reception FIFO to USB host input
            Cat(stream_in.payload, stream_in.last).eq(rx_fifo.r_data),
            stream_in.valid     .eq(rx_fifo.r_rdy),
            rx_fifo.r_en        .eq(stream_in.ready),

            # Connect the SPI bus to our SPI controller
            spi_bus.sck         .eq(spi.bus.sck),
            spi_bus.sdi         .eq(spi.bus.sdi),
            spi_bus.cs          .eq(spi.bus.cs),
            spi.bus.sdo         .eq(spi_bus.sdo),
        ]

        return m


class FlashBridge(Elaboratable):

    def create_descriptors(self, sharing):
        """ Create the descriptors we want to use for our device. """

        descriptors = DeviceDescriptorCollection()

        #
        # We'll add the major components of the descriptors we we want.
        # The collection we build here will be necessary to create a standard endpoint.
        #

        # We'll need a device descriptor...
        with descriptors.DeviceDescriptor() as d:
            d.idVendor           = VENDOR_ID
            d.idProduct          = PRODUCT_ID

            d.iManufacturer      = "Apollo Project"
            d.iProduct           = "Configuration Flash Bridge"

            d.bNumConfigurations = 1

        # ... and a description of the USB configuration we'll provide.
        with descriptors.ConfigurationDescriptor() as c:

            with c.InterfaceDescriptor() as i:
                i.bInterfaceNumber = 0
                i.bInterfaceClass = 0xFF
                i.bInterfaceSubclass = 0x01
                i.bInterfaceProtocol = 0x00

                with i.EndpointDescriptor() as e:
                    e.bEndpointAddress = BULK_ENDPOINT_NUMBER
                    e.wMaxPacketSize   = MAX_BULK_PACKET_SIZE

                with i.EndpointDescriptor() as e:
                    e.bEndpointAddress = 0x80 | BULK_ENDPOINT_NUMBER
                    e.wMaxPacketSize   = MAX_BULK_PACKET_SIZE

            # If sharing the port, add the Apollo stub interface.
            if sharing is not None:
                with c.InterfaceDescriptor() as i:
                    i.bInterfaceNumber = 1
                    i.bInterfaceClass = 0xFF
                    i.bInterfaceSubclass = 0x00
                    i.bInterfaceProtocol = ApolloAdvertiserRequestHandler.PROTOCOL_VERSION

        return descriptors

    def elaborate(self, platform):
        m = Module()

        # Generate our domain clocks/resets.
        m.submodules.car = platform.clock_domain_generator()

        # Create our USB device interface...
        phy_name = platform.apollo_gateware_phy
        ulpi = platform.request(phy_name)
        m.submodules.usb = usb = USBDevice(bus=ulpi)

        # Check how the port is shared with Apollo.
        sharing = platform.port_sharing(phy_name)

        # Create descriptors.
        descriptors = self.create_descriptors(sharing)

        # Add Microsoft OS 1.0 descriptors for Windows compatibility.
        descriptors.add_descriptor(get_string_descriptor("MSFT100\xee"), index=0xee)
        msft_descriptors = MicrosoftOS10DescriptorCollection()
        with msft_descriptors.ExtendedCompatIDDescriptor() as c:
            with c.Function() as f:
                f.bFirstInterfaceNumber = 0
                f.compatibleID          = 'WINUSB'
            if sharing is not None:
                with c.Function() as f:
                    f.bFirstInterfaceNumber = 1
                    f.compatibleID          = 'WINUSB'

        # Add our standard control endpoint to the device.
        control_ep = usb.add_standard_control_endpoint(descriptors, avoid_blockram=True)

        # Add handler for Microsoft descriptors.
        msft_handler = MicrosoftOS10RequestHandler(msft_descriptors, request_code=0xee)
        control_ep.add_request_handler(msft_handler)

        # Add our vendor request handler to the control endpoint.
        control_ep.add_request_handler(FlashBridgeRequestHandler(0))

        # If needed, create an advertiser and add its request handler.
        if sharing == "advertising":
            adv = m.submodules.adv = ApolloAdvertiser()
            control_ep.add_request_handler(adv.default_request_handler(1))

        # Add bridge submodule and input/output stream endpoints to our device.
        m.submodules.bridge = bridge = FlashBridgeSubmodule(BULK_ENDPOINT_NUMBER)
        usb.add_endpoint(bridge.endpoint_in)
        usb.add_endpoint(bridge.endpoint_out)

        # Connect our device
        m.d.comb += usb.connect.eq(1)

        return m


class FlashBridgeNotFound(IOError):
    pass

class FlashBridgeConnection:
    def __init__(self):
        # Try to create a connection to our configuration flash bridge.
        device = ApolloDebugger._find_device(
            ids=[(VENDOR_ID, PRODUCT_ID)],
            custom_match=self._find_cfg_flash_bridge
        )

        # If we couldn't find the bridge, bail out.
        if device is None:
            raise FlashBridgeNotFound("Unable to find device")

        self.device = device
        self.interface, self.endpoint = self._find_cfg_flash_bridge(device, get_ep=True)

    def __del__(self):
        self.request_handoff()

    @staticmethod
    def _find_cfg_flash_bridge(dev, get_ep=False):
        for cfg in dev:
            for intf in usb.util.find_descriptor(cfg, find_all=True, bInterfaceClass=0xFF, bInterfaceSubClass=0x01):
                if not get_ep:
                    return True
                return intf.bInterfaceNumber, intf[0].bEndpointAddress
        return None, None if get_ep else False

    def trigger_reconfiguration(self):
        """ Triggers the target FPGA to reconfigure itself from its flash chip. """
        request_type = usb.ENDPOINT_OUT | usb.RECIP_INTERFACE | usb.TYPE_VENDOR
        return self.device.ctrl_transfer(request_type, 0, wValue=0, wIndex=self.interface)

    def request_handoff(self):
        """ Requests the gateware to liberate the USB port. """
        try:
            ApolloDebugger._request_handoff(self.device)
        except Exception:
            pass

    def transfer(self, data):
        """ Performs a SPI transfer, targeting the configuration flash."""
        tx_sent = self.device.write(self.endpoint, data)
        assert tx_sent == len(data)
        rx_data = self.device.read(0x80 | self.endpoint, 512)
        assert len(rx_data) == tx_sent, f'Expected {tx_sent} bytes, received {len(rx_data)}'
        return rx_data
