/*	$NetBSD: usb_subr.c,v 1.27 1999/01/08 11:58:25 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
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
#if defined(__NetBSD__)
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#endif
#include <sys/proc.h>
#include <sys/select.h>

#include <dev/usb/usb.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#if defined(__FreeBSD__)
#include <machine/clock.h>
#define delay(d)         DELAY(d)
#endif

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) printf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) printf x
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

static usbd_status	usbd_set_config __P((usbd_device_handle, int));
char *usbd_get_string __P((usbd_device_handle, int, char *));
int usbd_getnewaddr __P((usbd_bus_handle bus));
#if defined(__NetBSD__)
int usbd_print __P((void *aux, const char *pnp));
int usbd_submatch __P((bdevice *, struct cfdata *cf, void *));
#endif
void usbd_free_iface_data __P((usbd_device_handle dev, int ifcno));
void usbd_kill_pipe __P((usbd_pipe_handle));
usbd_status usbd_probe_and_attach 
	__P((bdevice *parent, usbd_device_handle dev, int port, int addr));


#ifdef USBVERBOSE
typedef u_int16_t usb_vendor_id_t;
typedef u_int16_t usb_product_id_t;

/*
 * Descriptions of of known vendors and devices ("products").
 */
struct usb_knowndev {
	usb_vendor_id_t		vendor;
	usb_product_id_t	product;
	int			flags;
	char			*vendorname, *productname;
};
#define	USB_KNOWNDEV_NOPROD	0x01		/* match on vendor only */

#include <dev/usb/usbdevs_data.h>
#endif /* USBVERBOSE */

#ifdef USB_DEBUG
char *usbd_error_strs[] = {
	"NORMAL_COMPLETION",
	"IN_PROGRESS",
	"PENDING_REQUESTS",
	"NOT_STARTED",
	"INVAL",
	"IS_IDLE",
	"NOMEM",
	"CANCELLED",
	"BAD_ADDRESS",
	"IN_USE",
	"INTERFACE_NOT_ACTIVE",
	"NO_ADDR",
	"SET_ADDR_FAILED",
	"NO_POWER",
	"TOO_DEEP",
	"IOERROR",
	"NOT_CONFIGURED",
	"TIMEOUT",
	"SHORT_XFER",
	"STALLED",
	"XXX",
};
#endif

usbd_status
usbd_get_string_desc(dev, sindex, langid, sdesc)
	usbd_device_handle dev;
	int sindex;
	int langid;
	usb_string_descriptor_t *sdesc;
{
	usb_device_request_t req;
	usbd_status r;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, UDESC_STRING, sindex);
	USETW(req.wIndex, langid);
	USETW(req.wLength, 1);	/* only size byte first */
	r = usbd_do_request(dev, &req, sdesc);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	USETW(req.wLength, sdesc->bLength);	/* the whole string */
	return (usbd_do_request(dev, &req, sdesc));
}

char *
usbd_get_string(dev, si, buf)
	usbd_device_handle dev;
	int si;
	char *buf;
{
	int swap = dev->quirks->uq_flags & UQ_SWAP_UNICODE;
	usb_string_descriptor_t us;
	char *s;
	int i, n;
	u_int16_t c;
	usbd_status r;

	if (si == 0)
		return (0);
	if (dev->quirks->uq_flags & UQ_NO_STRINGS)
		return (0);
	if (dev->langid == USBD_NOLANG) {
		/* Set up default language */
		r = usbd_get_string_desc(dev, USB_LANGUAGE_TABLE, 0, &us);
		if (r != USBD_NORMAL_COMPLETION || us.bLength < 4) {
			dev->langid = 0; /* Well, just pick English then */
		} else {
			/* Pick the first language as the default. */
			dev->langid = UGETW(us.bString[0]);
		}
	}
	r = usbd_get_string_desc(dev, si, dev->langid, &us);
	if (r != USBD_NORMAL_COMPLETION)
		return (0);
	s = buf;
	n = us.bLength / 2 - 1;
	for (i = 0; i < n; i++) {
		c = UGETW(us.bString[i]);
		/* Convert from Unicode, handle buggy strings. */
		if ((c & 0xff00) == 0)
			*s++ = c;
		else if ((c & 0x00ff) == 0 && swap)
			*s++ = c >> 8;
		else 
			*s++ = '?';
	}
	*s++ = 0;
	return buf;
}

