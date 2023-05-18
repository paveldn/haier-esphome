# ESPHome Haier component

This is an implementation of the ESPHoime component to control HVAC on the base of the SmartAir2 and hOn Haier protocols (AC that is controlled by the hOn or SmartAir2 application).

## Short description

There are two versions of the Haier protocol. The older one is using an application called SmartAir2 and the newer one - an application called hOn. Both protocols are compatible on the transport level but have different commands to control appliances.

### SmartAir2

Older Haier models controlled by the SmartAir2 application are using the KZW-W002 module that looks like this:

<p><a href="./img/KZW-W002.jpg?raw=true"><img src="./img/KZW-W002.jpg?raw=true" height="50%" width="50%"></a></p>

This module can't be reused, and you need to replace it with an ESP (RPI pico w) module. The USB connector on a board doesn't support the USB protocol. It is a UART port that just uses a USB connector. To connect the ESP board to your AC you can cut a USB type A cable and connect wires to the climate connector.

**Haier UART pinout:**
| Board | USB | Wire color | ESP module |
| --- | --- | --- | --- |
| 5V | VCC | red | 5V |
| GND | GND | black | GND |
| TX | DATA+ | green | RX |
| RX | DATA- | white | TX |


<p><a href="./img/usb_pinout.png?raw=true"><img src="./img/usb_pinout.png?raw=true" height="50%" width="50%"></a></p>

### hOn 

## Short description

There are two versions of the Haier protocol. The older one is using an application called SmartAir2 and the newer one application - called hOn. Both protocols are compatible on the transport level but have different commands to control appliances.

### SmartAir2

Older Haier models controlled by the SmartAir2 application is using the KZW-W002 module that looks like this:

<p><a href="./img/KZW-W002.jpg?raw=true"><img src="./img/KZW-W002.jpg?raw=true" height="50%" width="50%"></a></p>

This module can't be reused and you need to replace it with an ESP (RPI pico w) module. USB port on a board doesn't support USB protocol it is a UART port that just uses a USB connector. To connect the ESP board to your AC you can cut a USB type A cable and connect wires to the climate connector.

**Haier UART pinout:**
| Board | USB | Wire color | ESP module |
| --- | --- | --- | --- |
| 5V | VCC | red | 5V |
| GND | GND | black | GND |
| TX | DATA+ | green | RX |
| RX | DATA- | white | TX |


<p><a href="./img/usb_pinout.png?raw=true"><img src="./img/usb_pinout.png?raw=true" height="50%" width="50%"></a></p>

### hOn 

You can use this component together with a native Haier ESP32 device: 
Newer Haier models using a module called ESP32-for-Haier. It is an ESP32 single-core board with an ESP32-S0WD chip. The module board looks like this: 

**Front:**

<p><a href="./img/ESP32_front.jpg?raw=true"><img src="./img/ESP32_front.jpg?raw=true" height="50%" width="50%"></a></p>

**Back:**

<a href="./img/ESP32_back.jpg?raw=true"><img src="./img/ESP32_back.jpg?raw=true" height="50%" width="50%"></a>

In some cases, you can reuse this module and flash it with ESPHome, but some new modules don't support this. They look the same but have encryption enabled.

