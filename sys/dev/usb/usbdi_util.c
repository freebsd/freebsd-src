/*	$NetBSD: usbdi_util.c,v 1.26 1999/11/28 22:49:53 augustss Exp $	*/
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
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/proc.h>
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/bus.h>
#endif

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

usbd_status
usbd_get_desc(dev, type, index, len, desc)
	usbd_device_handle dev;
	int type, index;
	int len;
	void *desc;
{
	usb_device_request_t req;

	DPRINTFN(3,("usbd_get_desc: type=%d, index=%d, len=%d\n",
		    type, index, len));

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, type, index);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);
	return (usbd_do_request(dev, &req, desc));
}

usbd_status
usbd_get_config_desc(usbd_device_handle dev, int confidx,
		     usb_config_descriptor_t *d)
{
	usbd_status err;

	DPRINTFN(3,("usbd_get_config_desc: confidx=%d\n", confidx));
	err = usbd_get_desc(dev, UDESC_CONFIG, confidx, 
			    USB_CONFIG_DESCRIPTOR_SIZE, d);
	if (err)
		return (err);
	if (d->bDescriptorType != UDESC_CONFIG) {
		DPRINTFN(-1,("usbd_get_config_desc: confidx=%d, bad desc "
			     "len=%d type=%d\n",
			     confidx, d->bLength, d->bDescriptorType));
		return (USBD_INVAL);
	}
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_get_config_desc_full(dev, conf, d, size)
	usbd_device_handle dev;
	int conf;
	void *d;
	int size;
{
	DPRINTFN(3,("usbd_get_config_desc_full: conf=%d\n", conf));
	return (usbd_get_desc(dev, UDESC_CONFIG, conf, size, d));
}

usbd_status
usbd_get_device_desc(dev, d)
	usbd_device_handle dev;
	usb_device_descriptor_t *d;
{
	DPRINTFN(3,("usbd_get_device_desc:\n"));
	return (usbd_get_desc(dev, UDESC_DEVICE, 
			     0, USB_DEVICE_DESCRIPTOR_SIZE, d));
}

usbd_status
usbd_get_device_status(dev, st)
	usbd_device_handle dev;
	usb_status_t *st;
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(usb_status_t));
	return (usbd_do_request(dev, &req, st));
}	

usbd_status
usbd_get_hub_status(dev, st)
	usbd_device_handle dev;
	usb_hub_status_t *st;
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_CLASS_DEVICE;
	req.bRequest = UR_GET_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(usb_hub_status_t));
	return (usbd_do_request(dev, &req, st));
}	

usbd_status
usbd_set_address(dev, addr)
	usbd_device_handle dev;
	int addr;
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_ADDRESS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return usbd_do_request(dev, &req, 0);
}

usbd_status
usbd_get_port_status(dev, port, ps)
	usbd_device_handle dev;
	int port;
	usb_port_status_t *ps;
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_CLASS_OTHER;
	req.bRequest = UR_GET_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, port);
	USETW(req.wLength, sizeof *ps);
	return (usbd_do_request(dev, &req, ps));
}

usbd_status
usbd_clear_hub_feature(dev, sel)
	usbd_device_handle dev;
	int sel;
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_CLASS_DEVICE;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, sel);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_set_hub_feature(dev, sel)
	usbd_device_handle dev;
	int sel;
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_CLASS_DEVICE;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, sel);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_clear_port_feature(dev, port, sel)
	usbd_device_handle dev;
	int port, sel;
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, sel);
	USETW(req.wIndex, port);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_set_port_feature(dev, port, sel)
	usbd_device_handle dev;
	int port, sel;
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, sel);
	USETW(req.wIndex, port);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}


