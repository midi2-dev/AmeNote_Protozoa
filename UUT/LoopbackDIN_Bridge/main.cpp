/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Michael Loh (AmeNote.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Demonstrates two independent, simultaneous USB MIDI 2.0 (UMP) interfaces
 * on one device (CFG_TUD_UMP=2, see tusb_config.h and usb_descriptors.cpp):
 *
 *   - tud_ump itf 0: raw UMP loopback (echoes whatever it receives, same
 *     behavior as lib/tusb_ump/examples/tusb_ump_lb).
 *   - tud_ump itf 1: DIN MIDI 1.0 <-> UMP bridge over hardware UART0, using
 *     the exact same UartDinBridge module as UUT/UART_DIN_Bridge, just
 *     pointed at itf 1 instead of itf 0 -- proof the module is reusable
 *     across interfaces, not just across boards.
 *
 * Wiring: same ProtoZOA DIN header pins as UUT/DIN_Bridge and
 * UUT/UART_DIN_Bridge (GPIO12=TX, GPIO13=RX, UART0). NOTE: stdio-over-UART is
 * disabled for this target in CMakeLists.txt -- there is no live console --
 * because GP0/1 (the console UART0 pins) would otherwise contend with
 * GPIO12/13 for the same UART0 peripheral, confirmed on real hardware to
 * block UART0 RX from ever seeing GPIO13 traffic. See the longer explanation
 * in UUT/UART_DIN_Bridge/main.cpp.
 */

#include "pico/stdio.h"
#include "pico/time.h"
#include "hardware/uart.h"

#include "tusb.h"
#include "ump_device.h"

#include "uart_din_bridge.h"

#define MIDI1_BAUD_RATE 31250
#define LOOPBACK_ITF    0
#define DIN_BRIDGE_ITF  1

static UartDinBridge dinBridge;

// Diagnostic: log every alt-setting change on either interface, so it can be
// correlated against the SET_INTERFACE control transfer in a USB trace.
void tud_ump_set_itf_cb(uint8_t itf, uint8_t alt) {
    printf("[%llu] ALT_SET itf=%d alt=%d (mVersion=%d)\n", time_us_64(), itf, alt, alt + 1);
}

// Interface 0: raw UMP loopback, no Discovery responder -- same scope as
// lib/tusb_ump/examples/tusb_ump_lb.
static void loopback_service() {
    if (!tud_ump_n_mounted(LOOPBACK_ITF)) return;
    if (!tud_ump_n_available(LOOPBACK_ITF)) return;

    uint32_t words[4];
    uint16_t count = tud_ump_read_ntoh(LOOPBACK_ITF, words, 4);
    if (count == 0) return;

    printf("[%llu] LOOPBACK echo alt=%d count=%d\n", time_us_64(), tud_alt_setting(LOOPBACK_ITF), count);
    tud_ump_write_hton(LOOPBACK_ITF, words, count);
}

int main() {
    stdio_init_all();

    printf("Starting AmeNote ProtoZOA Loopback + DIN Bridge\n");

    UartDinBridgeConfig cfg;
    cfg.uart = uart0;
    cfg.tx_pin = 12;
    cfg.rx_pin = 13;
    cfg.baud = MIDI1_BAUD_RATE;
    cfg.ump_itf = DIN_BRIDGE_ITF;
    cfg.group = 0;
    dinBridge.init(cfg);

    tusb_init();

    while (true) {
        tud_task();
        loopback_service();
        dinBridge.service();
    }

    return 0;
}
