Haier Smart Modules
===================

Older Haier Models
------------------

Older Haier models controlled by the SmartAir2 application use the KZW-W002 module, which looks like this:

.. raw:: html

  <p><a href="./images/KZW-W002.jpg?raw=true"><img src="./images/KZW-W002.jpg?raw=true" height="50%" width="50%"></a></p>

This module is based on ARM architecture and cannot be reused for ESPHome. The module works exclusively with the SmartAir2 application but supports both versions of Haier protocols. Therefore, it is possible that an HVAC system using the KZW-W002 module and the SmartAir2 application might also support the hOn protocol.

Newer Haier Models
------------------

Newer Haier models use a module called ESP32-for-Haier. This module is an ESP32 single-core board with an ESP32-S0WD chip. The module board looks like this:

**Front:**

.. raw:: html

  <p><a href="./images/ESP32_front.jpg?raw=true"><img src="./images/ESP32_front.jpg?raw=true" height="50%" width="50%"></a></p>
  
**Back:**

.. raw:: html

  <a href="./images/ESP32_back.jpg?raw=true"><img src="./images/ESP32_back.jpg?raw=true" height="50%" width="50%"></a>

These boards support only the newer Haier protocol and can work with the hOn application. Additionally, in some regions, the EVO application is used instead of hOn.

Sometimes these modules can be reused for ESPHome. **Make sure to back up your module before attempting to flash it.** Some of the newer ESP32-for-Haier modules are encrypted.

Example of ESPHome Configuration
--------------------------------

Here is an example of an ESPHome configuration for this module (can work only with esp-idf framework):

.. code-block:: yaml

  esphome:
    name: haier

  esp32:
    board: esp32dev
    framework:
      type: esp-idf
      sdkconfig_options:
        CONFIG_FREERTOS_UNICORE: y

  wifi:
    ssid: !secret wifi_ssid
    password: !secret wifi_password
