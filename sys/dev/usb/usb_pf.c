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

void
usbpf_attach(struct usb_bus *ubus)
{
	struct ifnet *ifp;

	ifp = ubus->ifp = if_alloc(IFT_USB);
	if_initname(ifp, "usbus", device_get_unit(ubus->bdev));
	ifp->if_flags = IFF_CANTCONFIG;
	if_attach(ifp);
	if_up(ifp);

	KASSERT(sizeof(struct usbpf_pkthdr) == USBPF_HDR_LEN,
	    ("wrong USB pf header length (%zd)", sizeof(struct usbpf_pkthdr)));

	/*
	 * XXX According to the specification of DLT_USB, it indicates packets
	 * beginning with USB setup header.  But not sure all packets would be.
	 */
	bpfattach(ifp, DLT_USB, USBPF_HDR_LEN);

	if (bootverbose)
		device_printf(ubus->parent, "usbpf attached\n");
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

void
usbpf_xfertap(struct usb_xfer *xfer, int type)
{
	struct usb_endpoint *ep = xfer->endpoint;
	struct usb_page_search res;
	struct usb_xfer_root *info = xfer->xroot;
	struct usb_bus *bus = info->bus;
	struct usbpf_pkthdr *up;
	usb_frlength_t isoc_offset = 0;
	int i;
	char *buf, *ptr, *end;

	if (!bpf_peers_present(bus->ifp->if_bpf))
		return;

	/*
	 * XXX TODO
	 * Allocating the buffer here causes copy operations twice what's
	 * really inefficient. Copying usbpf_pkthdr and data is for USB packet
	 * read filter to pass a virtually linear buffer.
	 */
	buf = ptr = malloc(sizeof(struct usbpf_pkthdr) + (USB_PAGE_SIZE * 5),
	    M_TEMP, M_NOWAIT);
	if (buf == NULL) {
		printf("usbpf_xfertap: out of memory\n");	/* XXX */
		return;
	}
	end = buf + sizeof(struct usbpf_pkthdr) + (USB_PAGE_SIZE * 5);

	bzero(ptr, sizeof(struct usbpf_pkthdr));
	up = (struct usbpf_pkthdr *)ptr;
	up->up_busunit = htole32(device_get_unit(bus->bdev));
	up->up_type = type;
	up->up_xfertype = ep->edesc->bmAttributes & UE_XFERTYPE;
	up->up_address = xfer->address;
	up->up_endpoint = xfer->endpointno;
	up->up_flags = htole32(usbpf_aggregate_xferflags(&xfer->flags));
	up->up_status = htole32(usbpf_aggregate_status(&xfer->flags_int));
	switch (type) {
	case USBPF_XFERTAP_SUBMIT:
		up->up_length = htole32(xfer->sumlen);
		up->up_frames = htole32(xfer->nframes);
		break;
	case USBPF_XFERTAP_DONE:
		up->up_length = htole32(xfer->actlen);
		up->up_frames = htole32(xfer->aframes);
		break;
	default:
		panic("wrong usbpf type (%d)", type);
	}

	up->up_error = htole32(xfer->error);
	up->up_interval = htole32(xfer->interval);
	ptr += sizeof(struct usbpf_pkthdr);

	for (i = 0; i < up->up_frames; i++) {
		if (ptr + sizeof(u_int32_t) >= end)
			goto done;
		*((u_int32_t *)ptr) = htole32(xfer->frlengths[i]);
		ptr += sizeof(u_int32_t);

		if (ptr + xfer->frlengths[i] >= end)
			goto done;
		if (xfer->flags_int.isochronous_xfr == 1) {
			usbd_get_page(&xfer->frbuffers[0], isoc_offset, &res);
			isoc_offset += xfer->frlengths[i];
		} else
			usbd_get_page(&xfer->frbuffers[i], 0, &res);
		bcopy(res.buffer, ptr, xfer->frlengths[i]);
		ptr += xfer->frlengths[i];
	}

	bpf_tap(bus->ifp->if_bpf, buf, ptr - buf);
done:
	free(buf, M_TEMP);
}
