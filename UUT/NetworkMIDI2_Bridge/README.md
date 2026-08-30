# UUT_NetworkMIDI2_Bridge

Bridges **Network MIDI 2.0** (UDP, via a Wiznet W5500 Ethernet expansion
board on the UUT J13 expansion header) to **USB MIDI 2.0** (tusb_ump), the
same way `UUT/DIN_Bridge` bridges a MIDI 1.0 DIN port to USB MIDI 2.0.

## SPI wiring

`wiznet_port/w5x00_spi.h` defines the SPI + reset pins for the Wiznet
W5x00 expansion board on the UUT J13 expansion header:

| RP2040 GPIO | Wiznet signal |
|---|---|
| GPIO9  | SPI_CS_N |
| GPIO10 | SPI_SCK |
| GPIO11 | SPI_MOSI |
| GPIO8  | SPI_MISO |
| GPIO3  | W5500_RST_N |
| GPIO15 | W5500_INT_N (not currently wired up; driver is polling-based) |

`wiznet_port/` also assumes a **W5500** chip (set via `WIZNET_CHIP` in
`CMakeLists.txt`). If your expansion board uses a W5100S instead, change
`WIZNET_CHIP` to `W5100S` and swap the `W5500/w5500.c` source/include paths
for `W5100S/w5100s.c`.

## Configuration

- **IP address**: DHCP by default (`NM2_USE_DHCP 1` in `main.cpp`). Set it
  to `0` and fill in `NM2_STATIC_IP`/`NM2_STATIC_SUBNET`/
  `NM2_STATIC_GATEWAY`/`NM2_STATIC_DNS` for a static address instead.
- **Session role**: `NM2_BRIDGE_ROLE` CMake cache variable, `HOST` (default,
  listens on UDP 5004 for a client) or `CLIENT` (connects out to a fixed
  host — edit `NM2_CLIENT_HOST_IP*`/`NM2_CLIENT_LOCAL_PORT` in `main.cpp`).
  ```
  cmake --preset release -DNM2_BRIDGE_ROLE=CLIENT
  ```
- **MAC address**: `wiznet_port/w5x00_lwip.c` sets a locally-administered
  default (`02:50:5A:4E:4D:32`). If more than one of these boards will
  share a LAN, make the low 3 bytes unique per board.

## Dependencies

- `lib/NetworkMIDI2` — AmeNote's Network MIDI 2.0 session library (binary
  distribution: headers + prebuilt static libs for `rp2040`).
- `lib/ioLibrary_Driver` — WIZnet's chip driver for the W5500/W5100S on
  RP2040 (just the `Ethernet/` subset actually used here), vendored (a
  plain copy, not a submodule) from WIZnet's `RP2040-HAT-LWIP-C` repo's
  `libraries/ioLibrary_Driver` -- see `lib/ioLibrary_Driver/VENDORED.md`
  for provenance. The small "port" glue files (`w5x00_spi.*`,
  `w5x00_lwip.*`, `lwipopts.h`) are separately vendored and customized in
  `wiznet_port/` per WIZnet's own documented workflow of copying and
  editing those files per board.
