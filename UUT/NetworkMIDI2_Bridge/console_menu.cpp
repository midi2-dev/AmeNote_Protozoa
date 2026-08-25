#include "console_menu.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pico/stdio.h"
#include "pico/time.h"

using networkmidi2::DiscoveredPeer;
using networkmidi2::IDiscovery;

namespace {

constexpr uint32_t kSetupPromptTimeoutMs = 3000;
constexpr uint32_t kHostBrowseMs         = 4000;
constexpr uint32_t kNetifReadyTimeoutMs  = 3000;
constexpr unsigned  kMaxListedHosts      = 8;

// Set by setConsolePump(); run inside every wait loop below so the USB
// device stack keeps being serviced while we sit at a prompt. See the
// comment on setConsolePump() in console_menu.h.
void (*gPump)() = nullptr;

void pumpTick() {
    if (gPump) gPump();
}

// Reads one line (echoed) into buf, up to bufSize-1 chars, terminated by
// CR or LF. Backspace/DEL deletes the previous character.
//
// Waits for input by polling rather than blocking in getchar(), so the wait
// keeps pumpTick() running -- USB enumeration is serviced from there, and a
// prompt that parked in getchar() would stall it (issue #19). This still
// waits indefinitely, so only call it on paths where somebody is known to
// be at the console; unattended paths must not prompt at all.
void readLine(char *buf, size_t bufSize) {
    size_t len = 0;
    buf[0] = '\0';
    while (true) {
        pumpTick();

        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) continue;
        if (c == '\r' || c == '\n') {
            printf("\r\n");
            break;
        }
        if ((c == '\b' || c == 0x7F) && len > 0) {
            len--;
            printf("\b \b");
        } else if (c >= 0x20 && c < 0x7F && len < bufSize - 1) {
            buf[len++] = (char) c;
            putchar(c);
        }
        buf[len] = '\0';
    }
}

// Reads a line, keeping `fallback` if the user just presses Enter. `fallback`
// may alias `buf` (e.g. "keep the current value") -- only copy when it doesn't.
void readLineWithDefault(char *buf, size_t bufSize, const char *fallback) {
    char entered[64] = {};
    readLine(entered, sizeof(entered));
    const char *src = entered[0] == '\0' ? fallback : entered;
    if (src != buf) {
        strncpy(buf, src, bufSize - 1);
        buf[bufSize - 1] = '\0';
    }
}

// Prompts for a dotted-decimal IP, re-prompting on invalid input. Enter
// alone keeps `fallback`.
void readIpWithDefault(const char *label, char *buf, size_t bufSize, const char *fallback) {
    uint8_t octets[4];
    while (true) {
        printf("%s [%s]: ", label, fallback);
        char entered[64] = {};
        readLine(entered, sizeof(entered));
        if (entered[0] == '\0') {
            strncpy(buf, fallback, bufSize - 1);
            buf[bufSize - 1] = '\0';
            return;
        }
        if (parseDottedIp(entered, octets)) {
            strncpy(buf, entered, bufSize - 1);
            buf[bufSize - 1] = '\0';
            return;
        }
        printf("Not a valid IPv4 address, try again.\r\n");
    }
}

void printCurrentConfig(const BridgeConfig &cfg) {
    printf("\r\nCurrent configuration:\r\n");
    printf("  Role:    %s\r\n", cfg.role == BridgeRole::Host ? "Host" : "Client");
    printf("  Name:    %s\r\n", cfg.name);
    if (cfg.useDhcp) {
        printf("  Network: DHCP / link-local\r\n");
    } else {
        printf("  Network: static %s / %s (gw %s, dns %s)\r\n",
               cfg.staticIp, cfg.staticNetmask, cfg.staticGateway, cfg.staticDns);
    }
    if (cfg.role == BridgeRole::Client && cfg.clientHostIp[0]) {
        printf("  Host:    %s:%u\r\n", cfg.clientHostIp, cfg.clientHostPort);
    }
}

} // namespace

void setConsolePump(void (*pump)()) {
    gPump = pump;
}

bool runConfigMenu(BridgeConfig &cfg) {
    printCurrentConfig(cfg);
    printf("\r\nPress any key within %u seconds to enter setup "
           "(or press ESC any time later while the bridge is running)...\r\n",
           kSetupPromptTimeoutMs / 1000);

    absolute_time_t deadline = make_timeout_time_ms(kSetupPromptTimeoutMs);
    while (absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
        pumpTick();
        if (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT) break;
    }
    // Drain anything queued (including the key that woke us) before we
    // start reading real menu input.
    while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT) {}
    if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) {
        printf("Continuing with the above configuration.\r\n");
        return false;
    }

    printf("\r\n--- NetworkMIDI2 Bridge setup ---\r\n");

    printf("Role -- [C]lient or [H]ost [%s]: ", cfg.role == BridgeRole::Host ? "H" : "C");
    char roleAns[8] = {};
    readLine(roleAns, sizeof(roleAns));
    if (roleAns[0] == 'H' || roleAns[0] == 'h') cfg.role = BridgeRole::Host;
    else if (roleAns[0] == 'C' || roleAns[0] == 'c') cfg.role = BridgeRole::Client;
    // else: keep whatever was loaded/default

    printf("Network MIDI name [%s]: ", cfg.name);
    readLineWithDefault(cfg.name, sizeof(cfg.name), cfg.name);

    printf("Network -- [D]HCP/link-local or [S]tatic IP [%s]: ", cfg.useDhcp ? "D" : "S");
    char netAns[8] = {};
    readLine(netAns, sizeof(netAns));
    if (netAns[0] == 'S' || netAns[0] == 's') {
        cfg.useDhcp = false;
        readIpWithDefault("  Static IP     ", cfg.staticIp, sizeof(cfg.staticIp), cfg.staticIp);
        readIpWithDefault("  Subnet mask   ", cfg.staticNetmask, sizeof(cfg.staticNetmask), cfg.staticNetmask);
        readIpWithDefault("  Gateway       ", cfg.staticGateway, sizeof(cfg.staticGateway), cfg.staticGateway);
        readIpWithDefault("  DNS server    ", cfg.staticDns, sizeof(cfg.staticDns), cfg.staticDns);
    } else if (netAns[0] == 'D' || netAns[0] == 'd') {
        cfg.useDhcp = true;
    }
    // else: keep whatever was loaded/default

    saveBridgeConfig(cfg);
    printf("Saved. (Client host selection follows once the network link is up.)\r\n");
    return true;
}