void
usbd_devinfo_vp(dev, v, p)
	usbd_device_handle dev;
	char *v, *p;
{
	usb_device_descriptor_t *udd = &dev->ddesc;
	char *vendor = 0, *product = 0;
#ifdef USBVERBOSE
	struct usb_knowndev *kdp;
#endif

	vendor = usbd_get_string(dev, udd->iManufacturer, v);
	product = usbd_get_string(dev, udd->iProduct, p);
#ifdef USBVERBOSE
	if (!vendor) {
		for(kdp = usb_knowndevs;
		    kdp->vendorname != NULL;
		    kdp++) {
			if (kdp->vendor == UGETW(udd->idVendor) && 
			    (kdp->product == UGETW(udd->idProduct) ||
			     (kdp->flags & USB_KNOWNDEV_NOPROD) != 0))
				break;
		}
		if (kdp->vendorname == NULL)
			vendor = product = NULL;
		else {
			vendor = kdp->vendorname;
			product = (kdp->flags & USB_KNOWNDEV_NOPROD) == 0 ?
				kdp->productname : NULL;
		}
	}
#endif
	if (vendor)
		strcpy(v, vendor);
	else
		sprintf(v, "vendor 0x%04x", UGETW(udd->idVendor));
	if (product)
		strcpy(p, product);
	else
		sprintf(p, "product 0x%04x", UGETW(udd->idProduct));
}

int
usbd_printBCD(cp, bcd)
	char *cp;
	int bcd;
{
	return (sprintf(cp, "%x.%02x", bcd >> 8, bcd & 0xff));
}

void
usbd_devinfo(dev, showclass, cp)
	usbd_device_handle dev;
	int showclass;
	char *cp;
{
	usb_device_descriptor_t *udd = &dev->ddesc;
	char vendor[USB_MAX_STRING_LEN];
	char product[USB_MAX_STRING_LEN];
	int bcdDevice, bcdUSB;

	usbd_devinfo_vp(dev, vendor, product);
	cp += sprintf(cp, "%s %s", vendor, product);
	if (showclass)
		cp += sprintf(cp, ", class %d/%d",
			      udd->bDeviceClass, udd->bDeviceSubClass);
	bcdUSB = UGETW(udd->bcdUSB);
	bcdDevice = UGETW(udd->bcdDevice);
	cp += sprintf(cp, ", rev ");
	cp += usbd_printBCD(cp, bcdUSB);
	*cp++ = '/';
	cp += usbd_printBCD(cp, bcdDevice);
	cp += sprintf(cp, ", addr %d", dev->address);
	*cp = 0;
}

/* Delay for a certain number of ms */
void
usb_delay_ms(bus, ms)
	usbd_bus_handle bus;
	u_int ms;
{
	/* Wait at least two clock ticks so we know the time has passed. */
	if (bus->use_polling)
		delay((ms+1) * 1000);
	else
		tsleep(&ms, PRIBIO, "usbdly", (ms*hz+999)/1000 + 1);
}

/* Delay given a device handle. */
void
usbd_delay_ms(dev, ms)
	usbd_device_handle dev;
	u_int ms;
{
	usb_delay_ms(dev->bus, ms);
}

usbd_status
usbd_reset_port(dev, port, ps)
	usbd_device_handle dev;
	int port;
	usb_port_status_t *ps;
{
	usb_device_request_t req;
	usbd_status r;
	int n;
	
	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, UHF_PORT_RESET);
	USETW(req.wIndex, port);
	USETW(req.wLength, 0);
	r = usbd_do_request(dev, &req, 0);
	DPRINTFN(1,("usbd_reset_port: port %d reset done, error=%d(%s)\n",
		    port, r, usbd_error_strs[r]));
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	n = 10;
	do {
		/* Wait for device to recover from reset. */
		usbd_delay_ms(dev, USB_PORT_RESET_DELAY);
		r = usbd_get_port_status(dev, port, ps);
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTF(("usbd_reset_port: get status failed %d\n",r));
			return (r);
		}
	} while ((UGETW(ps->wPortChange) & UPS_C_PORT_RESET) == 0 && --n > 0);
	if (n == 0) {
		printf("usbd_reset_port: timeout\n");
		return (USBD_IOERROR);
	}
	r = usbd_clear_port_feature(dev, port, UHF_C_PORT_RESET);
#ifdef USB_DEBUG
	if (r != USBD_NORMAL_COMPLETION)
		DPRINTF(("usbd_reset_port: clear port feature failed %d\n",r));
#endif

	/* Wait for the device to recover from reset. */
	usbd_delay_ms(dev, USB_PORT_RESET_RECOVERY);
	return (r);
}

