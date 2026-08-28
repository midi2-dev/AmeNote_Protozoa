# Vendored from Wiznet/ioLibrary_Driver

This directory is a vendored (not submoduled) subset of Wiznet's
[ioLibrary_Driver](https://github.com/Wiznet/ioLibrary_Driver), previously
pulled in transitively via the `lib/RP2040-HAT-LWIP-C` submodule
(`libraries/ioLibrary_Driver`) -- see
[midi2-dev/AmeNote_Protozoa#23](https://github.com/midi2-dev/AmeNote_Protozoa/issues/23)
for why that submodule chain was removed (it carried its own nested
`libraries/pico-sdk` submodule, an unused multi-gigabyte SDK checkout, that
every clone of this repo paid for).

- **Upstream repo:** https://github.com/Wiznet/ioLibrary_Driver
- **Vendored commit:** `ce4a7b6d07541bf0ba9f91e369276b38faa619bd` (2022-02-21)
- **Subset kept:** only `Ethernet/` (the actual driver code -- W5100/W5100S/W5200/W5300/W5500 register-level chip drivers, `socket.c`/`socket.h`, `wizchip_conf.c`/`wizchip_conf.h`), plus `license.txt` and the upstream `README.md`. Dropped `Application/` and `Internet/` (example protocol implementations -- DHCP/DNS/FTP/HTTP/MQTT/SNTP/loopback -- not used by this project) and the compiled `.chm` help files (binary, not source).
- **Actually used by this project:** only `Ethernet/socket.c`, `Ethernet/wizchip_conf.c`, and `Ethernet/W5500/w5500.c` -- see `UUT/NetworkMIDI2_Bridge/CMakeLists.txt`.

To pick up an upstream update, diff the relevant files against a fresh
checkout of the upstream repo at the desired commit and update this note's
"Vendored commit" line accordingly.
