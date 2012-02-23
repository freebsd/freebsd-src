/******************************************************************************
 * tpmif.h
 *
 * TPM I/O interface for Xen guest OSes.
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
 * Copyright (c) 2005, IBM Corporation
 *
 * Author: Stefan Berger, stefanb@us.ibm.com
 * Grant table support: Mahadevan Gomathisankaran
 *
 * This code has been derived from tools/libxc/xen/io/netif.h
 *
 * Copyright (c) 2003-2004, Keir Fraser
 */

#ifndef __XEN_PUBLIC_IO_TPMIF_H__
#define __XEN_PUBLIC_IO_TPMIF_H__

#include "../grant_table.h"

struct tpmif_tx_request {
    unsigned long addr;   /* Machine address of packet.   */
    grant_ref_t ref;      /* grant table access reference */
    uint16_t unused;
    uint16_t size;        /* Packet size in bytes.        */
};
typedef struct tpmif_tx_request tpmif_tx_request_t;

/*
 * The TPMIF_TX_RING_SIZE defines the number of pages the
 * front-end and backend can exchange (= size of array).
 */
typedef uint32_t TPMIF_RING_IDX;

#define TPMIF_TX_RING_SIZE 1

/* This structure must fit in a memory page. */

struct tpmif_ring {
    struct tpmif_tx_request req;
};
typedef struct tpmif_ring tpmif_ring_t;

struct tpmif_tx_interface {
    struct tpmif_ring ring[TPMIF_TX_RING_SIZE];
};
typedef struct tpmif_tx_interface tpmif_tx_interface_t;

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
