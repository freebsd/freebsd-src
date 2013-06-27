/******************************************************************************
 * blkif.h
 *
 * Unified block-device I/O interface for Xen guest OSes.
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
 * Copyright (c) 2012, Spectra Logic Corporation
 */

#ifndef __XEN_PUBLIC_IO_BLKIF_H__
#define __XEN_PUBLIC_IO_BLKIF_H__

#include "ring.h"
#include "../grant_table.h"

/*
 * Front->back notifications: When enqueuing a new request, sending a
 * notification can be made conditional on req_event (i.e., the generic
 * hold-off mechanism provided by the ring macros). Backends must set
 * req_event appropriately (e.g., using RING_FINAL_CHECK_FOR_REQUESTS()).
 *
 * Back->front notifications: When enqueuing a new response, sending a
 * notification can be made conditional on rsp_event (i.e., the generic
 * hold-off mechanism provided by the ring macros). Frontends must set
 * rsp_event appropriately (e.g., using RING_FINAL_CHECK_FOR_RESPONSES()).
 */

#ifndef blkif_vdev_t
#define blkif_vdev_t   uint16_t
#endif
#define blkif_sector_t uint64_t

/*
 * Feature and Parameter Negotiation
 * =================================
 * The two halves of a Xen block driver utilize nodes within the XenStore to
 * communicate capabilities and to negotiate operating parameters.  This
 * section enumerates these nodes which reside in the respective front and
 * backend portions of the XenStore, following the XenBus convention.
 *
 * All data in the XenStore is stored as strings.  Nodes specifying numeric
 * values are encoded in decimal.  Integer value ranges listed below are
 * expressed as fixed sized integer types capable of storing the conversion
 * of a properly formatted node string, without loss of information.
 *
 * Any specified default value is in effect if the corresponding XenBus node
 * is not present in the XenStore.
 *
 * XenStore nodes in sections marked "PRIVATE" are solely for use by the
 * driver side whose XenBus tree contains them.
 *
 * XenStore nodes marked "DEPRECATED" in their notes section should only be
 * used to provide interoperability with legacy implementations.
 *
 * See the XenBus state transition diagram below for details on when XenBus
 * nodes must be published and when they can be queried.
 *
 *****************************************************************************
 *                            Backend XenBus Nodes
 *****************************************************************************
 *
 *------------------ Backend Device Identification (PRIVATE) ------------------
 *
 * mode
 *      Values:         "r" (read only), "w" (writable)
 *
 *      The read or write access permissions to the backing store to be
 *      granted to the frontend.
 *
 * params
 *      Values:         string
 *
 *      Data used by the backend driver to locate and configure the backing
 *      device.  The format and semantics of this data vary according to the
 *      backing device in use and are outside the scope of this specification.
 *
 * type
 *      Values:         "file", "phy", "tap"
 *
 *      The type of the backing device/object.
 *
 *--------------------------------- Features ---------------------------------
 *
 * feature-barrier
 *      Values:         0/1 (boolean)
 *      Default Value:  0
 *
 *      A value of "1" indicates that the backend can process requests
 *      containing the BLKIF_OP_WRITE_BARRIER request opcode.  Requests
 *      of this type may still be returned at any time with the
 *      BLKIF_RSP_EOPNOTSUPP result code.
 *
 * feature-flush-cache
 *      Values:         0/1 (boolean)
 *      Default Value:  0
 *
 *      A value of "1" indicates that the backend can process requests
 *      containing the BLKIF_OP_FLUSH_DISKCACHE request opcode.  Requests
 *      of this type may still be returned at any time with the
 *      BLKIF_RSP_EOPNOTSUPP result code.
 *
 * feature-discard
 *      Values:         0/1 (boolean)
 *      Default Value:  0
 *
 *      A value of "1" indicates that the backend can process requests
 *      containing the BLKIF_OP_DISCARD request opcode.  Requests
 *      of this type may still be returned at any time with the
 *      BLKIF_RSP_EOPNOTSUPP result code.
 *
 *----------------------- Request Transport Parameters ------------------------
 *
 * max-ring-page-order
 *      Values:         <uint32_t>
 *      Default Value:  0
 *      Notes:          1, 3
 *
 *      The maximum supported size of the request ring buffer in units of
 *      lb(machine pages). (e.g. 0 == 1 page,  1 = 2 pages, 2 == 4 pages,
 *      etc.).
 *
 * max-ring-pages
 *      Values:         <uint32_t>
 *      Default Value:  1
 *      Notes:          DEPRECATED, 2, 3
 *
 *      The maximum supported size of the request ring buffer in units of
 *      machine pages.  The value must be a power of 2.
 *
 * max-requests         <uint32_t>
 *      Default Value:  BLKIF_MAX_RING_REQUESTS(PAGE_SIZE)
 *      Maximum Value:  BLKIF_MAX_RING_REQUESTS(PAGE_SIZE * max-ring-pages)
 *
 *      The maximum number of concurrent, logical requests supported by
 *      the backend.
 *
 *      Note: A logical request may span multiple ring entries.
 *
 * max-request-segments
 *      Values:         <uint8_t>
 *      Default Value:  BLKIF_MAX_SEGMENTS_PER_HEADER_BLOCK
 *      Maximum Value:  BLKIF_MAX_SEGMENTS_PER_REQUEST
 *
 *      The maximum value of blkif_request.nr_segments supported by
 *      the backend.
 *
 * max-request-size
 *      Values:         <uint32_t>
 *      Default Value:  BLKIF_MAX_SEGMENTS_PER_HEADER_BLOCK * PAGE_SIZE
 *      Maximum Value:  BLKIF_MAX_SEGMENTS_PER_REQUEST * PAGE_SIZE
 *
 *      The maximum amount of data, in bytes, that can be referenced by a
 *      request type that accesses frontend memory (currently BLKIF_OP_READ,
 *      BLKIF_OP_WRITE, or BLKIF_OP_WRITE_BARRIER).
 *
 *------------------------- Backend Device Properties -------------------------
 *
 * discard-alignment
 *      Values:         <uint32_t>
 *      Default Value:  0
 *      Notes:          4, 5
 *
 *      The offset, in bytes from the beginning of the virtual block device,
 *      to the first, addressable, discard extent on the underlying device.
 *
 * discard-granularity
 *      Values:         <uint32_t>
 *      Default Value:  <"sector-size">
 *      Notes:          4
 *
 *      The size, in bytes, of the individually addressable discard extents
 *      of the underlying device.
 *
 * discard-secure
 *      Values:         0/1 (boolean)
 *      Default Value:  0
 *
 *      A value of "1" indicates that the backend can process BLKIF_OP_DISCARD
 *      requests with the BLKIF_DISCARD_SECURE flag set.
 *
 * info
 *      Values:         <uint32_t> (bitmap)
 *
 *      A collection of bit flags describing attributes of the backing
 *      device.  The VDISK_* macros define the meaning of each bit
 *      location.
 *
 * sector-size
 *      Values:         <uint32_t>
 *
 *      The size, in bytes, of the individually addressible data blocks
 *      on the backend device.
 *
 * sectors
 *      Values:         <uint64_t>
 *
 *      The size of the backend device, expressed in units of its native
 *      sector size ("sector-size").
 *
 *****************************************************************************
 *                            Frontend XenBus Nodes
 *****************************************************************************
 *
 *----------------------- Request Transport Parameters -----------------------
 *
 * event-channel
 *      Values:         <uint32_t>
 *
 *      The identifier of the Xen event channel used to signal activity
 *      in the ring buffer.
 *
 * ring-ref
 *      Values:         <uint32_t>
 *      Notes:          6
 *
 *      The Xen grant reference granting permission for the backend to map
 *      the sole page in a single page sized ring buffer.
 *
 * ring-ref%u
 *      Values:         <uint32_t>
 *      Notes:          6
 *
 *      For a frontend providing a multi-page ring, a "number of ring pages"
 *      sized list of nodes, each containing a Xen grant reference granting
 *      permission for the backend to map the page of the ring located
 *      at page index "%u".  Page indexes are zero based.
 *
 * protocol
 *      Values:         string (XEN_IO_PROTO_ABI_*)
 *      Default Value:  XEN_IO_PROTO_ABI_NATIVE
 *
 *      The machine ABI rules governing the format of all ring request and
 *      response structures.
 *
 * ring-page-order
 *      Values:         <uint32_t>
 *      Default Value:  0
 *      Maximum Value:  MAX(ffs(max-ring-pages) - 1, max-ring-page-order)
 *      Notes:          1, 3
 *
 *      The size of the frontend allocated request ring buffer in units
 *      of lb(machine pages). (e.g. 0 == 1 page, 1 = 2 pages, 2 == 4 pages,
 *      etc.).
 *
 * num-ring-pages
 *      Values:         <uint32_t>
 *      Default Value:  1
 *      Maximum Value:  MAX(max-ring-pages,(0x1 << max-ring-page-order))
 *      Notes:          DEPRECATED, 2, 3
 *
 *      The size of the frontend allocated request ring buffer in units of
 *      machine pages.  The value must be a power of 2.
 *
 * max-requests
 *      Values:         <uint32_t>
 *      Default Value:  BLKIF_MAX_RING_REQUESTS(PAGE_SIZE)
 *      Maximum Value:  BLKIF_MAX_RING_REQUESTS(PAGE_SIZE * max-ring-pages)
 *
 *      The maximum number of concurrent, logical requests that will be
 *      issued by the frontend.
 *
 *      Note: A logical request may span multiple ring entries.
 *
 * max-request-segments
 *      Values:         <uint8_t>
 *      Default Value:  BLKIF_MAX_SEGMENTS_PER_HEADER_BLOCK
 *      Maximum Value:  MIN(255, backend/max-request-segments)
 *
 *      The maximum value the frontend will set in the
 *      blkif_request.nr_segments field.
 *
 * max-request-size
 *      Values:         <uint32_t>
 *      Default Value:  BLKIF_MAX_SEGMENTS_PER_HEADER_BLOCK * PAGE_SIZE
 *      Maximum Value:  max-request-segments * PAGE_SIZE
 *
 *      The maximum amount of data, in bytes, that can be referenced by
 *      a request type that accesses frontend memory (currently BLKIF_OP_READ,
 *      BLKIF_OP_WRITE, or BLKIF_OP_WRITE_BARRIER).
 *
 *------------------------- Virtual Device Properties -------------------------
 *
 * device-type
 *      Values:         "disk", "cdrom", "floppy", etc.
 *
 * virtual-device
 *      Values:         <uint32_t>
 *
 *      A value indicating the physical device to virtualize within the
 *      frontend's domain.  (e.g. "The first ATA disk", "The third SCSI
 *      disk", etc.)
 *
 *      See docs/misc/vbd-interface.txt for details on the format of this
 *      value.
 *
 * Notes
 * -----
 * (1) Multi-page ring buffer scheme first developed in the Citrix XenServer
 *     PV drivers.
 * (2) Multi-page ring buffer scheme first used in some Red Hat distributions
 *     including a distribution deployed on certain nodes of the Amazon
 *     EC2 cluster.
 * (3) Support for multi-page ring buffers was implemented independently,
 *     in slightly different forms, by both Citrix and Red Hat/Amazon.
 *     For full interoperability, block front and backends should publish
 *     identical ring parameters, adjusted for unit differences, to the
 *     XenStore nodes used in both schemes.
 * (4) Devices that support discard functionality may internally allocate
 *     space (discardable extents) in units that are larger than the
 *     exported logical block size.
 * (5) The discard-alignment parameter allows a physical device to be
 *     partitioned into virtual devices that do not necessarily begin or
 *     end on a discardable extent boundary.
 * (6) When there is only a single page allocated to the request ring,
 *     'ring-ref' is used to communicate the grant reference for this
 *     page to the backend.  When using a multi-page ring, the 'ring-ref'
 *     node is not created.  Instead 'ring-ref0' - 'ring-refN' are used.
 */

