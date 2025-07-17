/******************************************************************************
 * vscsiif.h
 *
 * Based on the blkif.h code.
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
 * Copyright(c) FUJITSU Limited 2008.
 */

#ifndef __XEN__PUBLIC_IO_SCSI_H__
#define __XEN__PUBLIC_IO_SCSI_H__

#include "ring.h"
#include "../grant_table.h"

/*
 * Feature and Parameter Negotiation
 * =================================
 * The two halves of a Xen pvSCSI driver utilize nodes within the XenStore to
 * communicate capabilities and to negotiate operating parameters.  This
 * section enumerates these nodes which reside in the respective front and
 * backend portions of the XenStore, following the XenBus convention.
 *
 * Any specified default value is in effect if the corresponding XenBus node
 * is not present in the XenStore.
 *
 * XenStore nodes in sections marked "PRIVATE" are solely for use by the
 * driver side whose XenBus tree contains them.
 *
 *****************************************************************************
 *                            Backend XenBus Nodes
 *****************************************************************************
 *
 *------------------ Backend Device Identification (PRIVATE) ------------------
 *
 * p-devname
 *      Values:         string
 *
 *      A free string used to identify the physical device (e.g. a disk name).
 *
 * p-dev
 *      Values:         string
 *
 *      A string specifying the backend device: either a 4-tuple "h:c:t:l"
 *      (host, controller, target, lun, all integers), or a WWN (e.g.
 *      "naa.60014054ac780582:0").
 *
 * v-dev
 *      Values:         string
 *
 *      A string specifying the frontend device in form of a 4-tuple "h:c:t:l"
 *      (host, controller, target, lun, all integers).
 *
 *--------------------------------- Features ---------------------------------
 *
 * feature-sg-grant
 *      Values:         unsigned [VSCSIIF_SG_TABLESIZE...65535]
 *      Default Value:  0
 *
 *      Specifies the maximum number of scatter/gather elements in grant pages
 *      supported. If not set, the backend supports up to VSCSIIF_SG_TABLESIZE
 *      SG elements specified directly in the request.
 *
 *****************************************************************************
 *                            Frontend XenBus Nodes
 *****************************************************************************
 *
 *----------------------- Request Transport Parameters -----------------------
 *
 * event-channel
 *      Values:         unsigned
 *
 *      The identifier of the Xen event channel used to signal activity
 *      in the ring buffer.
 *
 * ring-ref
 *      Values:         unsigned
 *
 *      The Xen grant reference granting permission for the backend to map
 *      the sole page in a single page sized ring buffer.
 *
 * protocol
 *      Values:         string (XEN_IO_PROTO_ABI_*)
 *      Default Value:  XEN_IO_PROTO_ABI_NATIVE
 *
 *      The machine ABI rules governing the format of all ring request and
 *      response structures.
 */

