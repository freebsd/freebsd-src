/*	$NetBSD: usbdi.c,v 1.71 2000/03/29 01:45:21 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include "usb_if.h"
#if defined(DIAGNOSTIC) && defined(__i386__)
#include <machine/cpu.h>
#endif
#endif
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#if defined(__FreeBSD__)
#include "usb_if.h"
#include <machine/clock.h>
#define delay(d)	DELAY(d)
#endif

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

Static usbd_status usbd_ar_pipe(usbd_pipe_handle pipe);
Static void usbd_do_request_async_cb 
    (usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void usbd_start_next(usbd_pipe_handle pipe);
Static usbd_status usbd_open_pipe_ival
    (usbd_interface_handle, u_int8_t, u_int8_t, usbd_pipe_handle *, int);

Static int usbd_nbuses = 0;

void
usbd_init()
{
	usbd_nbuses++;
}

void
usbd_finish()
{
	--usbd_nbuses;
}

Static __inline int usbd_xfer_isread(usbd_xfer_handle xfer);
Static __inline int
usbd_xfer_isread(xfer)
	usbd_xfer_handle xfer;
{
	if (xfer->rqflags & URQ_REQUEST)
		return (xfer->request.bmRequestType & UT_READ);
	else
		return (xfer->pipe->endpoint->edesc->bEndpointAddress &
			UE_DIR_IN);
}

#ifdef USB_DEBUG
void usbd_dump_queue(usbd_pipe_handle);

void
usbd_dump_queue(pipe)
	usbd_pipe_handle pipe;
{
	usbd_xfer_handle xfer;

	printf("usbd_dump_queue: pipe=%p\n", pipe);
	for (xfer = SIMPLEQ_FIRST(&pipe->queue);
	     xfer;
	     xfer = SIMPLEQ_NEXT(xfer, next)) {
		printf("  xfer=%p\n", xfer);
	}
}
#endif

usbd_status 
usbd_open_pipe(iface, address, flags, pipe)
	usbd_interface_handle iface;
	u_int8_t address;
	u_int8_t flags;
	usbd_pipe_handle *pipe;
{ 
	return (usbd_open_pipe_ival(iface, address, flags, pipe, 
				    USBD_DEFAULT_INTERVAL));
}

usbd_status 
usbd_open_pipe_ival(iface, address, flags, pipe, ival)
	usbd_interface_handle iface;
	u_int8_t address;
	u_int8_t flags;
	usbd_pipe_handle *pipe;
	int ival;
{ 
	usbd_pipe_handle p;
	struct usbd_endpoint *ep;
	usbd_status err;
	int i;

	DPRINTFN(3,("usbd_open_pipe: iface=%p address=0x%x flags=0x%x\n",
		    iface, address, flags));

	for (i = 0; i < iface->idesc->bNumEndpoints; i++) {
		ep = &iface->endpoints[i];
		if (ep->edesc == NULL)
			return (USBD_IOERROR);
		if (ep->edesc->bEndpointAddress == address)
			goto found;
	}
	return (USBD_BAD_ADDRESS);
 found:
	if ((flags & USBD_EXCLUSIVE_USE) && ep->refcnt != 0)
		return (USBD_IN_USE);
	err = usbd_setup_pipe(iface->device, iface, ep, ival, &p);
	if (err)
		return (err);
	LIST_INSERT_HEAD(&iface->pipes, p, next);
	*pipe = p;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_open_pipe_intr(iface, address, flags, pipe, priv, buffer, len, cb, ival)
	usbd_interface_handle iface;
	u_int8_t address;
	u_int8_t flags;
	usbd_pipe_handle *pipe;
	usbd_private_handle priv;
	void *buffer;
	u_int32_t len;
	usbd_callback cb;
	int ival;
{
	usbd_status err;
	usbd_xfer_handle xfer;
	usbd_pipe_handle ipipe;

	DPRINTFN(3,("usbd_open_pipe_intr: address=0x%x flags=0x%x len=%d\n",
		    address, flags, len));

	err = usbd_open_pipe_ival(iface, address, USBD_EXCLUSIVE_USE, 
				  &ipipe, ival);
	if (err)
		return (err);
	xfer = usbd_alloc_xfer(iface->device);
	if (xfer == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	usbd_setup_xfer(xfer, ipipe, priv, buffer, len, flags,
	    USBD_NO_TIMEOUT, cb);
	ipipe->intrxfer = xfer;
	ipipe->repeat = 1;
	err = usbd_transfer(xfer);
	*pipe = ipipe;
	if (err != USBD_IN_PROGRESS)
		goto bad2;
	return (USBD_NORMAL_COMPLETION);

 bad2:
	ipipe->intrxfer = NULL;
	ipipe->repeat = 0;
	usbd_free_xfer(xfer);
 bad1:
	usbd_close_pipe(ipipe);
	return (err);
}

usbd_status
usbd_close_pipe(pipe)
	usbd_pipe_handle pipe;
{
#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		printf("usbd_close_pipe: pipe==NULL\n");
		return (USBD_NORMAL_COMPLETION);
	}
#endif

	if (--pipe->refcnt != 0)
		return (USBD_NORMAL_COMPLETION);
	if (SIMPLEQ_FIRST(&pipe->queue) != 0)
		return (USBD_PENDING_REQUESTS);
	LIST_REMOVE(pipe, next);
	pipe->endpoint->refcnt--;
	pipe->methods->close(pipe);
#if defined(__NetBSD__) && defined(DIAGNOSTIC)
	if (callout_pending(&pipe->abort_handle)) {
		callout_stop(&pipe->abort_handle);
		printf("usbd_close_pipe: abort_handle pending");
	}
#endif
	if (pipe->intrxfer != NULL)
		usbd_free_xfer(pipe->intrxfer);
	free(pipe, M_USB);
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_transfer(xfer)
	usbd_xfer_handle xfer;
{
	usbd_pipe_handle pipe = xfer->pipe;
	usb_dma_t *dmap = &xfer->dmabuf;
	usbd_status err;
	u_int size;
	int s;

	DPRINTFN(5,("usbd_transfer: xfer=%p, flags=%d, pipe=%p, running=%d\n",
		    xfer, xfer->flags, pipe, pipe->running));
#ifdef USB_DEBUG
	if (usbdebug > 5)
		usbd_dump_queue(pipe);
#endif
	xfer->done = 0;

	if (pipe->aborting)
		return (USBD_CANCELLED);

	size = xfer->length;
	/* If there is no buffer, allocate one. */
	if (!(xfer->rqflags & URQ_DEV_DMABUF) && size != 0) {
		struct usbd_bus *bus = pipe->device->bus;

#ifdef DIAGNOSTIC
		if (xfer->rqflags & URQ_AUTO_DMABUF)
			printf("usbd_transfer: has old buffer!\n");
#endif
		err = bus->methods->allocm(bus, dmap, size);
		if (err)
			return (err);
		xfer->rqflags |= URQ_AUTO_DMABUF;
	}

	/* Copy data if going out. */
	if (!(xfer->flags & USBD_NO_COPY) && size != 0 && 
	    !usbd_xfer_isread(xfer))
		memcpy(KERNADDR(dmap, 0), xfer->buffer, size);

	err = pipe->methods->transfer(xfer);

	if (err != USBD_IN_PROGRESS && err) {
		/* The transfer has not been queued, so free buffer. */
		if (xfer->rqflags & URQ_AUTO_DMABUF) {
			struct usbd_bus *bus = pipe->device->bus;

			bus->methods->freem(bus, &xfer->dmabuf);
			xfer->rqflags &= ~URQ_AUTO_DMABUF;
		}
	}

	if (!(xfer->flags & USBD_SYNCHRONOUS))
		return (err);

	/* Sync transfer, wait for completion. */
	if (err != USBD_IN_PROGRESS)
		return (err);
	s = splusb();
	if (!xfer->done) {
		if (pipe->device->bus->use_polling)
			panic("usbd_transfer: not done\n");
		/* XXX Temporary hack XXX */
		if (xfer->flags & USBD_NO_TSLEEP) {
			int i;
			usbd_bus_handle bus = pipe->device->bus;
			int to = xfer->timeout * 1000;
			for (i = 0; i < to; i += 10) {
				delay(10);
				bus->methods->do_poll(bus);
				if (xfer->done)
					break;
			}
			if (!xfer->done) {
				pipe->methods->abort(xfer);
				xfer->status = USBD_TIMEOUT;
			}
		} else
		/* XXX End hack XXX */
			tsleep(xfer, PRIBIO, "usbsyn", 0);
	}
	splx(s);
	return (xfer->status);
}