usb_interface_descriptor_t *
usbd_find_idesc(cd, ifaceidx, altidx)
	usb_config_descriptor_t *cd;
	int ifaceidx;
	int altidx;
{
	char *p = (char *)cd;
	char *end = p + UGETW(cd->wTotalLength);
	usb_interface_descriptor_t *d;
	int curidx, lastidx, curaidx = 0;

	for (curidx = lastidx = -1; p < end; ) {
		d = (usb_interface_descriptor_t *)p;
		DPRINTFN(4,("usbd_find_idesc: idx=%d(%d) altidx=%d(%d) len=%d "
			    "type=%d\n", 
			    ifaceidx, curidx, altidx, curaidx,
			    d->bLength, d->bDescriptorType));
		if (d->bLength == 0) /* bad descriptor */
			break;
		p += d->bLength;
		if (p <= end && d->bDescriptorType == UDESC_INTERFACE) {
			if (d->bInterfaceNumber != lastidx) {
				lastidx = d->bInterfaceNumber;
				curidx++;
				curaidx = 0;
			} else
				curaidx++;
			if (ifaceidx == curidx && altidx == curaidx)
				return (d);
		}
	}
	return (0);
}

usb_endpoint_descriptor_t *
usbd_find_edesc(cd, ifaceidx, altidx, endptidx)
	usb_config_descriptor_t *cd;
	int ifaceidx;
	int altidx;
	int endptidx;
{
	char *p = (char *)cd;
	char *end = p + UGETW(cd->wTotalLength);
	usb_interface_descriptor_t *d;
	usb_endpoint_descriptor_t *e;
	int curidx;

	d = usbd_find_idesc(cd, ifaceidx, altidx);
	if (!d)
		return (0);
	if (endptidx >= d->bNumEndpoints) /* quick exit */
		return (0);

	curidx = -1;
	for (p = (char *)d + d->bLength; p < end; ) {
		e = (usb_endpoint_descriptor_t *)p;
		if (e->bLength == 0) /* bad descriptor */
			break;
		p += e->bLength;
		if (p <= end && e->bDescriptorType == UDESC_INTERFACE)
			return (0);
		if (p <= end && e->bDescriptorType == UDESC_ENDPOINT) {
			curidx++;
			if (curidx == endptidx)
				return (e);
		}
	}
	return (0);
}

usbd_status
usbd_fill_iface_data(dev, ifaceidx, altidx)
	usbd_device_handle dev;
	int ifaceidx;
	int altidx;
{
	usbd_interface_handle ifc = &dev->ifaces[ifaceidx];
	char *p, *end;
	int endpt, nendpt;

	DPRINTFN(4,("usbd_fill_iface_data: ifaceidx=%d altidx=%d\n",
		    ifaceidx, altidx));
	ifc->device = dev;
	ifc->idesc = usbd_find_idesc(dev->cdesc, ifaceidx, altidx);
	if (ifc->idesc == 0)
		return (USBD_INVAL);
	ifc->index = ifaceidx;
	ifc->altindex = altidx;
	nendpt = ifc->idesc->bNumEndpoints;
	DPRINTFN(10,("usbd_fill_iface_data: found idesc n=%d\n", nendpt));
	if (nendpt != 0) {
		ifc->endpoints = malloc(nendpt * sizeof(struct usbd_endpoint),
					M_USB, M_NOWAIT);
		if (ifc->endpoints == 0)
			return (USBD_NOMEM);
	} else
		ifc->endpoints = 0;
	ifc->priv = 0;
	p = (char *)ifc->idesc + ifc->idesc->bLength;
	end = (char *)dev->cdesc + UGETW(dev->cdesc->wTotalLength);
#define ed ((usb_endpoint_descriptor_t *)p)
	for (endpt = 0; endpt < nendpt; endpt++) {
		DPRINTFN(10,("usbd_fill_iface_data: endpt=%d\n", endpt));
		for (; p < end; p += ed->bLength) {
			ed = (usb_endpoint_descriptor_t *)p;
			DPRINTFN(10,("usbd_fill_iface_data: p=%p end=%p "
				     "len=%d type=%d\n",
				 p, end, ed->bLength, ed->bDescriptorType));
			if (p + ed->bLength <= end && ed->bLength != 0 &&
			    ed->bDescriptorType == UDESC_ENDPOINT)
				goto found;
			if (ed->bDescriptorType == UDESC_INTERFACE ||
			    ed->bLength == 0)
				break;
		}
		/* passed end, or bad desc */
		goto bad;
	found:
		ifc->endpoints[endpt].edesc = ed;
		ifc->endpoints[endpt].state = USBD_ENDPOINT_ACTIVE;
		ifc->endpoints[endpt].refcnt = 0;
		ifc->endpoints[endpt].toggle = 0;
		p += ed->bLength;
	}
#undef ed
	LIST_INIT(&ifc->pipes);
	ifc->state = USBD_INTERFACE_ACTIVE;
	return (USBD_NORMAL_COMPLETION);

 bad:
	free(ifc->endpoints, M_USB);
	return (USBD_INVAL);
}

