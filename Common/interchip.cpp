//
// Created by andrew on 27/05/22.
//
#include "hardware/spi.h"

#define PROTOZOA_INTERLINK_SPI spi0
#define PROTOZOA_SPI_RX_PIN 4
#define PROTOZOA_SPI_TX_PIN 7
#define PROTOZOA_SPI_CLK_PIN 6

#include "include/interchip.h"


void interchip::startup() {
    spi_init(PROTOZOA_INTERLINK_SPI, 1500000);

    // Set SPO = 9 and SPI = 1 for continuous data flow
    spi_set_format(PROTOZOA_INTERLINK_SPI, 8 /*num data bits*/, SPI_CPOL_0 /*CPOL*/, SPI_CPHA_1 /*CPHA*/, SPI_MSB_FIRST);

    spi_set_slave(PROTOZOA_INTERLINK_SPI, true);
    gpio_set_function(PROTOZOA_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PROTOZOA_SPI_CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PROTOZOA_SPI_TX_PIN, GPIO_FUNC_SPI);
    //gpio_set_function(PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI);
    // Make the SPI pins available to picotool
    //bi_decl(bi_4pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI));

}

void interchip::process() {
    while(spi_is_readable(PROTOZOA_INTERLINK_SPI)){
        spi_read_blocking(PROTOZOA_INTERLINK_SPI,  0, in_buf, 1);

        if(in_buf[0] >= 0x80){
            messType = in_buf[0] & 0xF0;
            switch(messType){
                case BUTTON_UP:
                    if(buttonup!= nullptr){
                        buttonup( in_buf[0] & 0xF);
                    }
                    break;
                case BUTTON_DOWN:
                    if(buttondown!= nullptr){
                        buttondown( in_buf[0] & 0xF);
                    }
                    break;
                case ENCODER:
                    if(encoder!= nullptr){
                        encoder( in_buf[0] & 0xF?1:-1);
                    }
                    break;
                case POTS:
                    analogMSB = -1;
                    potType = in_buf[0] & 0xF;
                    break;
            }
        }else if(messType == POTS){
            if(analogMSB==-1){
                analogMSB = in_buf[0];
            }else{
                if(analog!= nullptr) {
                    analog(potType, (analogMSB << 7) + in_buf[0]);
                }
                messType=0;
            }
        }
    }
}