/* Like usbd_transfer(), but waits for completion. */
usbd_status
usbd_sync_transfer(xfer)
	usbd_xfer_handle xfer;
{
	xfer->flags |= USBD_SYNCHRONOUS;
	return (usbd_transfer(xfer));
}

void *
usbd_alloc_buffer(xfer, size)
	usbd_xfer_handle xfer;
	u_int32_t size;
{
	struct usbd_bus *bus = xfer->device->bus;
	usbd_status err;

	err = bus->methods->allocm(bus, &xfer->dmabuf, size);
	if (err)
		return (0);
	xfer->rqflags |= URQ_DEV_DMABUF;
	return (KERNADDR(&xfer->dmabuf, 0));
}

void
usbd_free_buffer(xfer)
	usbd_xfer_handle xfer;
{
#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & (URQ_DEV_DMABUF | URQ_AUTO_DMABUF))) {
		printf("usbd_free_buffer: no buffer\n");
		return;
	}
#endif
	xfer->rqflags &= ~(URQ_DEV_DMABUF | URQ_AUTO_DMABUF);
	xfer->device->bus->methods->freem(xfer->device->bus, &xfer->dmabuf);
}

void *
usbd_get_buffer(xfer)
	usbd_xfer_handle xfer;
{
	if (!(xfer->rqflags & URQ_DEV_DMABUF))
		return (0);
	return (KERNADDR(&xfer->dmabuf, 0));
}