/*
 * STATE DIAGRAMS
 *
 *****************************************************************************
 *                                   Startup                                 *
 *****************************************************************************
 *
 * Tool stack creates front and back nodes with state XenbusStateInitialising.
 *
 * Front                                Back
 * =================================    =====================================
 * XenbusStateInitialising              XenbusStateInitialising
 *  o Query virtual device               o Query backend device identification
 *    properties.                          data.
 *  o Setup OS device instance.          o Open and validate backend device.
 *                                       o Publish backend features and
 *                                         transport parameters.
 *                                                      |
 *                                                      |
 *                                                      V
 *                                      XenbusStateInitWait
 *
 * o Query backend features and
 *   transport parameters.
 * o Allocate and initialize the
 *   request ring.
 * o Publish transport parameters
 *   that will be in effect during
 *   this connection.
 *              |
 *              |
 *              V
 * XenbusStateInitialised
 *
 *                                       o Query frontend transport parameters.
 *                                       o Connect to the request ring and
 *                                         event channel.
 *                                       o Publish backend device properties.
 *                                                      |
 *                                                      |
 *                                                      V
 *                                      XenbusStateConnected
 *
 *  o Query backend device properties.
 *  o Finalize OS virtual device
 *    instance.
 *              |
 *              |
 *              V
 * XenbusStateConnected
 *
 * Note: Drivers that do not support any optional features, or the negotiation
 *       of transport parameters, can skip certain states in the state machine:
 *
 *       o A frontend may transition to XenbusStateInitialised without
 *         waiting for the backend to enter XenbusStateInitWait.  In this
 *         case, default transport parameters are in effect and any
 *         transport parameters published by the frontend must contain
 *         their default values.
 *
 *       o A backend may transition to XenbusStateInitialised, bypassing
 *         XenbusStateInitWait, without waiting for the frontend to first
 *         enter the XenbusStateInitialised state.  In this case, default
 *         transport parameters are in effect and any transport parameters
 *         published by the backend must contain their default values.
 *
 *       Drivers that support optional features and/or transport parameter
 *       negotiation must tolerate these additional state transition paths.
 *       In general this means performing the work of any skipped state
 *       transition, if it has not already been performed, in addition to the
 *       work associated with entry into the current state.
 */

