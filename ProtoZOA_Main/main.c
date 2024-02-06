/**
 * @brief ProtoZOA_Main Software Entry
 * 
 * The ProtoZOA USB MIDI 2.0 Prototyping Tool, an open source project in conjunction of
 * members of the MIDI Association and AMEI to prototype and develop MIDI 2.0
 * concepts and features. The firmware is provided to members of the MIDI Association
 * and AMEI based on the indicated license. It is expected that members of the
 * licensed group will contibute to the firmware so we can cooperatively improve and
 * make consistent the implementations of MIDI 2.0.
 * 
 * COPYRIGHT (c) 2022 AMENOTE INC.
 *
 * This Software is subject to the AmeNote ProtoZOA Firmware License terms and conditions
 * outlined in the project README and may or may not become public, open-source at
 * some point in the future as decided by AmeNote, the MIDI Association and AMEI.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NON- INFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Created by: Michael Loh and Andrew Mee for AmeNote Inc.
 * Date: June 17, 2022
*/

#include "FreeRTOS.h"
#include "task.h"
#include "protozoa_main_tasks.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "DAP.h"
#include "DAP_config.h"
#include "tusb_edpt_handler.h"

// Needed to account for update in tinyUSB
#if __has_include("bsp/board_api.h")
#include "bsp/board_api.h"
#else
#include "bsp/board.h"
#endif

#include "tusb.h"

#include "picoprobe_config.h"
#include "probe.h"
#include "cdc_uart.h"
#include "get_serial.h"
#include "led.h"
#include "protozoa_main.h"

extern TaskHandle_t uart_taskhandle;

static uint8_t TxDataBuffer[CFG_TUD_HID_EP_BUFSIZE];
#define THREADED 1

static void prvSetupHardware( void );

TaskHandle_t dap_taskhandle, tud_taskhandle;

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );

void usb_thread(void *ptr)
{
    TickType_t wake;
    wake = xTaskGetTickCount();
    do {
        tud_task();
#ifdef PICOPROBE_USB_CONNECTED_LED
        if (!gpio_get(PICOPROBE_USB_CONNECTED_LED) && tud_ready())
            gpio_put(PICOPROBE_USB_CONNECTED_LED, 1);
        else
            gpio_put(PICOPROBE_USB_CONNECTED_LED, 0);
#endif
        // Go to sleep for up to a tick if nothing to do
        if (!tud_task_event_ready())
            xTaskDelayUntil(&wake, 1);
    } while (1);
}

// Workaround API change in 0.13
#if (TUSB_VERSION_MAJOR == 0) && (TUSB_VERSION_MINOR <= 12)
#define tud_vendor_flush(x) ((void)0)
#endif

/**
 * @brief pvrProtoZOA_main entry to main task.
 * Task entry for main ProtoZOA UUT task.
 * 
 * @param pvParameters 
 */
static void pvrProtoZOA_main( void *pvParameters )
{
    while ( 1 )
    {
        ProtoZOA_MAIN_task();
        vTaskDelay( 1 );
    }
}

/**
 * @brief vLaunch main task creation and FreeRTOS start.
 * Creates all tasks for ProtoZOA UUT then launches the FreeRTOS scheduler.
 * 
 */
void vLaunch( void)
{
    printf(" Starting ProtoZOA Main.\n");
    /* UART needs to preempt USB as if we don't, characters get lost */
    xTaskCreate(cdc_thread, "UART", configMINIMAL_STACK_SIZE, NULL, UART_TASK_PRIO, &uart_taskhandle);
    xTaskCreate(usb_thread, "TUD", configMINIMAL_STACK_SIZE, NULL, TUD_TASK_PRIO, &tud_taskhandle);
    /* Lowest priority thread is debug - need to shuffle buffers before we can toggle swd... */
    xTaskCreate(dap_thread, "DAP", configMINIMAL_STACK_SIZE, NULL, DAP_TASK_PRIO, &dap_taskhandle);

    // Create task to handle main function
    xTaskCreate(    pvrProtoZOA_main,           /* Task function */
                    "MAIN",                      /* The text name assigned to task */
                    PROTOZOA_MAIN_STACK_SIZE,    /* The size of stack to allocate to the task */
                    NULL,                       /* The parameter passed to the task */
                    PROTOZOA_MAIN_TASK_PRIORITY, /* The priority assigned to the task */
                    NULL                        /* The task handle, NULL if not required */
                    );

    vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the Idle and/or
	timer tasks to be created.  See the memory management section on the
	FreeRTOS web site for more details on the FreeRTOS heap
	http://www.freertos.org/a00111.html. */
	for( ;; );
}