/*
 * Xenstore format in practice
 * ===========================
 *
 * The backend driver uses a single_host:many_devices notation to manage domU
 * devices. Everything is stored in /local/domain/<backend_domid>/backend/vscsi/.
 * The xenstore layout looks like this (dom0 is assumed to be the backend_domid):
 *
 *     <domid>/<vhost>/feature-host = "0"
 *     <domid>/<vhost>/frontend = "/local/domain/<domid>/device/vscsi/0"
 *     <domid>/<vhost>/frontend-id = "<domid>"
 *     <domid>/<vhost>/online = "1"
 *     <domid>/<vhost>/state = "4"
 *     <domid>/<vhost>/vscsi-devs/dev-0/p-dev = "8:0:2:1" or "naa.wwn:lun"
 *     <domid>/<vhost>/vscsi-devs/dev-0/state = "4"
 *     <domid>/<vhost>/vscsi-devs/dev-0/v-dev = "0:0:0:0"
 *     <domid>/<vhost>/vscsi-devs/dev-1/p-dev = "8:0:2:2"
 *     <domid>/<vhost>/vscsi-devs/dev-1/state = "4"
 *     <domid>/<vhost>/vscsi-devs/dev-1/v-dev = "0:0:1:0"
 *
 * The frontend driver maintains its state in
 * /local/domain/<domid>/device/vscsi/.
 *
 *     <vhost>/backend = "/local/domain/0/backend/vscsi/<domid>/<vhost>"
 *     <vhost>/backend-id = "0"
 *     <vhost>/event-channel = "20"
 *     <vhost>/ring-ref = "43"
 *     <vhost>/state = "4"
 *     <vhost>/vscsi-devs/dev-0/state = "4"
 *     <vhost>/vscsi-devs/dev-1/state = "4"
 *
 * In addition to the entries for backend and frontend these flags are stored
 * for the toolstack:
 *
 *     <domid>/<vhost>/vscsi-devs/dev-1/p-devname = "/dev/$device"
 *     <domid>/<vhost>/libxl_ctrl_index = "0"
 *
 *
 * Backend/frontend protocol
 * =========================
 *
 * To create a vhost along with a device:
 *     <domid>/<vhost>/feature-host = "0"
 *     <domid>/<vhost>/frontend = "/local/domain/<domid>/device/vscsi/0"
 *     <domid>/<vhost>/frontend-id = "<domid>"
 *     <domid>/<vhost>/online = "1"
 *     <domid>/<vhost>/state = "1"
 *     <domid>/<vhost>/vscsi-devs/dev-0/p-dev = "8:0:2:1"
 *     <domid>/<vhost>/vscsi-devs/dev-0/state = "1"
 *     <domid>/<vhost>/vscsi-devs/dev-0/v-dev = "0:0:0:0"
 * Wait for <domid>/<vhost>/state + <domid>/<vhost>/vscsi-devs/dev-0/state become 4
 *
 * To add another device to a vhost:
 *     <domid>/<vhost>/state = "7"
 *     <domid>/<vhost>/vscsi-devs/dev-1/p-dev = "8:0:2:2"
 *     <domid>/<vhost>/vscsi-devs/dev-1/state = "1"
 *     <domid>/<vhost>/vscsi-devs/dev-1/v-dev = "0:0:1:0"
 * Wait for <domid>/<vhost>/state + <domid>/<vhost>/vscsi-devs/dev-1/state become 4
 *
 * To remove a device from a vhost:
 *     <domid>/<vhost>/state = "7"
 *     <domid>/<vhost>/vscsi-devs/dev-1/state = "5"
 * Wait for <domid>/<vhost>/state to become 4
 * Wait for <domid>/<vhost>/vscsi-devs/dev-1/state become 6
 * Remove <domid>/<vhost>/vscsi-devs/dev-1/{state,p-dev,v-dev,p-devname}
 * Remove <domid>/<vhost>/vscsi-devs/dev-1/
 *
 */

/* Requests from the frontend to the backend */

/*
 * Request a SCSI operation specified via a CDB in vscsiif_request.cmnd.
 * The target is specified via channel, id and lun.
 *
 * The operation to be performed is specified via a CDB in cmnd[], the length
 * of the CDB is in cmd_len. sc_data_direction specifies the direction of data
 * (to the device, from the device, or none at all).
 *
 * If data is to be transferred to or from the device the buffer(s) in the
 * guest memory is/are specified via one or multiple scsiif_request_segment
 * descriptors each specifying a memory page via a grant_ref_t, a offset into
 * the page and the length of the area in that page. All scsiif_request_segment
 * areas concatenated form the resulting data buffer used by the operation.
 * If the number of scsiif_request_segment areas is not too large (less than
 * or equal VSCSIIF_SG_TABLESIZE) the areas can be specified directly in the
 * seg[] array and the number of valid scsiif_request_segment elements is to be
 * set in nr_segments.
 *
 * If "feature-sg-grant" in the Xenstore is set it is possible to specify more
 * than VSCSIIF_SG_TABLESIZE scsiif_request_segment elements via indirection.
 * The maximum number of allowed scsiif_request_segment elements is the value
 * of the "feature-sg-grant" entry from Xenstore. When using indirection the
 * seg[] array doesn't contain specifications of the data buffers, but
 * references to scsiif_request_segment arrays, which in turn reference the
 * data buffers. While nr_segments holds the number of populated seg[] entries
 * (plus the set VSCSIIF_SG_GRANT bit), the number of scsiif_request_segment
 * elements referencing the target data buffers is calculated from the lengths
 * of the seg[] elements (the sum of all valid seg[].length divided by the
 * size of one scsiif_request_segment structure). The frontend may use a mix of
 * direct and indirect requests.
 */
#define VSCSIIF_ACT_SCSI_CDB         1

/*
 * Request abort of a running operation for the specified target given by
 * channel, id, lun and the operation's rqid in ref_rqid.
 */
#define VSCSIIF_ACT_SCSI_ABORT       2

