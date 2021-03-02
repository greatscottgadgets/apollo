#
# This file is part of LUNA.
#
# Copyright (c) 2020 Great Scott Gadgets <info@greatscottgadgets.com>
# SPDX-License-Identifier: BSD-3-Clause

""" Apollo-based ILA transports. """

from abc           import ABCMeta, abstractmethod

from vcd           import VCDWriter
from vcd.gtkw      import GTKWSave

from .support.bits import bits

class ILAFrontend(metaclass=ABCMeta):
    """ Class that communicates with an ILA module and emits useful output. """

    def __init__(self, ila):
        """
        Parameters:
            ila -- The ILA object to work with.
        """
        self.ila = ila
        self.samples = None


    @abstractmethod
    def _read_samples(self):
        """ Read samples from the target ILA. Should return an iterable of samples. """


    def _parse_sample(self, raw_sample):
        """ Converts a single binary sample to a dictionary of names -> sample values. """

        position = 0
        sample   = {}

        # Split our raw, bits(0) signal into smaller slices, and associate them with their names.
        for signal in self.ila.signals:
            signal_width = len(signal)
            signal_bits  = raw_sample[position : position + signal_width]
            position += signal_width

            sample[signal.name] = signal_bits

        return sample


    def _parse_samples(self, raw_samples):
        """ Converts raw, binary samples to dictionaries of name -> sample. """
        return [self._parse_sample(sample) for sample in raw_samples]


    def refresh(self):
        """ Fetches the latest set of samples from the target ILA. """
        self.samples = self._parse_samples(self._read_samples())


    def enumerate_samples(self):
        """ Returns an iterator that returns pairs of (timestamp, sample). """

        # If we don't have any samples, fetch samples from the ILA.
        if self.samples is None:
            self.refresh()

        timestamp = 0

        # Iterate over each sample...
        for sample in self.samples:
            yield timestamp, sample

            # ... and advance the timestamp by the relevant interval.
            timestamp += self.ila.sample_period


    def print_samples(self):
        """ Simple method that prints each of our samples; for simple CLI debugging."""

        for timestamp, sample in self.enumerate_samples():
            timestamp_scaled = 1000000 * timestamp
            print(f"{timestamp_scaled:08f}us: {sample}")



    def emit_vcd(self, filename, *, gtkw_filename=None, add_clock=True):
        """ Emits a VCD file containing the ILA samples.

        Parameters:
            filename      -- The filename to write to, or '-' to write to stdout.
            gtkw_filename -- If provided, a gtkwave save file will be generated that
                             automatically displays all of the relevant signals in the
                             order provided to the ILA.
            add_clock     -- If true or not provided, adds a replica of the ILA's sample
                             clock to make change points easier to see.
        """

        # Select the file-like object we're working with.
        if filename == "-":
            stream = sys.stdout
            close_after = False
        else:
            stream = open(filename, 'w')
            close_after = True

        # Create our basic VCD.
        with VCDWriter(stream, timescale=f"1 ns", date='today') as writer:
            first_timestamp = math.inf
            last_timestamp  = 0

            signals = {}

            # If we're adding a clock...
            if add_clock:
                clock_value  = 1
                clock_signal = writer.register_var('ila', 'ila_clock', 'integer', size=1, init=clock_value ^ 1)

            # Create named values for each of our signals.
            for signal in self.ila.signals:
                signals[signal.name] = writer.register_var('ila', signal.name, 'integer', size=len(signal))

            # Dump the each of our samples into the VCD.
            clock_time = 0
            for timestamp, sample in self.enumerate_samples():
                for signal_name, signal_value in sample.items():

                    # If we're adding a clock signal, add any changes necessary since
                    # the last value-change.
                    if add_clock:
                        while clock_time < timestamp:
                            writer.change(clock_signal, clock_time / 1e-9, clock_value)

                            clock_value ^= 1
                            clock_time  += (self.ila.sample_period / 2)

                    # Register the signal change.
                    writer.change(signals[signal_name], timestamp / 1e-9, signal_value.to_int())


        # If we're generating a GTKW, delegate that to our helper function.
        if gtkw_filename:
            assert(filename != '-')
            self._emit_gtkw(gtkw_filename, filename, add_clock=add_clock)


    def _emit_gtkw(self, filename, dump_filename, *, add_clock=True):
        """ Emits a GTKWave save file to accompany a generated VCD.

        Parameters:
            filename      -- The filename to write the GTKW save to.
            dump_filename -- The filename of the VCD that should be opened with this save.
            add_clock     -- True iff a clock signal should be added to the GTKW save.
        """

        with open(filename, 'w') as f:
            gtkw = GTKWSave(f)

            # Comments / context.
            gtkw.comment("Generated by the LUNA ILA.")

            # Add a reference to the dumpfile we're working with.
            gtkw.dumpfile(dump_filename)

            # If we're adding a clock, add it to the top of the view.
            gtkw.trace('ila.ila_clock')

            # Add each of our signals to the file.
            for signal in self.ila.signals:
                gtkw.trace(f"ila.{signal.name}")


    def interactive_display(self, *, add_clock=True):
        """ Attempts to spawn a GTKWave instance to display the ILA results interactively. """

        # Hack: generate files in a way that doesn't trip macOS's fancy guards.
        try:
            vcd_filename = os.path.join(tempfile.gettempdir(), os.urandom(24).hex() + '.vcd')
            gtkw_filename = os.path.join(tempfile.gettempdir(), os.urandom(24).hex() + '.gtkw')

            self.emit_vcd(vcd_filename, gtkw_filename=gtkw_filename)
            subprocess.run(["gtkwave", "-f", vcd_filename, "-a", gtkw_filename])
        finally:
            os.remove(vcd_filename)
            os.remove(gtkw_filename)


