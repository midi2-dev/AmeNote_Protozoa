#include "PicoMainTask.h"

#include "include/interchip.h"
#include "hardware/adc.h"

#include "FreeRTOS.h"
#include "task.h"

#include <midi/channel_voice_message.h>

#include <stdio.h>

interchip mainPico;
UMPRingBuffer<32> ControlMessageBuffer;

static void generateRandomSeed();
static void buttonDown(uint8_t button);
static void buttonUp(uint8_t button);
static void encoder(int dir);
static void analog(uint8_t pot, uint16_t value);

extern "C" void pvrPicoMain(void *pvParameters)
{
    printf("Starting AmeNote ProtoZOA\n");

    generateRandomSeed();

    mainPico.startup();
    mainPico.setButtonDown(buttonDown);
    mainPico.setButtonUp(buttonUp);
    mainPico.setAnalog(analog);
    mainPico.setEncoder(encoder);

    while (true) {
        // read SPI from Main
        mainPico.process();

        taskYIELD();

        //--- End loop
    }
}

void generateRandomSeed()
{
    // setting up random Seed - by connecting to a floating ADC and reading values into srand
    // this is not a perfect RNG but good enough for MIDI2.0 and better than nothing
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);
    uint32_t seed=0;
    for(int i=0;i<32;i+=3){
        uint result = adc_read();
        seed += (result & 0xFFF) << i;
        sleep_ms(result);
    }

    srand(seed);
}

static constexpr auto vel = midi::velocity{ midi::uint7_t{ 100 } };

void buttonDown(uint8_t button) {
    printf("Got Button Down %d \n", button);

    switch (button)
    {
    case CAP1:
    case CAP2:
    case CAP3:
    case CAP4:
    case CAP5:
    case CAP6:
    case CAPRATIO:
        ControlMessageBuffer.write(midi::make_midi2_note_on_message(0, 0, 60+button, vel));
        break;
    }
}

void buttonUp(uint8_t button)
{
    printf("Got Button Up %d \n", button);
    switch (button)
    {
    case CAP1:
    case CAP2:
    case CAP3:
    case CAP4:
    case CAP5:
    case CAP6:
    case CAPRATIO:
        ControlMessageBuffer.write(midi::make_midi2_note_off_message(0, 0, 60+button, vel));
        break;
    }
}

void encoder(int dir)
{
    printf("encoder Dir %d \n", dir);
}

void analog(uint8_t pot, uint16_t value)
{
    printf("Pot %d 0x%03x\n", pot, value);

    const auto v = midi::controller_value{ midi::upsample_x_to_ybit(value, 12, 32) };
    ControlMessageBuffer.write(midi::make_midi2_control_change_message(0, 0, pot==POT1?7:11, v));
}
