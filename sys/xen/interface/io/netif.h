/******************************************************************************
 * netif.h
 * 
 * Unified network-device I/O interface for Xen guest OSes.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2003-2004, Keir Fraser
 */

#ifndef __XEN_PUBLIC_IO_NETIF_H__
#define __XEN_PUBLIC_IO_NETIF_H__

#include "ring.h"
#include "../grant_table.h"

/*
 * Notifications after enqueuing any type of message should be conditional on
 * the appropriate req_event or rsp_event field in the shared ring.
 * If the client sends notification for rx requests then it should specify
 * feature 'feature-rx-notify' via xenbus. Otherwise the backend will assume
 * that it cannot safely queue packets (as it may not be kicked to send them).
 */

/*
 * This is the 'wire' format for packets:
 *  Request 1: netif_tx_request -- NETTXF_* (any flags)
 * [Request 2: netif_tx_extra]  (only if request 1 has NETTXF_extra_info)
 * [Request 3: netif_tx_extra]  (only if request 2 has XEN_NETIF_EXTRA_MORE)
 *  Request 4: netif_tx_request -- NETTXF_more_data
 *  Request 5: netif_tx_request -- NETTXF_more_data
 *  ...
 *  Request N: netif_tx_request -- 0
 */

/* Protocol checksum field is blank in the packet (hardware offload)? */
#define _NETTXF_csum_blank     (0)
#define  NETTXF_csum_blank     (1U<<_NETTXF_csum_blank)

/* Packet data has been validated against protocol checksum. */
#define _NETTXF_data_validated (1)
#define  NETTXF_data_validated (1U<<_NETTXF_data_validated)

/* Packet continues in the next request descriptor. */
#define _NETTXF_more_data      (2)
#define  NETTXF_more_data      (1U<<_NETTXF_more_data)

/* Packet to be followed by extra descriptor(s). */
#define _NETTXF_extra_info     (3)
#define  NETTXF_extra_info     (1U<<_NETTXF_extra_info)

struct netif_tx_request {
    grant_ref_t gref;      /* Reference to buffer page */
    uint16_t offset;       /* Offset within buffer page */
    uint16_t flags;        /* NETTXF_* */
    uint16_t id;           /* Echoed in response message. */
    uint16_t size;         /* Packet size in bytes.       */
};
typedef struct netif_tx_request netif_tx_request_t;

/* Types of netif_extra_info descriptors. */
#define XEN_NETIF_EXTRA_TYPE_NONE  (0)  /* Never used - invalid */
#define XEN_NETIF_EXTRA_TYPE_GSO   (1)  /* u.gso */
#define XEN_NETIF_EXTRA_TYPE_MAX   (2)

/* netif_extra_info flags. */
#define _XEN_NETIF_EXTRA_FLAG_MORE (0)
#define XEN_NETIF_EXTRA_FLAG_MORE  (1U<<_XEN_NETIF_EXTRA_FLAG_MORE)

/* GSO types - only TCPv4 currently supported. */
#define XEN_NETIF_GSO_TYPE_TCPV4        (1)

/*
 * This structure needs to fit within both netif_tx_request and
 * netif_rx_response for compatibility.
 */
struct netif_extra_info {
    uint8_t type;  /* XEN_NETIF_EXTRA_TYPE_* */
    uint8_t flags; /* XEN_NETIF_EXTRA_FLAG_* */

    union {
        struct {
            /*
             * Maximum payload size of each segment. For example, for TCP this
             * is just the path MSS.
             */
            uint16_t size;

            /*
             * GSO type. This determines the protocol of the packet and any
             * extra features required to segment the packet properly.
             */
            uint8_t type; /* XEN_NETIF_GSO_TYPE_* */

            /* Future expansion. */
            uint8_t pad;

            /*
             * GSO features. This specifies any extra GSO features required
             * to process this packet, such as ECN support for TCPv4.
             */
            uint16_t features; /* XEN_NETIF_GSO_FEAT_* */
        } gso;

        uint16_t pad[3];
    } u;
};

struct netif_tx_response {
    uint16_t id;
    int16_t  status;       /* NETIF_RSP_* */
};
typedef struct netif_tx_response netif_tx_response_t;

struct netif_rx_request {
    uint16_t    id;        /* Echoed in response message.        */
    grant_ref_t gref;      /* Reference to incoming granted frame */
};
typedef struct netif_rx_request netif_rx_request_t;

/* Packet data has been validated against protocol checksum. */
#define _NETRXF_data_validated (0)
#define  NETRXF_data_validated (1U<<_NETRXF_data_validated)

/* Protocol checksum field is blank in the packet (hardware offload)? */
#define _NETRXF_csum_blank     (1)
#define  NETRXF_csum_blank     (1U<<_NETRXF_csum_blank)

/* Packet continues in the next request descriptor. */
#define _NETRXF_more_data      (2)
#define  NETRXF_more_data      (1U<<_NETRXF_more_data)

/* Packet to be followed by extra descriptor(s). */
#define _NETRXF_extra_info     (3)
#define  NETRXF_extra_info     (1U<<_NETRXF_extra_info)

struct netif_rx_response {
    uint16_t id;
    uint16_t offset;       /* Offset in page of start of received packet  */
    uint16_t flags;        /* NETRXF_* */
    int16_t  status;       /* -ve: BLKIF_RSP_* ; +ve: Rx'ed pkt size. */
};
typedef struct netif_rx_response netif_rx_response_t;

/*
 * Generate netif ring structures and types.
 */

DEFINE_RING_TYPES(netif_tx, struct netif_tx_request, struct netif_tx_response);
DEFINE_RING_TYPES(netif_rx, struct netif_rx_request, struct netif_rx_response);

#define NETIF_RSP_DROPPED         -2
#define NETIF_RSP_ERROR           -1
#define NETIF_RSP_OKAY             0
/* No response: used for auxiliary requests (e.g., netif_tx_extra). */
#define NETIF_RSP_NULL             1

#endif

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
