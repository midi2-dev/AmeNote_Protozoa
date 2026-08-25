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

Role, device name, and DHCP-vs-static network settings are **runtime**
configuration (see `bridge_config.h`/`console_menu.h`, issue #17), persisted
to the last flash sector so they survive a power cycle — no rebuild needed
to change them:

- On first boot (or after holding the setup window open), a serial console
  menu (UART, 115200 8N1) walks through **Role** (`Host` listens on UDP
  5004 for a client; `Client` connects out to a host you pick from mDNS
  discovery or enter manually), **Network MIDI name**, and
  **DHCP vs. static IP**.
- While the bridge is running, press **ESC** at any time on the same UART
  to reboot straight back into this menu — no power cycle required.
- `NM2_BRIDGE_ROLE` (CMake cache variable, `HOST` or `CLIENT`) only seeds
  the very first boot's default role before any config has been saved to
  flash; it has no effect afterward.
  ```
  cmake --preset release -DNM2_BRIDGE_ROLE=CLIENT
  ```
- **MAC address**: `wiznet_port/w5x00_lwip.c` sets a locally-administered
  default (`02:50:5A:4E:4D:32`). If more than one of these boards will
  share a LAN, make the low 3 bytes unique per board.

### USB enumeration on Windows (issue #19)

Early builds failed to enumerate on Windows entirely: `main()` called
`tusb_init()` only after the boot-time setup menu, the W5500 bring-up, and
mDNS discovery, so the RP2040 never asserted its USB D+ pull-up until all of
that had finished — and an unattended first boot (Client role, no saved
host) blocked on that path indefinitely. Fixed by moving `tusb_init()` to
the top of `main()` and having every blocking wait (menu countdown,
`readLine()`, discovery browse) service `tud_task()` so enumeration
completes even while the board is sitting at a prompt. Verified end-to-end
on hardware: a Windows PC driving the board's native USB UMP port, through
an established NetworkMIDI2 session, echoed via `tools/mac_nm2_usb_loopback`
on a Mac acting as the session Host.

## Dependencies

- `lib/NetworkMIDI2` — AmeNote's Network MIDI 2.0 session library (binary
  distribution: headers + prebuilt static libs for `rp2040`).
- `lib/RP2040-HAT-LWIP-C` — WIZnet's official ioLibrary_Driver + lwIP port
  for the W5500/W5100S on RP2040. Only `libraries/ioLibrary_Driver` (the
  chip driver) is used from it directly; the small "port" glue files
  (`w5x00_spi.*`, `w5x00_lwip.*`, `lwipopts.h`) are vendored and customized
  in `wiznet_port/` per WIZnet's own documented workflow of copying and
  editing those files per board.
