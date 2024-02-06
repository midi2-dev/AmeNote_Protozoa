#include "Type25SerialTask.h"

#include "FreeRTOS.h"
#include "task.h"

#include "hardware/uart.h"
#include "hardware/gpio.h"

#include "UMPProcessing.h"
#include "SerialBracketing.h"
#include "dump_packet.h"

#include <stdio.h>

static SerialBracketing bracketing;
static void sendPacket(const midi::universal_packet &p)
{
    TRACE_OUTGOING_PACKET("Type25 out", p);

    uint8_t buffer[p.size()*4 + 2];
    uint8_t length = bracketing.encode(p, buffer);
    uart_write_blocking(uart1, buffer, length);
}

static UMPProcessing type25Serial("ProtoZOA Type25", sendPacket);

extern "C" void pvrType25Serial(void * /*pvParameters*/)
{
    //--------- Set up Expansion Port
    uart_init(uart1, 115200);
    gpio_set_function(8, GPIO_FUNC_UART);
    gpio_set_function(9, GPIO_FUNC_UART);

    while (uart_is_readable(uart1)) auto c = uart_getc(uart1);

    printf("Type25 Serial task initialized.\n");

    while (1)
    {
        taskYIELD();
        
        type25Serial.sendPendingUMPs();
        
        // Read From Serial
        char uBuf[1];
        while (uart_is_readable(uart1))
        {
            uBuf[0] = uart_getc(uart1);

            switch (bracketing.feed(uBuf[0]))
            {
            case SerialBracketing::SUCCESS:
                TRACE_INCOMING_PACKET("Type25 in", bracketing.ump);
                type25Serial.process(bracketing.ump);
                break;
            case SerialBracketing::CRC_ERROR:
                dump_packet("Type25 CRC error: ", bracketing.ump);
              #if PROTOZOA_SERIAL_BRACKET16
                printf("  expected 0x%02x, is 0x%02x\n", unsigned(uBuf[0]), unsigned(bracketing.checksum()));
              #endif
                break;
            case SerialBracketing::INVALID_UMP:
                dump_packet("Type25 Invalid UMP: ", bracketing.ump);
                break;
            default:
                break;
            }
        }
    }
}