usbd_xfer_handle 
usbd_alloc_xfer(dev)
	usbd_device_handle dev;
{
	usbd_xfer_handle xfer;

	xfer = dev->bus->methods->allocx(dev->bus);
	if (xfer == NULL)
		return (NULL);
	xfer->device = dev;
	usb_callout_init(xfer->timeout_handle);
	DPRINTFN(5,("usbd_alloc_xfer() = %p\n", xfer));
	return (xfer);
}

usbd_status 
usbd_free_xfer(xfer)
	usbd_xfer_handle xfer;
{
	DPRINTFN(5,("usbd_free_xfer: %p\n", xfer));
	if (xfer->rqflags & (URQ_DEV_DMABUF | URQ_AUTO_DMABUF))
		usbd_free_buffer(xfer);
#if defined(__NetBSD__) && defined(DIAGNOSTIC)
	if (callout_pending(&xfer->timeout_handle)) {
		callout_stop(&xfer->timeout_handle);
		printf("usbd_free_xfer: timout_handle pending");
	}
#endif
	xfer->device->bus->methods->freex(xfer->device->bus, xfer);
	return (USBD_NORMAL_COMPLETION);
}

void
usbd_setup_xfer(xfer, pipe, priv, buffer, length, flags, timeout, callback)
	usbd_xfer_handle xfer;
	usbd_pipe_handle pipe;
	usbd_private_handle priv;
	void *buffer;
	u_int32_t length;
	u_int16_t flags;
	u_int32_t timeout;
	void (*callback)(usbd_xfer_handle,
			      usbd_private_handle,
			      usbd_status);
{
	xfer->pipe = pipe;
	xfer->priv = priv;
	xfer->buffer = buffer;
	xfer->length = length;
	xfer->actlen = 0;
	xfer->flags = flags;
	xfer->timeout = timeout;
	xfer->status = USBD_NOT_STARTED;
	xfer->callback = callback;
	xfer->rqflags &= ~URQ_REQUEST;
	xfer->nframes = 0;
}

void
usbd_setup_default_xfer(xfer, dev, priv, timeout, req, buffer, 
			   length, flags, callback)
	usbd_xfer_handle xfer;
	usbd_device_handle dev;
	usbd_private_handle priv;
	u_int32_t timeout;
	usb_device_request_t *req;
	void *buffer;
	u_int32_t length;
	u_int16_t flags;
	void (*callback)(usbd_xfer_handle,
			      usbd_private_handle,
			      usbd_status);
{
	xfer->pipe = dev->default_pipe;
	xfer->priv = priv;
	xfer->buffer = buffer;
	xfer->length = length;
	xfer->actlen = 0;
	xfer->flags = flags;
	xfer->timeout = timeout;
	xfer->status = USBD_NOT_STARTED;
	xfer->callback = callback;
	xfer->request = *req;
	xfer->rqflags |= URQ_REQUEST;
	xfer->nframes = 0;
}

