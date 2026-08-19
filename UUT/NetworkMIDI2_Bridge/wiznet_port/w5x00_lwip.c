/**
 * Copyright (c) 2022 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <string.h>

#include "w5x00_lwip.h"

#include "socket.h"

#include "netif/etharp.h"

#include "pico/time.h"

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
// Locally-administered address (0x02 prefix) so it never collides with a
// real vendor OUI. TODO: if more than one NetworkMIDI2_Bridge board will
// share a LAN, make the low 3 bytes unique per board (e.g. from
// pico_unique_id / flash serial), matching how UUT/DIN_Bridge already
// pulls a unique ID via pico_unique_id for USB.
uint8_t mac[6] = {0x02, 0x50, 0x5A, 0x4E, 0x4D, 0x32}; // "PZ" "NM2"

static uint8_t tx_frame[ETHERNET_FRAME_MAX_SIZE];

// Bound on how long send_lwip() will wait for the W5500 to accept/complete a
// send before giving up. The vendor's original code spun on these same
// conditions unbounded, which can hang this project's single-threaded
// NO_SYS main loop forever if the link drops mid-send (no RTOS/watchdog to
// recover it).
#define SEND_WAIT_TIMEOUT_MS 100

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
int32_t send_lwip(uint8_t sn, uint8_t *buf, uint16_t len)
{
    uint16_t freesize = 0;

    freesize = getSn_TxMAX(sn);
    if (len > freesize)
        len = freesize; // check size not to exceed MAX size.

    wiz_send_data(sn, buf, len);
    setSn_CR(sn, Sn_CR_SEND);

    absolute_time_t deadline = make_timeout_time_ms(SEND_WAIT_TIMEOUT_MS);
    while (getSn_CR(sn))
    {
        if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0)
        {
            return -1;
        }
    }

    while (1)
    {
        uint8_t IRtemp = getSn_IR(sn);
        if (IRtemp & Sn_IR_SENDOK)
        {
            setSn_IR(sn, Sn_IR_SENDOK);
            // printf("Packet sent ok\n");
            break;
        }
        else if (IRtemp & Sn_IR_TIMEOUT)
        {
            setSn_IR(sn, Sn_IR_TIMEOUT);
            // printf("Socket is closed\n");
            //  There was a timeout
            return -1;
        }
        else if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0)
        {
            // Neither SENDOK nor TIMEOUT ever came back -- don't spin
            // forever; report failure so the caller/netif can react.
            return -1;
        }
    }

    return (int32_t)len;
}

int32_t recv_lwip(uint8_t sn, uint8_t *buf, uint16_t len)
{
    uint8_t head[2];
    uint16_t pack_len = 0;

    pack_len = getSn_RX_RSR(sn);

    if (pack_len > 0)
    {
        wiz_recv_data(sn, head, 2);
        setSn_CR(sn, Sn_CR_RECV);

        // byte size of data packet (2byte)
        pack_len = head[0];
        pack_len = (pack_len << 8) + head[1];
        pack_len -= 2;

        if (pack_len > len)
        {
            // Packet is bigger than buffer - drop the packet
            wiz_recv_ignore(sn, pack_len);
            setSn_CR(sn, Sn_CR_RECV);
            return 0;
        }

        wiz_recv_data(sn, buf, pack_len); // data copy
        setSn_CR(sn, Sn_CR_RECV);
    }

    return (int32_t)pack_len;
}

err_t netif_output(struct netif *netif, struct pbuf *p)
{
    uint32_t tot_len = 0;

    memset(tx_frame, 0x00, sizeof(tx_frame));

    for (struct pbuf *q = p; q != NULL; q = q->next)
    {
        if (tot_len + q->len > sizeof(tx_frame))
        {
            // Would overflow tx_frame -- drop rather than corrupt memory.
            return ERR_BUF;
        }

        memcpy(tx_frame + tot_len, q->payload, q->len);

        tot_len += q->len;

        if (q->len == q->tot_len)
        {
            break;
        }
    }

    if (tot_len < 60)
    {
        // pad
        tot_len = 60;
    }

    // Note: no software FCS/CRC is computed here -- the W5500 appends its
    // own frame check sequence in hardware for MACRAW sends.
    int32_t send_len = send_lwip(0, tx_frame, tot_len);
    if (send_len < 0)
    {
        return ERR_IF;
    }

    return ERR_OK;
}

void netif_link_callback(struct netif *netif)
{
    printf("netif link status changed %s\n", netif_is_link_up(netif) ? "up" : "down");
}

void netif_status_callback(struct netif *netif)
{
    printf("netif status changed %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
}

err_t netif_initialize(struct netif *netif)
{
    netif->linkoutput = netif_output;
    netif->output = etharp_output;
    netif->mtu = ETHERNET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;
    SMEMCPY(netif->hwaddr, mac, sizeof(netif->hwaddr));
    netif->hwaddr_len = sizeof(netif->hwaddr);
    return ERR_OK;
}
