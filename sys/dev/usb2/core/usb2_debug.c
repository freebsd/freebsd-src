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

#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_defs.h>

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_device.h>

/*
 * Define this unconditionally in case a kernel module is loaded that
 * has been compiled with debugging options.
 */
int	usb2_debug = 0;

SYSCTL_NODE(_hw, OID_AUTO, usb2, CTLFLAG_RW, 0, "USB debugging");
SYSCTL_INT(_hw_usb2, OID_AUTO, debug, CTLFLAG_RW,
    &usb2_debug, 0, "Debug level");

/*------------------------------------------------------------------------*
 *	usb2_dump_iface
 *
 * This function dumps information about an USB interface.
 *------------------------------------------------------------------------*/
void
usb2_dump_iface(struct usb2_interface *iface)
{
	printf("usb2_dump_iface: iface=%p\n", iface);
	if (iface == NULL) {
		return;
	}
	printf(" iface=%p idesc=%p altindex=%d\n",
	    iface, iface->idesc, iface->alt_index);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_dump_device
 *
 * This function dumps information about an USB device.
 *------------------------------------------------------------------------*/
void
usb2_dump_device(struct usb2_device *udev)
{
	printf("usb2_dump_device: dev=%p\n", udev);
	if (udev == NULL) {
		return;
	}
	printf(" bus=%p \n"
	    " address=%d config=%d depth=%d speed=%d self_powered=%d\n"
	    " power=%d langid=%d\n",
	    udev->bus,
	    udev->address, udev->curr_config_no, udev->depth, udev->speed,
	    udev->flags.self_powered, udev->power, udev->langid);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_dump_queue
 *
 * This function dumps the USB transfer that are queued up on an USB pipe.
 *------------------------------------------------------------------------*/
void
usb2_dump_queue(struct usb2_pipe *pipe)
{
	struct usb2_xfer *xfer;

	printf("usb2_dump_queue: pipe=%p xfer: ", pipe);
	TAILQ_FOREACH(xfer, &pipe->pipe_q.head, wait_entry) {
		printf(" %p", xfer);
	}
	printf("\n");
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_dump_pipe
 *
 * This function dumps information about an USB pipe.
 *------------------------------------------------------------------------*/
void
usb2_dump_pipe(struct usb2_pipe *pipe)
{
	if (pipe) {
		printf("usb2_dump_pipe: pipe=%p", pipe);

		printf(" edesc=%p isoc_next=%d toggle_next=%d",
		    pipe->edesc, pipe->isoc_next, pipe->toggle_next);

		if (pipe->edesc) {
			printf(" bEndpointAddress=0x%02x",
			    pipe->edesc->bEndpointAddress);
		}
		printf("\n");
		usb2_dump_queue(pipe);
	} else {
		printf("usb2_dump_pipe: pipe=NULL\n");
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_dump_xfer
 *
 * This function dumps information about an USB transfer.
 *------------------------------------------------------------------------*/
void
usb2_dump_xfer(struct usb2_xfer *xfer)
{
	printf("usb2_dump_xfer: xfer=%p\n", xfer);
	if (xfer == NULL) {
		return;
	}
	if (xfer->pipe == NULL) {
		printf("xfer %p: pipe=NULL\n",
		    xfer);
		return;
	}
	printf("xfer %p: udev=%p vid=0x%04x pid=0x%04x addr=%d "
	    "pipe=%p ep=0x%02x attr=0x%02x\n",
	    xfer, xfer->udev,
	    UGETW(xfer->udev->ddesc.idVendor),
	    UGETW(xfer->udev->ddesc.idProduct),
	    xfer->udev->address, xfer->pipe,
	    xfer->pipe->edesc->bEndpointAddress,
	    xfer->pipe->edesc->bmAttributes);
	return;
}
