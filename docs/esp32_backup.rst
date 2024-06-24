How to backup the original image and flash ESPHome to the ESP32 Haier module
============================================================================

**It is strongly recommended to make a backup of the original flash
content before flashing ESPHome!**

To make a backup and to flash the new firmware you will need to use a
USB to TTL converter and solder wires to access UART0 on board (or use
something like this: `Pogo Pin Probe Clip 2x5p 2.54
mm <https://www.tinytronics.nl/shop/en/tools-and-mounting/measuring/accessories/test-probe-with-clamp-pogo-pin-2x5p>`__)

**UART0 pinout:**

.. figure:: esphome-docs/climate/images/haier_pinout.jpg
    :align: center
    :width: 70.0%

To put the device in the flash mode you will need to shortcut GPIO0 to
the ground before powering the device.

Once the device is in flash mode you can make a full backup of the
original firmware in case you would like to return the module to its
factory state. To make a backup you can use
`esptool <https://github.com/espressif/esptool>`__. Command to make a
full flash backup:

**python esptool.py -b 115200 –port read_flash 0x00000 0x400000
flash_4M.bin**

After this, you can flash firmware using ESPHome tools (dashboard,
website, esphome command, etc)
