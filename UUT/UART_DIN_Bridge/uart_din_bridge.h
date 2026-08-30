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
 * Portable DIN MIDI 1.0 <-> USB MIDI 2.0 (UMP) bridge, built on the RP2040's
 * real hardware UART peripheral (uart0/uart1) rather than a PIO bit-banger.
 * Nothing here is board-specific: the only per-target inputs are the UART
 * instance, its TX/RX GPIO pins, baud rate, which tud_ump interface index
 * this bridge drives, and which UMP group the DIN port maps to. That makes
 * one UartDinBridge instance safe to reuse verbatim across boards and, on a
 * single board, to instantiate more than once against different tud_ump
 * interface indices (see UUT/LoopbackDIN_Bridge).
 */

#ifndef _UART_DIN_BRIDGE_H_
#define _UART_DIN_BRIDGE_H_

#include <cstdint>
#include "hardware/uart.h"

#include "include/bytestreamToUMP.h"
#include "include/umpToBytestream.h"
#include "include/umpProcessor.h"

struct UartDinBridgeConfig {
    uart_inst_t* uart;      // uart0 or uart1
    uint8_t tx_pin;         // GPIO carrying this UART's TX function
    uint8_t rx_pin;         // GPIO carrying this UART's RX function
    uint32_t baud = 31250;  // DIN MIDI 1.0 standard baud rate
    uint8_t ump_itf = 0;    // which tud_ump_* interface index this bridge drives
    uint8_t group = 0;      // UMP group (0-based) the DIN port maps to
    char const* function_block_name = "DIN Bridge";
};

// One instance = one physical DIN MIDI port bridged onto one USB UMP
// interface. Board bring-up (pin choice, which UART, itf index) is supplied
// via UartDinBridgeConfig; everything else is generic.
class UartDinBridge {
public:
    void init(const UartDinBridgeConfig& cfg);

    // Call every main-loop iteration (alongside tud_task()). Pumps both
    // directions: USB UMP -> DIN bytes, and DIN bytes -> USB UMP.
    void service();

    // Diagnostic counters, readable via SWD/gdb (e.g. `print bridge.rxByteCount`)
    // on boards/pin pairs where UartDinBridge's own printf() logging isn't
    // usable (see UUT/UART_DIN_Bridge/README.md for why that's the case on
    // ProtoZOA's DIN header pins).
    volatile uint32_t rxByteCount = 0;
    volatile uint32_t rxUmpCount = 0;
    volatile uint8_t lastRxByte = 0;
    volatile uint32_t txUmpWordCount = 0;
    volatile uint32_t txByteCount = 0;

private:
    // Endpoint/Function Block Discovery responders, registered with
    // _proc so a UMP-native host recognizes this bridge as a live,
    // single-function-block MIDI 2.0 endpoint.
    void notifyEndpoint(uint8_t majVer, uint8_t minVer, uint8_t filter);
    void notifyFunctionBlock(uint8_t fbIdx, uint8_t filter);

    UartDinBridgeConfig _cfg;
    bytestreamToUMP _rx;   // DIN bytes -> UMP
    umpToBytestream _tx;   // UMP -> DIN bytes
    umpProcessor _proc;    // answers Endpoint/Function Block Discovery
};

#endif /* _UART_DIN_BRIDGE_H_ */
