# LoopbackDIN_Bridge - two independent USB MIDI 2.0 (UMP) interfaces on one device

Demonstrates `tusb_ump`'s `ump_device` driver enumerating **two** simultaneous,
independently-addressable USB MIDI 2.0 (UMP) interfaces on a single device
(`CFG_TUD_UMP=2` in [`tusb_config.h`](tusb_config.h)):

- **`tud_ump` itf 0** - a raw UMP loopback, echoing back whatever it
  receives, on whichever alt setting (MIDI 1.0 or UMP) the host has
  selected. Same behavior as
  [`lib/tusb_ump/examples/tusb_ump_lb`](../../lib/tusb_ump/examples/tusb_ump_lb).
- **`tud_ump` itf 1** - a DIN MIDI 1.0 bridge over hardware UART0
  (GPIO12=TX, GPIO13=RX, the DIN header), using the *exact same*
  `UartDinBridge` module as [`UUT/UART_DIN_Bridge`](../UART_DIN_Bridge), just
  configured with `ump_itf = 1` instead of `0`. Like that example, this one
  has no live console (stdio-over-UART is disabled) -- see its README for
  why: GPIO12/13 need sole ownership of UART0 on this board.

The two interfaces are fully separate USB MIDIStreaming functions (separate
Audio-Control+MIDIStreaming interface pairs, separate bulk endpoints,
separate Group Terminal Blocks) -- traffic on one never appears on the
other. A MIDI 2.0-aware host sees this as one device exposing two distinct
ports/function blocks: "Loopback" and "DIN Bridge".

## Why this exists

`ump_device.cpp`'s class driver has always supported multiple UMP interfaces
(every `tud_ump_*` call takes an `itf` index), but no example in this repo
exercised it until now. This example is the concrete proof, and doubles as
the reference for the USB descriptor layout needed to declare a second
independent MIDIStreaming interface pair -- see the comments in
[`usb_descriptors.cpp`](usb_descriptors.cpp).

## Building

From the repo root:

```sh
cmake -S . -B build -G Ninja
cmake --build build --target UUT_LOOPBACK_DIN_BRIDGE
```

Flash the resulting `UUT_LOOPBACK_DIN_BRIDGE.uf2` to the ProtoZOA UUT Pico.
