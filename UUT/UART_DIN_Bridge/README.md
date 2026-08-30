# UART_DIN_Bridge - DIN MIDI 1.0 <-> USB MIDI 2.0 (UMP) over hardware UART

A DIN MIDI bridge, structured the same way as [`UUT/DIN_Bridge`](../DIN_Bridge)
(one USB MIDIStreaming interface, alt 0 = legacy MIDI 1.0 byte stream, alt 1 =
UMP), but with the physical DIN transport driven by the RP2040's real UART0
peripheral instead of a PIO bit-banger. It answers the host's UMP Endpoint and
Function Block Discovery requests, reporting one bidirectional function block
covering the single UMP group bridged to/from the DIN port.

## Hardware

ProtoZOA UUT, wired to the same DIN header pins `UUT/DIN_Bridge` uses:

- GPIO12 = UART0 TX -> DIN Out
- GPIO13 = UART0 RX -> DIN In

Both GPIOs support the RP2040's hardware UART0 function, so this is a
drop-in swap for `DIN_Bridge` on the same physical wiring -- a direct
hardware-UART vs. PIO-UART comparison.

### Trade-off: no live console while the bridge is running

GPIO12/13 only support hardware **UART0** on the RP2040, and UART0 is also
ProtoZOA's console/stdio UART (GPIO0/1, bridged out via PicoProbe).
`stdio_init_all()`'s default console claims GP0/1 as UART0 TX/RX. Tested
against a real ProtoZOA board with a physical DIN loopback partner (a Roland
UM-ONE): with that console claim in place, GPIO13 (DIN RX) never saw a
single byte arrive -- not corrupted, *zero*, independent of baud rate or
pull-ups. A live SWD/gdb read of a byte counter confirmed it. Disabling
stdio-over-UART (`pico_enable_stdio_uart(..., 0)` in `CMakeLists.txt`) so
GPIO12/13 has sole ownership of UART0 fixed it immediately -- the same test
then showed real DIN bytes and correctly-decoded UMP messages arriving.

So this example has **no live console** -- `UartDinBridge`'s `printf()`
diagnostics (see `uart_din_bridge.cpp`) are no-ops here, since nothing
backs `stdio`. They remain useful on other boards/pin pairs where the DIN
UART doesn't collide with stdio. This is also exactly why `UUT/DIN_Bridge`
uses a PIO bit-banger on these same pins instead of the UART0 hardware
block: PIO is a separate peripheral that doesn't contend with UART0 at all,
so it can coexist with the console.

For observing this bridge without a console, `UartDinBridge` exposes
`rxByteCount`/`rxUmpCount`/`lastRxByte` counters, readable via SWD (e.g.
`arm-none-eabi-gdb ... -ex "target extended-remote localhost:3333" -ex
"monitor halt" -ex "print dinBridge.rxByteCount"`).

## Porting to other targets

All the bridging logic lives in [`uart_din_bridge.h`](uart_din_bridge.h) /
[`uart_din_bridge.cpp`](uart_din_bridge.cpp): a `UartDinBridge` class
configured via `UartDinBridgeConfig` (UART instance, TX/RX pins, baud rate,
which `tud_ump` interface index it drives, which UMP group it maps to). It
has no ProtoZOA-specific code -- only pico-sdk `hardware_uart` calls and the
board-agnostic `bytestreamToUMP`/`umpToBytestream`/`umpProcessor` classes
from `lib/AM_MIDI2.0Lib`. To bridge DIN MIDI on another RP2040 target:

1. Carry `uart_din_bridge.h`/`uart_din_bridge.cpp` over unchanged.
2. In your own `main()`, fill in a `UartDinBridgeConfig` with that board's
   UART instance/pins and call `init()`/`service()` as shown in
   [`main.cpp`](main.cpp).

The only board-specific code is `main.cpp`'s bring-up (`stdio_init_all()`,
which GPIOs/UART instance to pass in) -- `ump_device.h`/`ump_device.cpp`
(the generic UMP class driver) and `usb_descriptors.cpp` need no changes to
port.

See [`UUT/LoopbackDIN_Bridge`](../LoopbackDIN_Bridge) for this same module
reused a second time, on a second `tud_ump` interface, inside a single
two-interface USB device.

## Building

From the repo root:

```sh
cmake -S . -B build -G Ninja
cmake --build build --target UUT_UART_DIN_BRIDGE
```

Flash the resulting `UUT_UART_DIN_BRIDGE.uf2` to the ProtoZOA UUT Pico.
