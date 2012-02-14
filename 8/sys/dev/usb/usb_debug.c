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

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_transfer.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h>

/*
 * Define this unconditionally in case a kernel module is loaded that
 * has been compiled with debugging options.
 */
int	usb_debug = 0;

SYSCTL_NODE(_hw, OID_AUTO, usb, CTLFLAG_RW, 0, "USB debugging");
SYSCTL_INT(_hw_usb, OID_AUTO, debug, CTLFLAG_RW,
    &usb_debug, 0, "Debug level");

TUNABLE_INT("hw.usb.debug", &usb_debug);

/*------------------------------------------------------------------------*
 *	usb_dump_iface
 *
 * This function dumps information about an USB interface.
 *------------------------------------------------------------------------*/
void
usb_dump_iface(struct usb_interface *iface)
{
	printf("usb_dump_iface: iface=%p\n", iface);
	if (iface == NULL) {
		return;
	}
	printf(" iface=%p idesc=%p altindex=%d\n",
	    iface, iface->idesc, iface->alt_index);
}

/*------------------------------------------------------------------------*
 *	usb_dump_device
 *
 * This function dumps information about an USB device.
 *------------------------------------------------------------------------*/
void
usb_dump_device(struct usb_device *udev)
{
	printf("usb_dump_device: dev=%p\n", udev);
	if (udev == NULL) {
		return;
	}
	printf(" bus=%p \n"
	    " address=%d config=%d depth=%d speed=%d self_powered=%d\n"
	    " power=%d langid=%d\n",
	    udev->bus,
	    udev->address, udev->curr_config_no, udev->depth, udev->speed,
	    udev->flags.self_powered, udev->power, udev->langid);
}

/*------------------------------------------------------------------------*
 *	usb_dump_queue
 *
 * This function dumps the USB transfer that are queued up on an USB endpoint.
 *------------------------------------------------------------------------*/
void
usb_dump_queue(struct usb_endpoint *ep)
{
	struct usb_xfer *xfer;

	printf("usb_dump_queue: endpoint=%p xfer: ", ep);
	TAILQ_FOREACH(xfer, &ep->endpoint_q.head, wait_entry) {
		printf(" %p", xfer);
	}
	printf("\n");
}

/*------------------------------------------------------------------------*
 *	usb_dump_endpoint
 *
 * This function dumps information about an USB endpoint.
 *------------------------------------------------------------------------*/
void
usb_dump_endpoint(struct usb_endpoint *ep)
{
	if (ep) {
		printf("usb_dump_endpoint: endpoint=%p", ep);

		printf(" edesc=%p isoc_next=%d toggle_next=%d",
		    ep->edesc, ep->isoc_next, ep->toggle_next);

		if (ep->edesc) {
			printf(" bEndpointAddress=0x%02x",
			    ep->edesc->bEndpointAddress);
		}
		printf("\n");
		usb_dump_queue(ep);
	} else {
		printf("usb_dump_endpoint: endpoint=NULL\n");
	}
}

/*------------------------------------------------------------------------*
 *	usb_dump_xfer
 *
 * This function dumps information about an USB transfer.
 *------------------------------------------------------------------------*/
void
usb_dump_xfer(struct usb_xfer *xfer)
{
	struct usb_device *udev;
	printf("usb_dump_xfer: xfer=%p\n", xfer);
	if (xfer == NULL) {
		return;
	}
	if (xfer->endpoint == NULL) {
		printf("xfer %p: endpoint=NULL\n",
		    xfer);
		return;
	}
	udev = xfer->xroot->udev;
	printf("xfer %p: udev=%p vid=0x%04x pid=0x%04x addr=%d "
	    "endpoint=%p ep=0x%02x attr=0x%02x\n",
	    xfer, udev,
	    UGETW(udev->ddesc.idVendor),
	    UGETW(udev->ddesc.idProduct),
	    udev->address, xfer->endpoint,
	    xfer->endpoint->edesc->bEndpointAddress,
	    xfer->endpoint->edesc->bmAttributes);
}
