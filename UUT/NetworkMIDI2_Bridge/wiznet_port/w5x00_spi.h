/**
 * Copyright (c) 2022 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _W5X00_SPI_H_
#define _W5X00_SPI_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */
/* SPI ---------------------------------------------------------------------
 * ProtoZOA UUT Pico wiring for a Wiznet W5x00 Ethernet expansion board on
 * the J13 expansion header. spi0 (GPIO4/6/7) is already committed to the
 * Main<->UUT high-speed link (see J10/J11/J12 in the schematic), so this
 * uses spi1 (RP2040 SPI1 alt-function pins for CS/SCK/MOSI/MISO).
 *
 * Confirmed (hardware-tested) RP2040 -> Wiznet mapping:
 *   GPIO9  -> SPI_CS_N
 *   GPIO10 -> SPI_SCK
 *   GPIO11 -> SPI_MOSI
 *   GPIO8  -> SPI_MISO (schematic net label read as GPIO12; GPIO8 is what
 *             actually works on real hardware -- see git history)
 *   GPIO3  -> W5500_RST_N
 *   GPIO15 -> W5500_INT_N (not currently used; driver is polling-based)
 *
 * NM2_WIZNET_BOARD_W5500_EVB_PICO (set via the NM2_WIZNET_BOARD CMake cache
 * variable): builds instead for WIZnet's own W5500-EVB-Pico eval board,
 * whose onboard W5500 is fixed-wired to spi0 on these pins (from WIZnet's
 * own RP2040-HAT-LWIP-C reference example). Used to isolate a software bug
 * from a ProtoZOA-specific wiring/hardware issue -- same driver code, a
 * known-good board.
 */
#if defined(NM2_WIZNET_BOARD_W5500_EVB_PICO)
#define SPI_PORT spi0

#define PIN_SCK 18
#define PIN_MOSI 19
#define PIN_MISO 16
#define PIN_CS 17
#define PIN_RST 20
#else
#define SPI_PORT spi1

#define PIN_SCK 10  /* SPI_SCK */
#define PIN_MOSI 11 /* SPI_MOSI */
#define PIN_MISO 8  /* SPI_MISO */
#define PIN_CS 9    /* SPI_CS_N */
#define PIN_RST 3   /* W5500_RST_N */
#endif

/* Use SPI DMA */
//#define USE_SPI_DMA // if you want to use SPI DMA, uncomment.

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* W5x00 */
/*! \brief Set CS pin
 *  \ingroup w5x00_spi
 *
 *  Set chip select pin of spi0 to low(Active low).
 *
 *  \param none
 */
static inline void wizchip_select(void);

/*! \brief Set CS pin
 *  \ingroup w5x00_spi
 *
 *  Set chip select pin of spi0 to high(Inactive high).
 *
 *  \param none
 */
static inline void wizchip_deselect(void);

/*! \brief Read from an SPI device, blocking
 *  \ingroup w5x00_spi
 *
 *  Set spi_read_blocking function.
 *  Read byte from SPI to rx_data buffer.
 *  Blocks until all data is transferred. No timeout, as SPI hardware always transfers at a known data rate.
 *
 *  \param none
 */
static uint8_t wizchip_read(void);

/*! \brief Write to an SPI device, blocking
 *  \ingroup w5x00_spi
 *
 *  Set spi_write_blocking function.
 *  Write byte from tx_data buffer to SPI device.
 *  Blocks until all data is transferred. No timeout, as SPI hardware always transfers at a known data rate.
 *
 *  \param tx_data Buffer of data to write
 */
static void wizchip_write(uint8_t tx_data);

#ifdef USE_SPI_DMA
/*! \brief Configure all DMA parameters and optionally start transfer
 *  \ingroup w5x00_spi
 *
 *  Configure all DMA parameters and read from DMA
 *
 *  \param pBuf Buffer of data to read
 *  \param len element count (each element is of size transfer_data_size)
 */
static void wizchip_read_burst(uint8_t *pBuf, uint16_t len);

/*! \brief Configure all DMA parameters and optionally start transfer
 *  \ingroup w5x00_spi
 *
 *  Configure all DMA parameters and write to DMA
 *
 *  \param pBuf Buffer of data to write
 *  \param len element count (each element is of size transfer_data_size)
 */
static void wizchip_write_burst(uint8_t *pBuf, uint16_t len);
#endif

/*! \brief Enter a critical section
 *  \ingroup w5x00_spi
 *
 *  Set ciritical section enter blocking function.
 *  If the spin lock associated with this critical section is in use, then this
 *  method will block until it is released.
 *
 *  \param none
 */
static void wizchip_critical_section_lock(void);

/*! \brief Release a critical section
 *  \ingroup w5x00_spi
 *
 *  Set ciritical section exit function.
 *  Release a critical section.
 *
 *  \param none
 */
static void wizchip_critical_section_unlock(void);

/*! \brief Initialize SPI instances and Set DMA channel
 *  \ingroup w5x00_spi
 *
 *  Set GPIO to spi0.
 *  Puts the SPI into a known state, and enable it.
 *  Set DMA channel completion channel.
 *
 *  \param none
 */
void wizchip_spi_initialize(void);

/*! \brief Initialize a critical section structure
 *  \ingroup w5x00_spi
 *
 *  The critical section is initialized ready for use.
 *  Registers callback function for critical section for WIZchip.
 *
 *  \param none
 */
void wizchip_cris_initialize(void);

/*! \brief W5x00 chip reset
 *  \ingroup w5x00_spi
 *
 *  Set a reset pin and reset.
 *
 *  \param none
 */
void wizchip_reset(void);

/*! \brief Initialize WIZchip
 *  \ingroup w5x00_spi
 *
 *  Set callback function to read/write byte using SPI.
 *  Set callback function for WIZchip select/deselect.
 *  Set memory size of W5x00 chip and monitor PHY link status.
 *
 *  \param none
 */
void wizchip_initialize(void);

/*! \brief Check chip version
 *  \ingroup w5x00_spi
 *
 *  Get version information.
 *
 *  \param none
 */
void wizchip_check(void);

/*! \brief Check chip version, without hanging on failure
 *  \ingroup w5x00_spi
 *
 *  Same version check as wizchip_check(), but returns false instead of
 *  parking in an infinite loop when the chip does not answer. Callers that
 *  have other work to keep doing -- USB, in this app's case -- must use
 *  this one; see the comment in the implementation and issue #19.
 *
 *  \return true if the W5x00 reported its expected version register.
 */
bool wizchip_check_ok(void);

/* Network */
/*! \brief Initialize network
 *  \ingroup w5x00_spi
 *
 *  Set network information.
 *
 *  \param net_info network information.
 */
void network_initialize(wiz_NetInfo net_info);

/*! \brief Print network information
 *  \ingroup w5x00_spi
 *
 *  Print network information about MAC address, IP address, Subnet mask, Gateway, DHCP and DNS address.
 *
 *  \param net_info network information.
 */
void print_network_information(wiz_NetInfo net_info);

#ifdef __cplusplus
}
#endif

#endif /* _W5X00_SPI_H_ */