void
usbd_setup_isoc_xfer(xfer, pipe, priv, frlengths, nframes, flags, callback)
	usbd_xfer_handle xfer;
	usbd_pipe_handle pipe;
	usbd_private_handle priv;
	u_int16_t *frlengths;
	u_int32_t nframes;
	u_int16_t flags;
	usbd_callback callback;
{
	xfer->pipe = pipe;
	xfer->priv = priv;
	xfer->buffer = 0;
	xfer->length = 0;
	xfer->actlen = 0;
	xfer->flags = flags;
	xfer->timeout = USBD_NO_TIMEOUT;
	xfer->status = USBD_NOT_STARTED;
	xfer->callback = callback;
	xfer->rqflags &= ~URQ_REQUEST;
	xfer->frlengths = frlengths;
	xfer->nframes = nframes;
}

void
usbd_get_xfer_status(xfer, priv, buffer, count, status)
	usbd_xfer_handle xfer;
	usbd_private_handle *priv;
	void **buffer;
	u_int32_t *count;
	usbd_status *status;
{
	if (priv != NULL)
		*priv = xfer->priv;
	if (buffer != NULL)
		*buffer = xfer->buffer;
	if (count != NULL)
		*count = xfer->actlen;
	if (status != NULL)
		*status = xfer->status;
}

usb_config_descriptor_t *
usbd_get_config_descriptor(dev)
	usbd_device_handle dev;
{
#ifdef DIAGNOSTIC
	if (dev == NULL) {
		printf("usbd_get_config_descriptor: dev == NULL\n");
		return (NULL);
	}
#endif
	return (dev->cdesc);
}

usb_interface_descriptor_t *
usbd_get_interface_descriptor(iface)
	usbd_interface_handle iface;
{
#ifdef DIAGNOSTIC
	if (iface == NULL) {
		printf("usbd_get_interface_descriptor: dev == NULL\n");
		return (NULL);
	}
#endif
	return (iface->idesc);
}

usb_device_descriptor_t *
usbd_get_device_descriptor(dev)
	usbd_device_handle dev;
{
	return (&dev->ddesc);
}

usb_endpoint_descriptor_t *
usbd_interface2endpoint_descriptor(iface, index)
	usbd_interface_handle iface;
	u_int8_t index;
{
	if (index >= iface->idesc->bNumEndpoints)
		return (0);
	return (iface->endpoints[index].edesc);
}

usbd_status 
usbd_abort_pipe(pipe)
	usbd_pipe_handle pipe;
{
	usbd_status err;
	int s;

#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		printf("usbd_close_pipe: pipe==NULL\n");
		return (USBD_NORMAL_COMPLETION);
	}
#endif
	s = splusb();
	err = usbd_ar_pipe(pipe);
	splx(s);
	return (err);
}
	
usbd_status 
usbd_clear_endpoint_stall(pipe)
	usbd_pipe_handle pipe;
{
	usbd_device_handle dev = pipe->device;
	usb_device_request_t req;
	usbd_status err;

	DPRINTFN(8, ("usbd_clear_endpoint_stall\n"));

	/* 
	 * Clearing en endpoint stall resets the enpoint toggle, so
	 * do the same to the HC toggle.
	 */
	pipe->methods->cleartoggle(pipe);

	req.bmRequestType = UT_WRITE_ENDPOINT;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, UF_ENDPOINT_HALT);
	USETW(req.wIndex, pipe->endpoint->edesc->bEndpointAddress);
	USETW(req.wLength, 0);
	err = usbd_do_request(dev, &req, 0);
#if 0
XXX should we do this?
	if (!err) {
		pipe->state = USBD_PIPE_ACTIVE;
		/* XXX activate pipe */
	}
#endif
	return (err);
}

