# USB PID assignments for test/example applications

Each test application's `usb_descriptors*.cpp` computes its USB PID from a
bitmap: bits 0-4 encode which TinyUSB class drivers are enabled (CDC/MSC/
HID/MIDI/VENDOR, same as upstream TinyUSB's convention), and bit 14 (`0x4000`)
marks it as a TinyUSB example-range PID. Bits 5-7 are used by some apps for
feature sub-variants (e.g. `usb_descriptors.bt.cpp`'s `_PID_BT` bit).

Bits 8-13 (`USB_PID_APP`, 6 bits -- correcting an earlier version of this
doc that said "bits 8-10") additionally encode which test application this
is, so that no two precompiled test apps ever enumerate with the same PID --
see issue #16 (host OS USB descriptor caches are keyed off VID/PID, and
users testing multiple precompiled apps on the same host were seeing stale
cached descriptors when the apps shared a PID). Bit 14 (`0x4000`, the
TinyUSB example-range marker) is the ceiling: `USB_PID_APP` must stay at or
below `0x3F` or it collides with that marker bit.

| `USB_PID_APP` | App | USB Product string | File |
|---|---|---|---|
| 0x01 | CME_WIDI_CORE_EXP | `USBMidiCmeWidiExp` | `UUT/CME_WIDI_CORE_EXP/usb_descriptors.cpp` |
| 0x02 | NetworkMIDI2_Bridge | `USBMidiNetworkBridge` | `UUT/NetworkMIDI2_Bridge/usb_descriptors.cpp` |
| 0x03 | USB_FunctionBlocks | `USBMidiFunctionBlocks` | `UUT/USB_FunctionBlocks/usb_descriptors.cpp` |
| 0x04 | DIN_Bridge | `USBMidiDinBridge` | `UUT/DIN_Bridge/usb_descriptors.cpp` |
| 0x05 | USB_MIDI_Echo | `USBMidiEcho` | `UUT_FreeRTOS/USB_MIDI_Echo/usb_descriptors.cpp` |
| 0x06 | FreeRTOS_Tasks (3groups variant) | `USBMidiFreeRtosTasks` | `UUT_FreeRTOS/FreeRTOS_Tasks/usb_descriptors.3groups.cpp` |
| 0x07 | FreeRTOS_Tasks (bt variant) | `USBMidiFreeRtosTasksBt` | `UUT_FreeRTOS/FreeRTOS_Tasks/usb_descriptors.bt.cpp` |
| 0x08-0x1F | *(reserved headroom for the sequence above)* | | |
| 0x20 | UART_DIN_Bridge | `USBMidiUartDinBridge` | `UUT/UART_DIN_Bridge/usb_descriptors.cpp` |
| 0x21 | LoopbackDIN_Bridge | `USBMidiLoopbackDinBridge` | `UUT/LoopbackDIN_Bridge/usb_descriptors.cpp` |

`UART_DIN_Bridge`/`LoopbackDIN_Bridge` deliberately jump to `0x20` rather
than continuing at `0x08`, leaving `0x08`-`0x1F` free for the existing
`UUT`/`UUT_FreeRTOS` app sequence to keep growing without ever bumping into
this pair.

The USB product string (`iProduct`) is likewise unique per app now, matching its `USB_PID_APP` -- previously several apps (`CME_WIDI_CORE_EXP`, `USB_MIDI_Echo`, and the `FreeRTOS_Tasks` 3groups variant) all identically reported "ProtoZOA" as their product name, which was just as ambiguous to a user picking a device off a list as the shared PID was.

`UUT/CDC_FunctionBlocks` has no descriptors of its own (it's driven over
SPI/interchip from another Pico's USB stack), so it needs no entry here.

When adding a new test application with its own USB descriptors, pick the
next unused `USB_PID_APP` value (0x08 onward, staying below the `0x20`
block reserved above unless you're adding another one-off/example app that
should also sit in its own headroom) and add a row above.
