/******************************************************************************
 * Argo : Hypervisor-Mediated data eXchange
 *
 * Derived from v4v, the version 2 of v2v.
 *
 * Copyright (c) 2010, Citrix Systems
 * Copyright (c) 2018-2019, BAE Systems
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
 */

#ifndef __XEN_PUBLIC_ARGO_H__
#define __XEN_PUBLIC_ARGO_H__

#include "xen.h"

#define XEN_ARGO_DOMID_ANY       DOMID_INVALID

/* The maximum size of an Argo ring is defined to be: 16MB (0x1000000 bytes). */
#define XEN_ARGO_MAX_RING_SIZE  (0x1000000ULL)

/* Fixed-width type for "argo port" number. Nothing to do with evtchns. */
typedef uint32_t xen_argo_port_t;

/* gfn type: 64-bit fixed-width on all architectures */
typedef uint64_t xen_argo_gfn_t;

/*
 * XEN_ARGO_MAXIOV : maximum number of iovs accepted in a single sendv.
 * Caution is required if this value is increased: this determines the size of
 * an array of xen_argo_iov_t structs on the hypervisor stack, so could cause
 * stack overflow if the value is too large.
 * The Linux Argo driver never passes more than two iovs.
*/
#define XEN_ARGO_MAXIOV          8U

typedef struct xen_argo_iov
{
    XEN_GUEST_HANDLE(uint8) iov_hnd;
    uint32_t iov_len;
    uint32_t pad;
} xen_argo_iov_t;

typedef struct xen_argo_addr
{
    xen_argo_port_t aport;
    domid_t domain_id;
    uint16_t pad;
} xen_argo_addr_t;

typedef struct xen_argo_send_addr
{
    struct xen_argo_addr src;
    struct xen_argo_addr dst;
} xen_argo_send_addr_t;

typedef struct xen_argo_ring
{
    /* Guests should use atomic operations to access rx_ptr */
    uint32_t rx_ptr;
    /* Guests should use atomic operations to access tx_ptr */
    uint32_t tx_ptr;
    /*
     * Header space reserved for later use. Align the start of the ring to a
     * multiple of the message slot size.
     */
    uint8_t reserved[56];
    uint8_t ring[XEN_FLEX_ARRAY_DIM];
} xen_argo_ring_t;

typedef struct xen_argo_register_ring
{
    xen_argo_port_t aport;
    domid_t partner_id;
    uint16_t pad;
    uint32_t len;
} xen_argo_register_ring_t;

typedef struct xen_argo_unregister_ring
{
    xen_argo_port_t aport;
    domid_t partner_id;
    uint16_t pad;
} xen_argo_unregister_ring_t;

/* Messages on the ring are padded to a multiple of this size. */
#define XEN_ARGO_MSG_SLOT_SIZE 0x10

/*
 * Notify flags
 */
/* Ring exists */
#define XEN_ARGO_RING_EXISTS            (1U << 0)
/* Ring is shared, not unicast */
#define XEN_ARGO_RING_SHARED            (1U << 1)
/* Ring is empty */
#define XEN_ARGO_RING_EMPTY             (1U << 2)
/* Sufficient space to queue space_required bytes might exist */
#define XEN_ARGO_RING_SUFFICIENT        (1U << 3)
/* Insufficient ring size for space_required bytes */
#define XEN_ARGO_RING_EMSGSIZE          (1U << 4)
/* Too many domains waiting for available space signals for this ring */
#define XEN_ARGO_RING_EBUSY             (1U << 5)

typedef struct xen_argo_ring_data_ent
{
    struct xen_argo_addr ring;
    uint16_t flags;
    uint16_t pad;
    uint32_t space_required;
    uint32_t max_message_size;
} xen_argo_ring_data_ent_t;

typedef struct xen_argo_ring_data
{
    uint32_t nent;
    uint32_t pad;
    struct xen_argo_ring_data_ent data[XEN_FLEX_ARRAY_DIM];
} xen_argo_ring_data_t;

struct xen_argo_ring_message_header
{
    uint32_t len;
    struct xen_argo_addr source;
    uint32_t message_type;
    uint8_t data[XEN_FLEX_ARRAY_DIM];
};

/*
 * Hypercall operations
 */