void
usbd_free_iface_data(dev, ifcno)
	usbd_device_handle dev;
	int ifcno;
{
	usbd_interface_handle ifc = &dev->ifaces[ifcno];
	if (ifc->endpoints)
		free(ifc->endpoints, M_USB);
}

static usbd_status
usbd_set_config(dev, conf)
	usbd_device_handle dev;
	int conf;
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_CONFIG;
	USETW(req.wValue, conf);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_set_config_no(dev, no, msg)
	usbd_device_handle dev;
	int no;
	int msg;
{
	int index;
	usb_config_descriptor_t cd;
	usbd_status r;

	DPRINTFN(5,("usbd_set_config_no: %d\n", no));
	/* Figure out what config index to use. */
	for (index = 0; index < dev->ddesc.bNumConfigurations; index++) {
		r = usbd_get_config_desc(dev, index, &cd);
		if (r != USBD_NORMAL_COMPLETION)
			return (r);
		if (cd.bConfigurationValue == no)
			return (usbd_set_config_index(dev, index, msg));
	}
	return (USBD_INVAL);
}

usbd_status
usbd_set_config_index(dev, index, msg)
	usbd_device_handle dev;
	int index;
	int msg;
{
	usb_status_t ds;
	usb_hub_status_t hs;
	usb_config_descriptor_t cd, *cdp;
	usbd_status r;
	int ifcidx, nifc, len, selfpowered, power;

	DPRINTFN(5,("usbd_set_config_index: dev=%p index=%d\n", dev, index));

	/* XXX check that all interfaces are idle */
	if (dev->config != 0) {
		DPRINTF(("usbd_set_config_index: free old config\n"));
		/* Free all configuration data structures. */
		nifc = dev->cdesc->bNumInterface;
		for (ifcidx = 0; ifcidx < nifc; ifcidx++)
			usbd_free_iface_data(dev, ifcidx);
		free(dev->ifaces, M_USB);
		free(dev->cdesc, M_USB);
		dev->ifaces = 0;
		dev->cdesc = 0;
		dev->config = 0;
		dev->state = USBD_DEVICE_ADDRESSED;
	}

	/* Figure out what config number to use. */
	r = usbd_get_config_desc(dev, index, &cd);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	len = UGETW(cd.wTotalLength);
	cdp = malloc(len, M_USB, M_NOWAIT);
	if (cdp == 0)
		return (USBD_NOMEM);
	r = usbd_get_desc(dev, UDESC_CONFIG, index, len, cdp);
	if (r != USBD_NORMAL_COMPLETION)
		goto bad;
	if (cdp->bDescriptorType != UDESC_CONFIG) {
		DPRINTFN(-1,("usbd_set_config_index: bad desc %d\n",
			     cdp->bDescriptorType));
		r = USBD_INVAL;
		goto bad;
	}
	selfpowered = 0;
	if (cdp->bmAttributes & UC_SELF_POWERED) {
		/* May be self powered. */
		if (cdp->bmAttributes & UC_BUS_POWERED) {
			/* Must ask device. */
			if (dev->quirks->uq_flags & UQ_HUB_POWER) {
				/* Buggy hub, use hub descriptor. */
				r = usbd_get_hub_status(dev, &hs);
				if (r == USBD_NORMAL_COMPLETION && 
				    !(UGETW(hs.wHubStatus) & UHS_LOCAL_POWER))
					selfpowered = 1;
			} else {
				r = usbd_get_device_status(dev, &ds);
				if (r == USBD_NORMAL_COMPLETION && 
				    (UGETW(ds.wStatus) & UDS_SELF_POWERED))
					selfpowered = 1;
			}
			DPRINTF(("usbd_set_config_index: status=0x%04x, "
				 "error=%d(%s)\n",
				 UGETW(ds.wStatus), r, usbd_error_strs[r]));
		} else
			selfpowered = 1;
	}
	DPRINTF(("usbd_set_config_index: (addr %d) attr=0x%02x, "
		 "selfpowered=%d, power=%d, powerquirk=%x\n", 
		 dev->address, cdp->bmAttributes, 
		 selfpowered, cdp->bMaxPower * 2,
		 dev->quirks->uq_flags & UQ_HUB_POWER));
#ifdef USB_DEBUG
	if (!dev->powersrc) {
		printf("usbd_set_config_index: No power source?\n");
		return (USBD_IOERROR);
	}
#endif
	power = cdp->bMaxPower * 2;
	if (power > dev->powersrc->power) {
		/* XXX print nicer message. */
		if (msg)
			printf("%s: device addr %d (config %d) exceeds power "
				 "budget, %d mA > %d mA\n",
			       USBDEVNAME(dev->bus->bdev), dev->address, 
			       cdp->bConfigurationValue, 
			       power, dev->powersrc->power);
		r = USBD_NO_POWER;
		goto bad;
	}
	dev->power = power;
	dev->self_powered = selfpowered;

	DPRINTF(("usbd_set_config_index: set config %d\n",
		 cdp->bConfigurationValue));
	r = usbd_set_config(dev, cdp->bConfigurationValue);
	if (r != USBD_NORMAL_COMPLETION) {
		DPRINTF(("usbd_set_config_index: setting config=%d failed, "
			 "error=%d(%s)\n",
			 cdp->bConfigurationValue, r, usbd_error_strs[r]));
		goto bad;
	}
	DPRINTF(("usbd_set_config_index: setting new config %d\n",
		 cdp->bConfigurationValue));
	nifc = cdp->bNumInterface;
	dev->ifaces = malloc(nifc * sizeof(struct usbd_interface), 
			     M_USB, M_NOWAIT);
	if (dev->ifaces == 0) {
		r = USBD_NOMEM;
		goto bad;
	}
	DPRINTFN(5,("usbd_set_config_index: dev=%p cdesc=%p\n", dev, cdp));
	dev->cdesc = cdp;
	dev->config = cdp->bConfigurationValue;
	dev->state = USBD_DEVICE_CONFIGURED;
	for (ifcidx = 0; ifcidx < nifc; ifcidx++) {
		r = usbd_fill_iface_data(dev, ifcidx, 0);
		if (r != USBD_NORMAL_COMPLETION) {
			while (--ifcidx >= 0)
				usbd_free_iface_data(dev, ifcidx);
			goto bad;
		}
	}

	return (USBD_NORMAL_COMPLETION);

 bad:
	free(cdp, M_USB);
	return (r);
}

