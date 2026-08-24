//
// UUT_NetworkMIDI2_Bridge -- runtime configuration (role, name, network
// settings), persisted across reboots in the last flash sector. Replaces
// the compile-time NM2_USE_DHCP / NM2_STATIC_* / NM2_BRIDGE_ROLE_CLIENT
// #defines that main.cpp used to require a rebuild to change.
//

#pragma once

#include <cstdint>
#include <cstdio>

enum class BridgeRole : uint8_t { Client = 0, Host = 1 };

struct BridgeConfig {
#ifdef NM2_BRIDGE_DEFAULT_ROLE_HOST
    BridgeRole role            = BridgeRole::Host;  // first-boot default, see CMakeLists.txt's NM2_BRIDGE_ROLE
#else
    BridgeRole role            = BridgeRole::Client; // first-boot default, see CMakeLists.txt's NM2_BRIDGE_ROLE
#endif
    char       name[64]        = "NetworkMIDIUMPBridge";
    bool       useDhcp         = true; // false = use the static* fields below
    char       staticIp[16]    = "192.168.1.200";
    char       staticNetmask[16] = "255.255.255.0";
    char       staticGateway[16] = "192.168.1.1";
    char       staticDns[16]     = "192.168.1.1";

    // Client role only: the host to connect to, as chosen from the mDNS
    // discovery list or entered manually in the config menu.
    char       clientHostIp[16] = "";
    uint16_t   clientHostPort   = 5004; // Network MIDI 2.0's recommended default port
};

// Loads the saved config from flash into `cfg`. If no valid config has ever
// been saved (first boot, or a CRC/magic mismatch), `cfg` is left at the
// BridgeConfig{} defaults above.
void loadBridgeConfig(BridgeConfig &cfg);

// Persists `cfg` to flash so it survives a power cycle / reset.
void saveBridgeConfig(const BridgeConfig &cfg);

// Parses a dotted-decimal IPv4 string ("192.168.1.1") into 4 octets.
// Returns false (and leaves `out` unchanged) if `s` is not a valid address.
bool parseDottedIp(const char *s, uint8_t out[4]);
