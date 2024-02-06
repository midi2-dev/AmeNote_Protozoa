<img src="doc/images/AmeNoteHoriz.png"
     alt="AmeNote Logo"
     style="center; margin-right: 100px;" />

# AmeNote<sup>TM</sup> ProtoZOA<sup>TM</sup>
<img src="doc/images/AmeNoteProtoZOA.png"
     alt="AmeNote ProtoZOA"
     style="center; margin-right: 600px;" />

AmeNote ProtoZOA Prototyping Tool with Optional Display Module and CME WIDI BLE Module

## Welcome

Welcome to the world of MIDI 2.0 Prototyping. Initially ProtoZOA was concieved as a prototyping tool for the developer to experiment with and develop USB MIDI 2.0 along with other members of the MIDI Association with the coal of creating complete, consistent and industry approved implementations of MIDI 2.0 on USB.

As the development continued, ProtoZOA has evolved to be a prototyping tool for MIDI 2.0 in general. ProtoZOA along with this Github community ([https://github.com/midi2-dev/Amenote_Protozoa](https://github.com/midi2-dev/Amenote_Protozoa)) creates a community fostering the advancement of MIDI 2.0 with current capabilities and activities.

We look forward to collaborating with you, making MIDI 2.0 great!

- [Getting Started Guide](doc/QuickStartGuide)
- [User Manual](doc/UserManual)
- [Developer Guide](/doc/DeveloperGuide)

Some other resources available:
- [ProtoZOA Schematic](https://github.com/midi2-dev/Amenote_Protozoa/blob/main/doc/Resources/ProtoZOA%20--%20Schematic%20--%202022-06-01%20--%2012h15.pdf)
- [ProtoZOA Board General Layout](https://github.com/mid2i-dev/Amenote_Protozoa/blob/main/doc/Resources/ProtoZOA%20--%20Front%20view.pdf) (larger PDF document to print if you like for easier reference)

## What You See Here

We recommend you work through the above resources to help you understand and work with your ProtoZOA kit. We also recommend you review the [Getting Started Guide](doc/QuickStartGuide) if you are currently waiting for your ProtoZOA to show up. It includes a list of other resources and materials you will need to make the most of your ProtoZOA device.

This git has the following sub directories:

| Directory / File  | Resource  |
|:----------|:----------|
| ProtoZOA_Main    | Code and build for the Main Pico on ProtoZOA - the one soldered onto main board.    |
| ProtoZOA_PicoProbe    | Code and build for the PicoProbe utility from Raspberry configured for use on ProtoZOA development board. Can be loaded onto either Main or UUT Pico. See [User Manual](doc/UserManual) for more information.    |
| ProtoZOA_UUT    | Code and build for the UUT (Unit Under Test) Pico on ProtoZOA - the one on connectors.    |
| doc | ProtoZOA support documentation. |
| CMakeLists.txt | cmake build support file |
| Contribution.md | Contribution agreement |
| License.md | License agreement |
| README.md | This file. |
| pico_sdk_import.cmake | SDK connection file for Pico development |

## MIDI Association ([www.midi.org](http://www.midi.org))
ProtoZOA is part of the offering from the MIDI Association towards their mission for corporate members to:
- Develop and enhance MIDI to respond to new market needs
- Create new MIDI 2.0 standards with broad industry participation
- Ensure the interoperability of MIDI products
- Protect the term MIDI and MIDI logo markets
- Promote the use of MIDI technology and products.

Currently all commercial members of the MIDI Association have or can receive a single ProtoZOA. If you are a commercial member and have not received a ProtoZOA, please be sure to contact the MIDI Association at [info@midi.org](mailto:info@midi.org). Note that the MIDI Association has the right to terminate this offering at any time.

## Further ProtoZOA's / Accessories

Current commercial MIDI Association members can obtain additional ProtoZOA Prototyping tools by contacting AmeNote at [info@AmeNote.com](mailto:info@AmeNote.com). Note that the ProtoZOA's are assembled in batches by hand. In response, AmeNote will provide you with more details in response to your inquiry.

Availability for non-commercial MIDI Association members is anticipated to occur by early 2023, coinciding with some or all firmware source becoming publically available.

AmeNote (and others) will be releasing additional expansion accessories for use with ProtoZOA from time to time. Currently in development or externally availabe are:
- LCD Display Module with joystick and funciton buttons (see User Guide for more info)
- CME WIDI Core Bluetooth module ([www.cme-pro.com](https://www.cme-pro.com/))
- AmeNote ProtoZOA PoE Ethernet Expansion Module (estimated late August, 2022).
- Type 25 Expansion Module (tbd)
- External Power Supply - see User Manual for more details.
- Micro-USB to USB A (host) Adapter - see User Manual for more details.

### License
The ProtoZOA hardware and associated developed firmware is Copyright (c) AmeNote Inc., 2022.

AmeNote Inc. desires for whole community involvement and for the group to all benefit from the development and advancement of ProtoZOA and the associated firmware, therefore a very open license agreement exists with the ultimate goal of all firmware resources becoming open source and open hardware reference designs for public use. The decision of open to public will be jointly decided by AmeNote and the MIDI Association.

The reader agrees to the terms of the [ProtoZOA License](License.md) and [Contribution Agreement](Contribution.md).

## Contributors

We wish to thank and acknowledge all contributors to this project. Please review our [Contributors](doc/Contributors.md) document regularly.

##### AmeNote, AmeNote Logo and ProtoZOA are trademarks of AmeNote Inc.
##### Copyright (c) AmeNote Inc., 2022.
