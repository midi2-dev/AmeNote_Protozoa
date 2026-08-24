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
//
// `pollNetwork` is called every ~50ms while browsing -- this app runs lwIP
// with NO_SYS=1 (no tcpip thread), so without pumping the W5500 RX path and
// lwIP's timers here (normally done by main()'s own loop, which hasn't
// started yet at this point), no Ethernet frames -- including mDNS
// responses -- would ever be read off the wire and discovery could never
// succeed no matter how long the browse window is.
//
// `isNetworkReady` (e.g. LwipMdnsDiscovery::isNetifReady) is polled before
// calling disc.browse(): browse() sends its mDNS query synchronously and
// without retry, so calling it before DHCP has assigned an address means
// the one query it gets to send goes out with a bogus 0.0.0.0 source and is
// silently dropped by real responders -- wait (briefly, still pumping
// pollNetwork) for a valid address first.
void runClientHostSelect(BridgeConfig &cfg, networkmidi2::IDiscovery &disc,
                          void (*pollNetwork)(), bool (*isNetworkReady)());
