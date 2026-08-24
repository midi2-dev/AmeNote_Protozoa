#include "bridge_config.h"

#include <cstring>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/platform.h"

namespace {

// Same technique as lib/NetworkMIDI2/examples/midi_bridge/lwip/main.cpp's
// FlashConfig: a magic-tagged struct written to the last flash sector.
constexpr uint32_t kFlashMagic  = 0x324D4E42u; // "BNM2", bump on layout change
constexpr uint32_t kFlashOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

struct FlashConfig {
    uint32_t   magic;
    BridgeRole role;
    char       name[64];
    uint8_t    useDhcp;
    char       staticIp[16];
    char       staticNetmask[16];
    char       staticGateway[16];
    char       staticDns[16];
    char       clientHostIp[16];
    uint16_t   clientHostPort;
};
static_assert(sizeof(FlashConfig) <= FLASH_PAGE_SIZE,
              "FlashConfig must fit in one flash page");

const FlashConfig *flashCfg() {
    return reinterpret_cast<const FlashConfig *>(XIP_BASE + kFlashOffset);
}

void copyStr(char *dst, size_t dstSize, const char *src, size_t srcSize) {
    size_t n = dstSize < srcSize ? dstSize : srcSize;
    memcpy(dst, src, n);
    dst[dstSize - 1] = '\0';
}

} // namespace

void loadBridgeConfig(BridgeConfig &cfg) {
    const FlashConfig *saved = flashCfg();
    if (saved->magic != kFlashMagic) return; // keep BridgeConfig{} defaults

    cfg.role    = saved->role;
    cfg.useDhcp = saved->useDhcp != 0;
    cfg.clientHostPort = saved->clientHostPort;
    copyStr(cfg.name, sizeof(cfg.name), saved->name, sizeof(saved->name));
    copyStr(cfg.staticIp, sizeof(cfg.staticIp), saved->staticIp, sizeof(saved->staticIp));
    copyStr(cfg.staticNetmask, sizeof(cfg.staticNetmask), saved->staticNetmask, sizeof(saved->staticNetmask));
    copyStr(cfg.staticGateway, sizeof(cfg.staticGateway), saved->staticGateway, sizeof(saved->staticGateway));
    copyStr(cfg.staticDns, sizeof(cfg.staticDns), saved->staticDns, sizeof(saved->staticDns));
    copyStr(cfg.clientHostIp, sizeof(cfg.clientHostIp), saved->clientHostIp, sizeof(saved->clientHostIp));
}

void saveBridgeConfig(const BridgeConfig &cfg) {
    FlashConfig out{};
    out.magic  = kFlashMagic;
    out.role   = cfg.role;
    out.useDhcp = cfg.useDhcp ? 1 : 0;
    out.clientHostPort = cfg.clientHostPort;
    copyStr(out.name, sizeof(out.name), cfg.name, sizeof(cfg.name));
    copyStr(out.staticIp, sizeof(out.staticIp), cfg.staticIp, sizeof(cfg.staticIp));
    copyStr(out.staticNetmask, sizeof(out.staticNetmask), cfg.staticNetmask, sizeof(cfg.staticNetmask));
    copyStr(out.staticGateway, sizeof(out.staticGateway), cfg.staticGateway, sizeof(cfg.staticGateway));
    copyStr(out.staticDns, sizeof(out.staticDns), cfg.staticDns, sizeof(cfg.staticDns));
    copyStr(out.clientHostIp, sizeof(out.clientHostIp), cfg.clientHostIp, sizeof(cfg.clientHostIp));

    // flash_range_program requires a whole number of flash pages; pad the
    // write buffer out to FLASH_PAGE_SIZE regardless of sizeof(FlashConfig).
    alignas(4) uint8_t page[FLASH_PAGE_SIZE] = {};
    memcpy(page, &out, sizeof(out));

    // Flash write requires interrupts disabled; safe here since this app is
    // single-core and the write happens outside the tud_task()/tick() loop.
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(kFlashOffset, FLASH_SECTOR_SIZE);
    flash_range_program(kFlashOffset, page, sizeof(page));
    restore_interrupts(ints);
}

bool parseDottedIp(const char *s, uint8_t out[4]) {
    unsigned a, b, c, d;
    char extra;
    if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    out[0] = (uint8_t) a;
    out[1] = (uint8_t) b;
    out[2] = (uint8_t) c;
    out[3] = (uint8_t) d;
    return true;
}