/* XXX add function for alternate settings */

usbd_status
usbd_setup_pipe(dev, iface, ep, pipe)
	usbd_device_handle dev;
	usbd_interface_handle iface; 
	struct usbd_endpoint *ep;
	usbd_pipe_handle *pipe;
{
	usbd_pipe_handle p;
	usbd_status r;

	DPRINTFN(1,("usbd_setup_pipe: dev=%p iface=%p ep=%p pipe=%p\n",
		    dev, iface, ep, pipe));
	p = malloc(dev->bus->pipe_size, M_USB, M_NOWAIT);
	if (p == 0)
		return (USBD_NOMEM);
	p->device = dev;
	p->iface = iface;
	p->state = USBD_PIPE_ACTIVE;
	p->endpoint = ep;
	ep->refcnt++;
	p->refcnt = 1;
	p->intrreqh = 0;
	p->running = 0;
	SIMPLEQ_INIT(&p->queue);
	r = dev->bus->open_pipe(p);
	if (r != USBD_NORMAL_COMPLETION) {
		DPRINTFN(-1,("usbd_setup_pipe: endpoint=0x%x failed, error=%d"
			 "(%s)\n",
			 ep->edesc->bEndpointAddress, r, usbd_error_strs[r]));
		free(p, M_USB);
		return (r);
	}
	*pipe = p;
	return (USBD_NORMAL_COMPLETION);
}

/* Abort the device control pipe. */
void
usbd_kill_pipe(pipe)
	usbd_pipe_handle pipe;
{
	pipe->methods->close(pipe);
	pipe->endpoint->refcnt--;
	free(pipe, M_USB);
}

int
usbd_getnewaddr(bus)
	usbd_bus_handle bus;
{
	int addr;

	for (addr = 1; addr < USB_MAX_DEVICES; addr++)
		if (bus->devices[addr] == 0)
			return (addr);
	return (-1);
}