/*
 * Request a device reset of the specified target (channel and id).
 */
#define VSCSIIF_ACT_SCSI_RESET       3

/*
 * Preset scatter/gather elements for a following request. Deprecated.
 * Keeping the define only to avoid usage of the value "4" for other actions.
 */
#define VSCSIIF_ACT_SCSI_SG_PRESET   4

/*
 * Maximum scatter/gather segments per request.
 *
 * Considering balance between allocating at least 16 "vscsiif_request"
 * structures on one page (4096 bytes) and the number of scatter/gather
 * elements needed, we decided to use 26 as a magic number.
 *
 * If "feature-sg-grant" is set, more scatter/gather elements can be specified
 * by placing them in one or more (up to VSCSIIF_SG_TABLESIZE) granted pages.
 * In this case the vscsiif_request seg elements don't contain references to
 * the user data, but to the SG elements referencing the user data.
 */
#define VSCSIIF_SG_TABLESIZE             26

/*
 * based on Linux kernel 2.6.18, still valid
 *
 * Changing these values requires support of multiple protocols via the rings
 * as "old clients" will blindly use these values and the resulting structure
 * sizes.
 */
#define VSCSIIF_MAX_COMMAND_SIZE         16
#define VSCSIIF_SENSE_BUFFERSIZE         96
#define VSCSIIF_PAGE_SIZE              4096

struct scsiif_request_segment {
    grant_ref_t gref;
    uint16_t offset;
    uint16_t length;
};
typedef struct scsiif_request_segment vscsiif_segment_t;

#define VSCSIIF_SG_PER_PAGE (VSCSIIF_PAGE_SIZE / sizeof(struct scsiif_request_segment))

/* Size of one request is 252 bytes */
struct vscsiif_request {
    uint16_t rqid;          /* private guest value, echoed in resp  */
    uint8_t act;            /* command between backend and frontend */
    uint8_t cmd_len;        /* valid CDB bytes */

    uint8_t cmnd[VSCSIIF_MAX_COMMAND_SIZE]; /* the CDB */
    uint16_t timeout_per_command;   /* deprecated: timeout in secs, 0=default */
    uint16_t channel, id, lun;      /* (virtual) device specification */
    uint16_t ref_rqid;              /* command abort reference */
    uint8_t sc_data_direction;      /* for DMA_TO_DEVICE(1)
                                       DMA_FROM_DEVICE(2)
                                       DMA_NONE(3) requests  */
    uint8_t nr_segments;            /* Number of pieces of scatter-gather */
/*
 * flag in nr_segments: SG elements via grant page
 *
 * If VSCSIIF_SG_GRANT is set, the low 7 bits of nr_segments specify the number
 * of grant pages containing SG elements. Usable if "feature-sg-grant" set.
 */
#define VSCSIIF_SG_GRANT    0x80

    vscsiif_segment_t seg[VSCSIIF_SG_TABLESIZE];
    uint32_t reserved[3];
};
typedef struct vscsiif_request vscsiif_request_t;

/*
 * The following interface is deprecated!
 */
#define VSCSIIF_SG_LIST_SIZE ((sizeof(vscsiif_request_t) - 4) \
                              / sizeof(vscsiif_segment_t))

struct vscsiif_sg_list {
    /* First two fields must match struct vscsiif_request! */
    uint16_t rqid;          /* private guest value, must match main req */
    uint8_t act;            /* VSCSIIF_ACT_SCSI_SG_PRESET */
    uint8_t nr_segments;    /* Number of pieces of scatter-gather */
    vscsiif_segment_t seg[VSCSIIF_SG_LIST_SIZE];
};
typedef struct vscsiif_sg_list vscsiif_sg_list_t;
/* End of deprecated interface */

/* Size of one response is 252 bytes */
struct vscsiif_response {
    uint16_t rqid;          /* identifies request */
    uint8_t act;            /* deprecated: valid only if SG_PRESET supported */
    uint8_t sense_len;
    uint8_t sense_buffer[VSCSIIF_SENSE_BUFFERSIZE];
    int32_t rslt;
    uint32_t residual_len;     /* request bufflen -
                                  return the value from physical device */
    uint32_t reserved[36];
};
typedef struct vscsiif_response vscsiif_response_t;

DEFINE_RING_TYPES(vscsiif, struct vscsiif_request, struct vscsiif_response);


#endif  /*__XEN__PUBLIC_IO_SCSI_H__*/
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
