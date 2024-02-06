#include "DINSerialTask.h"

#include "FreeRTOS.h"
#include "FreeRTOS_Tasks.h"
#include "task.h"

#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

#include "dump_packet.h"

#include <midi/midi1_byte_stream.h>
#include <midi/data_message.h>

#include <stdio.h>

static void pio_rx_init(PIO piorx, uint smrx)
{
    // Set up the state machine we're going to use to receive them.
    uint offset = pio_add_program(piorx, &uart_rx_program);
    uart_rx_program_init(piorx, smrx, offset, 13, 31250);
}

static void pio_tx_init(PIO piotx, uint smtx)
{
    uint offset = pio_add_program(piotx, &uart_tx_program);
    uart_tx_program_init(piotx, smtx, offset, 12, 31250);
}

PIO pio = pio0;
uint smRx = 0;
uint smTx = 1;

midi::midi1_byte_stream_parser DIN2UMP(
    DINPortInGroup,
    [](midi::universal_packet p) {
        TRACE_INCOMING_PACKET("DIN Serial In", p);
        DINPortReceiveBuffer.write(p);
    }
);

UMPRingBuffer<256> DINPortReceiveBuffer;
UMPRingBuffer<256> DINPortSendBuffer;
static uint16_t sendReadPtr = 0; 

extern "C" void pvrDINSerial(void * /*pvParameters*/)
{
    midi::universal_packet p;
    uint8_t bs_buffer[8];

    //---------- Setup MIDI Din Ports
    // Setup pio for receive
    pio_rx_init(pio, smRx);
    // Setup pio for transmit
    pio_tx_init(pio, smTx);

    printf("5-pin DIN task initialized.\n");

    while (1)
    {
        taskYIELD();

        // Read from DIN Port
        while ( !pio_sm_is_rx_fifo_empty(pio, smRx) )
        {
            // Get a character from the buffer
            uint8_t ch = uart_rx_program_getc(pio, smRx);
            if(ch == 0xFE)
                continue; // Skip ActiveSense
            
            DIN2UMP.feed(ch);
        }

        while (DINPortSendBuffer.read(sendReadPtr, p))
        {
            TRACE_OUTGOING_PACKET("DIN Serial Out", p);
                        
            auto bytes = midi::to_midi1_byte_stream(p, bs_buffer);
            uint8_t *s = bs_buffer;
            while (bytes--) uart_tx_program_putc(pio, smTx, *s++);
        }
    }
}
