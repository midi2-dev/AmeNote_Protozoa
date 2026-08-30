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
 * DIN MIDI 1.0 <-> USB MIDI 2.0 (UMP) bridge over the RP2040's hardware UART0
 * peripheral, wired to ProtoZOA's DIN header pins (GPIO12=TX, GPIO13=RX) --
 * the same physical pins UUT/DIN_Bridge drives via a PIO bit-banger.
 *
 * NOTE: GPIO12/13 only support hardware UART0 on the RP2040, which is also
 * ProtoZOA's console/stdio UART (GPIO0/1, bridged out via PicoProbe) -- both
 * pin pairs share the same UART0 peripheral. stdio_init_all()'s default
 * console claims GP0/1 as UART0 TX/RX; with that claim in place, confirmed
 * by testing against a real ProtoZOA board with a physical DIN loopback
 * partner, UART0's RX routing never sees anything arrive on GPIO13 at all
 * (not merely corrupted -- zero bytes, independent of baud or pull-ups).
 * CMakeLists.txt therefore disables stdio-over-UART (and USB) for this
 * target entirely, so GPIO12/13 has sole ownership of UART0 -- there is no
 * live console for this example. This is also why UUT/DIN_Bridge uses a PIO
 * bit-banger instead of the UART0 hardware block on these same pins: PIO is
 * a separate peripheral that doesn't contend with UART0 at all, so it can
 * coexist with the console. UartDinBridge's own printf() diagnostics (see
 * uart_din_bridge.cpp) are therefore no-ops here, but remain useful on
 * boards/pin pairs where the DIN UART doesn't collide with console/stdio.
 *
 * Only this file's board bring-up (which GPIOs/UART instance, stdio_init_all())
 * is ProtoZOA-specific. All the bridging logic lives in uart_din_bridge.h/.cpp,
 * which takes its UART instance, pins, baud rate and tud_ump interface index
 * as plain configuration -- carry those two files over unchanged to bridge DIN
 * MIDI on any other RP2040 target's UART.
 */

#include "pico/stdio.h"
#include "pico/time.h"
#include "hardware/uart.h"

#include "tusb.h"
#include "ump_device.h"

#include "uart_din_bridge.h"

#define MIDI1_BAUD_RATE 31250

static UartDinBridge dinBridge;

// Diagnostic: log every alt-setting change so it can be correlated against
// the SET_INTERFACE control transfer in a USB trace (e.g. Beagle).
void tud_ump_set_itf_cb(uint8_t itf, uint8_t alt) {
    printf("[%llu] ALT_SET itf=%d alt=%d (mVersion=%d)\n", time_us_64(), itf, alt, alt + 1);
}

int main() {
    stdio_init_all();

    printf("Starting AmeNote ProtoZOA UART DIN Bridge\n");

    UartDinBridgeConfig cfg;
    cfg.uart = uart0;
    cfg.tx_pin = 12;
    cfg.rx_pin = 13;
    cfg.baud = MIDI1_BAUD_RATE;
    cfg.ump_itf = 0;
    cfg.group = 0;
    dinBridge.init(cfg);

    tusb_init();

    while (true) {
        tud_task();
        dinBridge.service();
    }

    return 0;
}
