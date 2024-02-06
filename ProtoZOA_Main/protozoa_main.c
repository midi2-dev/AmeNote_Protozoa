/**
 * @brief ProtoZOA_Main Primary Loop
 * 
 * The ProtoZOA USB MIDI 2.0 Prototyping Tool, an open source project in conjunction of
 * members of the MIDI Association and AMEI to prototype and develop MIDI 2.0
 * concepts and features. The firmware is provided to members of the MIDI Association
 * and AMEI based on the indicated license. It is expected that members of the
 * licensed group will contibute to the firmware so we can cooperatively improve and
 * make consistent the implementations of MIDI 2.0.
 * 
 * NOTE: The following improvements are planned for this software (not in priority):
 * - implement a full data exchange protocol to UUT instead of simple to get
 *   more capability and bi-directional communications
 * - create a terminal mode for display to allow for more useful information
 *   to be displayed
 * - implement routines for cap touch IC to allow unique control of LED outputs
 * - implement MODE selection of cap touch
 * - divide functions into libraries to allow for more flexible development on Main Pico.
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

// GIO Defines for Main Pico
#define DISPLAY_UP 2
#define DISPLAY_CENTER 3
#define DISPLAY_A 15
#define DISPLAY_LEFT 16
#define DISPLAY_B 17
#define DISPLAY_DOWN 18
#define DISPLAY_RIGHT 20
#define POT1 27
#define POT2 26
#define RE_A_PIN 21
#define RE_B_PIN 22

#define SPI0_CLK 6
#define SPI0_TX 7
#define SPI0_RX 4

#define SPI1_CLK 10
#define SPI1_TX 11
#define SPI1_RX 12
#define SPI1_CS_hCAP_lDISP 9
#define DISPLAY_RESET 28
#define DISPLAY_DATA 8

#define DISPLAY_BACKLIGHT 13

#define CAP_TOUCH_INT_PIN 19

// SPI Bit Rates
#define LCD_BAUD 10000000
#define CAPS_BAUD 1500000

// Includes
#include "protozoa_main.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include <hardware/spi.h>
#include <hardware/gpio.h>
#include <hardware/adc.h>
#include <hardware/pwm.h>

#include "cdc_uart.h"

#include "lcd/GUI_Paint.h"
#include "lcd/LCD_1in14.h"

#include "amenotelogo.h"

// Globals
uint slice_num;
uint16_t *BlackImage;
uint8_t bState;
uint16_t currPos = 0;
uint32_t encoderMissCW = 0;
uint32_t encoderMissCCW = 0;

bool captouch_int = false;
uint16_t key1PressCount = 0;

char debugMsg[60];
uint16_t oldCapskeys = 0;
uint16_t oldPot1;
uint16_t oldPot2;

// Local declarations
void gpio_cb( uint gpio, uint32_t events );
void setupLCD();
void displayLogo();
void displayDataScreen();
bool captouch_reset( void );
uint8_t captouch_deviceID( void );
uint16_t captouch_getkeystatus();
uint8_t captouch_write_read( uint8_t writeChar );

/**
 * @brief hardware setup for ProtoZOA Main Pico function.
 * Hardware configuration for use by the Main Pico functions which handle
 * cap touch, display, pots and encoder as well as exchange information
 * to the UUT Pico.
 */
