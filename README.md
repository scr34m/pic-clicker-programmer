MicroElektronika's PIC clicker programmer OSX / Linux
===

A small commandline libusb application which is used to programm MicroElektronika's [PIC clicker](https://shop.mikroe.com/development-boards/starter/clicker/pic18fj) development board with pre loaded USB HID bootloader. 

Bootloader source code can be found in "mikroProg_Suite_For_PIC" package under "C:\Program Files\Mikroelektronika\mikroC PRO for PIC\Examples\Other\USB HID Bootloader"

Original application for [Windows](https://download.mikroe.com/examples/starter-boards/clicker/pic/pic-clicker-pic18f47j53-mikrobootloader-usb-hid-v100.zip) by MicroElektronika. 

The goal was to port Windows version, so on OSX / Linux possible to burn code to the PIC, so it simplifies development with SDCC.

For Intel HEX parsing the [ihex](https://github.com/arkku/ihex) library used.

Example Usage
===

Default ids are vendor: 0x1234, product: 0x001, but you can change it.

    # Simple usage:
    prog ~/Documents/PIC/Blink/blink.hex

    # Changing VID and PID to listening (decimal format!):
    prog ~/Documents/PIC/Blink/blink.hex 8738 1000
