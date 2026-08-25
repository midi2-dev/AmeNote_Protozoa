//
// UUT_NetworkMIDI2_Bridge -- interactive boot-time setup menu, driven over
// the app's existing debug UART (stdio_init_all()/printf(), the same
// channel main.cpp already uses for logging). See issue #17: role, name,
// and network settings used to require a rebuild to change.
//

#pragma once

#include "bridge_config.h"
#include "networkmidi2/Discovery.h"

// Callback this module runs inside every wait loop it has -- the setup
// prompt's countdown, readLine()'s wait for the next keypress, and the
// host-discovery browse window. main() passes one that services tud_task().
//
// Why it exists: USB is now initialised before this menu runs (issue #19),
// and the device stack only answers the host's enumeration control
// transfers from tud_task(). Without pumping it here, a boot that sits in
// the menu -- or blocks on a prompt with nobody at the console -- would
// leave the host's GET_DESCRIPTOR requests unanswered and enumeration
// would fail. Pass nullptr to disable pumping.
void setConsolePump(void (*pump)());

// Called once at boot, before the W5500/lwIP network is brought up. Prints
// the current (saved or default) configuration and gives the user a few
// seconds to press a key to enter setup; if nothing is pressed in time,
// returns false immediately and `cfg` is unchanged (already loaded via
// loadBridgeConfig()). If the user enters setup, walks role -> name ->
// DHCP/static (+ static IP/subnet/gateway/DNS prompts) and saves the
// result to flash before returning true.
bool runConfigMenu(BridgeConfig &cfg);

// Called after the network is up, when cfg.role is Client and either the
// user just walked the setup menu or no host has ever been configured.
// Browses for "_midi2._udp" hosts via `disc`, presents a numbered list,
// and lets the user pick one (or enter an IP manually). Updates
// cfg.clientHostIp/clientHostPort and saves to flash.
//
// Returns true if cfg now names a host to connect to.
//
// `interactive` says whether a user is known to be sitting at the console
// (i.e. runConfigMenu() returned true, so somebody pressed a key). When it
// is false we are only here because no host was ever configured, and there
// may be no serial console attached at all -- so this never blocks on
// console input: if discovery turns up nothing it gives up and returns
// false rather than prompting for an IP nobody is there to type. Blocking
// there was the boot-time hang behind issue #19, which stalled main()
// before it ever reached the USB stack.
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
bool runClientHostSelect(BridgeConfig &cfg, networkmidi2::IDiscovery &disc,
                          void (*pollNetwork)(), bool (*isNetworkReady)(),
                          bool interactive);