usbd_status 
usbd_clear_endpoint_stall_async(pipe)
	usbd_pipe_handle pipe;
{
	usbd_device_handle dev = pipe->device;
	usb_device_request_t req;
	usbd_status err;

	pipe->methods->cleartoggle(pipe);

	req.bmRequestType = UT_WRITE_ENDPOINT;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, UF_ENDPOINT_HALT);
	USETW(req.wIndex, pipe->endpoint->edesc->bEndpointAddress);
	USETW(req.wLength, 0);
	err = usbd_do_request_async(dev, &req, 0);
	return (err);
}

void
usbd_clear_endpoint_toggle(usbd_pipe_handle pipe)
{
	pipe->methods->cleartoggle(pipe);
}

usbd_status 
usbd_endpoint_count(iface, count)
	usbd_interface_handle iface;
	u_int8_t *count;
{
	*count = iface->idesc->bNumEndpoints;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_interface_count(dev, count)
	usbd_device_handle dev;
	u_int8_t *count;
{
	if (dev->cdesc == NULL)
		return (USBD_NOT_CONFIGURED);
	*count = dev->cdesc->bNumInterface;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_interface2device_handle(iface, dev)
	usbd_interface_handle iface;
	usbd_device_handle *dev;
{
	*dev = iface->device;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_device2interface_handle(dev, ifaceno, iface)
	usbd_device_handle dev;
	u_int8_t ifaceno;
	usbd_interface_handle *iface;
{
	if (dev->cdesc == NULL)
		return (USBD_NOT_CONFIGURED);
	if (ifaceno >= dev->cdesc->bNumInterface)
		return (USBD_INVAL);
	*iface = &dev->ifaces[ifaceno];
	return (USBD_NORMAL_COMPLETION);
}

usbd_device_handle
usbd_pipe2device_handle(pipe)
	usbd_pipe_handle pipe;
{
	return (pipe->device);
}

/* XXXX use altno */
usbd_status
usbd_set_interface(iface, altidx)
	usbd_interface_handle iface;
	int altidx;
{
	usb_device_request_t req;
	usbd_status err;

	if (LIST_FIRST(&iface->pipes) != 0)
		return (USBD_IN_USE);

	if (iface->endpoints)
		free(iface->endpoints, M_USB);
	iface->endpoints = 0;
	iface->idesc = 0;

	err = usbd_fill_iface_data(iface->device, iface->index, altidx);
	if (err)
		return (err);

	req.bmRequestType = UT_WRITE_INTERFACE;
	req.bRequest = UR_SET_INTERFACE;
	USETW(req.wValue, iface->idesc->bAlternateSetting);
	USETW(req.wIndex, iface->idesc->bInterfaceNumber);
	USETW(req.wLength, 0);
	return (usbd_do_request(iface->device, &req, 0));
}

int
usbd_get_no_alts(cdesc, ifaceno)
	usb_config_descriptor_t *cdesc;
	int ifaceno;
{
	char *p = (char *)cdesc;
	char *end = p + UGETW(cdesc->wTotalLength);
	usb_interface_descriptor_t *d;
	int n;

	for (n = 0; p < end; p += d->bLength) {
		d = (usb_interface_descriptor_t *)p;
		if (p + d->bLength <= end && 
		    d->bDescriptorType == UDESC_INTERFACE &&
		    d->bInterfaceNumber == ifaceno)
			n++;
	}
	return (n);
}

int
usbd_get_interface_altindex(iface)
	usbd_interface_handle iface;
{
	return (iface->altindex);
}

usbd_status
usbd_get_interface(iface, aiface)
	usbd_interface_handle iface;
	u_int8_t *aiface;
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_INTERFACE;
	req.bRequest = UR_GET_INTERFACE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, iface->idesc->bInterfaceNumber);
	USETW(req.wLength, 1);
	return (usbd_do_request(iface->device, &req, aiface));
}

/*** Internal routines ***/

/* Dequeue all pipe operations, called at splusb(). */
Static usbd_status
usbd_ar_pipe(pipe)
	usbd_pipe_handle pipe;
{
	usbd_xfer_handle xfer;

	SPLUSBCHECK;

	DPRINTFN(2,("usbd_ar_pipe: pipe=%p\n", pipe));
#ifdef USB_DEBUG
	if (usbdebug > 5)
		usbd_dump_queue(pipe);
#endif
	pipe->repeat = 0;
	pipe->aborting = 1;
	while ((xfer = SIMPLEQ_FIRST(&pipe->queue)) != NULL) {
		DPRINTFN(2,("usbd_ar_pipe: pipe=%p xfer=%p (methods=%p)\n", 
			    pipe, xfer, pipe->methods));
		/* Make the HC abort it (and invoke the callback). */
		pipe->methods->abort(xfer);
		/* XXX only for non-0 usbd_clear_endpoint_stall(pipe); */
	}
	pipe->aborting = 0;
	return (USBD_NORMAL_COMPLETION);
}

/* Called at splusb() */
void
usb_transfer_complete(xfer)
	usbd_xfer_handle xfer;
{
	usbd_pipe_handle pipe = xfer->pipe;
	usb_dma_t *dmap = &xfer->dmabuf;
	int repeat = pipe->repeat;
	int polling;

	SPLUSBCHECK;

	DPRINTFN(5, ("usb_transfer_complete: pipe=%p xfer=%p status=%d "
		     "actlen=%d\n", pipe, xfer, xfer->status, xfer->actlen));

#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		printf("usbd_transfer_cb: pipe==0, xfer=%p\n", xfer);
		return;
	}
#endif
	polling = pipe->device->bus->use_polling;
	/* XXXX */
	if (polling)
		pipe->running = 0;

	if (!(xfer->flags & USBD_NO_COPY) && xfer->actlen != 0 &&
	    usbd_xfer_isread(xfer)) {
#ifdef DIAGNOSTIC
		if (xfer->actlen > xfer->length) {
			printf("usb_transfer_complete: actlen > len %d > %d\n",
			       xfer->actlen, xfer->length);
			xfer->actlen = xfer->length;
		}
#endif
		memcpy(xfer->buffer, KERNADDR(dmap, 0), xfer->actlen);
	}

	/* if we allocated the buffer in usbd_transfer() we free it here. */
	if (xfer->rqflags & URQ_AUTO_DMABUF) {
		if (!repeat) {
			struct usbd_bus *bus = pipe->device->bus;
			bus->methods->freem(bus, dmap);
			xfer->rqflags &= ~URQ_AUTO_DMABUF;
		}
	}

	if (!repeat) {
		/* Remove request from queue. */
#ifdef DIAGNOSTIC
		if (xfer != SIMPLEQ_FIRST(&pipe->queue))
			printf("usb_transfer_complete: bad dequeue %p != %p\n",
			       xfer, SIMPLEQ_FIRST(&pipe->queue));
#endif
		SIMPLEQ_REMOVE_HEAD(&pipe->queue, xfer, next);
	}
	DPRINTFN(5,("usb_transfer_complete: repeat=%d new head=%p\n", 
		    repeat, SIMPLEQ_FIRST(&pipe->queue)));

	/* Count completed transfers. */
	++pipe->device->bus->stats.uds_requests
		[pipe->endpoint->edesc->bmAttributes & UE_XFERTYPE];

	xfer->done = 1;
	if (!xfer->status && xfer->actlen < xfer->length &&
	    !(xfer->flags & USBD_SHORT_XFER_OK)) {
		DPRINTFN(-1,("usbd_transfer_cb: short transfer %d<%d\n",
			     xfer->actlen, xfer->length));
		xfer->status = USBD_SHORT_XFER;
	}

	if (xfer->callback)
		xfer->callback(xfer, xfer->priv, xfer->status);

#ifdef DIAGNOSTIC
	if (pipe->methods->done != NULL)
		pipe->methods->done(xfer);
	else
		printf("usb_transfer_complete: pipe->methods->done == NULL\n");
#else
	pipe->methods->done(xfer);
#endif

	if ((xfer->flags & USBD_SYNCHRONOUS) && !polling)
		wakeup(xfer);

	if (!repeat) {
		/* XXX should we stop the queue on all errors? */
		if ((xfer->status == USBD_CANCELLED
		     || xfer->status == USBD_TIMEOUT)
		    && pipe->iface != NULL)		/* not control pipe */
			pipe->running = 0;
		else
			usbd_start_next(pipe);
	}
}

