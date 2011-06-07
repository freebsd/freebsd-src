/*-
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_core.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/usb_pf.h>
#include <dev/usb/usb_transfer.h>

static int usb_no_pf;

SYSCTL_INT(_hw_usb, OID_AUTO, no_pf, CTLFLAG_RW,
    &usb_no_pf, 0, "Set to disable USB packet filtering");

TUNABLE_INT("hw.usb.no_pf", &usb_no_pf);

void
usbpf_attach(struct usb_bus *ubus)
{
	struct ifnet *ifp;

	if (usb_no_pf != 0) {
		ubus->ifp = NULL;
		return;
	}

	ifp = ubus->ifp = if_alloc(IFT_USB);
	if (ifp == NULL) {
		device_printf(ubus->parent, "usbpf: Could not allocate "
		    "instance\n");
		return;
	}

	if_initname(ifp, "usbus", device_get_unit(ubus->bdev));
	ifp->if_flags = IFF_CANTCONFIG;
	if_attach(ifp);
	if_up(ifp);

	/*
	 * XXX According to the specification of DLT_USB, it indicates
	 * packets beginning with USB setup header. But not sure all
	 * packets would be.
	 */
	bpfattach(ifp, DLT_USB, USBPF_HDR_LEN);

	if (bootverbose)
		device_printf(ubus->parent, "usbpf: Attached\n");
}

void
usbpf_detach(struct usb_bus *ubus)
{
	struct ifnet *ifp = ubus->ifp;

	if (ifp != NULL) {
		bpfdetach(ifp);
		if_down(ifp);
		if_detach(ifp);
		if_free(ifp);
	}
	ubus->ifp = NULL;
}

static uint32_t
usbpf_aggregate_xferflags(struct usb_xfer_flags *flags)
{
	uint32_t val = 0;

	if (flags->force_short_xfer == 1)
		val |= USBPF_FLAG_FORCE_SHORT_XFER;
	if (flags->short_xfer_ok == 1)
		val |= USBPF_FLAG_SHORT_XFER_OK;
	if (flags->short_frames_ok == 1)
		val |= USBPF_FLAG_SHORT_FRAMES_OK;
	if (flags->pipe_bof == 1)
		val |= USBPF_FLAG_PIPE_BOF;
	if (flags->proxy_buffer == 1)
		val |= USBPF_FLAG_PROXY_BUFFER;
	if (flags->ext_buffer == 1)
		val |= USBPF_FLAG_EXT_BUFFER;
	if (flags->manual_status == 1)
		val |= USBPF_FLAG_MANUAL_STATUS;
	if (flags->no_pipe_ok == 1)
		val |= USBPF_FLAG_NO_PIPE_OK;
	if (flags->stall_pipe == 1)
		val |= USBPF_FLAG_STALL_PIPE;
	return (val);
}

static uint32_t
usbpf_aggregate_status(struct usb_xfer_flags_int *flags)
{
	uint32_t val = 0;

	if (flags->open == 1)
		val |= USBPF_STATUS_OPEN;
	if (flags->transferring == 1)
		val |= USBPF_STATUS_TRANSFERRING;
	if (flags->did_dma_delay == 1)
		val |= USBPF_STATUS_DID_DMA_DELAY;
	if (flags->did_close == 1)
		val |= USBPF_STATUS_DID_CLOSE;
	if (flags->draining == 1)
		val |= USBPF_STATUS_DRAINING;
	if (flags->started == 1)
		val |= USBPF_STATUS_STARTED;
	if (flags->bandwidth_reclaimed == 1)
		val |= USBPF_STATUS_BW_RECLAIMED;
	if (flags->control_xfr == 1)
		val |= USBPF_STATUS_CONTROL_XFR;
	if (flags->control_hdr == 1)
		val |= USBPF_STATUS_CONTROL_HDR;
	if (flags->control_act == 1)
		val |= USBPF_STATUS_CONTROL_ACT;
	if (flags->control_stall == 1)
		val |= USBPF_STATUS_CONTROL_STALL;
	if (flags->short_frames_ok == 1)
		val |= USBPF_STATUS_SHORT_FRAMES_OK;
	if (flags->short_xfer_ok == 1)
		val |= USBPF_STATUS_SHORT_XFER_OK;
#if USB_HAVE_BUSDMA
	if (flags->bdma_enable == 1)
		val |= USBPF_STATUS_BDMA_ENABLE;
	if (flags->bdma_no_post_sync == 1)
		val |= USBPF_STATUS_BDMA_NO_POST_SYNC;
	if (flags->bdma_setup == 1)
		val |= USBPF_STATUS_BDMA_SETUP;
#endif
	if (flags->isochronous_xfr == 1)
		val |= USBPF_STATUS_ISOCHRONOUS_XFR;
	if (flags->curr_dma_set == 1)
		val |= USBPF_STATUS_CURR_DMA_SET;
	if (flags->can_cancel_immed == 1)
		val |= USBPF_STATUS_CAN_CANCEL_IMMED;
	if (flags->doing_callback == 1)
		val |= USBPF_STATUS_DOING_CALLBACK;

	return (val);
}

static int
usbpf_xfer_frame_is_read(struct usb_xfer *xfer, uint32_t frame)
{
	int isread;

	if ((frame == 0) && (xfer->flags_int.control_xfr != 0) &&
	    (xfer->flags_int.control_hdr != 0)) {
		/* special case */
		if (xfer->flags_int.usb_mode == USB_MODE_DEVICE) {
			/* The device controller writes to memory */
			isread = 1;
		} else {
			/* The host controller reads from memory */
			isread = 0;
		}
	} else {
		isread = USB_GET_DATA_ISREAD(xfer);
	}
	return (isread);
}

