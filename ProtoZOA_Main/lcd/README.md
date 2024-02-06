![AmeNote Logo](https://amenote.com/wp-content/uploads/2022/05/cropped-Amenote-Horizontal-True-White-Medium-2240-1-1.jpg)

# ProtoZOA USB MIDI 2.0 Tool
## An open project for MIDI 2.0 Standards Prototyping and Development by AmeNote Inc.
Copyright(C) 2022 - AmeNote Inc.

## LCD 1.14 Inch Optional Display Module Library

The ProtoZOA prototyping tool accomodates an optional display module at U3. By default it supports the 4 way joystick
plus center button press, the two function buttons A and B plus the 1.14 inch LCD display.

The LCD module can be searched and purchased on the Internet from several suppliers. We obtain our modules from DigiKey
which had several units in stock at writing and regularly gets more.
https://www.digikey.com/en/products/detail/seeed-technology-co-ltd/103030400/14317043

## More Information
More information on the LCD module including the pinouts of the connector and the LCD driver library can be found
at the WaveShare website (the manufacturers of the module).
https://www.waveshare.com/wiki/Pico-LCD-1.14

NOTE: The ProtoZOA shares the SPI connection to the display module with the cap touch module. Mainly done due to
limited I/O count, this is acheived by having the chip select line (GP9) to an inverter to the cap touch IC.
Therefore the LCD CS maintains its current level for enable, however the disable is now inverted for the enable to
the captouch IC. You cannot assume the state of the SPI hardware when starting to use communications to the
LCD and should configure just prior to asserting chip select. The examples here should show this clearly.