usbd_status
usbd_probe_and_attach(parent, dev, port, addr)
	bdevice *parent;
	usbd_device_handle dev;
	int port;
	int addr;
{
	struct usb_attach_arg uaa;
	usb_device_descriptor_t *dd = &dev->ddesc;
#if defined(__NetBSD__)
	int found = 0;
#endif
	int r, i, confi, nifaces;
	usbd_interface_handle ifaces[256]; /* 256 is the absolute max */

#if defined(__FreeBSD__)
/* XXX uaa is a static var. Not a problem as it _should_ be used only
 * during probe and attach. Should be changed however
 */
	bdevice bdev;
	bdev = device_add_child(*parent, NULL, -1, &uaa);
	if (!bdev) {
	    printf("%s: Device creation failed\n", USBDEVNAME(dev->bus->bdev));
	    return (USBD_INVAL);
	}
#endif

	uaa.device = dev;
	uaa.iface = 0;
	uaa.ifaces = 0;
	uaa.nifaces = 0;
	uaa.usegeneric = 0;
	uaa.port = port;
	uaa.configno = UHUB_UNK_CONFIGURATION;
	uaa.ifaceno = UHUB_UNK_INTERFACE;

	/* First try with device specific drivers. */
	if (USB_DO_ATTACH(dev, bdev, parent, &uaa, usbd_print, usbd_submatch))
		return (USBD_NORMAL_COMPLETION);

	DPRINTF(("usbd_probe_and_attach: no device specific driver found\n"));

	/* Next try with interface drivers. */
	for (confi = 0; confi < dd->bNumConfigurations; confi++) {
		DPRINTFN(1,("usbd_probe_and_attach: trying config idx=%d\n",
			    confi));
		r = usbd_set_config_index(dev, confi, 1);
		if (r != USBD_NORMAL_COMPLETION) {
#ifdef USB_DEBUG
			DPRINTF(("%s: port %d, set config at addr %d failed, "
				 "error=%d(%s)\n", USBDEVNAME(*parent), port,
				 addr, r, usbd_error_strs[r]));
#else
			printf("%s: port %d, set config at addr %d failed\n",
			       USBDEVNAME(*parent), port, addr);
#endif
#if defined(__FreeBSD__)
			device_delete_child(*parent, bdev);
#endif
			return (r);
		}
		nifaces = dev->cdesc->bNumInterface;
		uaa.configno = dev->cdesc->bConfigurationValue;
		for (i = 0; i < nifaces; i++)
			ifaces[i] = &dev->ifaces[i];
		uaa.ifaces = ifaces;
		uaa.nifaces = nifaces;
		for (i = 0; i < nifaces; i++) {
			if (!ifaces[i])
				continue; /* interface already claimed */
			uaa.iface = ifaces[i];
			uaa.ifaceno = ifaces[i]->idesc->bInterfaceNumber;
			if (USB_DO_ATTACH(dev, bdev, parent, &uaa, usbd_print, 
					  usbd_submatch)) {
#if defined(__NetBSD__)
				found++;
				ifaces[i] = 0; /* consumed */
#elif defined(__FreeBSD__)
				/* XXX FreeBSD can't handle multiple intfaces
				 *     on 1 device yet */
				return (USBD_NORMAL_COMPLETION);
#endif
			}
		}
#if defined(__NetBSD__)
		if (found != 0)
			return (USBD_NORMAL_COMPLETION);
#endif
	}
	/* No interfaces were attached in any of the configurations. */
	if (dd->bNumConfigurations > 1)/* don't change if only 1 config */
		usbd_set_config_index(dev, 0, 0);

	DPRINTF(("usbd_probe_and_attach: no interface drivers found\n"));

	/* Finally try the generic driver. */
	uaa.iface = 0;
	uaa.usegeneric = 1;
	uaa.configno = UHUB_UNK_CONFIGURATION;
	uaa.ifaceno = UHUB_UNK_INTERFACE;
	if (USB_DO_ATTACH(dev, bdev, parent, &uaa, usbd_print, usbd_submatch))
		return (USBD_NORMAL_COMPLETION);

	/* 
	 * The generic attach failed, but leave the device as it is.
	 * We just did not find any drivers, that's all.  The device is
	 * fully operational and not harming anyone.
	 */
	DPRINTF(("usbd_probe_and_attach: generic attach failed\n"));
#if defined(__FreeBSD__)
/*
 * XXX should we delete the child again? Left for now to avoid dangling
 * references.
	device_delete_child(*parent, bdev);
*/
#endif
	return (USBD_NORMAL_COMPLETION);
}



/*
 * Called when a new device has been put in the powered state,
 * but not yet in the addressed state.
 * Get initial descriptor, set the address, get full descriptor,
 * and attach a driver.
 */
usbd_status
usbd_new_device(parent, bus, depth, lowspeed, port, up)
	bdevice *parent;
	usbd_bus_handle bus;
	int depth;
	int lowspeed;
	int port;
	struct usbd_port *up;
{
	usbd_device_handle dev;
	usb_device_descriptor_t *dd;
	usbd_status r;
	int addr;
	int i;

	DPRINTF(("usbd_new_device bus=%p depth=%d lowspeed=%d\n",
		 bus, depth, lowspeed));
	addr = usbd_getnewaddr(bus);
	if (addr < 0) {
		printf("%s: No free USB addresses, new device ignored.\n", 
		       USBDEVNAME(bus->bdev));
		return (USBD_NO_ADDR);
	}

	dev = malloc(sizeof *dev, M_USB, M_NOWAIT);
	if (dev == 0)
		return (USBD_NOMEM);
	memset(dev, 0, sizeof(*dev));

	dev->bus = bus;

	/* Set up default endpoint handle. */
	dev->def_ep.edesc = &dev->def_ep_desc;
	dev->def_ep.state = USBD_ENDPOINT_ACTIVE;