bool runClientHostSelect(BridgeConfig &cfg, IDiscovery &disc,
                          void (*pollNetwork)(), bool (*isNetworkReady)(),
                          bool interactive) {
    // browse() sends its mDNS query once, synchronously, with no retry --
    // calling it before the netif has a valid IPv4 address (DHCP still in
    // progress) burns the whole browse window on a query that goes out with
    // a bogus 0.0.0.0 source and gets silently dropped. Wait briefly for a
    // valid address first, still pumping the network meanwhile.
    absolute_time_t readyDeadline = make_timeout_time_ms(kNetifReadyTimeoutMs);
    while (!isNetworkReady() && absolute_time_diff_us(get_absolute_time(), readyDeadline) > 0) {
        pollNetwork();
        pumpTick();
    }

    printf("\r\nSearching for NetworkMIDI2 hosts (_midi2._udp) for %u seconds...\r\n",
           kHostBrowseMs / 1000);
    printf("(Enter at any time to stop early and pick from what's found so far,\r\n"
           " or 'm' + Enter to enter a host IP manually.)\r\n");

    disc.browse();

    DiscoveredPeer hosts[kMaxListedHosts];
    unsigned       hostCount = 0;
    bool           manual    = false;
    bool           sawInput  = interactive; // did anyone answer the console?

    absolute_time_t deadline = make_timeout_time_ms(kHostBrowseMs);
    while (absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
        // Pump the W5500 RX path and lwIP's timers -- main()'s own loop that
        // normally does this hasn't started yet at this point in boot, and
        // this app runs lwIP with NO_SYS=1, so without this nothing (DHCP,
        // mDNS responses, anything) would ever be processed during browse.
        pollNetwork();
        pumpTick();

        DiscoveredPeer peer;
        while (hostCount < kMaxListedHosts && disc.nextDiscovered(peer)) {
            hosts[hostCount++] = peer;
            uint32_t ip = peer.endpoint.ipv4;
            printf("  [%u] %s  %u.%u.%u.%u:%u\r\n", hostCount, peer.epName,
                   (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
                   peer.endpoint.port);
        }

        // Poll rather than waiting 50ms per iteration: the loop body has to
        // get back to pumpTick() promptly or USB enumeration stalls here.
        int c = getchar_timeout_us(0);
        if (c == '\r' || c == '\n') {
            sawInput = true;
            break;
        }
        if (c == 'm' || c == 'M') {
            sawInput = true;
            manual    = true;
            break;
        }
    }
    disc.stopBrowse();

    if (!manual && hostCount == 0) {
        // Nothing on the wire and nobody typing: we are on an unattended
        // boot (issue #19 -- a board plugged into a PC's USB port with no
        // serial console and, often, no Ethernet). Prompting here would
        // block in getchar() forever and main() would never get to run the
        // bridge at all. Give up instead; ESC re-enters setup later.
        if (!sawInput) {
            printf("No hosts found and no console input -- continuing without a host.\r\n"
                    "Press ESC once running to enter setup and choose one.\r\n");
            return false;
        }
        printf("No hosts found -- enter one manually.\r\n");
        manual = true;
    }

    if (manual) {
        readIpWithDefault("Host IP", cfg.clientHostIp, sizeof(cfg.clientHostIp),
                           cfg.clientHostIp[0] ? cfg.clientHostIp : "192.168.1.100");
        printf("Host port [%u]: ", cfg.clientHostPort);
        char portAns[8] = {};
        readLine(portAns, sizeof(portAns));
        if (portAns[0]) cfg.clientHostPort = (uint16_t) atoi(portAns);
    } else {
        unsigned choice = 0;
        if (!sawInput) {
            // Unattended boot that did find hosts. Nobody is there to pick
            // one, and prompting would block in readLine() forever -- which
            // is what left the board with no USB at all in issue #19. Take
            // the first host found; ESC re-enters setup to change it.
            choice = 1;
            printf("No console input -- defaulting to host [1] (%s).\r\n", hosts[0].epName);
        }
        while (choice < 1 || choice > hostCount) {
            printf("Select host [1-%u]: ", hostCount);
            char ans[8] = {};
            readLine(ans, sizeof(ans));
            choice = (unsigned) atoi(ans);
        }
        const DiscoveredPeer &picked = hosts[choice - 1];
        uint32_t ip = picked.endpoint.ipv4;
        snprintf(cfg.clientHostIp, sizeof(cfg.clientHostIp), "%u.%u.%u.%u",
                  (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
        cfg.clientHostPort = picked.endpoint.port;
    }

    saveBridgeConfig(cfg);
    printf("Will connect to %s:%u\r\n", cfg.clientHostIp, cfg.clientHostPort);
    return cfg.clientHostIp[0] != '\0';
}