class AsyncSerialILAFrontend(ILAFrontend):
    """ UART-based ILA transport.

    Parameters
    ------------
    port: string
        The serial port to use to connect. This is typically a path on *nix systems.
    ila: IntegratedLogicAnalyzer
        The ILA object to work with.
    """

    def __init__(self, *args, ila, **kwargs):
        import serial

        self._port = serial.Serial(*args, **kwargs)
        self._port.reset_input_buffer()

        super().__init__(ila)


    def _split_samples(self, all_samples):
        """ Returns an iterator that iterates over each sample in the raw binary of samples. """

        sample_width_bytes = self.ila.bytes_per_sample

        # Iterate over each sample, and yield its value as a bits object.
        for i in range(0, len(all_samples), sample_width_bytes):
            raw_sample    = all_samples[i:i + sample_width_bytes]
            sample_length = len(Cat(self.ila.signals))

            yield bits.from_bytes(raw_sample, length=sample_length, byteorder='big')


    def _read_samples(self):
        """ Reads a set of ILA samples, and returns them. """

        sample_width_bytes = self.ila.bytes_per_sample
        total_to_read      = self.ila.sample_depth * sample_width_bytes

        # Fetch all of our samples from the given device.
        all_samples = self._port.read(total_to_read)
        return list(self._split_samples(all_samples))


if __name__ == "__main__":
    unittest.main()


class ApolloILAFrontend(ILAFrontend):
    """ Apollo-based transport for ILA samples. """

    def __init__(self, debugger, *, ila, use_inverted_cs=False):
        """
        Parameters:
            debugger        -- The apollo debugger connection to use for transport.
            ila             -- The ILA object to work with.
            use_inverted_cs -- Use a simple CS multiplexing scheme, where the ILA samples
                               are read out by pulsing SCK while CS is not asserted.
        """
        self._debugger = debugger
        self._use_inverted_cs = use_inverted_cs

        super().__init__(ila)


    def _split_samples(self, all_samples):
        """ Returns an iterator that iterates over each sample in the raw binary of samples. """

        from nmigen import Cat

        sample_width_bytes = self.ila.bytes_per_sample

        # Iterate over each sample, and yield its value as a bits object.
        for i in range(0, len(all_samples), sample_width_bytes):
            raw_sample    = all_samples[i:i + sample_width_bytes]
            sample_length = len(Cat(self.ila.signals))

            yield bits.from_bytes(raw_sample, length=sample_length, byteorder='big')


    def _read_samples(self):
        """ Reads a set of ILA samples, and returns them. """

        sample_width_bytes = self.ila.bytes_per_sample
        total_to_read      = self.ila.sample_depth * sample_width_bytes

        # Fetch all of our samples from the given device.
        all_samples = \
             self._debugger.spi.transfer(b"\0" * total_to_read, invert_cs=self._use_inverted_cs)

        return list(self._split_samples(all_samples))