/*
 * REQUEST CODES.
 */
#define BLKIF_OP_READ              0
#define BLKIF_OP_WRITE             1
/*
 * All writes issued prior to a request with the BLKIF_OP_WRITE_BARRIER
 * operation code ("barrier request") must be completed prior to the
 * execution of the barrier request.  All writes issued after the barrier
 * request must not execute until after the completion of the barrier request.
 *
 * Optional.  See "feature-barrier" XenBus node documentation above.
 */
#define BLKIF_OP_WRITE_BARRIER     2
/*
 * Commit any uncommitted contents of the backing device's volatile cache
 * to stable storage.
 *
 * Optional.  See "feature-flush-cache" XenBus node documentation above.
 */
#define BLKIF_OP_FLUSH_DISKCACHE   3
/*
 * Used in SLES sources for device specific command packet
 * contained within the request. Reserved for that purpose.
 */
#define BLKIF_OP_RESERVED_1        4
/*
 * Indicate to the backend device that a region of storage is no longer in
 * use, and may be discarded at any time without impact to the client.  If
 * the BLKIF_DISCARD_SECURE flag is set on the request, all copies of the
 * discarded region on the device must be rendered unrecoverable before the
 * command returns.
 *
 * This operation is analogous to performing a trim (ATA) or unmap (SCSI),
 * command on a native device.
 *
 * More information about trim/unmap operations can be found at:
 * http://t13.org/Documents/UploadedDocuments/docs2008/
 *     e07154r6-Data_Set_Management_Proposal_for_ATA-ACS2.doc
 * http://www.seagate.com/staticfiles/support/disc/manuals/
 *     Interface%20manuals/100293068c.pdf
 *
 * Optional.  See "feature-discard", "discard-alignment",
 * "discard-granularity", and "discard-secure" in the XenBus node
 * documentation above.
 */