usbd_status
usb_insert_transfer(xfer)
	usbd_xfer_handle xfer;
{
	usbd_pipe_handle pipe = xfer->pipe;
	usbd_status err;
	int s;

	DPRINTFN(5,("usb_insert_transfer: pipe=%p running=%d timeout=%d\n", 
		    pipe, pipe->running, xfer->timeout));
	s = splusb();
	SIMPLEQ_INSERT_TAIL(&pipe->queue, xfer, next);
	if (pipe->running)
		err = USBD_IN_PROGRESS;
	else {
		pipe->running = 1;
		err = USBD_NORMAL_COMPLETION;
	}
	splx(s);
	return (err);
}

/* Called at splusb() */
void
usbd_start_next(pipe)
	usbd_pipe_handle pipe;
{
	usbd_xfer_handle xfer;
	usbd_status err;

	SPLUSBCHECK;

#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		printf("usbd_start_next: pipe == NULL\n");
		return;
	}
	if (pipe->methods == NULL || pipe->methods->start == NULL) {
		printf("usbd_start_next: pipe=%p no start method\n", pipe);
		return;
	}
#endif

	/* Get next request in queue. */
	xfer = SIMPLEQ_FIRST(&pipe->queue);
	DPRINTFN(5, ("usbd_start_next: pipe=%p, xfer=%p\n", pipe, xfer));
	if (xfer == NULL) {
		pipe->running = 0;
	} else {
		err = pipe->methods->start(xfer);
		if (err != USBD_IN_PROGRESS) {
			printf("usbd_start_next: error=%d\n", err);
			pipe->running = 0;
			/* XXX do what? */
		}
	}
}