/**
 * @brief main entry point for ProtoZOA_Main Pico.
 * The main code entry for the ProtoZOA Main Pico board. Configures processor and
 * launches the second core for PicoProbe while using the first core for handling
 * of CapTouch, pots, encoder and display while communicating with the UUT Pico
 * through dedicated SPI link.
 * 
 * @return int status where 0 is exit without error.
 */
int main(void) {
    stdio_init_all();

    board_init();

    // Main Setup for ProtoZOA
    sleep_ms(100);

        /* Configure the hardware ready to run the demo. */
    prvSetupHardware();


    const char *rtos_name;
#if ( portSUPPORT_SMP == 1 )
    rtos_name = "FreeRTOS SMP";
#else
    rtos_name = "FreeRTOS";
#endif

#if ( portSUPPORT_SMP == 1 ) && ( configNUM_CORES == 2 )
    printf("%s on both cores:\n", rtos_name);
    vLaunch();
#endif

#if ( mainRUN_ON_CORE == 1 )
    printf("%s on core 1:\n", rtos_name);
    multicore_launch_core1(vLaunch);
    while (true);
#else
    printf("%s on core 0:\n", rtos_name);
    vLaunch();
#endif

    return 0;
}

static void prvSetupHardware( void )
{
    // Initialize stdio for board.
    stdio_init_all();

    // Configure for use of on board LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, 1);
    gpio_put(PICO_DEFAULT_LED_PIN, !PICO_DEFAULT_LED_PIN_INVERTED);

    tusb_init();
    usb_serial_init();
    cdc_uart_init();
    DAP_Setup();
    stdio_uart_init();

    ProtoZOA_Main_setup();
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* RxDataBuffer, uint16_t bufsize)
{
  uint32_t response_size = TU_MIN(CFG_TUD_HID_EP_BUFSIZE, bufsize);

  // This doesn't use multiple report and report ID
  (void) itf;
  (void) report_id;
  (void) report_type;

  DAP_ProcessCommand(RxDataBuffer, TxDataBuffer);

  tud_hid_report(0, TxDataBuffer, response_size);
}

#if (PICOPROBE_DEBUG_PROTOCOL == PROTO_DAP_V2)
extern uint8_t const desc_ms_os_20[];

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  switch (request->bmRequestType_bit.type)
  {
    case TUSB_REQ_TYPE_VENDOR:
      switch (request->bRequest)
      {
        case 1:
          if ( request->wIndex == 7 )
          {
            // Get Microsoft OS 2.0 compatible descriptor
            uint16_t total_len;
            memcpy(&total_len, desc_ms_os_20+8, 2);

            return tud_control_xfer(rhport, request, (void*) desc_ms_os_20, total_len);
          }else
          {
            return false;
          }

        default: break;
      }
    break;
    default: break;
  }

  // stall unknown request
  return false;
}
#endif

/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
    free memory available in the FreeRTOS heap.  pvPortMalloc() is called
    internally by FreeRTOS API functions that create tasks, queues, software
    timers, and semaphores.  The size of the FreeRTOS heap is set by the
    configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */

    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
    volatile size_t xFreeHeapSpace;

    /* This is just a trivial example of an idle hook.  It is called on each
    cycle of the idle task.  It must *NOT* attempt to block.  In this case the
    idle task just queries the amount of FreeRTOS heap that remains.  See the
    memory management section on the http://www.FreeRTOS.org web site for memory
    management options.  If there is a lot of heap memory free then the
    configTOTAL_HEAP_SIZE value in FreeRTOSConfig.h can be reduced to free up
    RAM. */
    xFreeHeapSpace = xPortGetFreeHeapSize();

    /* Remove compiler warning about xFreeHeapSpace being set but never used. */
    ( void ) xFreeHeapSpace;
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
    ;
}
