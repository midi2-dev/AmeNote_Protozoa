//
// UUT_NetworkMIDI2_Bridge -- interactive boot-time setup menu, driven over
// the app's existing debug UART (stdio_init_all()/printf(), the same
// channel main.cpp already uses for logging). See issue #17: role, name,
// and network settings used to require a rebuild to change.
//

#pragma once

#include "bridge_config.h"
#include "networkmidi2/Discovery.h"

// Called once at boot, before the W5500/lwIP network is brought up. Prints
// the current (saved or default) configuration and gives the user a few
// seconds to press a key to enter setup; if nothing is pressed in time,
// returns false immediately and `cfg` is unchanged (already loaded via
// loadBridgeConfig()). If the user enters setup, walks role -> name ->
// DHCP/static (+ static IP/subnet/gateway/DNS prompts) and saves the
// result to flash before returning true.
bool runConfigMenu(BridgeConfig &cfg);

// Called after the network is up, only when runConfigMenu() returned true
// and cfg.role is Client. Browses for "_midi2._udp" hosts via `disc`,
// presents a numbered list, and lets the user pick one (or enter an IP
// manually). Updates cfg.clientHostIp/clientHostPort and saves to flash.
void runClientHostSelect(BridgeConfig &cfg, networkmidi2::IDiscovery &disc);
