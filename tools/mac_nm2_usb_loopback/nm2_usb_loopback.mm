/**
 * nm2_usb_loopback -- Mac-side round-trip test for UUT_NetworkMIDI2_Bridge.
 *
 * ProtoZOA runs unmodified as a plain USB MIDI <-> Network MIDI 2.0 bridge.
 * This tool does the loopback itself, entirely on the Mac side, so both
 * bridge directions get exercised without touching the firmware:
 *
 *   1. Opens a NetworkMIDI2 client session to ProtoZOA (UDP).
 *   2. Opens a CoreMIDI connection to ProtoZOA's USB MIDI 2.0 endpoint.
 *   3. Sends one seed Note On over the network.
 *   4. Whatever arrives over the network gets echoed straight back out over
 *      USB; whatever arrives over USB gets echoed straight back out over the
 *      network -- so the same message bounces network -> USB -> network ->
 *      USB ... indefinitely, continuously exercising both directions of the
 *      ProtoZOA bridge until Ctrl-C.
 *
 * Build (from repo root):
 *   clang++ -std=c++17 -fobjc-arc \
 *     -I lib/NetworkMIDI2/include -I lib/NetworkMIDI2/transports/posix \
 *     tools/mac_nm2_usb_loopback/nm2_usb_loopback.mm \
 *     lib/NetworkMIDI2/lib/macos/arm64/libnm2_transport_posix.a \
 *     lib/NetworkMIDI2/lib/macos/arm64/libnetworkmidi2.a \
 *     -framework CoreMIDI -framework CoreFoundation \
 *     -o tools/mac_nm2_usb_loopback/nm2_usb_loopback
 *
 * Run:
 *   tools/mac_nm2_usb_loopback/nm2_usb_loopback --host 10.0.0.13 --port 5004
 *
 * If auto-detection of ProtoZOA's CoreMIDI source/destination picks the
 * wrong device, pass --src-index/--dst-index using the numbers printed at
 * startup.
 */

#import <CoreMIDI/CoreMIDI.h>
#include <arpa/inet.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <string>
#include <unistd.h>

#include "networkmidi2/NetworkMidiSession.h"
#include "PosixUdpTransport.h"

using namespace networkmidi2;

// ---------------------------------------------------------------------------
// Globals -- both the NetworkMIDI2 tick loop and CoreMIDI's own callback
// thread need to reach these.
// ---------------------------------------------------------------------------
static std::atomic<bool>  gRunning{true};
static NetworkMidiSession *gSession       = nullptr;
static MIDIPortRef          gMidiOutPort  = 0;
static MIDIEndpointRef      gMidiDest     = 0;

static void handleSigint(int) { gRunning = false; }

static void printUmp(const char *prefix, const uint32_t *words, size_t count) {
    printf("%s:", prefix);
    for (size_t i = 0; i < count; i++) printf(" %08X", words[i]);
    printf("\n");
}

// ---------------------------------------------------------------------------
// CoreMIDI -> Network: echo whatever arrives from ProtoZOA's USB output
// straight back out over the NetworkMIDI2 session.
// ---------------------------------------------------------------------------
static void midiReceived(const MIDIEventList *evtlist, void *srcConnRefCon) {
    (void) srcConnRefCon;
    const MIDIEventPacket *packet = &evtlist->packet[0];
    for (UInt32 p = 0; p < evtlist->numPackets; p++) {
        printUmp("[USB  -> ]", packet->words, packet->wordCount);
        if (gSession != nullptr) {
            gSession->sendUmp(packet->words, packet->wordCount);
        }
        packet = MIDIEventPacketNext(packet);
    }
}

// ---------------------------------------------------------------------------
// Network -> CoreMIDI: echo whatever arrives from ProtoZOA over the network
// straight back out over USB.
// ---------------------------------------------------------------------------
static void onNetworkUmp(void *ctx, const uint32_t *words, size_t wordCount) {
    (void) ctx;
    printUmp("[NET  -> ]", words, wordCount);

    if (gMidiOutPort == 0 || gMidiDest == 0) return;

    uint8_t buf[sizeof(MIDIEventList)];
    MIDIEventList *evtList = reinterpret_cast<MIDIEventList *>(buf);
    MIDIEventPacket *packet = MIDIEventListInit(evtList, kMIDIProtocol_2_0);
    MIDIEventListAdd(evtList, sizeof(buf), packet, 0, wordCount, words);
    MIDISendEventList(gMidiOutPort, gMidiDest, evtList);
}

static void onNetworkStateChange(void *ctx, SessionState newState) {
    (void) ctx;
    static const char *kStateNames[] = {
            "Idle", "PendingInvitation", "AuthRequired",
            "Established", "PendingReset", "PendingBye",
    };
    printf("[nm2] State -> %s\n", kStateNames[(int) newState]);
}

