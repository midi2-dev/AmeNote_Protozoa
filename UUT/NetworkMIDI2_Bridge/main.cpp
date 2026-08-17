//
// UUT_NetworkMIDI2_Bridge -- bridges Network MIDI 2.0 (UDP, via a Wiznet
// W5500 Ethernet expansion board on the UUT J13 header) to USB MIDI 2.0
// (tusb_ump). Structured the same way as UUT/DIN_Bridge: a tight
// tud_task()-driven poll loop with no RTOS, just with a NetworkMIDI2
// session standing in for the DIN UART.
//

#include "pico/stdio/driver.h"
#include "pico/time.h"
#include "pico/unique_id.h"

// Wiznet W5500 + lwIP bring-up (vendored/customized, see wiznet_port/).
#include "port_common.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "w5x00_spi.h"
#include "w5x00_lwip.h"

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#include "lwip/dhcp.h"

// NetworkMIDI2 session (binary distribution, lwIP transport).
#include "networkmidi2/NetworkMidiSession.h"
#include "networkmidi2/Types.h"
#include "LwipUdpTransport.h"

// USB MIDI 2.0 (tusb_ump) + UMP Endpoint/Function Block Discovery, same as
// DIN_Bridge.
#include "tusb.h"
#include "ump_device.h"
#include "include/umpProcessor.h"
#include "include/umpMessageCreate.h"

using namespace networkmidi2;

// ---------------------------------------------------------------------------
// Network configuration
// ---------------------------------------------------------------------------
#define NM2_USE_DHCP 1 // 0 = use the NM2_STATIC_* addresses below instead

#if !NM2_USE_DHCP
static uint8_t NM2_STATIC_IP[4]      = {192, 168, 1, 200};
static uint8_t NM2_STATIC_SUBNET[4]  = {255, 255, 255, 0};
static uint8_t NM2_STATIC_GATEWAY[4] = {192, 168, 1, 1};
static uint8_t NM2_STATIC_DNS[4]     = {192, 168, 1, 1};
#endif

#define NM2_SESSION_PORT 5004 // Network MIDI 2.0's recommended default port

#ifdef NM2_BRIDGE_ROLE_CLIENT
// CLIENT role: fixed host to connect to. Edit for your setup, or switch
// back to -DNM2_BRIDGE_ROLE=HOST (the CMake default) to listen instead.
#define NM2_CLIENT_HOST_IP0 192
#define NM2_CLIENT_HOST_IP1 168
#define NM2_CLIENT_HOST_IP2 1
#define NM2_CLIENT_HOST_IP3 100
#define NM2_CLIENT_LOCAL_PORT 5005
#endif

// Minimal UMP Endpoint Discovery identity, matching DIN_Bridge -- AmeNote
// does not have a registered SysEx manufacturer ID, so this uses the
// reserved "educational/non-commercial" prefix (0x7D) per the MIDI
// Association spec.
#define DEVICE_MFRID 0x7D, 0x00, 0x00
#define DEVICE_FAMID 0x00, 0x00
#define DEVICE_MODELID 0x00, 0x00
#define DEVICE_VERSIONID 0, 1, 0, 0
#define DEVICE_MIDIENDPOINTNAME "AmeNote ProtoZOA NetworkMIDI2 Bridge"

// Defined in wiznet_port/w5x00_lwip.c.
extern uint8_t mac[6];

// ---------------------------------------------------------------------------
// lwIP / W5500 state
// ---------------------------------------------------------------------------
static struct netif g_netif;
#define SOCKET_MACRAW 0

// ---------------------------------------------------------------------------
// NetworkMIDI2 session state
// ---------------------------------------------------------------------------
static LwipUdpTransport nm2Transport;
static NetworkMidiSession *nm2Session = nullptr;

umpProcessor UMPHandler;

void midiendpoint(uint8_t majVer, uint8_t minVer, uint8_t filter);
void functionblock(uint8_t fbIdx, uint8_t filter);

// Diagnostic: log every alt-setting change, matching DIN_Bridge.
void tud_ump_set_itf_cb(uint8_t itf, uint8_t alt) {
    printf("[%llu] ALT_SET itf=%d alt=%d (mVersion=%d)\n", time_us_64(), itf, alt, alt + 1);
}