#define BLKIF_OP_DISCARD           5

/*
 * Maximum scatter/gather segments associated with a request header block.
 * This is carefully chosen so that sizeof(blkif_ring_t) <= PAGE_SIZE.
 * NB. This could be 12 if the ring indexes weren't stored in the same page.
 */
#define BLKIF_MAX_SEGMENTS_PER_HEADER_BLOCK  11

/*
 * Maximum scatter/gather segments associated with a segment block.
 */
#define BLKIF_MAX_SEGMENTS_PER_SEGMENT_BLOCK 14

/*
 * Maximum scatter/gather segments per request (header + segment blocks).
 */
#define BLKIF_MAX_SEGMENTS_PER_REQUEST 255

/*
 * NB. first_sect and last_sect in blkif_request_segment, as well as
 * sector_number in blkif_request, are always expressed in 512-byte units.
 * However they must be properly aligned to the real sector size of the
 * physical disk, which is reported in the "sector-size" node in the backend
 * xenbus info. Also the xenbus "sectors" node is expressed in 512-byte units.
 */
struct blkif_request_segment {
    grant_ref_t gref;        /* reference to I/O buffer frame        */
    /* @first_sect: first sector in frame to transfer (inclusive).   */
    /* @last_sect: last sector in frame to transfer (inclusive).     */
    uint8_t     first_sect, last_sect;
};
typedef struct blkif_request_segment blkif_request_segment_t;

