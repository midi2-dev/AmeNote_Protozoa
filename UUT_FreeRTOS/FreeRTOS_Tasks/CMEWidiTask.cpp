#include "CMEWidiTask.h"

#include "FreeRTOS.h"
#include "FreeRTOS_Tasks.h"
#include "task.h"

#include "hardware/uart.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"

#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

#include "dump_packet.h"

#include <midi/midi1_byte_stream.h>
#include <midi/data_message.h>

#include <stdio.h>

void setupCME(bool);

midi::midi1_byte_stream_parser BT2UMP(
    CMEWidiGroup,
    [](midi::universal_packet p) {
        TRACE_INCOMING_PACKET("CME Widi In", p);

        CMEWidiReceiveBuffer.write(p);
    }
);

UMPRingBuffer<256> CMEWidiReceiveBuffer;
UMPRingBuffer<256> CMEWidiSendBuffer;
static uint16_t sendReadPtr = 0;

extern "C" void pvrCMEWidiCore(void * /*pvParameters*/)
{
    midi::universal_packet p;
    uint8_t bs_buffer[8];

    setupCME(false); // Set to true to have Full Rate

    printf("CME Widi task initialized.\n");

    while (1)
    {
        taskYIELD();

        // Read Expansion Port
        while (uart_is_readable(uart1))
        {
            uint8_t ch = uart_getc(uart1);
            if (ch == 0xFE)
                continue; // skip ActiveSense

          #if DPROTOZOA_TRACE_INCOMING_TRAFFIC
            printf("CME Widi In byte %02x\n", unsigned(ch));
          #endif

            BT2UMP.feed(ch);
        }

        while (CMEWidiSendBuffer.read(sendReadPtr, p))
        {
            TRACE_OUTGOING_PACKET("CME Widi Out", p);

            auto bytes = midi::to_midi1_byte_stream(p, bs_buffer);
            uint8_t *s = bs_buffer;
            while (bytes--) uart_putc_raw(uart1, *s++);
        }
    }
}

#define CME_UART            uart1
#define CME_UART_TX         8 // GPIO8
#define CME_UART_RX         9 // GPIO9

#define MIDI1_BAUD_RATE 31250
#define CMEFULL_BAUD_RATE 400000
#define BT_BAUD_RATE CMEFULL_BAUD_RATE

//--- CME Defines do not change
#define CME_ON_PIN 10
#define CME_BUTTON_PIN 11
#define CME_STATUS_1_PIN 16
#define CME_STATUS_2_PIN 15
#define CME_HSDISABLE_PIN 2
#define CME_RESET_PIN 3

void setupCME(bool fullRate)
{
    // Setup CME IO Pins

    // Setup UART baud rate and also configure CME module for selected rate
    // must be done before CME module reset.
    gpio_init(CME_HSDISABLE_PIN);
    gpio_set_dir(CME_HSDISABLE_PIN, GPIO_OUT);
    if(fullRate)
    {
        uart_init(CME_UART, CMEFULL_BAUD_RATE);
        gpio_put(CME_HSDISABLE_PIN,false); // sets to normal speed of 400000
    }
    else
    {
        uart_init(CME_UART, MIDI1_BAUD_RATE);
        gpio_put(CME_HSDISABLE_PIN,true); // sets to normal speed of 31250
    }

    // Setup Pico pins for UART function
    gpio_set_function(CME_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(CME_UART_RX, GPIO_FUNC_UART);
   
    gpio_init(CME_ON_PIN);
    gpio_set_dir(CME_ON_PIN, GPIO_OUT);
    gpio_put(CME_ON_PIN, false);
   
    // Following used if want to force an operation status - we can use just default startup.
    // gpio_init(CME_BUTTON_PIN);
    // gpio_set_dir(CME_BUTTON_PIN, GPIO_OUT);
    //gpio_put(CME_BUTTON_PIN, true);
    
    gpio_init(CME_STATUS_1_PIN);
    gpio_set_dir(CME_STATUS_1_PIN, GPIO_IN);
    gpio_init(CME_STATUS_2_PIN);
    gpio_set_dir(CME_STATUS_2_PIN, GPIO_IN);

    gpio_init(CME_RESET_PIN);
    gpio_set_dir(CME_RESET_PIN, GPIO_OUT);
    gpio_put(CME_RESET_PIN, true); // to not want in reset

    // bring up the CME module
    // powerup
    gpio_put(CME_ON_PIN, true);
}
