/*
 * usbif.h
 *
 * USB I/O interface for Xen guest OSes.
 *
 * Copyright (C) 2009, FUJITSU LABORATORIES LTD.
 * Author: Noboru Iwamatsu <n_iwamatsu@jp.fujitsu.com>
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
 */

#ifndef __XEN_PUBLIC_IO_USBIF_H__
#define __XEN_PUBLIC_IO_USBIF_H__

#include "ring.h"
#include "../grant_table.h"

enum usb_spec_version {
	USB_VER_UNKNOWN = 0,
	USB_VER_USB11,
	USB_VER_USB20,
	USB_VER_USB30,	/* not supported yet */
};

/*
 *  USB pipe in usbif_request
 *
 *  bits 0-5 are specific bits for virtual USB driver.
 *  bits 7-31 are standard urb pipe.
 *
 *  - port number(NEW):	bits 0-4
 *  				(USB_MAXCHILDREN is 31)
 *
 *  - operation flag(NEW):	bit 5
 *  				(0 = submit urb,
 *  				 1 = unlink urb)
 *
 *  - direction:		bit 7
 *  				(0 = Host-to-Device [Out]
 *                           1 = Device-to-Host [In])
 *
 *  - device address:	bits 8-14
 *
 *  - endpoint:		bits 15-18
 *
 *  - pipe type:		bits 30-31
 *  				(00 = isochronous, 01 = interrupt,
 *                           10 = control, 11 = bulk)
 */
#define usbif_pipeportnum(pipe) ((pipe) & 0x1f)
#define usbif_setportnum_pipe(pipe, portnum) \
	((pipe)|(portnum))

#define usbif_pipeunlink(pipe) ((pipe) & 0x20)
#define usbif_pipesubmit(pipe) (!usbif_pipeunlink(pipe))
#define usbif_setunlink_pipe(pipe) ((pipe)|(0x20))

#define USBIF_BACK_MAX_PENDING_REQS (128)
#define USBIF_MAX_SEGMENTS_PER_REQUEST (16)

/*
 * RING for transferring urbs.
 */
struct usbif_request_segment {
	grant_ref_t gref;
	uint16_t offset;
	uint16_t length;
};

struct usbif_urb_request {
	uint16_t id; /* request id */
	uint16_t nr_buffer_segs; /* number of urb->transfer_buffer segments */

	/* basic urb parameter */
	uint32_t pipe;
	uint16_t transfer_flags;
	uint16_t buffer_length;
	union {
		uint8_t ctrl[8]; /* setup_packet (Ctrl) */

		struct {
			uint16_t interval; /* maximum (1024*8) in usb core */
			uint16_t start_frame; /* start frame */
			uint16_t number_of_packets; /* number of ISO packet */
			uint16_t nr_frame_desc_segs; /* number of iso_frame_desc segments */
		} isoc;

		struct {
			uint16_t interval; /* maximum (1024*8) in usb core */
			uint16_t pad[3];
		} intr;

		struct {
			uint16_t unlink_id; /* unlink request id */
			uint16_t pad[3];
		} unlink;

	} u;

	/* urb data segments */
	struct usbif_request_segment seg[USBIF_MAX_SEGMENTS_PER_REQUEST];
};
typedef struct usbif_urb_request usbif_urb_request_t;

struct usbif_urb_response {
	uint16_t id; /* request id */
	uint16_t start_frame;  /* start frame (ISO) */
	int32_t status; /* status (non-ISO) */
	int32_t actual_length; /* actual transfer length */
	int32_t error_count; /* number of ISO errors */
};
typedef struct usbif_urb_response usbif_urb_response_t;

DEFINE_RING_TYPES(usbif_urb, struct usbif_urb_request, struct usbif_urb_response);
#define USB_URB_RING_SIZE __CONST_RING_SIZE(usbif_urb, PAGE_SIZE)

/*
 * RING for notifying connect/disconnect events to frontend
 */
struct usbif_conn_request {
	uint16_t id;
};
typedef struct usbif_conn_request usbif_conn_request_t;

struct usbif_conn_response {
	uint16_t id; /* request id */
	uint8_t portnum; /* port number */
	uint8_t speed; /* usb_device_speed */
};
typedef struct usbif_conn_response usbif_conn_response_t;

DEFINE_RING_TYPES(usbif_conn, struct usbif_conn_request, struct usbif_conn_response);
#define USB_CONN_RING_SIZE __CONST_RING_SIZE(usbif_conn, PAGE_SIZE)

#endif /* __XEN_PUBLIC_IO_USBIF_H__ */
