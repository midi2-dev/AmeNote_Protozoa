//
// UUT_NetworkMIDI2_Bridge -- bridges Network MIDI 2.0 (UDP, via a Wiznet
// W5500 Ethernet expansion board on the UUT J13 header) to USB MIDI 2.0
// (tusb_ump). Structured the same way as UUT/DIN_Bridge: a tight
// tud_task()-driven poll loop with no RTOS, just with a NetworkMIDI2
// session standing in for the DIN UART.
//

#include "hardware/watchdog.h"
#include "pico/stdio.h"
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
#include "LwipMdnsDiscovery.h"

// Runtime configuration (role, name, network settings) + boot-time setup
// menu -- see issue #17. Replaces the compile-time #defines this file used
// to have for NM2_USE_DHCP / NM2_STATIC_* / NM2_BRIDGE_ROLE_CLIENT.
#include "bridge_config.h"
#include "console_menu.h"

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
// Role, device name, and DHCP-vs-static network settings are runtime
// configuration now (see bridge_config.h/console_menu.h, issue #17) rather
// than compile-time #defines -- gBridgeConfig is loaded from flash (or
// defaulted) and optionally edited via the boot-time setup menu in main().
static BridgeConfig gBridgeConfig;

#define NM2_SESSION_PORT 5004 // Network MIDI 2.0's recommended default port
static uint16_t NM2_CLIENT_LOCAL_PORT = 5005;

// Pressing this key at any time while the bridge is running reboots into
// the setup menu, so missing the boot-time setup window (or wanting to
// change something) doesn't require a physical power cycle.
constexpr int kReconfigureKey = 0x1B; // ESC