/*
 * XEN_ARGO_OP_register_ring
 *
 * Register a ring using the guest-supplied memory pages.
 * Also used to reregister an existing ring (eg. after resume from hibernate).
 *
 * The first argument struct indicates the port number for the ring to register
 * and the partner domain, if any, that is to be allowed to send to the ring.
 * A wildcard (XEN_ARGO_DOMID_ANY) may be supplied instead of a partner domid,
 * and if the hypervisor has wildcard sender rings enabled, this will allow
 * any domain (XSM notwithstanding) to send to the ring.
 *
 * The second argument is an array of guest frame numbers and the third argument
 * indicates the size of the array. This operation only supports 4K-sized pages.
 *
 * arg1: XEN_GUEST_HANDLE(xen_argo_register_ring_t)
 * arg2: XEN_GUEST_HANDLE(xen_argo_gfn_t)
 * arg3: unsigned long npages
 * arg4: unsigned long flags (32-bit value)
 */
#define XEN_ARGO_OP_register_ring     1

/* Register op flags */
/*
 * Fail exist:
 * If set, reject attempts to (re)register an existing established ring.
 * If clear, reregistration occurs if the ring exists, with the new ring
 * taking the place of the old, preserving tx_ptr if it remains valid.
 */
#define XEN_ARGO_REGISTER_FLAG_FAIL_EXIST  0x1

#ifdef __XEN__
/* Mask for all defined flags. */
#define XEN_ARGO_REGISTER_FLAG_MASK XEN_ARGO_REGISTER_FLAG_FAIL_EXIST
#endif

/*
 * XEN_ARGO_OP_unregister_ring
 *
 * Unregister a previously-registered ring, ending communication.
 *
 * arg1: XEN_GUEST_HANDLE(xen_argo_unregister_ring_t)
 * arg2: NULL
 * arg3: 0 (ZERO)
 * arg4: 0 (ZERO)
 */
#define XEN_ARGO_OP_unregister_ring     2

/*
 * XEN_ARGO_OP_sendv
 *
 * Send a list of buffers contained in iovs.
 *
 * The send address struct specifies the source and destination addresses
 * for the message being sent, which are used to find the destination ring:
 * Xen first looks for a most-specific match with a registered ring with
 *  (id.addr == dst) and (id.partner == sending_domain) ;
 * if that fails, it then looks for a wildcard match (aka multicast receiver)
 * where (id.addr == dst) and (id.partner == DOMID_ANY).
 *
 * For each iov entry, send iov_len bytes from iov_base to the destination ring.
 * If insufficient space exists in the destination ring, it will return -EAGAIN
 * and Xen will notify the caller when sufficient space becomes available.
 *
 * The message type is a 32-bit data field available to communicate message
 * context data (eg. kernel-to-kernel, rather than application layer).
 *
 * arg1: XEN_GUEST_HANDLE(xen_argo_send_addr_t) source and dest addresses
 * arg2: XEN_GUEST_HANDLE(xen_argo_iov_t) iovs
 * arg3: unsigned long niov
 * arg4: unsigned long message type (32-bit value)
 */
#define XEN_ARGO_OP_sendv               3

/*
 * XEN_ARGO_OP_notify
 *
 * Asks Xen for information about other rings in the system.
 *
 * ent->ring is the xen_argo_addr_t of the ring you want information on.
 * Uses the same ring matching rules as XEN_ARGO_OP_sendv.
 *
 * ent->space_required : if this field is not null then Xen will check
 * that there is space in the destination ring for this many bytes of payload.
 * If the ring is too small for the requested space_required, it will set the
 * XEN_ARGO_RING_EMSGSIZE flag on return.
 * If sufficient space is available, it will set XEN_ARGO_RING_SUFFICIENT
 * and CANCEL any pending notification for that ent->ring; otherwise it
 * will schedule a notification event and the flag will not be set.
 *
 * These flags are set by Xen when notify replies:
 * XEN_ARGO_RING_EXISTS     ring exists
 * XEN_ARGO_RING_SHARED     ring is registered for wildcard partner
 * XEN_ARGO_RING_EMPTY      ring is empty
 * XEN_ARGO_RING_SUFFICIENT sufficient space for space_required is there
 * XEN_ARGO_RING_EMSGSIZE   space_required is too large for the ring size
 * XEN_ARGO_RING_EBUSY      too many domains waiting for available space signals
 *
 * arg1: XEN_GUEST_HANDLE(xen_argo_ring_data_t) ring_data (may be NULL)
 * arg2: NULL
 * arg3: 0 (ZERO)
 * arg4: 0 (ZERO)
 */
#define XEN_ARGO_OP_notify              4

#endif