// Reply to a host's UMP Endpoint Discovery request (Stream message, MT=0xF).
// Same pattern as DIN_Bridge/main.cpp.
void midiendpoint(uint8_t majVer, uint8_t minVer, uint8_t filter) {
    (void) majVer;
    (void) minVer;

    if (filter & 0x1) {
        std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointInfoNotify(
                1, true, true, false, false);
        tud_ump_write_hton(0, UMP.data(), 4);
    }

    if (filter & 0x2) {
        std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointDeviceInfoNotify(
                {DEVICE_MFRID}, {DEVICE_FAMID}, {DEVICE_MODELID}, {DEVICE_VERSIONID});
        tud_ump_write_hton(0, UMP.data(), 4);
    }

    if (filter & 0x4) {
        int nameLength = strlen(DEVICE_MIDIENDPOINTNAME);
        for (uint8_t offset = 0; offset < nameLength; offset += 14) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointTextNotify(
                    MIDIENDPOINT_NAME_NOTIFICATION, offset, (uint8_t *) DEVICE_MIDIENDPOINTNAME, nameLength);
            tud_ump_write_hton(0, UMP.data(), 4);
        }
    }
}

// Reply to a host's UMP Function Block Discovery request. This bridge
// exposes a single bidirectional function block covering the one UMP group
// carried over the network session.
void functionblock(uint8_t fbIdx, uint8_t filter) {
    if (fbIdx != 0 && fbIdx != 0xFF) return;

    if (filter & 0x1) {
        std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockInfoNotify(
                0, true, 3 /*bidirectional*/, false /*sender*/, false /*recv*/,
                0 /*firstGroup*/, 1 /*groupLength*/, 0x00 /*midiCISupport*/,
                0 /*isMIDI1: full MIDI 2.0 bandwidth over the network transport*/,
                0 /*maxS8Streams*/);
        tud_ump_write_hton(0, UMP.data(), 4);
    }

    if (filter & 0x2) {
        char const *name = "NetworkMIDI2 Bridge";
        std::array<uint32_t, 4> UMP = UMPMessage::mtFFunctionBlockNameNotify(
                0, 0, (uint8_t *) name, strlen(name));
        tud_ump_write_hton(0, UMP.data(), 4);
    }
}

// Called synchronously from within nm2Session->tick() for each UMP message
// received over the network. Per NetworkMIDI2's docs this must not block or
// call sendUmp()/close() -- tud_ump_write_hton() is a non-blocking FIFO
// write, matching how DIN_Bridge forwards DIN bytes to USB directly.
static void onNetworkUmp(void *ctx, const uint32_t *words, size_t wordCount) {
    (void) ctx;
    tud_ump_write_hton(0, const_cast<uint32_t *>(words), (uint8_t) wordCount);
}

static void onNetworkStateChange(void *ctx, SessionState newState) {
    (void) ctx;
    static const char *kStateNames[] = {
            "Idle", "PendingInvitation", "AuthRequired",
            "Established", "PendingReset", "PendingBye",
    };
    printf("[%llu] NM2 session state -> %s\n", time_us_64(), kStateNames[(int) newState]);
}

// ---------------------------------------------------------------------------
// Wiznet W5500 + lwIP netif bring-up
// ---------------------------------------------------------------------------
static void wiznet_lwip_init() {
    wizchip_spi_initialize();
    wizchip_cris_initialize();

    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    setSHAR(mac);
    ctlwizchip(CW_RESET_PHY, 0);

    wiz_NetInfo netInfo = {};
    memcpy(netInfo.mac, mac, 6);
#if NM2_USE_DHCP
    netInfo.dhcp = NETINFO_DHCP;
#else
    netInfo.dhcp = NETINFO_STATIC;
    memcpy(netInfo.ip, NM2_STATIC_IP, 4);
    memcpy(netInfo.sn, NM2_STATIC_SUBNET, 4);
    memcpy(netInfo.gw, NM2_STATIC_GATEWAY, 4);
    memcpy(netInfo.dns, NM2_STATIC_DNS, 4);
#endif
    network_initialize(netInfo);
    print_network_information(netInfo);

    lwip_init();

#if NM2_USE_DHCP
    netif_add(&g_netif, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, netif_initialize, netif_input);
#else
    ip4_addr_t ip, sn, gw;
    IP4_ADDR(&ip, NM2_STATIC_IP[0], NM2_STATIC_IP[1], NM2_STATIC_IP[2], NM2_STATIC_IP[3]);
    IP4_ADDR(&sn, NM2_STATIC_SUBNET[0], NM2_STATIC_SUBNET[1], NM2_STATIC_SUBNET[2], NM2_STATIC_SUBNET[3]);
    IP4_ADDR(&gw, NM2_STATIC_GATEWAY[0], NM2_STATIC_GATEWAY[1], NM2_STATIC_GATEWAY[2], NM2_STATIC_GATEWAY[3]);
    netif_add(&g_netif, &ip, &sn, &gw, NULL, netif_initialize, netif_input);
#endif
    g_netif.name[0] = 'e';
    g_netif.name[1] = '0';

    netif_set_link_callback(&g_netif, netif_link_callback);
    netif_set_status_callback(&g_netif, netif_status_callback);

    if (socket(SOCKET_MACRAW, Sn_MR_MACRAW, NM2_SESSION_PORT, 0x00) < 0) {
        printf("MACRAW socket open failed\n");
    }

    netif_set_default(&g_netif);
    netif_set_link_up(&g_netif);
    netif_set_up(&g_netif);

#if NM2_USE_DHCP
    dhcp_start(&g_netif);
#endif
}

