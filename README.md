# ESPHome Haier component

This implementation of the ESPHoime component to control HVAC on the base of the hOn Haier protocol (AC that is controlled by the hOn application). Keep in mind that the smartAir2 HVAC protocol is not currently supported.

You can use this component together with a native Haier ESP32 device: 

**Front:**

<p><a href="https://github.com/paveldn/ESP32-S0WD-Haier/blob/master/img/ESP32_front.jpg?raw=true"><img src="https://github.com/paveldn/ESP32-S0WD-Haier/blob/master/img/ESP32_front.jpg?raw=true" height="50%" width="50%"></a></p>

**Back:**

<a href="https://github.com/paveldn/ESP32-S0WD-Haier/blob/master/img/ESP32_front.jpg?raw=true"><img src="https://github.com/paveldn/ESP32-S0WD-Haier/blob/master/img/ESP32_back.jpg?raw=true" height="50%" width="50%"></a>

but also you can use any other ESP32 or ESP8266 board.

# Configuration example

```  
uart:
  baud_rate: 9600
  tx_pin: 17
  rx_pin: 16
  id: ac_port  

climate:
  - platform: haier
    id: ac_port
    name: Haier AC 
    uart_id: ac_port
    wifi_signal: true           # Optional, default true, enables WiFI signal transmission from ESP to AC
    beeper: true                # Optional, default true, disables beep on commands from ESP
    outdoor_temperature:        # Optional, outdoor temperature sensor
      name: Haier AC outdoor temperature
    visual:                     # Optional, you can use it to limit min and max temperatures in UI (not working for remote!)
      min_temperature: 16 °C
      max_temperature: 30 °C
      temperature_step: 1 °C
    supported_swing_modes:      # Optional, can be used to disable some swing modes if your AC does not support it
    - 'OFF'
    - VERTICAL
    - HORIZONTAL
    - BOTH
```

**Configuration variables**

* **id (Optional, ID):** Manually specify the ID used for code generation
* **uart_id (Optional, ID):** ID of the UART port to communicate with AC
* **name (Required, string):** The name of the climate device
* **wifi_signal (Optional, boolean):** If true - send wifi signal level to AC
* **beeper (Optional, boolean):** Can be used to disable beeping on commands from AC
* **outdoor_temperature (Optional):** Temperature sensor for outdoor temperature
  * **name (Required, string):** The name of the sensor.
  * **id (Optional, ID):** ID of the sensor, can be used for code generation
  * All other options from Sensor.
* **supported_swing_modes (Optional, list):** Can be used to disable some swing modes if your AC does not support it

# How to flash the original ESP32 Haier module

To flash the firmware you will need to use a USB to TTL converter and solder wires to access UART0 on board (or use something like this: [Pogo Pin Probe Clip 2x5p 2.54 mm]( https://www.tinytronics.nl/shop/en/tools-and-mounting/measuring/accessories/test-probe-with-clamp-pogo-pin-2x5p))

**UART0 pinout:**
<p><a href="https://github.com/paveldn/ESP32-S0WD-Haier/blob/master/img/ESP32_front.jpg?raw=true"><img src="https://github.com/paveldn/ESP32-S0WD-Haier/blob/master/img/ESP32_Haier_UAR0_pinout.jpg?raw=true" height="50%" width="50%"></a></p>

To put the device in the flash mode you will need to shortcut GPIO0 to the ground before powering the device.

Once the device is in flash mode you can make a full backup of the original firmware in case you would like to return the module to its factory state. To make a backup you can use [esptool](https://github.com/espressif/esptool). Command to make a full flash backup: 

**python esptool.py -b 115200 --port <port_name> read_flash 0x00000 0x400000 flash_4M.bin**

After this, you can flash firmware using ESPHome tools (dashboard, website, esphome command, etc)
