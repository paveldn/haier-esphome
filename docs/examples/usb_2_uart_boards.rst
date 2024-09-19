List of confirmed board that supports that have native USB support and can communicate using UART protocol
==========================================================================================================

Here you can find a list of confirmed boards that have native USB support and can communicate using UART protocol with sample configuration for each case. Thease configurations not compleate and should be considered as a starting point for integrating your Haier AC.


ESP32-S3 based boards
---------------------

Currently, the following boards have native USB support and can communicate using UART protocol:

- `M5Stack AtomS3U <https://shop.m5stack.com/products/atoms3u>`_
- `Lilygo T-Dongle S3 <https://www.lilygo.cc/products/t-dongle-s3?variant=42455191519413>`_
- `M5Stamp ESP32S3 Module <https://shop.m5stack.com/products/m5stamp-esp32s3-module>`_ with USB-C to USB-A male adapter.

**Sample ESPHome Configuration that works for all this boards:**

.. example_yaml:: usb_s3.yaml

ESP32-C3 based boards
---------------------

Currently, only one board with ESP32-C3 confirmed that have native USB support and can communicate using UART protocol:

- `M5Stamp C3U (white color) <https://shop.m5stack.com/products/m5stamp-c3u-mate-with-pin-headers>`_ with USB-C to USB-A male adapter. **But be careful: M5Stamp C3 board (black color, without U at the end) have a dedicated chip for USB and can't be used for UART communication!**

**Sample ESPHome Configuration that works for this board:**

.. example_yaml:: usb_c3u.yaml

ESP32-C6 based boards
---------------------

Curently ESP32-C6 based boards are not supported by platformio version that used by ESPHome. But it is posible to use it with custom platform and SDK version.
Curently, only confirmed board with ESP32-C6 that can be used for UART over USB communication:

- `M5Stack NanoC6 Dev Kit <https://shop.m5stack.com/products/m5stack-nanoc6-dev-kit>`_ with USB-C to USB-A male adapter.

**Sample ESPHome Configuration that works for this board:**

.. example_yaml:: usb_c6.yaml