usbd_status
usbd_set_protocol(iface, report)
	usbd_interface_handle iface;
	int report;
{
	usb_interface_descriptor_t *id = usbd_get_interface_descriptor(iface);
	usbd_device_handle dev;
	usb_device_request_t req;
	usbd_status err;

	DPRINTFN(4, ("usbd_set_protocol: iface=%p, report=%d, endpt=%d\n",
		     iface, report, id->bInterfaceNumber));
	if (id == NULL)
		return (USBD_IOERROR);
	err = usbd_interface2device_handle(iface, &dev);
	if (err)
		return (err);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_PROTOCOL;
	USETW(req.wValue, report);
	USETW(req.wIndex, id->bInterfaceNumber);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_set_report(iface, type, id, data, len)
	usbd_interface_handle iface;
	int type;
	int id;
	void *data;
	int len;
{
	usb_interface_descriptor_t *ifd = usbd_get_interface_descriptor(iface);
	usbd_device_handle dev;
	usb_device_request_t req;
	usbd_status err;

	DPRINTFN(4, ("usbd_set_report: len=%d\n", len));
	if (ifd == NULL)
		return (USBD_IOERROR);
	err = usbd_interface2device_handle(iface, &dev);
	if (err)
		return (err);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_REPORT;
	USETW2(req.wValue, type, id);
	USETW(req.wIndex, ifd->bInterfaceNumber);
	USETW(req.wLength, len);
	return (usbd_do_request(dev, &req, data));
}

usbd_status
usbd_set_report_async(iface, type, id, data, len)
	usbd_interface_handle iface;
	int type;
	int id;
	void *data;
	int len;
{
	usb_interface_descriptor_t *ifd = usbd_get_interface_descriptor(iface);
	usbd_device_handle dev;
	usb_device_request_t req;
	usbd_status err;

	DPRINTFN(4, ("usbd_set_report_async: len=%d\n", len));
	if (ifd == NULL)
		return (USBD_IOERROR);
	err = usbd_interface2device_handle(iface, &dev);
	if (err)
		return (err);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_REPORT;
	USETW2(req.wValue, type, id);
	USETW(req.wIndex, ifd->bInterfaceNumber);
	USETW(req.wLength, len);
	return (usbd_do_request_async(dev, &req, data));
}

usbd_status
usbd_get_report(iface, type, id, data, len)
	usbd_interface_handle iface;
	int type;
	int id;
	void *data;
	int len;
{
	usb_interface_descriptor_t *ifd = usbd_get_interface_descriptor(iface);
	usbd_device_handle dev;
	usb_device_request_t req;
	usbd_status err;

	DPRINTFN(4, ("usbd_set_report: len=%d\n", len));
	if (id == NULL)
		return (USBD_IOERROR);
	err = usbd_interface2device_handle(iface, &dev);
	if (err)
		return (err);
	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_REPORT;
	USETW2(req.wValue, type, id);
	USETW(req.wIndex, ifd->bInterfaceNumber);
	USETW(req.wLength, len);
	return (usbd_do_request(dev, &req, data));
}

usbd_status
usbd_set_idle(iface, duration, id)
	usbd_interface_handle iface;
	int duration;
	int id;
{
	usb_interface_descriptor_t *ifd = usbd_get_interface_descriptor(iface);
	usbd_device_handle dev;
	usb_device_request_t req;
	usbd_status err;

	DPRINTFN(4, ("usbd_set_idle: %d %d\n", duration, id));
	if (ifd == NULL)
		return (USBD_IOERROR);
	err = usbd_interface2device_handle(iface, &dev);
	if (err)
		return (err);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_IDLE;
	USETW2(req.wValue, duration, id);
	USETW(req.wIndex, ifd->bInterfaceNumber);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_get_report_descriptor(dev, ifcno, repid, size, d)
	usbd_device_handle dev;
	int ifcno;
	int repid;
	int size;
	void *d;
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_INTERFACE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, UDESC_REPORT, repid);
	USETW(req.wIndex, ifcno);
	USETW(req.wLength, size);
	return (usbd_do_request(dev, &req, d));
}

usb_hid_descriptor_t *
usbd_get_hid_descriptor(ifc)
	usbd_interface_handle ifc;
{
	usb_interface_descriptor_t *idesc = usbd_get_interface_descriptor(ifc);
	usbd_device_handle dev;
	usb_config_descriptor_t *cdesc;
	usb_hid_descriptor_t *hd;
	char *p, *end;
	usbd_status err;

	if (idesc == NULL)
		return (0);
	err = usbd_interface2device_handle(ifc, &dev);
	if (err)
		return (0);
	cdesc = usbd_get_config_descriptor(dev);

	p = (char *)idesc + idesc->bLength;
	end = (char *)cdesc + UGETW(cdesc->wTotalLength);

	for (; p < end; p += hd->bLength) {
		hd = (usb_hid_descriptor_t *)p;
		if (p + hd->bLength <= end && hd->bDescriptorType == UDESC_HID)
			return (hd);
		if (hd->bDescriptorType == UDESC_INTERFACE)
			break;
	}
	return (0);
}

