Hardware keyboard remapper
==========================

This software is intened to run on embeded computers to provide a transparent
configurable keyboard remapper: Plug a keyboard at one end, a computer at the
other, provide the effective and target keyboard mappings and it will do the rest.

Hardware requirements
---------------------

You need either one board that can provide one USB Host and one USB device port,
or two interconnected boards with a OTG USB port. This software was successfuly tested on:

- One Intel Galileo2 board.
- Two raspberry PI zero connected through 10 GPIOs.

At the time of this writing, the only suitable board in the raspberry family
is the PI zero, since a USB device port is required to emulate a keyboard.


Deployment on two PI zeros
--------------------------

In this setup one PI is connected to a keyboard, and the other to a computer.
The two boards communicate using a serial link using 10 of their generic GPIOs
(to leave the only UART available free for console debugging).

### Hardware wiring

To achieve this setup you need to wire the following pins from the two GPIO headers:
- At least one GROUND pin.
- One of the two 5V pins (that way the computer plugged in USB to one of the
boards will power both of them)
- 10 of the GPIOs, it doesn't matter which.

### Kernel update

The board that will be connected to the computer will require a kernel update
to enable the USB gadget drivers. You can follow the tutorial from here:
https://learn.adafruit.com/turning-your-raspberry-pi-zero-into-a-usb-gadget/serial-gadget

### Software install, board 1 (to keyboard)

This board runs the simplest part of the software, 'kbdtobang': it just listens
for keyboard events and transmit them down the serial-bitbang link to the
other board.

Compiles kbdtobang either natively on the PI, or using a cross-compiler:

    g++ -std=c++11 -pthread -I. -Ibitbang -o kbdtobang kbdtobang.cc \
    bitbang/bitbang.cc bitbang/gpio.cc bitbang/gpio_sys.cc bitbang/gpio_dummy.cc

Then copy bitbang to the PI, and run 'sudo crontab -e', and copy the following line
in the editor:

    @reboot /home/pi/kbdtobang sys 4,5,6,13,19,26,12,16,20,21 1000

Replace the comma-separated list of numbers by the 10 GPIO pin numbers you wired

### Software install, board 2 (to computer)




