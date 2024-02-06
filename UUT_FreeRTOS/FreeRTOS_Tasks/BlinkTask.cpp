#include "BlinkTask.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"

/**
 * @brief pvrBlink Pico Board LED Regular Blink Task
 * Blinks on board LED of Pico at approximately 2 Hz.
 * 
 * @param pvParameters Not Used
 */
extern "C" void pvrBlink( void *pvParameters )
{
    while (1)
    {
        gpio_xor_mask( 1u << PICO_DEFAULT_LED_PIN );        // change LED state
        vTaskDelay( 1000 / portTICK_PERIOD_MS );            // every second
    }
}

