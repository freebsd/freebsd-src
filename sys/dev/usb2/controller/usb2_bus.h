/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _USB2_BUS_H_
#define	_USB2_BUS_H_

/*
 * The following structure defines the USB explore message sent to the
 * USB explore process.
 */

struct usb2_bus_msg {
	struct usb2_proc_msg hdr;
	struct usb2_bus *bus;
};

/*
 * The following structure defines the USB statistics structure.
 */
struct usb2_bus_stat {
	uint32_t uds_requests[4];
};

/*
 * The following structure defines an USB BUS. There is one USB BUS
 * for every Host or Device controller.
 */
struct usb2_bus {
	struct usb2_bus_stat stats_err;
	struct usb2_bus_stat stats_ok;
	struct usb2_process explore_proc;
	struct usb2_bus_msg explore_msg[2];
	struct usb2_bus_msg detach_msg[2];
	struct mtx mtx;			/* This mutex protects the USB
					 * hardware */
	struct usb2_perm perm;
	struct usb2_xfer_queue intr_q;

	device_t bdev;			/* filled by HC driver */

	struct usb2_dma_parent_tag dma_parent_tag[1];
	struct usb2_dma_tag dma_tags[USB_BUS_DMA_TAG_MAX];

	struct usb2_bus_methods *methods;	/* filled by HC driver */
	struct usb2_device *devices[USB_MAX_DEVICES];

	uint32_t uframe_usage[USB_HS_MICRO_FRAMES_MAX];
	uint32_t transfer_count[4];
	uint16_t isoc_time_last;	/* in milliseconds */

	uint8_t	alloc_failed;		/* Set if memory allocation failed. */
	uint8_t	driver_added_refcount;	/* Current driver generation count */
	uint8_t	usbrev;			/* USB revision. See "USB_REV_XXX". */

	uint8_t	devices_max;		/* maximum number of USB devices */
	uint8_t	do_probe;		/* set if USB BUS should be re-probed */

	union {
		struct usb2_hw_ep_scratch hw_ep_scratch[1];
		struct usb2_temp_setup temp_setup[1];
		uint8_t	data[128];
	}	scratch[1];
};

#endif					/* _USB2_BUS_H_ */