// ---------------------------------------------------------------------------
// CoreMIDI endpoint discovery
// ---------------------------------------------------------------------------
static std::string endpointName(MIDIObjectRef obj) {
    CFStringRef name = nullptr;
    if (MIDIObjectGetStringProperty(obj, kMIDIPropertyDisplayName, &name) != noErr || name == nullptr) {
        if (MIDIObjectGetStringProperty(obj, kMIDIPropertyName, &name) != noErr || name == nullptr) {
            return "(unnamed)";
        }
    }
    char buf[256] = {};
    CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8);
    CFRelease(name);
    return std::string(buf);
}

static bool nameMatches(const std::string &name) {
    static const char *needles[] = {"ProtoZOA", "NM2", "Network Bridge"};
    for (const char *n : needles) {
        if (name.find(n) != std::string::npos) return true;
    }
    return false;
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
    int srcIndexOverride = -1;
    int dstIndexOverride = -1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--host") && i + 1 < argc) hostIp = parseIpv4(argv[++i]);
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) hostPort = (uint16_t) strtoul(argv[++i], nullptr, 10);
        else if (!strcmp(argv[i], "--local") && i + 1 < argc) localPort = (uint16_t) strtoul(argv[++i], nullptr, 10);
        else if (!strcmp(argv[i], "--src-index") && i + 1 < argc) srcIndexOverride = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dst-index") && i + 1 < argc) dstIndexOverride = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("Usage: %s [--host <ip>] [--port <port>] [--local <port>] "
                   "[--src-index N] [--dst-index N]\n", argv[0]);
            return 0;
        }
    }

    signal(SIGINT, handleSigint);
    setvbuf(stdout, nullptr, _IOLBF, 0); // line-buffer stdout even when piped

    // -------------------------------------------------------------------
    // CoreMIDI setup
    // -------------------------------------------------------------------
    MIDIClientRef client = 0;
    MIDIClientCreateWithBlock(CFSTR("nm2_usb_loopback"), &client, ^(const MIDINotification *){});

    printf("CoreMIDI sources:\n");
    MIDIEndpointRef srcEndpoint = 0;
    ItemCount numSrc = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < numSrc; i++) {
        MIDIEndpointRef ep = MIDIGetSource(i);
        std::string name = endpointName(ep);
        printf("  [%2llu] %s\n", (unsigned long long) i, name.c_str());
        if (srcIndexOverride >= 0 ? (int) i == srcIndexOverride : (srcEndpoint == 0 && nameMatches(name))) {
            srcEndpoint = ep;
        }
    }

    printf("CoreMIDI destinations:\n");
    ItemCount numDst = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < numDst; i++) {
        MIDIEndpointRef ep = MIDIGetDestination(i);
        std::string name = endpointName(ep);
        printf("  [%2llu] %s\n", (unsigned long long) i, name.c_str());
        if (dstIndexOverride >= 0 ? (int) i == dstIndexOverride : (gMidiDest == 0 && nameMatches(name))) {
            gMidiDest = ep;
        }
    }

    if (srcEndpoint == 0 || gMidiDest == 0) {
        fprintf(stderr, "Could not auto-detect ProtoZOA's USB MIDI endpoints. "
                        "Re-run with --src-index/--dst-index from the list above.\n");
        return 1;
    }

    MIDIPortRef inPort = 0;
    MIDIInputPortCreateWithProtocol(client, CFSTR("In"), kMIDIProtocol_2_0, &inPort, ^(const MIDIEventList *evtlist, void *srcConnRefCon) {
        midiReceived(evtlist, srcConnRefCon);
    });
    MIDIPortConnectSource(inPort, srcEndpoint, nullptr);
    MIDIOutputPortCreate(client, CFSTR("Out"), &gMidiOutPort);

    // -------------------------------------------------------------------
    // NetworkMIDI2 client setup
    // -------------------------------------------------------------------
    EndpointInfo info;
    info.setName("Mac NM2/USB Loopback");
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

    // -------------------------------------------------------------------
    // Main loop -- also seeds one note once Established.
    // -------------------------------------------------------------------
    bool seeded = false;
    while (gRunning) {
        session.tick();

        if (!seeded && session.state() == SessionState::Established) {
            seeded = true;
            uint32_t noteOn[2] = {0x40903C00u, 0xFFFF0000u}; // MT4 Note On, ch0, note 60 (middle C)
            printf("Seeding loopback with one Note On (note 60)...\n");
            session.sendUmp(noteOn, 2);
        }

        usleep(1000);
    }

    printf("Closing...\n");
    session.close();
    return 0;
}