usbd_status
usbd_alloc_report_desc(ifc, descp, sizep, mem)
	usbd_interface_handle ifc;
	void **descp;
	int *sizep;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int mem;
#elif defined(__FreeBSD__)
	struct malloc_type *mem;
#endif
	
{
	usb_interface_descriptor_t *id;
	usb_hid_descriptor_t *hid;
	usbd_device_handle dev;
	usbd_status err;

	err = usbd_interface2device_handle(ifc, &dev);
	if (err)
		return (err);
	id = usbd_get_interface_descriptor(ifc);
	if (id == NULL)
		return (USBD_INVAL);
	hid = usbd_get_hid_descriptor(ifc);
	if (hid == NULL)
		return (USBD_IOERROR);
	*sizep = UGETW(hid->descrs[0].wDescriptorLength);
	*descp = malloc(*sizep, mem, M_NOWAIT);
	if (*descp == NULL)
		return (USBD_NOMEM);
	/* XXX should not use 0 Report ID */
	err = usbd_get_report_descriptor(dev, id->bInterfaceNumber, 0, 
				       *sizep, *descp);
	if (err) {
		free(*descp, mem);
		*descp = NULL;
		return (err);
	}
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_get_config(dev, conf)
	usbd_device_handle dev;
	u_int8_t *conf;
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_CONFIG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 1);
	return (usbd_do_request(dev, &req, conf));
}

Static void usbd_bulk_transfer_cb(usbd_xfer_handle xfer, 
		usbd_private_handle priv, usbd_status status);
Static void
usbd_bulk_transfer_cb(xfer, priv, status)
	usbd_xfer_handle xfer;
	usbd_private_handle priv;
	usbd_status status;
{
	wakeup(xfer);
}

usbd_status
usbd_bulk_transfer(xfer, pipe, flags, timeout, buf, size, lbl)
	usbd_xfer_handle xfer;
	usbd_pipe_handle pipe;
	u_int16_t flags;
	u_int32_t timeout;
	void *buf;
	u_int32_t *size;
	char *lbl;
{
	usbd_status err;
	int s, error;

	usbd_setup_xfer(xfer, pipe, 0, buf, *size,
			flags, timeout, usbd_bulk_transfer_cb);
	DPRINTFN(1, ("usbd_bulk_transfer: start transfer %d bytes\n", *size));
	s = splusb();		/* don't want callback until tsleep() */
	err = usbd_transfer(xfer);
	if (err != USBD_IN_PROGRESS) {
		splx(s);
		return (err);
	}
	error = tsleep((caddr_t)xfer, PZERO | PCATCH, lbl, 0);
	splx(s);
	if (error) {
		DPRINTF(("usbd_bulk_transfer: tsleep=%d\n", error));
		usbd_abort_pipe(pipe);
		return (USBD_INTERRUPTED);
	}
	usbd_get_xfer_status(xfer, 0, 0, size, &err);
	DPRINTFN(1,("usbd_bulk_transfer: transferred %d\n", *size));
	if (err) {
		DPRINTF(("usbd_bulk_transfer: error=%d\n", err));
		usbd_clear_endpoint_stall(pipe);
	}
	return (err);
}

void
usb_detach_wait(dv)
	device_ptr_t dv;
{
	DPRINTF(("usb_detach_wait: waiting for %s\n", USBDEVPTRNAME(dv)));
	if (tsleep(dv, PZERO, "usbdet", hz * 60))
		printf("usb_detach_wait: %s didn't detach\n",
		        USBDEVPTRNAME(dv));
	DPRINTF(("usb_detach_wait: %s done\n", USBDEVPTRNAME(dv)));
}       

void
usb_detach_wakeup(dv)
	device_ptr_t dv;
{
	DPRINTF(("usb_detach_wakeup: for %s\n", USBDEVPTRNAME(dv)));
	wakeup(dv);
}       
