/*
 * lwipopts.h for UUT_NetworkMIDI2_Bridge.
 *
 * Adapted from lib/RP2040-HAT-LWIP-C/port/lwip/lwipopts.h and
 * lib/NetworkMIDI2/examples/midi_bridge/lwip/lwipopts.h: NO_SYS=1
 * bare-metal poll mode (no FreeRTOS/tcpip thread), matching how this
 * project's main.cpp drives everything from a single tud_task()/
 * session.tick() loop, same shape as UUT/DIN_Bridge.
 *
 * TCP/DNS/HTTP are disabled since this bridge only needs UDP (Network
 * MIDI 2.0's transport) and DHCP (also UDP) to obtain an address.
 */
#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#define NO_SYS 1
#define MEM_ALIGNMENT 4
#define LWIP_RAW 1
#define LWIP_NETCONN 0
#define LWIP_SOCKET 0
#define LWIP_DHCP 1
#define LWIP_DNS 0
#define LWIP_ICMP 1
#define LWIP_UDP 1
#define LWIP_TCP 0
#define MEM_SIZE 4096

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