/*
 * Starting ring element for any I/O request.
 *
 * One or more segment blocks can be inserted into the request ring
 * just after a blkif_request_t, allowing requests to operate on
 * up to BLKIF_MAX_SEGMENTS_PER_REQUEST.
 *
 * BLKIF_SEGS_TO_BLOCKS() can be used on blkif_requst.nr_segments
 * to determine the number of contiguous ring entries associated
 * with this request.
 *
 * Note:  Due to the way Xen request rings operate, the producer and
 *        consumer indices of the ring must be incremented by the
 *        BLKIF_SEGS_TO_BLOCKS() value of the associated request.
 *        (e.g. a response to a 3 ring entry request must also consume
 *        3 entries in the ring, even though only the first ring entry
 *        in the response has any data.)
 */
struct blkif_request {
    uint8_t        operation;    /* BLKIF_OP_???                         */
    uint8_t        nr_segments;  /* number of segments                   */
    blkif_vdev_t   handle;       /* only for read/write requests         */
    uint64_t       id;           /* private guest value, echoed in resp  */
    blkif_sector_t sector_number;/* start sector idx on disk (r/w only)  */
    blkif_request_segment_t seg[BLKIF_MAX_SEGMENTS_PER_HEADER_BLOCK];
};
typedef struct blkif_request blkif_request_t;

/*
 * A segment block is a ring request structure that contains only
 * segment data.
 *
 * sizeof(struct blkif_segment_block) <= sizeof(struct blkif_request)
 */
struct blkif_segment_block {
    blkif_request_segment_t seg[BLKIF_MAX_SEGMENTS_PER_SEGMENT_BLOCK];
};
typedef struct blkif_segment_block blkif_segment_block_t;

/*
 * Cast to this structure when blkif_request.operation == BLKIF_OP_DISCARD
 * sizeof(struct blkif_request_discard) <= sizeof(struct blkif_request)
 */
struct blkif_request_discard {
    uint8_t        operation;    /* BLKIF_OP_DISCARD                     */
    uint8_t        flag;         /* BLKIF_DISCARD_SECURE or zero         */
#define BLKIF_DISCARD_SECURE (1<<0)  /* ignored if discard-secure=0      */
    blkif_vdev_t   handle;       /* same as for read/write requests      */
    uint64_t       id;           /* private guest value, echoed in resp  */
    blkif_sector_t sector_number;/* start sector idx on disk             */
    uint64_t       nr_sectors;   /* number of contiguous sectors to discard*/
};
typedef struct blkif_request_discard blkif_request_discard_t;

struct blkif_response {
    uint64_t        id;              /* copied from request */
    uint8_t         operation;       /* copied from request */
    int16_t         status;          /* BLKIF_RSP_???       */
};
typedef struct blkif_response blkif_response_t;

/*
 * STATUS RETURN CODES.
 */
 /* Operation not supported (only happens on barrier writes). */
#define BLKIF_RSP_EOPNOTSUPP  -2
 /* Operation failed for some unspecified reason (-EIO). */
#define BLKIF_RSP_ERROR       -1
 /* Operation completed successfully. */
#define BLKIF_RSP_OKAY         0

/*
 * Generate blkif ring structures and types.
 */
DEFINE_RING_TYPES(blkif, struct blkif_request, struct blkif_response);

/*
 * Index to, and treat as a segment block, an entry in the ring.
 */
#define BLKRING_GET_SEG_BLOCK(_r, _idx)                                 \
    (((blkif_segment_block_t *)RING_GET_REQUEST(_r, _idx))->seg)

/*
 * The number of ring request blocks required to handle an I/O
 * request containing _segs segments.
 */
#define BLKIF_SEGS_TO_BLOCKS(_segs)                                     \
    ((((_segs - BLKIF_MAX_SEGMENTS_PER_HEADER_BLOCK)                    \
     + (BLKIF_MAX_SEGMENTS_PER_SEGMENT_BLOCK - 1))                      \
    / BLKIF_MAX_SEGMENTS_PER_SEGMENT_BLOCK) + /*header_block*/1)

#define VDISK_CDROM        0x1
#define VDISK_REMOVABLE    0x2
#define VDISK_READONLY     0x4

#endif /* __XEN_PUBLIC_IO_BLKIF_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
