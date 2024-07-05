# Apollo FPGA Programmer / Debugger

Apollo is the on-board debugger and programmer on [Cynthion](https://greatscottgadgets.com/cynthion/). It is used to load gateware over USB onto Cynthion's FPGA. Alternatively it may be used as an on-board or external debugger for certain other FPGA platforms.

Apollo consists of two main parts: firmware for the on-board debug microcontroller and Python-based software for the host computer.

## Installing Host Software

To install the apollo-fpga Python module and the `apollo` command-line tool or to upgrade them to the latest version run:
```
pip install --upgrade apollo-fpga
```

## Building and Installing Firmware

To upgrade Apollo firmware on a Cynthion it is typically not necessary to compile the firmware yourself. Instead follow [Upgrading Cynthion Device Firmware](https://cynthion.readthedocs.io/en/latest/getting_started.html#upgrading-cynthion-device-firmware).

To compile and install onto Cynthion run:

```
$ cd apollo/firmware
$ make APOLLO_BOARD=cynthion get-deps dfu
```

This will download dependencies, compile the firmware, and install it onto Cynthion with [Saturn-V](https://github.com/greatscottgadgets/saturn-v).

Alternatively you can use variables to specify an older hardware revision:

```
$ cd apollo/firmware
$ make APOLLO_BOARD=cynthion BOARD_REVISION_MAJOR=0 BOARD_REVISION_MINOR=3 get-deps dfu
```

Once installation is complete, LED A should activate, indicating that Apollo is running.