	/* Set up default endpoint descriptor. */
	dev->def_ep_desc.bLength = USB_ENDPOINT_DESCRIPTOR_SIZE;
	dev->def_ep_desc.bDescriptorType = UDESC_ENDPOINT;
	dev->def_ep_desc.bEndpointAddress = USB_CONTROL_ENDPOINT;
	dev->def_ep_desc.bmAttributes = UE_CONTROL;
	USETW(dev->def_ep_desc.wMaxPacketSize, USB_MAX_IPACKET);
	dev->def_ep_desc.bInterval = 0;

	dev->state = USBD_DEVICE_DEFAULT;
	dev->quirks = &usbd_no_quirk;
	dev->address = USB_START_ADDR;
	dev->ddesc.bMaxPacketSize = 0;
	dev->lowspeed = lowspeed != 0;
	dev->depth = depth;
	dev->powersrc = up;
	dev->langid = USBD_NOLANG;

	/* Establish the the default pipe. */
	r = usbd_setup_pipe(dev, 0, &dev->def_ep, &dev->default_pipe);
	if (r != USBD_NORMAL_COMPLETION) {
		usbd_remove_device(dev, up);
		return (r);
	}

	up->device = dev;
	dd = &dev->ddesc;
	/* Try a few times in case the device is slow (i.e. outside specs.) */
	for (i = 0; i < 5; i++) {
		/* Get the first 8 bytes of the device descriptor. */
		r = usbd_get_desc(dev, UDESC_DEVICE, 0, USB_MAX_IPACKET, dd);
		if (r == USBD_NORMAL_COMPLETION)
			break;
		usbd_delay_ms(dev, 200);
	}
	if (r != USBD_NORMAL_COMPLETION) {
		DPRINTFN(-1, ("usbd_new_device: addr=%d, getting first desc "
			      "failed\n",
			      addr));
		usbd_remove_device(dev, up);
		return (r);
	}

	if (dd->bDescriptorType != UDESC_DEVICE) {
		/* Illegal device descriptor */
		DPRINTFN(-1,("usbd_new_device: illegal descriptor %d\n",
			     dd->bDescriptorType));
		usbd_remove_device(dev, up);
		return (USBD_INVAL);
	}

	DPRINTF(("usbd_new_device: adding unit addr=%d, rev=%02x, class=%d, "
		 "subclass=%d, protocol=%d, maxpacket=%d, ls=%d\n", 
		 addr,UGETW(dd->bcdUSB), dd->bDeviceClass, dd->bDeviceSubClass,
		 dd->bDeviceProtocol, dd->bMaxPacketSize, dev->lowspeed));

	USETW(dev->def_ep_desc.wMaxPacketSize, dd->bMaxPacketSize);

	/* Get the full device descriptor. */
	r = usbd_get_device_desc(dev, dd);
	if (r != USBD_NORMAL_COMPLETION) {
		DPRINTFN(-1, ("usbd_new_device: addr=%d, getting full desc "
			      "failed\n", addr));
		usbd_remove_device(dev, up);
		return (r);
	}

	/* Figure out what's wrong with this device. */
	dev->quirks = usbd_find_quirk(dd);

	/* Set the address */
	r = usbd_set_address(dev, addr);
	if (r != USBD_NORMAL_COMPLETION) {
		DPRINTFN(-1,("usb_new_device: set address %d failed\n",addr));
		r = USBD_SET_ADDR_FAILED;
		usbd_remove_device(dev, up);
		return (r);
	}
	/* Allow device time to set new address */
	usbd_delay_ms(dev, USB_SET_ADDRESS_SETTLE);

	dev->address = addr;	/* New device address now */
	dev->state = USBD_DEVICE_ADDRESSED;
	bus->devices[addr] = dev;

	/* Assume 100mA bus powered for now. Changed when configured. */
	dev->power = USB_MIN_POWER;
	dev->self_powered = 0;

	DPRINTF(("usbd_new_device: new dev (addr %d), dev=%p, parent=%p\n", 
		 addr, dev, parent));

	r = usbd_probe_and_attach(parent, dev, port, addr);
	if (r != USBD_NORMAL_COMPLETION) {
		usbd_remove_device(dev, up);
		return (r);
  	}
 
  	return (USBD_NORMAL_COMPLETION);
}

void
usbd_remove_device(dev, up)
	usbd_device_handle dev;
	struct usbd_port *up;
{
	DPRINTF(("usbd_remove_device: %p\n", dev));
  
	if (dev->default_pipe)
		usbd_kill_pipe(dev->default_pipe);
	up->device = 0;
	dev->bus->devices[dev->address] = 0;

	free(dev, M_USB);
}

