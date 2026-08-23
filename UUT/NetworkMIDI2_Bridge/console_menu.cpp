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
constexpr unsigned  kMaxListedHosts      = 8;

// Reads one line (echoed) into buf, up to bufSize-1 chars, terminated by
// CR or LF. Backspace/DEL deletes the previous character. Blocking.
void readLine(char *buf, size_t bufSize) {
    size_t len = 0;
    buf[0] = '\0';
    while (true) {
        int c = getchar();
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

bool runConfigMenu(BridgeConfig &cfg) {
    printCurrentConfig(cfg);
    printf("\r\nPress any key within %u seconds to enter setup "
           "(or press ESC any time later while the bridge is running)...\r\n",
           kSetupPromptTimeoutMs / 1000);

    absolute_time_t deadline = make_timeout_time_ms(kSetupPromptTimeoutMs);
    while (absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
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

void runClientHostSelect(BridgeConfig &cfg, IDiscovery &disc) {
    printf("\r\nSearching for NetworkMIDI2 hosts (_midi2._udp) for %u seconds...\r\n",
           kHostBrowseMs / 1000);
    printf("(Enter at any time to stop early and pick from what's found so far,\r\n"
           " or 'm' + Enter to enter a host IP manually.)\r\n");

    disc.browse();

    DiscoveredPeer hosts[kMaxListedHosts];
    unsigned       hostCount = 0;
    bool           manual    = false;

    absolute_time_t deadline = make_timeout_time_ms(kHostBrowseMs);
    while (absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
        DiscoveredPeer peer;
        while (hostCount < kMaxListedHosts && disc.nextDiscovered(peer)) {
            hosts[hostCount++] = peer;
            uint32_t ip = peer.endpoint.ipv4;
            printf("  [%u] %s  %u.%u.%u.%u:%u\r\n", hostCount, peer.epName,
                   (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
                   peer.endpoint.port);
        }

        int c = getchar_timeout_us(50 * 1000);
        if (c == '\r' || c == '\n') {
            break;
        }
        if (c == 'm' || c == 'M') {
            manual = true;
            break;
        }
    }
    disc.stopBrowse();

    if (!manual && hostCount == 0) {
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
}