void ProtoZOA_Main_setup() {
    //spi0 for communication to UUT
    spi_init(spi0, 1500000 ); // 1.5 Mbit
    gpio_set_function(SPI0_CLK, GPIO_FUNC_SPI);
    gpio_set_function(SPI0_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI0_TX, GPIO_FUNC_SPI);
    spi_set_format(spi0, 8 /*num data bits*/, SPI_CPOL_0 /*CPOL*/, SPI_CPHA_1 /*CPHA*/, SPI_MSB_FIRST);

    // Encoder pins
    gpio_init(RE_A_PIN);gpio_set_dir(RE_A_PIN, GPIO_IN);gpio_pull_up(RE_A_PIN);
    gpio_init(RE_B_PIN);gpio_set_dir(RE_B_PIN, GPIO_IN);gpio_pull_up(RE_B_PIN);
    //gpio_set_input_hysteresis_enabled(RE_B_PIN, false);
    gpio_set_irq_enabled_with_callback(RE_A_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, gpio_cb);

    //Display Buttons
    gpio_init(DISPLAY_UP);gpio_set_dir(DISPLAY_UP, GPIO_IN);gpio_pull_up(DISPLAY_UP);
    gpio_set_irq_enabled(DISPLAY_UP, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_init(DISPLAY_CENTER);gpio_set_dir(DISPLAY_CENTER, GPIO_IN);gpio_pull_up(DISPLAY_CENTER);
    gpio_set_irq_enabled(DISPLAY_CENTER, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_init(DISPLAY_A);gpio_set_dir(DISPLAY_A, GPIO_IN);gpio_pull_up(DISPLAY_A);
    gpio_set_irq_enabled(DISPLAY_A, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_init(DISPLAY_B);gpio_set_dir(DISPLAY_B, GPIO_IN);gpio_pull_up(DISPLAY_B);
    gpio_set_irq_enabled(DISPLAY_B, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_init(DISPLAY_LEFT);gpio_set_dir(DISPLAY_LEFT, GPIO_IN);gpio_pull_up(DISPLAY_LEFT);
    gpio_set_irq_enabled(DISPLAY_LEFT, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_init(DISPLAY_DOWN);gpio_set_dir(DISPLAY_DOWN, GPIO_IN);gpio_pull_up(DISPLAY_DOWN);
    gpio_set_irq_enabled(DISPLAY_DOWN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_init(DISPLAY_RIGHT);gpio_set_dir(DISPLAY_RIGHT, GPIO_IN);gpio_pull_up(DISPLAY_RIGHT);
    gpio_set_irq_enabled(DISPLAY_RIGHT, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    // ADC Pins - Pots
    adc_init();
    adc_gpio_init(POT2);
    adc_gpio_init(POT1);

    //SPI1 : CAPS / Display
    spi_init(spi1, LCD_BAUD ); // was 10000 x 1000 which is 10 Mbit. Fastest cap touch can do is 1.5 Mbit
    gpio_set_function(SPI1_CLK, GPIO_FUNC_SPI);
    gpio_set_function(SPI1_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI1_TX, GPIO_FUNC_SPI);
    spi_set_format(spi1, 8 /*num data bits*/, SPI_CPOL_1 /*CPOL*/, SPI_CPHA_1 /*CPHA*/, SPI_MSB_FIRST);

    gpio_init(SPI1_CS_hCAP_lDISP);gpio_set_dir(SPI1_CS_hCAP_lDISP, GPIO_OUT);

    setupLCD();

    displayLogo();

    spi_set_baudrate(spi1, CAPS_BAUD);

    // Cap Touch interrupt pin
    gpio_init(CAP_TOUCH_INT_PIN);gpio_set_dir(CAP_TOUCH_INT_PIN, GPIO_IN);
    gpio_pull_up(CAP_TOUCH_INT_PIN);
    gpio_set_irq_enabled(CAP_TOUCH_INT_PIN, GPIO_IRQ_EDGE_FALL, true);

    gpio_put(SPI1_CS_hCAP_lDISP,1);
    uint8_t setup1[] = {
            0x90,
            (1 << 7) | (0 << 6) | (1 << 5) | (1 << 4) | 2
    };
    uint8_t setup2[] = {       0x91,
           (0<<2) | (0<<1) | (0<<0)
    };
   spi_write_blocking(spi1, setup1, 2);
   sleep_ms( 160 );
   spi_write_blocking(spi1, setup2, 2);
}

/**
 * @brief ProtoZOA Main Pico Task
 * Handles servicing of cap touch, pots and encoder exchanging events with the UUT Pico through
 * dedicated SPI.
 * 
 */
void ProtoZOA_MAIN_task(){

    adc_select_input(0);
    uint16_t result1 = adc_read();
    if(abs(result1-oldPot1) > 40){
        uint8_t change[] = {
                0xA1,
                result1 >> 7 & 0x7F,
                result1  & 0x7F
        };
        spi_write_blocking(spi0, change, 3);
        oldPot1 = result1;
    }


    adc_select_input(1);
    uint16_t result2 = adc_read() ;
    if(abs(result2-oldPot2) > 40){
        uint8_t change[] = {
                0xA0,
                result2 >> 7 & 0x7F,
                result2  & 0x7F
        };

        spi_write_blocking(spi0, change, 3);
        oldPot2 = result2;
    }


    if(captouch_int){
        captouch_int = false;
        uint16_t keys = captouch_getkeystatus();
        uint16_t diff = keys ^ oldCapskeys;
        if(diff){
            for(uint8_t i=0; i<7;i++){
               if(diff & (1<<i)){
                    uint8_t change[] = {
                            (keys & (1<<i)?0x90:0x80) + (6-i)
                    };
                    spi_write_blocking(spi0, change, 1);
                }
            }

        }
        oldCapskeys = keys;

        sleep_ms(100);

    }

    uint8_t in_buf[1];
    if(spi_is_readable(spi0)){
        spi_read_blocking(spi0,  0, in_buf, 1);
        printf(" spi0 M %02x \n",in_buf[0]);
    }

}

/**
 * @brief Interrupt service for GPI pins.
 * Handles interrupts for configured input pins.
 * 
 * @param gpio the gpio causing interrupt
 * @param events the event that occurred
 */
void gpio_cb( uint gpio, uint32_t events )
{
    // Handling of Encoder events
    if ( gpio == RE_A_PIN )
    {
        //sprintf( debugMsg," - Encoder detected!\n"  ); debug(debugMsg);
        if ( events & GPIO_IRQ_EDGE_FALL )
        {
            bState = gpio_get(RE_B_PIN);
        }
        if ( events & GPIO_IRQ_EDGE_RISE )
        {
            if ( bState != gpio_get(RE_B_PIN) )
            {
                // B State changed between falling and rising edge of A meaning proper
                // transition and rotation detection is valid
                if ( bState )
                {
                    // if was high then CW
                    currPos++;
                    //sprintf( debugMsg, "+++ Rotate CW : %d\n", currPos ); debug(debugMsg);
                    uint8_t ch[] = {0xB1};
                    spi_write_read_blocking(spi0, ch,ch , 1);

                }
                else
                {
                    // was low so CCW
                    currPos--;
                    //sprintf(  debugMsg,"--- Rotate CCW: %d\n", currPos ); debug(debugMsg);
                    uint8_t ch[] = {
                            0xB0
                    };
                    spi_write_read_blocking(spi0, ch,ch , 1);
                }
            }
            else
            {
                if ( bState )
                    encoderMissCW++;
                else
                    encoderMissCCW++;
            }
        }
    }

    // Handling of interrupt line from Cap Touch IC
    if ( gpio == CAP_TOUCH_INT_PIN )
    {
        if ( events & GPIO_IRQ_EDGE_FALL ){
            captouch_int = true;
        }
    }

    // Handling of pushbutton / joystick events on display module
    if( gpio == DISPLAY_UP){
        if ( events & GPIO_IRQ_EDGE_FALL ){
            uint8_t change[] = {0x97 };
            spi_write_blocking(spi0, change, 1);
        }
        if ( events & GPIO_IRQ_EDGE_RISE ) {
            uint8_t change[] = {0x87};
            spi_write_blocking(spi0, change, 1);
        }
    }
    if( gpio == DISPLAY_CENTER){
        if ( events & GPIO_IRQ_EDGE_FALL ){
            uint8_t change[] = {0x9B};
            spi_write_blocking(spi0, change, 1);
        }
        if ( events & GPIO_IRQ_EDGE_RISE ) {
            uint8_t change[] = {0x8B ,1};
            spi_write_blocking(spi0, change, 1);
        }
    }
    if( gpio == DISPLAY_A){
        if ( events & GPIO_IRQ_EDGE_FALL ){
            uint8_t change[] = {0x9C};
            spi_write_blocking(spi0, change, 1);
        }
        if ( events & GPIO_IRQ_EDGE_RISE ) {
            uint8_t change[] = {0x8C };
            spi_write_blocking(spi0, change, 1);
        }
    }
    if( gpio == DISPLAY_LEFT){
        if ( events & GPIO_IRQ_EDGE_FALL ){
            uint8_t change[] = {0x99 };
            spi_write_blocking(spi0, change, 1);
        }
        if ( events & GPIO_IRQ_EDGE_RISE ) {
            uint8_t change[] = {0x89 };
            spi_write_blocking(spi0, change, 1);
        }
    }
    if( gpio == DISPLAY_B){
        if ( events & GPIO_IRQ_EDGE_FALL ){
            uint8_t change[] = {0x9D };
            spi_write_blocking(spi0, change, 1);
        }
        if ( events & GPIO_IRQ_EDGE_RISE ) {
            uint8_t change[] = {0x8D };
            spi_write_blocking(spi0, change, 1);
        }
    }
    if( gpio == DISPLAY_DOWN){
        if ( events & GPIO_IRQ_EDGE_FALL ){
            uint8_t change[] = {0x98 };
            spi_write_blocking(spi0, change, 1);
        }
        if ( events & GPIO_IRQ_EDGE_RISE ) {
            uint8_t change[] = {0x88 };
            spi_write_blocking(spi0, change, 1);
        }
    }
    if( gpio == DISPLAY_RIGHT){
        if ( events & GPIO_IRQ_EDGE_FALL ){
            uint8_t change[] = {0x9A };
            spi_write_blocking(spi0, change, 1);
        }
        if ( events & GPIO_IRQ_EDGE_RISE ) {
            uint8_t change[] = {0x8A };
            spi_write_blocking(spi0, change, 1);
        }
    }

    // Clear the interrupt event
    gpio_acknowledge_irq(gpio, events);

}

/**
 * @brief setup of the LCD display module
 * Sets up the display module and associated GPI interrupts for buttons.
 * Note that if no display module installed, routines will still setup
 * the associated pins, but no calls to be serviced as there would be no
 * buttons to cause events. Also all SPI sent commands to Display module
 * would just be dropped - with no response expected. SPI only out to display,
 * no input from display expected.
 * 
 */
void setupLCD() {


    //DISPLAY INIT
    gpio_init(DISPLAY_RESET);gpio_set_dir(DISPLAY_RESET, GPIO_OUT);
    gpio_init(DISPLAY_DATA);gpio_set_dir(DISPLAY_DATA, GPIO_OUT);
    gpio_init(DISPLAY_BACKLIGHT);gpio_set_dir(DISPLAY_DATA, GPIO_OUT);

    gpio_put(SPI1_CS_hCAP_lDISP, 1);
    gpio_put(DISPLAY_DATA, 0);
    gpio_put(DISPLAY_BACKLIGHT, 1);

    //DISPLAY BACKLIGHT PWM Config
    gpio_set_function(DISPLAY_BACKLIGHT, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(DISPLAY_BACKLIGHT);
    pwm_set_wrap(slice_num, 100);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 1);
    pwm_set_clkdiv(slice_num,50);
    pwm_set_enabled(slice_num, true);

    pwm_set_chan_level(slice_num, PWM_CHAN_B, 50);

    LCD_1IN14_Init(HORIZONTAL);
    LCD_1IN14_Clear(WHITE);
    //LCD_SetBacklight(1023);
    uint32_t Imagesize = LCD_1IN14_HEIGHT * LCD_1IN14_WIDTH * 2;

    if ((BlackImage = (uint16_t *) malloc(Imagesize)) == NULL) {
        //sprintf(debugMsg,"Failed to apply for black memory...\r\n");debug(debugMsg);
        return;
    }
    // /*1.Create a new image cache named IMAGE_RGB and fill it with white*/
    Paint_NewImage((uint8_t *) BlackImage, LCD_1IN14.WIDTH, LCD_1IN14.HEIGHT, 0, WHITE);
    Paint_SetScale(65);
    Paint_Clear(WHITE);
    Paint_SetRotate(ROTATE_0);
    Paint_Clear(WHITE);

}

void displayLogo(){
    LCD_1IN14_Clear(WHITE);
    Paint_DrawImage(image_data_amenotelogo,0,37,240,61);
    LCD_1IN14_Display(BlackImage);

}

void displayDataScreen(){
    LCD_1IN14_Clear(BLACK);
    Paint_Clear(BLACK);
    Paint_DrawString_EN(1, 2, "UMP:", &Font20, WHITE, BLACK);
    Paint_DrawString_EN(1, 22, "DIN:", &Font20, BLUE, BLACK);
    Paint_DrawString_EN(1, 42, "EXP:", &Font20, YELLOW, BLACK);
    LCD_1IN14_Display(BlackImage);
}


uint8_t captouch_write_read( uint8_t writeChar )
{
    uint8_t readChar;

    // Set CS for captouch
    gpio_put(SPI1_CS_hCAP_lDISP,1);

    // Write / Read Byte
    spi_write_read_blocking(spi1, &writeChar, &readChar, 1);

    // Set CS for not captouch
    gpio_put(SPI1_CS_hCAP_lDISP,0);

    return readChar;
}

uint8_t captouch_write_read1( uint8_t writeChar )
{
    uint8_t readChar;

    // Set CS for captouch
    gpio_put(SPI1_CS_hCAP_lDISP,1);

    // Write / Read Byte
    spi_write_read_blocking(spi1, &writeChar, &readChar, 1);

    if ( readChar == 0x55 ) // if was idle, then commanded byte should follow
    {
        sleep_us(150);
        spi_read_blocking(spi1, 0x00, &readChar, 1);
    }

    // Set CS for not captouch
   gpio_put(SPI1_CS_hCAP_lDISP,0);

    return readChar;
}


uint8_t captouch_write_read2( uint8_t writeChar )
{
    uint8_t readChar;
    uint16_t returnWord = 0x00;

    // Set CS for captouch
    gpio_put(SPI1_CS_hCAP_lDISP,1);

    // Write / Read Byte
    spi_write_read_blocking(spi1, &writeChar, &readChar, 1);

    //sprintf( debugMsg, " : %d\n", readChar );debug(debugMsg);

    if ( readChar == 0x55 ) // if was idle, then commanded byte should follow
    {
        sleep_us(150);
        spi_read_blocking(spi1, 0x00, &readChar, 1);
        //sprintf( debugMsg, " : %d\n", readChar );debug(debugMsg);
        returnWord = (uint16_t)readChar << 8;
        sleep_us(150);
        spi_read_blocking(spi1, 0x00, &readChar, 1);
        //sprintf( debugMsg, " : %d\n", readChar );debug(debugMsg);
        returnWord |= (uint16_t)readChar;
    }

    // Set CS for not captouch
   gpio_put(SPI1_CS_hCAP_lDISP,0);

    return readChar;
}

bool captouch_reset( void )
{
    // Send reset command
    captouch_write_read( 0x04 );

    // Need to wait for 160 ms after reset for device to wake up again
    sleep_ms( 160 );

    return true;
}

uint8_t captouch_deviceID( void )
{
    // send device ID command
    uint8_t readByte = captouch_write_read1( 0xc9 );

    return readByte;
}

uint16_t captouch_getkeystatus()
{
    return captouch_write_read2(0xc1);
}