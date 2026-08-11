# UUT_NetworkMIDI2_Bridge

Bridges **Network MIDI 2.0** (UDP, via a Wiznet W5500 Ethernet expansion
board on the UUT J13 expansion header) to **USB MIDI 2.0** (tusb_ump), the
same way `UUT/DIN_Bridge` bridges a MIDI 1.0 DIN port to USB MIDI 2.0.

## Before first flash: confirm the SPI wiring

`wiznet_port/w5x00_spi.h` defines the SPI + reset pins as a best-effort
reading of `doc/Resources/ProtoZOA -- Schematic...pdf` (sheet 3, "UUT and
Expansion", connector J13 — net names `XPNSN_SPI_TX`/`XPNSN_SPI_CLK`/
`XPNSN_SPI_RX` and spares `XPNSN_PIN_A`/`XPNSN_PIN_B`). PDF text extraction
could not fully disambiguate the exact GPIO numbers from OCR'd text alone —
**verify against the KiCad source (`UUT and Expansion/Sheet_03.kicad_sch`)
or a continuity check before powering up a board**, then update the
`PIN_SCK` / `PIN_MOSI` / `PIN_MISO` / `PIN_CS` / `PIN_RST` defines in that
file if they're wrong.

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
- `lib/RP2040-HAT-LWIP-C` — WIZnet's official ioLibrary_Driver + lwIP port
  for the W5500/W5100S on RP2040. Only `libraries/ioLibrary_Driver` (the
  chip driver) is used from it directly; the small "port" glue files
  (`w5x00_spi.*`, `w5x00_lwip.*`, `lwipopts.h`) are vendored and customized
  in `wiznet_port/` per WIZnet's own documented workflow of copying and
  editing those files per board.
