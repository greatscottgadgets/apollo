# Apollo FPGA Programmer / Debugger

Apollo is the on-board debugger and programmer on [Cynthion](https://greatscottgadgets.com/cynthion/). It is used to load gateware over USB onto Cynthion's FPGA. Alternatively it may be used as an on-board or external debugger for certain other FPGA platforms.

Apollo consists of two main parts: firmware for the on-board debug microcontroller and Python-based software for the host computer.

## Building and Installing Firmware

First activate Cynthion's Saturn-V bootloader by holding down the PROGRAM button while connecting power or while pressing and releasing the RESET button. LED C will blink, indicating that Saturn-V is running.

To compile for the latest Cynthion hardware revision, type:

```
$ cd apollo/firmware
$ make APOLLO_BOARD=cynthion get-deps dfu
```

This will download dependencies, compile the firmware, and install it onto Cynthion with Saturn-V.

Alternatively you can use variables to specify the hardware revision:

```
$ cd apollo/firmware
$ make APOLLO_BOARD=cynthion BOARD_REVISION_MAJOR=1 BOARD_REVISION_MINOR=3 get-deps dfu
```

Once installation is complete, LED E should blink, indicating that Apollo is running and idle.