// Pump any pending Ethernet frames from the W5500 up into lwIP. Must be
// called every loop iteration alongside sys_check_timeouts() since this
// project runs lwIP in NO_SYS=1 (no tcpip thread) -- same pattern as the
// RP2040-HAT-LWIP-C dhcp_dns example this was adapted from.
static void wiznet_lwip_poll() {
    uint16_t pack_len = 0;
    getsockopt(SOCKET_MACRAW, SO_RECVBUF, &pack_len);
    if (pack_len == 0) return;

    static uint8_t packetBuf[ETHERNET_MTU];
    pack_len = recv_lwip(SOCKET_MACRAW, packetBuf, pack_len);
    if (pack_len == 0) return;

    struct pbuf *p = pbuf_alloc(PBUF_RAW, pack_len, PBUF_POOL);
    if (p == nullptr) return;
    pbuf_take(p, packetBuf, pack_len);

    LINK_STATS_INC(link.recv);
    if (g_netif.input(p, &g_netif) != ERR_OK) {
        pbuf_free(p);
    }
}

int main() {
    stdio_init_all();

    printf("Starting AmeNote ProtoZOA NetworkMIDI2 Bridge\n");

    wiznet_lwip_init();

    // Respond to the USB host's UMP Endpoint/Function Block Discovery
    // requests, matching DIN_Bridge.
    UMPHandler.setMidiEndpoint(midiendpoint);
    UMPHandler.setFunctionBlock(functionblock);

    tusb_init();

    EndpointInfo info;
    info.setName("ProtoZOA NM2 Bridge");
    info.setProductId("com.amenote.protozoa.nm2-bridge");

    NetworkMidiSession::Callbacks cb;
    cb.ctx           = nullptr;
    cb.onUmp          = onNetworkUmp;
    cb.onStateChange = onNetworkStateChange;

    static NetworkMidiSession session(nm2Transport, info, cb);
    nm2Session = &session;

#ifdef NM2_BRIDGE_ROLE_CLIENT
    UdpEndpoint hostEp;
    hostEp.ipv4 = (uint32_t(NM2_CLIENT_HOST_IP0) << 24) | (uint32_t(NM2_CLIENT_HOST_IP1) << 16) |
                  (uint32_t(NM2_CLIENT_HOST_IP2) << 8) | uint32_t(NM2_CLIENT_HOST_IP3);
    hostEp.port = NM2_SESSION_PORT;
    nm2Session->beginClient(hostEp, NM2_CLIENT_LOCAL_PORT);
#else
    nm2Session->beginHost(NM2_SESSION_PORT);
#endif

    // ------- Loop: pump lwIP/W5500, USB, and the NM2 session -------
    while (true) {
        tud_task();

        wiznet_lwip_poll();
        sys_check_timeouts();

        // USB -> Network: drain tusb_ump and forward each UMP message into
        // the NetworkMIDI2 session's TX FIFO.
        if (tud_ump_n_mounted(0) && tud_ump_n_available(0)) {
            uint32_t UMPpacket[4];
            uint8_t umpCount = tud_ump_read_ntoh(0, UMPpacket, 4);
            if (umpCount) {
                for (uint8_t i = 0; i < umpCount; i++) {
                    // Endpoint/Function Block Discovery Stream messages are
                    // handled here; everything else (Channel Voice, etc.)
                    // passes straight through to the network session.
                    UMPHandler.processUMP(UMPpacket[i]);
                }
                nm2Session->sendUmp(UMPpacket, umpCount);
            }
        }

        // Network -> USB happens inside tick() via onNetworkUmp() above.
        nm2Session->tick();
    }
    return 0;
}