usbd_status
usbd_do_request(dev, req, data)
	usbd_device_handle dev;
	usb_device_request_t *req;
	void *data;
{
	return (usbd_do_request_flags(dev, req, data, 0, 0));
}

usbd_status
usbd_do_request_flags(dev, req, data, flags, actlen)
	usbd_device_handle dev;
	usb_device_request_t *req;
	void *data;
	u_int16_t flags;
	int *actlen;
{
	usbd_xfer_handle xfer;
	usbd_status err;

#ifdef DIAGNOSTIC
#if defined(__i386__) && defined(__FreeBSD__)
	KASSERT(curthread->td_intr_nesting_level == 0,
	       	("usbd_do_request: in interrupt context"));
#endif
	if (dev->bus->intr_context) {
		printf("usbd_do_request: not in process context\n");
		return (USBD_INVAL);
	}
#endif

	xfer = usbd_alloc_xfer(dev);
	if (xfer == NULL)
		return (USBD_NOMEM);
	usbd_setup_default_xfer(xfer, dev, 0, USBD_DEFAULT_TIMEOUT, req,
				   data, UGETW(req->wLength), flags, 0);
	err = usbd_sync_transfer(xfer);
#if defined(USB_DEBUG) || defined(DIAGNOSTIC)
	if (xfer->actlen > xfer->length)
		DPRINTF(("usbd_do_request: overrun addr=%d type=0x%02x req=0x"
			 "%02x val=%d index=%d rlen=%d length=%d actlen=%d\n",
			 dev->address, xfer->request.bmRequestType,
			 xfer->request.bRequest, UGETW(xfer->request.wValue),
			 UGETW(xfer->request.wIndex), 
			 UGETW(xfer->request.wLength), 
			 xfer->length, xfer->actlen));
#endif
	if (actlen != NULL)
		*actlen = xfer->actlen;
	if (err == USBD_STALLED) {
		/* 
		 * The control endpoint has stalled.  Control endpoints
		 * should not halt, but some may do so anyway so clear
		 * any halt condition.
		 */
		usb_device_request_t treq;
		usb_status_t status;
		u_int16_t s;
		usbd_status nerr;

		treq.bmRequestType = UT_READ_ENDPOINT;
		treq.bRequest = UR_GET_STATUS;
		USETW(treq.wValue, 0);
		USETW(treq.wIndex, 0);
		USETW(treq.wLength, sizeof(usb_status_t));
		usbd_setup_default_xfer(xfer, dev, 0, USBD_DEFAULT_TIMEOUT,
					   &treq, &status,sizeof(usb_status_t),
					   0, 0);
		nerr = usbd_sync_transfer(xfer);
		if (nerr)
			goto bad;
		s = UGETW(status.wStatus);
		DPRINTF(("usbd_do_request: status = 0x%04x\n", s));
		if (!(s & UES_HALT))
			goto bad;
		treq.bmRequestType = UT_WRITE_ENDPOINT;
		treq.bRequest = UR_CLEAR_FEATURE;
		USETW(treq.wValue, UF_ENDPOINT_HALT);
		USETW(treq.wIndex, 0);
		USETW(treq.wLength, 0);
		usbd_setup_default_xfer(xfer, dev, 0, USBD_DEFAULT_TIMEOUT,
					   &treq, &status, 0, 0, 0);
		nerr = usbd_sync_transfer(xfer);
		if (nerr)
			goto bad;
	}

 bad:
	usbd_free_xfer(xfer);
	return (err);
}

