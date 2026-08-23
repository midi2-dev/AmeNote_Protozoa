/*
 * lwipopts.h for UUT_NetworkMIDI2_Bridge.
 *
 * Adapted from lib/RP2040-HAT-LWIP-C/port/lwip/lwipopts.h and
 * lib/NetworkMIDI2/examples/midi_bridge/lwip/lwipopts.h: NO_SYS=1
 * bare-metal poll mode (no FreeRTOS/tcpip thread), matching how this
 * project's main.cpp drives everything from a single tud_task()/
 * session.tick() loop, same shape as UUT/DIN_Bridge.
 *
 * TCP/HTTP are disabled since this bridge only needs UDP (Network MIDI
 * 2.0's transport), DHCP (also UDP), and mDNS (also UDP) to obtain an
 * address and discover/advertise Network MIDI 2.0 peers.
 */
#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#define NO_SYS 1
#define MEM_ALIGNMENT 4
#define LWIP_RAW 1
#define LWIP_NETCONN 0
#define LWIP_SOCKET 0
#define LWIP_DHCP 1
#define LWIP_DNS 1
#define LWIP_ICMP 1
#define LWIP_UDP 1
#define LWIP_TCP 0
#define MEM_SIZE 4096

// mDNS (_midi2._udp advertise/browse, see issue #17's client-mode host
// list) -- same settings as lib/NetworkMIDI2/examples/midi_bridge/lwip's
// lwipopts.h, which this app's LwipMdnsDiscovery is shared with.
#define LWIP_IGMP 1 // required for mDNS multicast join
#define LWIP_MDNS_RESPONDER 1
#define MDNS_MAX_SERVICES 2 // _midi2._udp + spare
// LWIP_NUM_NETIF_CLIENT_DATA: user-allocatable per-netif data slots
// (Pico SDK 2.2.0+ uses LWIP_NUM_NETIF_CLIENT_DATA, not the older
// LWIP_NETIF_CLIENT_DATA).
#define LWIP_NUM_NETIF_CLIENT_DATA 1
// Route dns_gethostbyname("name.local") through the mDNS responder.
#define LWIP_DNS_SUPPORT_MDNS_QUERIES 1
// Enable the DNS-SD browse API (mdns_search_service).
#define LWIP_MDNS_SEARCH 1

// MEMP_NUM_SYS_TIMEOUT: pool of simultaneous active timeouts. The SDK
// default (LWIP_NUM_SYS_TIMEOUT_INTERNAL) does not reserve slots for mDNS
// announce/query retransmissions, which crashes ("sys_timeout: timeout !=
// NULL, pool MEMP_SYS_TIMEOUT is empty") as soon as mDNS is exercised
// alongside DHCP/IGMP. Same value and reasoning as lib/NetworkMIDI2/
// examples/midi_bridge/lwip/lwipopts.h.
#define MEMP_NUM_SYS_TIMEOUT 20

// disable ACD to avoid build errors
// http://lwip.100.n7.nabble.com/Build-issue-if-LWIP-DHCP-is-set-to-0-td33280.html
#define LWIP_DHCP_DOES_ACD_CHECK 0

#define ETH_PAD_SIZE 0
#define LWIP_IP_ACCEPT_UDP_PORT(p) ((p) == PP_NTOHS(67))

#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_STATUS_CALLBACK 1

#define LWIP_RAND_WIZ() ((u32_t)rand())

#define LWIP_DEBUG 0

#endif /* __LWIPOPTS_H__ */