static uint32_t
usbpf_xfer_precompute_size(struct usb_xfer *xfer, int type)
{
	uint32_t totlen;
	uint32_t x;
	uint32_t nframes;

	if (type == USBPF_XFERTAP_SUBMIT)
		nframes = xfer->nframes;
	else
		nframes = xfer->aframes;

	totlen = USBPF_HDR_LEN + (USBPF_FRAME_HDR_LEN * nframes);

	/* precompute all trace lengths */
	for (x = 0; x != nframes; x++) {
		if (usbpf_xfer_frame_is_read(xfer, x)) {
			if (type != USBPF_XFERTAP_SUBMIT) {
				totlen += USBPF_FRAME_ALIGN(
				    xfer->frlengths[x]);
			}
		} else {
			if (type == USBPF_XFERTAP_SUBMIT) {
				totlen += USBPF_FRAME_ALIGN(
				    xfer->frlengths[x]);
			}
		}
	}
	return (totlen);
}

void
usbpf_xfertap(struct usb_xfer *xfer, int type)
{
	struct usb_bus *bus;
	struct usbpf_pkthdr *up;
	struct usbpf_framehdr *uf;
	usb_frlength_t offset;
	uint32_t totlen;
	uint32_t frame;
	uint32_t temp;
	uint32_t nframes;
	uint32_t x;
	uint8_t *buf;
	uint8_t *ptr;

	bus = xfer->xroot->bus;

	/* sanity checks */
	if (usb_no_pf != 0)
		return;
	if (bus->ifp == NULL)
		return;
	if (!bpf_peers_present(bus->ifp->if_bpf))
		return;

	totlen = usbpf_xfer_precompute_size(xfer, type);

	if (type == USBPF_XFERTAP_SUBMIT)
		nframes = xfer->nframes;
	else
		nframes = xfer->aframes;

	/*
	 * XXX TODO XXX
	 *
	 * When BPF supports it we could pass a fragmented array of
	 * buffers avoiding the data copy operation here.
	 */
	buf = ptr = malloc(totlen, M_TEMP, M_NOWAIT);
	if (buf == NULL) {
		device_printf(bus->parent, "usbpf: Out of memory\n");
		return;
	}

	up = (struct usbpf_pkthdr *)ptr;
	ptr += USBPF_HDR_LEN;

	/* fill out header */
	temp = device_get_unit(bus->bdev);
	up->up_totlen = htole32(totlen);
	up->up_busunit = htole32(temp);
	up->up_address = xfer->xroot->udev->device_index;
	if (xfer->flags_int.usb_mode == USB_MODE_DEVICE)
		up->up_mode = USBPF_MODE_DEVICE;
	else
		up->up_mode = USBPF_MODE_HOST;
	up->up_type = type;
	up->up_xfertype = xfer->endpoint->edesc->bmAttributes & UE_XFERTYPE;
	temp = usbpf_aggregate_xferflags(&xfer->flags);
	up->up_flags = htole32(temp);
	temp = usbpf_aggregate_status(&xfer->flags_int);
	up->up_status = htole32(temp);
	temp = xfer->error;
	up->up_error = htole32(temp);
	temp = xfer->interval;
	up->up_interval = htole32(temp);
	up->up_frames = htole32(nframes);
	temp = xfer->max_packet_size;
	up->up_packet_size = htole32(temp);
	temp = xfer->max_packet_count;
	up->up_packet_count = htole32(temp);
	temp = xfer->endpointno;
	up->up_endpoint = htole32(temp);
	up->up_speed = xfer->xroot->udev->speed;

	/* clear reserved area */
	memset(up->up_reserved, 0, sizeof(up->up_reserved));

	/* init offset and frame */
	offset = 0;
	frame = 0;

	/* iterate all the USB frames and copy data, if any */
	for (x = 0; x != nframes; x++) {
		uint32_t length;
		int isread;

		/* get length */
		length = xfer->frlengths[x];

		/* get frame header pointer */
		uf = (struct usbpf_framehdr *)ptr;
		ptr += USBPF_FRAME_HDR_LEN;

		/* fill out packet header */
		uf->length = htole32(length);
		uf->flags = 0;

		/* get information about data read/write */
		isread = usbpf_xfer_frame_is_read(xfer, x);

		/* check if we need to copy any data */
		if (isread) {
			if (type == USBPF_XFERTAP_SUBMIT)
				length = 0;
			else {
				uf->flags |= htole32(
				    USBPF_FRAMEFLAG_DATA_FOLLOWS);
			}
		} else {
			if (type != USBPF_XFERTAP_SUBMIT)
				length = 0;
			else {
				uf->flags |= htole32(
				    USBPF_FRAMEFLAG_DATA_FOLLOWS);
			}
		}

		/* check if data is read direction */
		if (isread)
			uf->flags |= htole32(USBPF_FRAMEFLAG_READ);

		/* copy USB data, if any */
		if (length != 0) {
			/* copy data */
			usbd_copy_out(&xfer->frbuffers[frame],
			    offset, ptr, length);

			/* align length */
			temp = USBPF_FRAME_ALIGN(length);

			/* zero pad */
			if (temp != length)
				memset(ptr + length, 0, temp - length);

			ptr += temp;
		}

		if (xfer->flags_int.isochronous_xfr) {
			offset += usbd_xfer_old_frame_length(xfer, x);
		} else {
			frame ++;
		}
	}

	bpf_tap(bus->ifp->if_bpf, buf, totlen);

	free(buf, M_TEMP);
}