**Warning!** The new generation of ESP32-Haier devices has encryption enabled, so they can only be flashed with firmware that is signed with a private key. There is no way to make them work with ESPHome, so if you try to do it, the board will get into a boot loop with error 
`rst:0x10 (RTCWDT_RTC_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)`
The only way to recover this board is to flash it with the original image. So before starting your experiments make a backup image: [How to backup the original image and flash ESPHome to the ESP32 Haier module](#how-to-backup-the-original-image-and-flash-esphome-to-the-esp32-haier-module)

Also, you can use any other ESP32, ESP8266 or a RPI pico W board. In this case, you will need to cut the original wire or make a connector yourself (the board has a JST SM04B-GHS-TB connector)

## Configuration

The configuration will be a little bit different for different protocols. For example, the SmartAir2 protocol doesn't support cleaning, setting air direction (just swing on/off) etc.

### hOn configuration example

```  
uart:
  baud_rate: 9600
  tx_pin: 17
  rx_pin: 16
  id: ac_port  

climate:
  - platform: haier
    id: haier_ac
    protocol: hOn
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
    supported_modes:            # Optional, can be used to disable some modes if you don't need them
    - 'OFF'
    - AUTO
    - COOL
    - HEAT
    - DRY
    - FAN_ONLY
    supported_swing_modes:      # Optional, can be used to disable some swing modes if your AC does not support it
    - 'OFF'
    - VERTICAL
    - HORIZONTAL
    - BOTH
```

### SmartAir2 configuration example

```  
uart:
  baud_rate: 9600
  tx_pin: 1
  rx_pin: 3
  id: ac_port  

climate:
  - platform: haier
    id: haier_ac
    protocol: smartAir2
    name: Haier AC 
    uart_id: ac_port
    visual:                     # Optional, you can use it to limit min and max temperatures in UI (not working for remote!)
      min_temperature: 16 °C
      max_temperature: 30 °C
      temperature_step: 1 °C
    supported_modes:            # Optional, can be used to disable some modes if you don't need them
    - 'OFF'
    - AUTO
    - COOL
    - HEAT
    - DRY
    - FAN_ONLY
    supported_swing_modes:      # Optional, can be used to disable some swing modes if your AC does not support it
    - 'OFF'
    - VERTICAL
    - HORIZONTAL
    - BOTH
```

### Configuration variables

- **id (Optional, [ID](https://esphome.io/guides/configuration-types.html#config-id)):** Manually specify the ID used for code generation
- **uart_id (Optional, [ID](https://esphome.io/guides/configuration-types.html#config-id)):** ID of the UART port to communicate with AC
- **protocol (Required, string):** Defines protocol of communication with AC. Possible values: hon or smartair2
- **name (Required, string):** The name of the climate device
- **wifi_signal (Optional, boolean):** If true - send wifi signal level to AC
- **beeper (Optional, boolean):** (supported only by hOn) Can be used to disable beeping on commands from AC
- **outdoor_temperature (Optional):** (supported only by hOn) Temperature sensor for outdoor temperature
  - **name (Required, string):** The name of the sensor.
  - **id (Optional, [ID](https://esphome.io/guides/configuration-types.html#config-id)):** ID of the sensor, can be used for code generation
  - All other options from Sensor.
- **supported_modes (Optional, list):** Can be used to disable some of AC modes. Possible values: OFF (use quotes in opposite case ESPHome will convert it to False), AUTO, COOL, HEAT, DRY, FAN_ONLY
- **supported_swing_modes (Optional, list):** Can be used to disable some swing modes if your AC does not support it. Possible values: OFF (use quotes in opposite case ESPHome will convert it to False), VERTICAL, HORIZONTAL, BOTH
- All other options from [Climate](https://esphome.io/components/climate/index.html#config-climate).

## Automations

Haier climate support some actiuons:

### climate.haier.power_on Action

This action turns AC power on

```
on_...:
  then:
    climate.haier.power_on: device_id
```

### climate.haier.power_off Action

This action turns AC power off

```
on_...:
  then:
    climate.haier.power_off: device_id
```

### climate.haier.power_toggle Action

This action toggles AC power

```
on_...:
  then:
    climate.haier.power_toggle: device_id
```

### climate.haier.display_on Action

This action turns the AC display on

```
on_...:
  then:
    climate.haier.display_on: device_id
```

### climate.haier.display_off Action

This action turns the AC display off

```
on_...:
  then:
    climate.haier.display_off: device_id
```

### climate.haier.health_on Action

Turn on health mode ([UV light sterilization](https://www.haierhvac.eu/en/node/1809))

```
on_...:
  then:
    climate.haier.health_on: device_id
```

### climate.haier.health_off Action

Turn off health mode

```
on_...:
  then:
    climate.haier.health_off: device_id
```

### climate.haier.beeper_on Action

(supported only by hOn)  This action enables beep feedback on every command sent to AC

```
on_...:
  then:
    climate.haier.beeper_on: device_id
```

### climate.haier.beeper_off Action

(supported only by hOn) This action disables beep feedback on every command sent to AC (keep in mind that this will not work for IR remote commands)

```
on_...:
  then:
    climate.haier.beeper_off: device_id
```

### climate.haier.set_vertical_airflow Action

(supported only by hOn) Set direction for vertical airflow if the vertical swing is disabled. Possible values: Up, Center, Down.

```
on_...:
  then:
    - climate.haier.set_vertical_airflow:
      id: device_id
      vertical_airflow: Up
```

### climate.haier.set_horizontal_airflow Action

(supported only by hOn) Set direction for horizontal airflow if the horizontal swing is disabled. Possible values: Left, Center, Right.

```
on_...:
  then:
    - climate.haier.set_horizontal_airflow:
      id: device_id
      vertical_airflow: Right
```

### climate.haier.start_self_cleaning Action

(supported only by hOn) Start [self-cleaning](https://www.haier.com/in/blogs/beat-the-summer-heat-with-haier-self-cleaning-ac.shtml) 

```
on_...:
  then:
    - climate.haier.start_self_cleaning: device_id
```

### climate.haier.start_steri_cleaning Action

(supported only by hOn) 56°C steri-cleaning

```
on_...:
  then:
    - climate.haier.start_steri_cleaning: device_id
```

## How to backup the original image and flash ESPHome to the ESP32 Haier module

**It is strongly recommended to make a backup of the original flash content before flashing ESPHome!**

To make a backup and to flash the new firmware you will need to use a USB to TTL converter and solder wires to access UART0 on board (or use something like this: [Pogo Pin Probe Clip 2x5p 2.54 mm]( https://www.tinytronics.nl/shop/en/tools-and-mounting/measuring/accessories/test-probe-with-clamp-pogo-pin-2x5p))

**UART0 pinout:**
<p><a href="./img/ESP32_Haier_UAR0_pinout.jpg?raw=true"><img src="./img/ESP32_Haier_UAR0_pinout.jpg?raw=true" height="50%" width="50%"></a></p>

To put the device in the flash mode you will need to shortcut GPIO0 to the ground before powering the device.

Once the device is in flash mode you can make a full backup of the original firmware in case you would like to return the module to its factory state. To make a backup you can use [esptool](https://github.com/espressif/esptool). Command to make a full flash backup: 

**python esptool.py -b 115200 --port <port_name> read_flash 0x00000 0x400000 flash_4M.bin**

After this, you can flash firmware using ESPHome tools (dashboard, website, esphome command, etc)
