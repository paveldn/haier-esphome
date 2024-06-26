Overview of Communication Protocol between Haier Air Conditioner and ESP
========================================================================

The data packets exchanged between the air conditioner and the ESP are structured as follows:

- FF FF: Packet start indicator
- 1 byte: Length
- 1 byte: Flags (typically 0x40 for hOn indicating the packet contains a CRC16 checksum at the end, and 0x00 for smartAir2 without CRC16)
- 5 bytes: Reserved for future use (usually zeros for hOn and zeros with 01 at the end for smartAir2)
- 1 byte: Packet type
- n bytes: Data
- 1 byte: Checksum (modulo 256 sum of all packet bytes)
- 2 bytes: CRC16 (if specified by flags)

To view the complete packet, enable logs at the VERBOSE level. Generally, this is unnecessary as the transport protocol remains consistent. There are additional nuances, but we will not delve too deeply into them here.

As far as I can tell, Haier uses this same protocol for other equipment as well (at least for washing machines, dryers, and dishwashers). I have a C++ library independent of ESPHome that works with this protocol: `HaierProtocol <https://github.com/paveldn/HaierProtocol>`_

We will now focus solely on hOn. Our current interest lies in the packet type and data. Typically, requests to the air conditioner use an odd packet type ids, and responses use an even type ids:

- 0x01: Status request or control
- 0x02: Status response from the air conditioner

All packet types can be found here: `Haier Frame Types. <https://github.com/paveldn/HaierProtocol/blob/main/include/protocol/haier_frame_types.h>`_

Our focus will be on packet type 0x01 and response 0x02. For some packets, including these, the first 2 bytes are subcommands. We are interested in the following 3 subcommands:

- 0x4D01: Request normal data
- 0x4DFE: Request extended data (big data)
- 0x5C01: Send state to the air conditioner (control)

Example log entries (DEBUG level) illustrate this:

Request for normal data:

.. code-block:: 

    20:07:46  [D]  [haier.protocol:019]  Sending frame: type 01, data: 4D 01


Response:

.. code-block:: 

    20:07:46  [D]  [haier.protocol:019]  Frame found: type 02, data: 6D 01 07 06 C2 00 02 00 00 07 00 00 3E 00 50 00 00 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

Request for extended data:

.. code-block:: 

    20:07:57  [D]  [haier.protocol:019]  Sending frame: type 01, data: 4D FE

Response:

.. code-block:: 

    20:07:57  [D]  [haier.protocol:019]  Frame found: type 02, data: 7D 01 07 06 C2 00 02 00 00 07 00 00 3E 00 50 00 00 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 66 4A 4D 4D 4C 00 01 FF 02 20 00 32


The status response usually appears as follows:

- 2 bytes: Subcommand (6D 01 for normal data or control confirmation, 7D 01 for extended data)
- 10 bytes: Air conditioner parameters (mode, flaps, temperature, on/off status, cleaning, etc.)
- 16 bytes: Additional sensors, most of which are unused (possibly for future models)
- 2 bytes: Zeros (possibly reserved for future use)

For extended data:
- 14 bytes: Extended sensors (energy consumption, temperatures of different parts, current strength, etc.), not all of which are used

The structures for all these data points can be found here: `Haier ESPHome Packet Structures. <https://github.com/paveldn/haier-esphome/blob/experimental/components/haier/hon_packet.h>`_

This structure works for 99% of air conditioners, though variations exist with different inserts between these sections. For instance, some manufacturers add an extra byte for special parameter control, shifting all sensor data.

Note that my component requests extended data once every 2 intervals only if at least one sensor using these data is added. Therefore, packet part sizes can vary.

Finding Packet Sizes
====================

For Big Data:
-------------

Compare the response to the request type 01, data: 4D 01 with the response to type 01, data: 4D FE.
The size difference is the big data packet size (everything else is the same as a normal packet).
To identify the start of the control packet (sometimes preceded by an insignificant header to be skipped), use the following algorithm:

For control part:
-----------------

Turn on the air conditioner using the remote (set any mode where the temperature can be adjusted).
Set the temperature to 20 degrees. In the status packet, the target temperature is always at the start of the air conditioner parameters block, encoded as:

- 0 -> 16°C
- 1 -> 17°C
- 2 -> 18°C
- ...

For 20°C, look for the value 0x04 in the packet. If multiple such bytes exist, set 21°C and look for 0x05, etc., until found uniquely. This will likely be byte 3, immediately after the subcommand.
To locate the start of the sensor part, follow these steps:

For sensors part:
-----------------

The sensors always start with the room temperature (1 byte), available at all times and encoded in 0.5°C increments. Match the temperature displayed on your air conditioner's screen.
For example, if the display shows 25.5°C, search for 25.5 x 2 = 51 => 0x33 in the packet. The next byte after room temperature is humidity (percentage), often unsupported by most air conditioners, likely resulting in 0.
The third byte in the sensor packet is the outdoor temperature with an offset of -64°C. For instance, if the outdoor temperature is 28°C, search for 28 + 64 = 92 => 0x5C. Note that if the air conditioner is off, this value remains unchanged, reflecting the last measured value.
Look for approximate values with a tolerance of ±3°C for outdoor temperature.

Configuring the Component
-------------------------

Once you have identified all sizes and offsets, configure the component with the following parameters:

- status_message_header_size: Number of bytes to skip before the control packet starts (default is 0, likely unnecessary). I have seen only one case with a header: `Issue Comment <https://github.com/paveldn/haier-esphome/issues/39#issuecomment-2161347577>`_.
- control_packet_size: Size of the control packet, with a minimum and default value of 10. Count the bytes from the first control packet byte (target temperature) to room temperature.
- sensors_packet_size: Size of the sensor packet, with a minimum and default value of 18. Count the bytes from room temperature to the end of the short packet.

For some models, I have observed a control_packet_size of 12. For some ducted air conditioners, it is 18. The only exception is the model Casarte CAS35MW1/R3-W, 1U35MW1/R3 with:

.. code-block:: 

    status_message_header_size: 38
    control_packet_size: 10
    sensors_packet_size: 24


