/**
 * Copyright (c) 2022 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _W5x00_LWIP_H_
#define _W5x00_LWIP_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */
#include "lwip/netif.h"

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */
/* LWIP */
#define ETHERNET_MTU 1500

// Buffer size for a full raw Ethernet frame (14-byte header + up to
// ETHERNET_MTU bytes of payload), with headroom -- NOT the same thing as
// ETHERNET_MTU (netif->mtu, an IP-payload-only figure). Any buffer that
// receives/holds a whole frame off the wire (e.g. tx_frame in
// w5x00_lwip.c, or a caller's RX buffer passed to recv_lwip()) must be
// sized to this, not to ETHERNET_MTU alone.
#define ETHERNET_FRAME_MAX_SIZE 1542

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/*! \brief send an ethernet packet
 *  \ingroup w5x00_lwip
 *
 *  It is used to send outgoing data to the socket.
 *
 *  \param sn socket number
 *  \param buf a pointer to the data to send
 *  \param len the length of data in packet
 *  \return he sent data size
 */
int32_t send_lwip(uint8_t sn, uint8_t *buf, uint16_t len);

/*! \brief read an ethernet packet
 *  \ingroup w5x00_lwip
 *
 *  It is used to read incoming data from the socket.
 *
 *  \param sn socket number
 *  \param buf a pointer buffer to read incoming data
 *  \param len the length of the data in the packet
 *  \return the real received data size
 */
int32_t recv_lwip(uint8_t sn, uint8_t *buf, uint16_t len);

/*! \brief callback function
 *  \ingroup w5x00_lwip
 *
 *  This function is called by ethernet_output() when it wants
 *  to send a packet on the interface. This function outputs
 *  the pbuf as-is on the link medium.
 *
 *  \param netif a pre-allocated netif structure
 *  \param p main packet buffer struct
 *  \return ERR_OK if data was sent.
 */
err_t netif_output(struct netif *netif, struct pbuf *p);

/*! \brief callback function
 *  \ingroup w5x00_lwip
 *
 *  Callback function for link.
 *
 *  \param netif a pre-allocated netif structure
 */
void netif_link_callback(struct netif *netif);

/*! \brief callback function
 *  \ingroup w5x00_lwip
 *
 *   Callback function for status.
 *
 *   \param netif a pre-allocated netif structure
 */
void netif_status_callback(struct netif *netif);

/*! \brief callback function
 *  \ingroup w5x00_lwip
 *
 *  Callback function that initializes the interface.
 *
 *  \param netif a pre-allocated netif structure
 *  \return ERR_OK if Network interface initialized
 */
err_t netif_initialize(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif /* _W5x00_LWIP_H_ */