// Minimal UMP Endpoint Discovery identity, matching DIN_Bridge -- AmeNote
// does not have a registered SysEx manufacturer ID, so this uses the
// reserved "educational/non-commercial" prefix (0x7D) per the MIDI
// Association spec.
#define DEVICE_MFRID 0x7D, 0x00, 0x00
#define DEVICE_FAMID 0x00, 0x00
#define DEVICE_MODELID 0x00, 0x00
#define DEVICE_VERSIONID 0, 1, 0, 0

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
        int nameLength = strlen(gBridgeConfig.name);
        for (uint8_t offset = 0; offset < nameLength; offset += 14) {
            std::array<uint32_t, 4> UMP = UMPMessage::mtFMidiEndpointTextNotify(
                    MIDIENDPOINT_NAME_NOTIFICATION, offset, (uint8_t *) gBridgeConfig.name, nameLength);
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
static void wiznet_lwip_init(const BridgeConfig &cfg) {
    wizchip_spi_initialize();
    wizchip_cris_initialize();

    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    setSHAR(mac);
    ctlwizchip(CW_RESET_PHY, 0);

    wiz_NetInfo netInfo = {};
    memcpy(netInfo.mac, mac, 6);
    if (cfg.useDhcp) {
        netInfo.dhcp = NETINFO_DHCP;
    } else {
        netInfo.dhcp = NETINFO_STATIC;
        parseDottedIp(cfg.staticIp, netInfo.ip);
        parseDottedIp(cfg.staticNetmask, netInfo.sn);
        parseDottedIp(cfg.staticGateway, netInfo.gw);
        parseDottedIp(cfg.staticDns, netInfo.dns);
    }
    network_initialize(netInfo);
    print_network_information(netInfo);

    lwip_init();

    if (cfg.useDhcp) {
        netif_add(&g_netif, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, netif_initialize, netif_input);
    } else {
        ip4_addr_t ip, sn, gw;
        IP4_ADDR(&ip, netInfo.ip[0], netInfo.ip[1], netInfo.ip[2], netInfo.ip[3]);
        IP4_ADDR(&sn, netInfo.sn[0], netInfo.sn[1], netInfo.sn[2], netInfo.sn[3]);
        IP4_ADDR(&gw, netInfo.gw[0], netInfo.gw[1], netInfo.gw[2], netInfo.gw[3]);
        netif_add(&g_netif, &ip, &sn, &gw, NULL, netif_initialize, netif_input);
    }
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

    if (cfg.useDhcp) {
        dhcp_start(&g_netif);
    }
}

// Pump any pending Ethernet frames from the W5500 up into lwIP. Must be
// called every loop iteration alongside sys_check_timeouts() since this
// project runs lwIP in NO_SYS=1 (no tcpip thread) -- same pattern as the
// RP2040-HAT-LWIP-C dhcp_dns example this was adapted from.
static void wiznet_lwip_poll() {
    uint16_t pending = 0;
    getsockopt(SOCKET_MACRAW, SO_RECVBUF, &pending);
    if (pending == 0) return;

    // recv_lwip()'s bounds check compares the incoming frame's declared
    // length against the `len` we pass here -- it must be the actual
    // capacity of packetBuf (sizeof(packetBuf)), NOT `pending` (the total
    // bytes currently queued in the W5500's RX buffer, which can be larger
    // than a single frame and is unrelated to packetBuf's size). Passing
    // `pending` here made the bounds check a no-op and let a full-size
    // Ethernet frame overflow a too-small buffer.
    static uint8_t packetBuf[ETHERNET_FRAME_MAX_SIZE];
    uint16_t pack_len = recv_lwip(SOCKET_MACRAW, packetBuf, sizeof(packetBuf));
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

    loadBridgeConfig(gBridgeConfig);
    bool enteredSetup = runConfigMenu(gBridgeConfig);

    wiznet_lwip_init(gBridgeConfig);

    static LwipMdnsDiscovery mdnsDisc;

    // Client role: pick a host now that the network is up, either because
    // the user just walked the setup menu or because no host has ever been
    // configured (first boot).
    if (gBridgeConfig.role == BridgeRole::Client &&
        (enteredSetup || gBridgeConfig.clientHostIp[0] == '\0')) {
        runClientHostSelect(gBridgeConfig, mdnsDisc);
    }

    // Respond to the USB host's UMP Endpoint/Function Block Discovery
    // requests, matching DIN_Bridge.
    UMPHandler.setMidiEndpoint(midiendpoint);
    UMPHandler.setFunctionBlock(functionblock);

    tusb_init();

    EndpointInfo info;
    info.setName(gBridgeConfig.name);
    info.setProductId("com.amenote.protozoa.nm2-bridge");

    NetworkMidiSession::Callbacks cb;
    cb.ctx           = nullptr;
    cb.onUmp          = onNetworkUmp;
    cb.onStateChange = onNetworkStateChange;

    static NetworkMidiSession session(nm2Transport, info, cb);
    nm2Session = &session;

    if (gBridgeConfig.role == BridgeRole::Client) {
        uint8_t hostOctets[4];
        parseDottedIp(gBridgeConfig.clientHostIp, hostOctets);
        UdpEndpoint hostEp;
        hostEp.ipv4 = (uint32_t(hostOctets[0]) << 24) | (uint32_t(hostOctets[1]) << 16) |
                      (uint32_t(hostOctets[2]) << 8) | uint32_t(hostOctets[3]);
        hostEp.port = gBridgeConfig.clientHostPort;
        nm2Session->beginClient(hostEp, NM2_CLIENT_LOCAL_PORT);
    } else {
        nm2Session->beginHost(NM2_SESSION_PORT, &mdnsDisc);
    }

    printf("Bridge running. Press ESC at any time to reboot into setup.\n");

    // ------- Loop: pump lwIP/W5500, USB, and the NM2 session -------
    while (true) {
        tud_task();

        if (getchar_timeout_us(0) == kReconfigureKey) {
            printf("ESC pressed -- rebooting into setup...\n");
            watchdog_reboot(0, 0, 0);
            while (true) tight_loop_contents(); // wait for the reset to take effect
        }

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

                // UMP Stream messages (Message Type 0xF -- Endpoint/Function
                // Block Discovery and their replies) are answered locally
                // above via midiendpoint()/functionblock(); they are USB<->
                // host session-management traffic, not MIDI data, and must
                // not also be relayed onto the NetworkMIDI2 session.
                uint8_t messageType = (UMPpacket[0] >> 28) & 0xF;
                if (messageType != 0xF) {
                    if (!nm2Session->sendUmp(UMPpacket, umpCount)) {
                        printf("[%llu] NM2 sendUmp dropped %u word(s) "
                               "(FIFO full or session not established)\n",
                               time_us_64(), umpCount);
                    }
                }
            }
        }

        // Network -> USB happens inside tick() via onNetworkUmp() above.
        nm2Session->tick();
    }
    return 0;
}