#if defined(__NetBSD__)  
int
usbd_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct usb_attach_arg *uaa = aux;
	char devinfo[1024];

	DPRINTFN(15, ("usbd_print dev=%p\n", uaa->device));
	if (pnp) {
		if (!uaa->usegeneric)
			return (QUIET);
		usbd_devinfo(uaa->device, 1, devinfo);
		printf("%s, %s", devinfo, pnp);
	}
	if (uaa->port != 0)
		printf(" port %d", uaa->port);
	if (uaa->configno != UHUB_UNK_CONFIGURATION)
		printf(" configuration %d", uaa->configno);
	if (uaa->ifaceno != UHUB_UNK_INTERFACE)
		printf(" interface %d", uaa->ifaceno);
	return (UNCONF);
}

int
usbd_submatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct usb_attach_arg *uaa = aux;

	if ((uaa->port != 0 &&
	     cf->uhubcf_port != UHUB_UNK_PORT &&
	     cf->uhubcf_port != uaa->port) ||
	    (uaa->configno != UHUB_UNK_CONFIGURATION &&
	     cf->uhubcf_configuration != UHUB_UNK_CONFIGURATION &&
	     cf->uhubcf_configuration != uaa->configno) ||
	    (uaa->ifaceno != UHUB_UNK_INTERFACE &&
	     cf->uhubcf_interface != UHUB_UNK_INTERFACE &&
	     cf->uhubcf_interface != uaa->ifaceno))
		return 0;
	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}
#endif

usbd_status
usb_insert_transfer(reqh)
	usbd_request_handle reqh;
{
	usbd_pipe_handle pipe = reqh->pipe;
	usbd_interface_handle iface = pipe->iface;

	if (pipe->state == USBD_PIPE_IDLE ||
	    (iface && iface->state == USBD_INTERFACE_IDLE))
		return (USBD_IS_IDLE);
	SIMPLEQ_INSERT_TAIL(&pipe->queue, reqh, next);
	if (pipe->state != USBD_PIPE_ACTIVE ||
	    (iface && iface->state != USBD_INTERFACE_ACTIVE))
		return (USBD_NOT_STARTED);
	if (pipe->running)
		return (USBD_IN_PROGRESS);
	pipe->running = 1;
	return (USBD_NORMAL_COMPLETION);
}

void
usb_start_next(pipe)
	usbd_pipe_handle pipe;
{
	usbd_request_handle reqh;
	usbd_status r;

#ifdef DIAGNOSTIC
	if (SIMPLEQ_FIRST(&pipe->queue) == 0) {
		printf("usb_start_next: empty\n");
		return;
	}
#endif

	/* First remove remove old */
	SIMPLEQ_REMOVE_HEAD(&pipe->queue, SIMPLEQ_FIRST(&pipe->queue), next);
	if (pipe->state != USBD_PIPE_ACTIVE) {
		pipe->running = 0;
		return;
	}
	reqh = SIMPLEQ_FIRST(&pipe->queue);
	DPRINTFN(5, ("usb_start_next: start reqh=%p\n", reqh));
	if (!reqh)
		pipe->running = 0;
	else {
		r = pipe->methods->start(reqh);
		if (r != USBD_IN_PROGRESS) {
			printf("usb_start_next: error=%d\n", r);
			pipe->running = 0;
			/* XXX do what? */
		}
	}
}

void
usbd_fill_deviceinfo(dev, di)
	usbd_device_handle dev;
	struct usb_device_info *di;
{
	struct usbd_port *p;
	int i, r, s;

	di->config = dev->config;
	usbd_devinfo_vp(dev, di->vendor, di->product);
	usbd_printBCD(di->revision, UGETW(dev->ddesc.bcdDevice));
	di->vendorNo = UGETW(dev->ddesc.idVendor);
	di->productNo = UGETW(dev->ddesc.idProduct);
	di->class = dev->ddesc.bDeviceClass;
	di->power = dev->self_powered ? 0 : dev->power;
	di->lowspeed = dev->lowspeed;
	di->addr = dev->address;
	if (dev->hub) {
		for (i = 0; 
		     i < sizeof(di->ports) / sizeof(di->ports[0]) &&
			     i < dev->hub->hubdesc.bNbrPorts;
		     i++) {
			p = &dev->hub->ports[i];
			if (p->device)
				r = p->device->address;
			else {
				s = UGETW(p->status.wPortStatus);
				if (s & UPS_PORT_ENABLED)
					r = USB_PORT_ENABLED;
				else if (s & UPS_SUSPEND)
					r = USB_PORT_SUSPENDED;
				else if (s & UPS_PORT_POWER)
					r = USB_PORT_POWERED;
				else
					r = USB_PORT_DISABLED;
			}
			di->ports[i] = r;
		}
		di->nports = dev->hub->hubdesc.bNbrPorts;
	} else
		di->nports = 0;
}
