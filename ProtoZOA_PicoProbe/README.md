<img src="../doc/images/AmeNoteHoriz.png"
     alt="AmeNote Logo"
     style="center; margin-right: 100px;" />

# ProtoZOA<sup>TM</sup> PicoProbe

PicoProbe is a useful developer tool supplied by Raspberry Pi (Trading) Ltd. for use with the Raspberry Pi Pico and the RP2040 MCU. It takes advantage of the highly flexible PIO feastures of the RP2040 to create a SWD debug master port which in combination with the PicoProbe firmware provides a gdb SWD interface plus a UART for a user console port.

The picoprobe software provided by Raspberry uses GPIO pins that were required for different purposes on the ProtoZOA implementation. Therefore AmeNote made appropriate adjustments to utilize alternate pins. AmeNote provides this adjusted implementation for use of PicoProbe on the ProtoZOA Prototyping tool.

See original Raspberry [README](https://RaspberryReadme.md).

### Summary of Changes

The following are the pinout changes adjusted to run PicoProbe on the AmeNote ProtoZOA Prototyping tool. All adjustments are made in the picoprobe_config.h file which can easily be further adjusted and recompiled for the readers needs.

| <center>Function</center>  | <center>Original Pin Assignment</center>  | <center>Adjusted Pin Assignment</center>  |
|:----------|:----------|:----------|
| <center>UART0 Tx</center>   | <center>GP4</center>    | <center>GP0</center>    |
| <center>UART0 Rx</center> | <center>GP5</center> | <center>GP1</center> |
| <center>SWCLK</center> | <center>2</center> | <center>14</center> |
| <center>SWDIO</center> | <center>3</center> | <center>5</center> |

## Notices
AmeNote does not warrant or support this code, it is provided for ProtoZOA users convenience only. Further use and up to date development details can be found in the appropriate Raspberry Pico development communitites.

To learn how to use PicoProbe including how to configure your gdb and IDE, please refer to the [Raspberry Pi Pico Getting Started Guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf). Pay close attention to section 5.1, Installing OpenOCD and Chapter 6, Debugging with SWD.

PicoProbe is Copyright 2020 (c) 2020 Raspberry Pi (Trading) Ltd.

The PicoProbe [license](License.md) is direct from Raspberry Pico SDK repository.


##### AmeNote, AmeNote Logo and ProtoZOA are trademarks of AmeNote Inc., 2022.
##### Copyright (c) AmeNote Inc., 2022.
