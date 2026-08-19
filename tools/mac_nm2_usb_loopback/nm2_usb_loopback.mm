/**
 * nm2_usb_loopback -- Mac-side network echo for UUT_NetworkMIDI2_Bridge.
 *
 * ProtoZOA runs unmodified as a plain USB MIDI <-> Network MIDI 2.0 bridge.
 * This tool is a pure NetworkMIDI2 client: whatever it receives from
 * ProtoZOA over the network, it sends straight back over the network. No
 * CoreMIDI/USB involvement on the Mac side at all -- USB MIDI only enters
 * the picture on ProtoZOA's end, driven externally (e.g. PocketMIDI feeding
 * ProtoZOA's USB MIDI input, or a monitor watching its USB MIDI output).
 *
 * This exercises both directions of ProtoZOA's own bridge: whatever comes
 * in over USB gets forwarded to the network, echoed straight back by this
 * tool, and forwarded back out over USB by ProtoZOA again.
 *
 * Build (from repo root):
 *   clang++ -std=c++17 -fobjc-arc \
 *     -I lib/NetworkMIDI2/include -I lib/NetworkMIDI2/transports/posix \
 *     tools/mac_nm2_usb_loopback/nm2_usb_loopback.mm \
 *     lib/NetworkMIDI2/lib/macos/arm64/libnm2_transport_posix.a \
 *     lib/NetworkMIDI2/lib/macos/arm64/libnetworkmidi2.a \
 *     -o tools/mac_nm2_usb_loopback/nm2_usb_loopback
 *
 * Run:
 *   tools/mac_nm2_usb_loopback/nm2_usb_loopback --host 10.0.0.13 --port 5004
 */

#include <arpa/inet.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <vector>

#include "networkmidi2/NetworkMidiSession.h"
#include "PosixUdpTransport.h"

using namespace networkmidi2;

static std::atomic<bool>  gRunning{true};
static NetworkMidiSession *gSession = nullptr;

// onNetworkUmp() runs synchronously inside session.tick() and, per
// NetworkMidiSession.h, must not call sendUmp() re-entrantly from there.
// Queue the echo instead and flush it from main()'s loop after tick()
// returns -- both run on the same (single) thread, so no locking is
// needed, just deferral past the end of the tick() call.
struct PendingEcho {
    uint32_t words[4];
    size_t wordCount;
};
static std::vector<PendingEcho> gPendingEchoes;

static void handleSigint(int) { gRunning = false; }

// Echo whatever arrives from ProtoZOA straight back over the network.
static void onNetworkUmp(void *ctx, const uint32_t *words, size_t wordCount) {
    (void) ctx;
    printf("[NET  -> ]:");
    for (size_t i = 0; i < wordCount; i++) printf(" %08X", words[i]);
    printf("\n");

    PendingEcho echo;
    echo.wordCount = wordCount > 4 ? 4 : wordCount;
    memcpy(echo.words, words, echo.wordCount * sizeof(uint32_t));
    gPendingEchoes.push_back(echo);
}

static void onNetworkStateChange(void *ctx, SessionState newState) {
    (void) ctx;
    static const char *kStateNames[] = {
            "Idle", "PendingInvitation", "AuthRequired",
            "Established", "PendingReset", "PendingBye",
    };
    printf("[nm2] State -> %s\n", kStateNames[(int) newState]);
}

static uint32_t parseIpv4(const char *s) {
    struct in_addr a{};
    if (::inet_pton(AF_INET, s, &a) != 1) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", s);
        exit(1);
    }
    return ntohl(a.s_addr);
}

int main(int argc, char *argv[]) {
    uint32_t hostIp = 0x0A00000Du; // 10.0.0.13
    uint16_t hostPort = 5004;
    uint16_t localPort = 5006;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--host") && i + 1 < argc) hostIp = parseIpv4(argv[++i]);
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) hostPort = (uint16_t) strtoul(argv[++i], nullptr, 10);
        else if (!strcmp(argv[i], "--local") && i + 1 < argc) localPort = (uint16_t) strtoul(argv[++i], nullptr, 10);
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("Usage: %s [--host <ip>] [--port <port>] [--local <port>]\n", argv[0]);
            return 0;
        }
    }

    signal(SIGINT, handleSigint);
    setvbuf(stdout, nullptr, _IOLBF, 0);

    EndpointInfo info;
    info.setName("Mac NM2 Network Loopback");
    info.setProductId("MAC-LOOPBACK-0001");

    PosixUdpTransport transport;

    NetworkMidiSession::Callbacks cb;
    cb.onUmp         = onNetworkUmp;
    cb.onStateChange = onNetworkStateChange;
    cb.ctx           = nullptr;

    NetworkMidiSession session(transport, info, cb);
    gSession = &session;

    UdpEndpoint hostEp;
    hostEp.ipv4 = hostIp;
    hostEp.port = hostPort;

    printf("Connecting to %u.%u.%u.%u:%u ...\n",
           (hostIp >> 24) & 0xFF, (hostIp >> 16) & 0xFF, (hostIp >> 8) & 0xFF, hostIp & 0xFF, hostPort);
    session.beginClient(hostEp, localPort);

    while (gRunning) {
        session.tick();

        for (const auto &echo : gPendingEchoes) {
            gSession->sendUmp(echo.words, echo.wordCount);
        }
        gPendingEchoes.clear();

        usleep(1000);
    }

    printf("Closing...\n");
    session.close();
    return 0;
}