void
usbd_do_request_async_cb(xfer, priv, status)
	usbd_xfer_handle xfer;
	usbd_private_handle priv;
	usbd_status status;
{
#if defined(USB_DEBUG) || defined(DIAGNOSTIC)
	if (xfer->actlen > xfer->length)
		DPRINTF(("usbd_do_request: overrun addr=%d type=0x%02x req=0x"
			 "%02x val=%d index=%d rlen=%d length=%d actlen=%d\n",
			 xfer->pipe->device->address, 
			 xfer->request.bmRequestType,
			 xfer->request.bRequest, UGETW(xfer->request.wValue),
			 UGETW(xfer->request.wIndex), 
			 UGETW(xfer->request.wLength), 
			 xfer->length, xfer->actlen));
#endif
	usbd_free_xfer(xfer);
}

/*
 * Execute a request without waiting for completion.
 * Can be used from interrupt context.
 */
usbd_status
usbd_do_request_async(dev, req, data)
	usbd_device_handle dev;
	usb_device_request_t *req;
	void *data;
{
	usbd_xfer_handle xfer;
	usbd_status err;

	xfer = usbd_alloc_xfer(dev);
	if (xfer == NULL)
		return (USBD_NOMEM);
	usbd_setup_default_xfer(xfer, dev, 0, USBD_DEFAULT_TIMEOUT, req,
	    data, UGETW(req->wLength), 0, usbd_do_request_async_cb);
	err = usbd_transfer(xfer);
	if (err != USBD_IN_PROGRESS) {
		usbd_free_xfer(xfer);
		return (err);
	}
	return (USBD_NORMAL_COMPLETION);
}

const struct usbd_quirks *
usbd_get_quirks(dev)
	usbd_device_handle dev;
{
	return (dev->quirks);
}

/* XXX do periodic free() of free list */

/*
 * Called from keyboard driver when in polling mode.
 */
void
usbd_dopoll(iface)
	usbd_interface_handle iface;
{
	iface->device->bus->methods->do_poll(iface->device->bus);
}

void
usbd_set_polling(iface, on)
	usbd_interface_handle iface;
	int on;
{
	if (on)
		iface->device->bus->use_polling++;
	else
		iface->device->bus->use_polling--;
}


usb_endpoint_descriptor_t *
usbd_get_endpoint_descriptor(iface, address)
	usbd_interface_handle iface;
	u_int8_t address;
{
	struct usbd_endpoint *ep;
	int i;

	for (i = 0; i < iface->idesc->bNumEndpoints; i++) {
		ep = &iface->endpoints[i];
		if (ep->edesc->bEndpointAddress == address)
			return (iface->endpoints[i].edesc);
	}
	return (0);
}

/*
 * Search for a vendor/product pair in an array.  The item size is
 * given as an argument.
 */
const struct usb_devno *
usb_match_device(const struct usb_devno *tbl, u_int nentries, u_int sz,
		 u_int16_t vendor, u_int16_t product)
{
	while (nentries-- > 0) {
		if (tbl->ud_vendor == vendor && tbl->ud_product == product)
			return (tbl);
		tbl = (const struct usb_devno *)((const char *)tbl + sz);
	}
	return (NULL);
}

#if defined(__FreeBSD__)
int
usbd_driver_load(module_t mod, int what, void *arg)
{
	/* XXX should implement something like a function that removes all generic devices */
 
 	return (0);
}

#endif
