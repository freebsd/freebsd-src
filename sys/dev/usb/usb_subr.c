/*	$NetBSD: usb_subr.c,v 1.76 2000/04/27 15:26:50 augustss Exp $	*/
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
#include <sys/device.h>
#include <sys/select.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#endif
#include <sys/proc.h>

#include <machine/bus.h>

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
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

Static usbd_status usbd_set_config(usbd_device_handle, int);
Static void usbd_devinfo_vp(usbd_device_handle, char *, char *, int);
Static char *usbd_get_string(usbd_device_handle, int, char *);
Static int usbd_getnewaddr(usbd_bus_handle bus);
#if defined(__NetBSD__)
Static int usbd_print(void *aux, const char *pnp);
Static int usbd_submatch(device_ptr_t, struct cfdata *cf, void *);
#elif defined(__OpenBSD__)
Static int usbd_print(void *aux, const char *pnp);
Static int usbd_submatch(device_ptr_t, void *, void *);
#endif
Static void usbd_free_iface_data(usbd_device_handle dev, int ifcno);
Static void usbd_kill_pipe(usbd_pipe_handle);
Static usbd_status usbd_probe_and_attach(device_ptr_t parent,
				 usbd_device_handle dev, int port, int addr);

Static u_int32_t usb_cookie_no = 0;

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

Static const char * const usbd_error_strs[] = {
	"NORMAL_COMPLETION",
	"IN_PROGRESS",
	"PENDING_REQUESTS",
	"NOT_STARTED",
	"INVAL",
	"NOMEM",
	"CANCELLED",
	"BAD_ADDRESS",
	"IN_USE",
	"NO_ADDR",
	"SET_ADDR_FAILED",
	"NO_POWER",
	"TOO_DEEP",
	"IOERROR",
	"NOT_CONFIGURED",
	"TIMEOUT",
	"SHORT_XFER",
	"STALLED",
	"INTERRUPTED",
	"XXX",
};

const char *
usbd_errstr(usbd_status err)
{
	static char buffer[5];

	if (err < USBD_ERROR_MAX) {
		return usbd_error_strs[err];
	} else {
		snprintf(buffer, sizeof buffer, "%d", err);
		return buffer;
	}
}

usbd_status
usbd_get_string_desc(usbd_device_handle dev, int sindex, int langid,
		     usb_string_descriptor_t *sdesc)
{
	usb_device_request_t req;
	usbd_status err;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, UDESC_STRING, sindex);
	USETW(req.wIndex, langid);
	USETW(req.wLength, 1);	/* only size byte first */
	err = usbd_do_request(dev, &req, sdesc);
	if (err)
		return (err);
	USETW(req.wLength, sdesc->bLength);	/* the whole string */
	return (usbd_do_request(dev, &req, sdesc));
}

char *
usbd_get_string(usbd_device_handle dev, int si, char *buf)
{
	int swap = dev->quirks->uq_flags & UQ_SWAP_UNICODE;
	usb_string_descriptor_t us;
	char *s;
	int i, n;
	u_int16_t c;
	usbd_status err;

	if (si == 0)
		return (0);
	if (dev->quirks->uq_flags & UQ_NO_STRINGS)
		return (0);
	if (dev->langid == USBD_NOLANG) {
		/* Set up default language */
		err = usbd_get_string_desc(dev, USB_LANGUAGE_TABLE, 0, &us);
		if (err || us.bLength < 4) {
			dev->langid = 0; /* Well, just pick English then */
		} else {
			/* Pick the first language as the default. */
			dev->langid = UGETW(us.bString[0]);
		}
	}
	err = usbd_get_string_desc(dev, si, dev->langid, &us);
	if (err)
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
	return (buf);
}

Static void
usbd_trim_spaces(char *p)
{
	char *q, *e;

	if (p == NULL)
		return;
	q = e = p;
	while (*q == ' ')	/* skip leading spaces */
		q++;
	while ((*p = *q++))	/* copy string */
		if (*p++ != ' ') /* remember last non-space */
			e = p;
	*e = 0;			/* kill trailing spaces */
}

Static void
usbd_devinfo_vp(usbd_device_handle dev, char *v, char *p, int usedev)
{
	usb_device_descriptor_t *udd = &dev->ddesc;
	char *vendor = 0, *product = 0;
#ifdef USBVERBOSE
	const struct usb_knowndev *kdp;
#endif

	if (dev == NULL) {
		v[0] = p[0] = '\0';
		return;
	}

	if (usedev) {
		vendor = usbd_get_string(dev, udd->iManufacturer, v);
		usbd_trim_spaces(vendor);
		product = usbd_get_string(dev, udd->iProduct, p);
		usbd_trim_spaces(product);
	} else {
		vendor = NULL;
		product = NULL;
	}
#ifdef USBVERBOSE
	if (vendor == NULL || product == NULL) {
		for(kdp = usb_knowndevs;
		    kdp->vendorname != NULL;
		    kdp++) {
			if (kdp->vendor == UGETW(udd->idVendor) && 
			    (kdp->product == UGETW(udd->idProduct) ||
			     (kdp->flags & USB_KNOWNDEV_NOPROD) != 0))
				break;
		}
		if (kdp->vendorname != NULL) {
			if (vendor == NULL)
			    vendor = kdp->vendorname;
			if (product == NULL)
			    product = (kdp->flags & USB_KNOWNDEV_NOPROD) == 0 ?
				kdp->productname : NULL;
		}
	}
#endif
	if (vendor != NULL && *vendor)
		strcpy(v, vendor);
	else
		sprintf(v, "vendor 0x%04x", UGETW(udd->idVendor));
	if (product != NULL && *product)
		strcpy(p, product);
	else
		sprintf(p, "product 0x%04x", UGETW(udd->idProduct));
}

int
usbd_printBCD(char *cp, int bcd)
{
	return (sprintf(cp, "%x.%02x", bcd >> 8, bcd & 0xff));
}

void
usbd_devinfo(usbd_device_handle dev, int showclass, char *cp)
{
	usb_device_descriptor_t *udd = &dev->ddesc;
	char vendor[USB_MAX_STRING_LEN];
	char product[USB_MAX_STRING_LEN];
	int bcdDevice, bcdUSB;

	usbd_devinfo_vp(dev, vendor, product, 1);
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
usb_delay_ms(usbd_bus_handle bus, u_int ms)
{
	/* Wait at least two clock ticks so we know the time has passed. */
	if (bus->use_polling || cold)
		delay((ms+1) * 1000);
	else
		tsleep(&ms, PRIBIO, "usbdly", (ms*hz+999)/1000 + 1);
}

/* Delay given a device handle. */
void
usbd_delay_ms(usbd_device_handle dev, u_int ms)
{
	usb_delay_ms(dev->bus, ms);
}

usbd_status
usbd_reset_port(usbd_device_handle dev, int port, usb_port_status_t *ps)
{
	usb_device_request_t req;
	usbd_status err;
	int n;
	
	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, UHF_PORT_RESET);
	USETW(req.wIndex, port);
	USETW(req.wLength, 0);
	err = usbd_do_request(dev, &req, 0);
	DPRINTFN(1,("usbd_reset_port: port %d reset done, error=%s\n",
		    port, usbd_errstr(err)));
	if (err)
		return (err);
	n = 10;
	do {
		/* Wait for device to recover from reset. */
		usbd_delay_ms(dev, USB_PORT_RESET_DELAY);
		err = usbd_get_port_status(dev, port, ps);
		if (err) {
			DPRINTF(("usbd_reset_port: get status failed %d\n",
				 err));
			return (err);
		}
	} while ((UGETW(ps->wPortChange) & UPS_C_PORT_RESET) == 0 && --n > 0);
	if (n == 0)
		return (USBD_TIMEOUT);
	err = usbd_clear_port_feature(dev, port, UHF_C_PORT_RESET);
#ifdef USB_DEBUG
	if (err)
		DPRINTF(("usbd_reset_port: clear port feature failed %d\n",
			 err));
#endif

	/* Wait for the device to recover from reset. */
	usbd_delay_ms(dev, USB_PORT_RESET_RECOVERY);
	return (err);
}

usb_interface_descriptor_t *
usbd_find_idesc(usb_config_descriptor_t *cd, int ifaceidx, int altidx)
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
	return (NULL);
}

usb_endpoint_descriptor_t *
usbd_find_edesc(usb_config_descriptor_t *cd, int ifaceidx, int altidx, 
		int endptidx)
{
	char *p = (char *)cd;
	char *end = p + UGETW(cd->wTotalLength);
	usb_interface_descriptor_t *d;
	usb_endpoint_descriptor_t *e;
	int curidx;

	d = usbd_find_idesc(cd, ifaceidx, altidx);
	if (d == NULL)
		return (NULL);
	if (endptidx >= d->bNumEndpoints) /* quick exit */
		return (NULL);

	curidx = -1;
	for (p = (char *)d + d->bLength; p < end; ) {
		e = (usb_endpoint_descriptor_t *)p;
		if (e->bLength == 0) /* bad descriptor */
			break;
		p += e->bLength;
		if (p <= end && e->bDescriptorType == UDESC_INTERFACE)
			return (NULL);
		if (p <= end && e->bDescriptorType == UDESC_ENDPOINT) {
			curidx++;
			if (curidx == endptidx)
				return (e);
		}
	}
	return (NULL);
}

usbd_status
usbd_fill_iface_data(usbd_device_handle dev, int ifaceidx, int altidx)
{
	usbd_interface_handle ifc = &dev->ifaces[ifaceidx];
	usb_interface_descriptor_t *idesc;
	char *p, *end;
	int endpt, nendpt;

	DPRINTFN(4,("usbd_fill_iface_data: ifaceidx=%d altidx=%d\n",
		    ifaceidx, altidx));
	idesc = usbd_find_idesc(dev->cdesc, ifaceidx, altidx);
	if (idesc == NULL)
		return (USBD_INVAL);
	ifc->device = dev;
	ifc->idesc = idesc;
	ifc->index = ifaceidx;
	ifc->altindex = altidx;
	nendpt = ifc->idesc->bNumEndpoints;
	DPRINTFN(4,("usbd_fill_iface_data: found idesc nendpt=%d\n", nendpt));
	if (nendpt != 0) {
		ifc->endpoints = malloc(nendpt * sizeof(struct usbd_endpoint),
					M_USB, M_NOWAIT);
		if (ifc->endpoints == NULL)
			return (USBD_NOMEM);
	} else
		ifc->endpoints = NULL;
	ifc->priv = NULL;
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
			if (ed->bLength == 0 ||
			    ed->bDescriptorType == UDESC_INTERFACE)
				break;
		}
		/* passed end, or bad desc */
		DPRINTF(("usbd_fill_iface_data: bad descriptor(s): %s\n",
			 ed->bLength == 0 ? "0 length" :
			 ed->bDescriptorType == UDESC_INTERFACE ? "iface desc":
			 "out of data"));
		goto bad;
	found:
		ifc->endpoints[endpt].edesc = ed;
		ifc->endpoints[endpt].refcnt = 0;
		p += ed->bLength;
	}
#undef ed
	LIST_INIT(&ifc->pipes);
	return (USBD_NORMAL_COMPLETION);

 bad:
	if (ifc->endpoints != NULL) {
		free(ifc->endpoints, M_USB);
		ifc->endpoints = NULL;
	}
	return (USBD_INVAL);
}

void
usbd_free_iface_data(usbd_device_handle dev, int ifcno)
{
	usbd_interface_handle ifc = &dev->ifaces[ifcno];
	if (ifc->endpoints)
		free(ifc->endpoints, M_USB);
}

Static usbd_status
usbd_set_config(usbd_device_handle dev, int conf)
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
usbd_set_config_no(usbd_device_handle dev, int no, int msg)
{
	int index;
	usb_config_descriptor_t cd;
	usbd_status err;

	if (no == USB_UNCONFIG_NO)
		return (usbd_set_config_index(dev, USB_UNCONFIG_INDEX, msg));

	DPRINTFN(5,("usbd_set_config_no: %d\n", no));
	/* Figure out what config index to use. */
	for (index = 0; index < dev->ddesc.bNumConfigurations; index++) {
		err = usbd_get_config_desc(dev, index, &cd);
		if (err)
			return (err);
		if (cd.bConfigurationValue == no)
			return (usbd_set_config_index(dev, index, msg));
	}
	return (USBD_INVAL);
}

usbd_status
usbd_set_config_index(usbd_device_handle dev, int index, int msg)
{
	usb_status_t ds;
	usb_config_descriptor_t cd, *cdp;
	usbd_status err;
	int ifcidx, nifc, len, selfpowered, power;

	DPRINTFN(5,("usbd_set_config_index: dev=%p index=%d\n", dev, index));

	/* XXX check that all interfaces are idle */
	if (dev->config != USB_UNCONFIG_NO) {
		DPRINTF(("usbd_set_config_index: free old config\n"));
		/* Free all configuration data structures. */
		nifc = dev->cdesc->bNumInterface;
		for (ifcidx = 0; ifcidx < nifc; ifcidx++)
			usbd_free_iface_data(dev, ifcidx);
		free(dev->ifaces, M_USB);
		free(dev->cdesc, M_USB);
		dev->ifaces = NULL;
		dev->cdesc = NULL;
		dev->config = USB_UNCONFIG_NO;
	}

	if (index == USB_UNCONFIG_INDEX) {
		/* We are unconfiguring the device, so leave unallocated. */
		DPRINTF(("usbd_set_config_index: set config 0\n"));
		err = usbd_set_config(dev, USB_UNCONFIG_NO);
		if (err)
			DPRINTF(("usbd_set_config_index: setting config=0 "
				 "failed, error=%s\n", usbd_errstr(err)));
		return (err);
	}

	/* Get the short descriptor. */
	err = usbd_get_config_desc(dev, index, &cd);
	if (err)
		return (err);
	len = UGETW(cd.wTotalLength);
	cdp = malloc(len, M_USB, M_NOWAIT);
	if (cdp == NULL)
		return (USBD_NOMEM);
	/* Get the full descriptor. */
	err = usbd_get_desc(dev, UDESC_CONFIG, index, len, cdp);
	if (err)
		goto bad;
	if (cdp->bDescriptorType != UDESC_CONFIG) {
		DPRINTFN(-1,("usbd_set_config_index: bad desc %d\n",
			     cdp->bDescriptorType));
		err = USBD_INVAL;
		goto bad;
	}

	/* Figure out if the device is self or bus powered. */
	selfpowered = 0;
	if (!(dev->quirks->uq_flags & UQ_BUS_POWERED) &&
	    (cdp->bmAttributes & UC_SELF_POWERED)) {
		/* May be self powered. */
		if (cdp->bmAttributes & UC_BUS_POWERED) {
			/* Must ask device. */
			err = usbd_get_device_status(dev, &ds);
			if (!err && (UGETW(ds.wStatus) & UDS_SELF_POWERED))
				selfpowered = 1;
			DPRINTF(("usbd_set_config_index: status=0x%04x, "
				 "error=%s\n",
				 UGETW(ds.wStatus), usbd_errstr(err)));
		} else
			selfpowered = 1;
	}
	DPRINTF(("usbd_set_config_index: (addr %d) attr=0x%02x, "
		 "selfpowered=%d, power=%d\n", 
		 dev->address, cdp->bmAttributes, 
		 selfpowered, cdp->bMaxPower * 2));

	/* Check if we have enough power. */
#ifdef USB_DEBUG
	if (dev->powersrc == NULL) {
		DPRINTF(("usbd_set_config_index: No power source?\n"));
		return (USBD_IOERROR);
	}
#endif
	power = cdp->bMaxPower * 2;
	if (power > dev->powersrc->power) {
		DPRINTF(("power exceeded %d %d\n", power,dev->powersrc->power));
		/* XXX print nicer message. */
		if (msg)
			printf("%s: device addr %d (config %d) exceeds power "
				 "budget, %d mA > %d mA\n",
			       USBDEVNAME(dev->bus->bdev), dev->address, 
			       cdp->bConfigurationValue, 
			       power, dev->powersrc->power);
		err = USBD_NO_POWER;
		goto bad;
	}
	dev->power = power;
	dev->self_powered = selfpowered;

	/* Set the actual configuration value. */
	DPRINTF(("usbd_set_config_index: set config %d\n",
		 cdp->bConfigurationValue));
	err = usbd_set_config(dev, cdp->bConfigurationValue);
	if (err) {
		DPRINTF(("usbd_set_config_index: setting config=%d failed, "
			 "error=%s\n",
			 cdp->bConfigurationValue, usbd_errstr(err)));
		goto bad;
	}

	/* Allocate and fill interface data. */
	nifc = cdp->bNumInterface;
	dev->ifaces = malloc(nifc * sizeof(struct usbd_interface), 
			     M_USB, M_NOWAIT);
	if (dev->ifaces == NULL) {
		err = USBD_NOMEM;
		goto bad;
	}
	DPRINTFN(5,("usbd_set_config_index: dev=%p cdesc=%p\n", dev, cdp));
	dev->cdesc = cdp;
	dev->config = cdp->bConfigurationValue;
	for (ifcidx = 0; ifcidx < nifc; ifcidx++) {
		err = usbd_fill_iface_data(dev, ifcidx, 0);
		if (err) {
			while (--ifcidx >= 0)
				usbd_free_iface_data(dev, ifcidx);
			goto bad;
		}
	}

	return (USBD_NORMAL_COMPLETION);

 bad:
	free(cdp, M_USB);
	return (err);
}

/* XXX add function for alternate settings */

usbd_status
usbd_setup_pipe(usbd_device_handle dev, usbd_interface_handle iface,
		struct usbd_endpoint *ep, int ival, usbd_pipe_handle *pipe)
{
	usbd_pipe_handle p;
	usbd_status err;

	DPRINTFN(1,("usbd_setup_pipe: dev=%p iface=%p ep=%p pipe=%p\n",
		    dev, iface, ep, pipe));
	p = malloc(dev->bus->pipe_size, M_USB, M_NOWAIT);
	if (p == NULL)
		return (USBD_NOMEM);
	p->device = dev;
	p->iface = iface;
	p->endpoint = ep;
	ep->refcnt++;
	p->refcnt = 1;
	p->intrxfer = 0;
	p->running = 0;
	p->repeat = 0;
	p->interval = ival;
	SIMPLEQ_INIT(&p->queue);
	err = dev->bus->methods->open_pipe(p);
	if (err) {
		DPRINTFN(-1,("usbd_setup_pipe: endpoint=0x%x failed, error="
			 "%s\n",
			 ep->edesc->bEndpointAddress, usbd_errstr(err)));
		free(p, M_USB);
		return (err);
	}
	/* Clear any stall and make sure DATA0 toggle will be used next. */
	if (UE_GET_ADDR(ep->edesc->bEndpointAddress) != USB_CONTROL_ENDPOINT)
		usbd_clear_endpoint_stall(p);
	*pipe = p;
	return (USBD_NORMAL_COMPLETION);
}

/* Abort the device control pipe. */
void
usbd_kill_pipe(usbd_pipe_handle pipe)
{
	pipe->methods->close(pipe);
	pipe->endpoint->refcnt--;
	free(pipe, M_USB);
}

int
usbd_getnewaddr(usbd_bus_handle bus)
{
	int addr;

	for (addr = 1; addr < USB_MAX_DEVICES; addr++)
		if (bus->devices[addr] == 0)
			return (addr);
	return (-1);
}


usbd_status
usbd_probe_and_attach(device_ptr_t parent, usbd_device_handle dev,
		      int port, int addr)
{
	struct usb_attach_arg uaa;
	usb_device_descriptor_t *dd = &dev->ddesc;
	int found, i, confi, nifaces;
	usbd_status err;
	device_ptr_t dv;
	usbd_interface_handle ifaces[256]; /* 256 is the absolute max */

#if defined(__FreeBSD__)
	/* 
	 * XXX uaa is a static var. Not a problem as it _should_ be used only
	 * during probe and attach. Should be changed however.
	 */
	device_t bdev;
	bdev = device_add_child(parent, NULL, -1);
	device_set_ivars(bdev, &uaa);
	if (!bdev) {
	    printf("%s: Device creation failed\n", USBDEVNAME(dev->bus->bdev));
	    return (USBD_INVAL);
	}
	device_quiet(bdev);
#endif

	uaa.device = dev;
	uaa.iface = NULL;
	uaa.ifaces = NULL;
	uaa.nifaces = 0;
	uaa.usegeneric = 0;
	uaa.port = port;
	uaa.configno = UHUB_UNK_CONFIGURATION;
	uaa.ifaceno = UHUB_UNK_INTERFACE;
	uaa.vendor = UGETW(dd->idVendor);
	uaa.product = UGETW(dd->idProduct);
	uaa.release = UGETW(dd->bcdDevice);

	/* First try with device specific drivers. */
	DPRINTF(("usbd_probe_and_attach: trying device specific drivers\n"));
	dv = USB_DO_ATTACH(dev, bdev, parent, &uaa, usbd_print, usbd_submatch);
	if (dv) {
		dev->subdevs = malloc(2 * sizeof dv, M_USB, M_NOWAIT);
		if (dev->subdevs == NULL)
			return (USBD_NOMEM);
		dev->subdevs[0] = dv;
		dev->subdevs[1] = 0;
		return (USBD_NORMAL_COMPLETION);
	}

	DPRINTF(("usbd_probe_and_attach: no device specific driver found\n"));

	DPRINTF(("usbd_probe_and_attach: looping over %d configurations\n",
		 dd->bNumConfigurations));
	/* Next try with interface drivers. */
	for (confi = 0; confi < dd->bNumConfigurations; confi++) {
		DPRINTFN(1,("usbd_probe_and_attach: trying config idx=%d\n",
			    confi));
		err = usbd_set_config_index(dev, confi, 1);
		if (err) {
#ifdef USB_DEBUG
			DPRINTF(("%s: port %d, set config at addr %d failed, "
				 "error=%s\n", USBDEVPTRNAME(parent), port,
				 addr, usbd_errstr(err)));
#else
			printf("%s: port %d, set config at addr %d failed\n",
			       USBDEVPTRNAME(parent), port, addr);
#endif
#if defined(__FreeBSD__)
			device_delete_child(parent, bdev);
#endif

 			return (err);
		}
		nifaces = dev->cdesc->bNumInterface;
		uaa.configno = dev->cdesc->bConfigurationValue;
		for (i = 0; i < nifaces; i++)
			ifaces[i] = &dev->ifaces[i];
		uaa.ifaces = ifaces;
		uaa.nifaces = nifaces;
		dev->subdevs = malloc((nifaces+1) * sizeof dv, M_USB,M_NOWAIT);
		if (dev->subdevs == NULL) {
#if defined(__FreeBSD__)
			device_delete_child(parent, bdev);
#endif
			return (USBD_NOMEM);
		}

		found = 0;
		for (i = 0; i < nifaces; i++) {
			if (ifaces[i] == NULL)
				continue; /* interface already claimed */
			uaa.iface = ifaces[i];
			uaa.ifaceno = ifaces[i]->idesc->bInterfaceNumber;
			dv = USB_DO_ATTACH(dev, bdev, parent, &uaa, usbd_print,
					   usbd_submatch);
			if (dv != NULL) {
				dev->subdevs[found++] = dv;
				dev->subdevs[found] = 0;
				ifaces[i] = 0; /* consumed */

#if defined(__FreeBSD__)
				/* create another child for the next iface */
				bdev = device_add_child(parent, NULL, -1);
				device_set_ivars(bdev, &uaa);
				if (!bdev) {
					printf("%s: Device creation failed\n",
					USBDEVNAME(dev->bus->bdev));
					return (USBD_NORMAL_COMPLETION);
				}
				device_quiet(bdev);
#endif
			}
		}
		if (found != 0) {
#if defined(__FreeBSD__)
			/* remove the last created child again; it is unused */
			device_delete_child(parent, bdev);
#endif
			return (USBD_NORMAL_COMPLETION);
		}
		free(dev->subdevs, M_USB);
		dev->subdevs = 0;
	}
	/* No interfaces were attached in any of the configurations. */

	if (dd->bNumConfigurations > 1) /* don't change if only 1 config */
		usbd_set_config_index(dev, 0, 0);

	DPRINTF(("usbd_probe_and_attach: no interface drivers found\n"));

	/* Finally try the generic driver. */
	uaa.iface = NULL;
	uaa.usegeneric = 1;
	uaa.configno = UHUB_UNK_CONFIGURATION;
	uaa.ifaceno = UHUB_UNK_INTERFACE;
	uaa.vendor = UHUB_UNK_VENDOR;
	uaa.product = UHUB_UNK_PRODUCT;
	uaa.release = UHUB_UNK_RELEASE;
	dv = USB_DO_ATTACH(dev, bdev, parent, &uaa, usbd_print, usbd_submatch);
	if (dv != NULL) {
		dev->subdevs = malloc(2 * sizeof dv, M_USB, M_NOWAIT);
		if (dev->subdevs == 0)
			return (USBD_NOMEM);
		dev->subdevs[0] = dv;
		dev->subdevs[1] = 0;
		return (USBD_NORMAL_COMPLETION);
	}

	/* 
	 * The generic attach failed, but leave the device as it is.
	 * We just did not find any drivers, that's all.  The device is
	 * fully operational and not harming anyone.
	 */
	DPRINTF(("usbd_probe_and_attach: generic attach failed\n"));
#if defined(__FreeBSD__)
	device_delete_child(parent, bdev);
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
usbd_new_device(device_ptr_t parent, usbd_bus_handle bus, int depth,
		int lowspeed, int port, struct usbd_port *up)
{
	usbd_device_handle dev;
	usb_device_descriptor_t *dd;
	usbd_status err;
	int addr;
	int i;

	DPRINTF(("usbd_new_device bus=%p port=%d depth=%d lowspeed=%d\n",
		 bus, port, depth, lowspeed));
	addr = usbd_getnewaddr(bus);
	if (addr < 0) {
		printf("%s: No free USB addresses, new device ignored.\n", 
		       USBDEVNAME(bus->bdev));
		return (USBD_NO_ADDR);
	}

	dev = malloc(sizeof *dev, M_USB, M_NOWAIT);
	if (dev == NULL)
		return (USBD_NOMEM);
	memset(dev, 0, sizeof(*dev));

	dev->bus = bus;

	/* Set up default endpoint handle. */
	dev->def_ep.edesc = &dev->def_ep_desc;

	/* Set up default endpoint descriptor. */
	dev->def_ep_desc.bLength = USB_ENDPOINT_DESCRIPTOR_SIZE;
	dev->def_ep_desc.bDescriptorType = UDESC_ENDPOINT;
	dev->def_ep_desc.bEndpointAddress = USB_CONTROL_ENDPOINT;
	dev->def_ep_desc.bmAttributes = UE_CONTROL;
	USETW(dev->def_ep_desc.wMaxPacketSize, USB_MAX_IPACKET);
	dev->def_ep_desc.bInterval = 0;

	dev->quirks = &usbd_no_quirk;
	dev->address = USB_START_ADDR;
	dev->ddesc.bMaxPacketSize = 0;
	dev->lowspeed = lowspeed != 0;
	dev->depth = depth;
	dev->powersrc = up;
	dev->langid = USBD_NOLANG;
	dev->cookie.cookie = ++usb_cookie_no;

	/* Establish the default pipe. */
	err = usbd_setup_pipe(dev, 0, &dev->def_ep, USBD_DEFAULT_INTERVAL,
			      &dev->default_pipe);
	if (err) {
		usbd_remove_device(dev, up);
		return (err);
	}

	up->device = dev;
	dd = &dev->ddesc;
	/* Try a few times in case the device is slow (i.e. outside specs.) */
	for (i = 0; i < 3; i++) {
		/* Get the first 8 bytes of the device descriptor. */
		err = usbd_get_desc(dev, UDESC_DEVICE, 0, USB_MAX_IPACKET, dd);
		if (!err)
			break;
		usbd_delay_ms(dev, 200);
	}
	if (err) {
		DPRINTFN(-1, ("usbd_new_device: addr=%d, getting first desc "
			      "failed\n", addr));
		usbd_remove_device(dev, up);
		return (err);
	}

	DPRINTF(("usbd_new_device: adding unit addr=%d, rev=%02x, class=%d, "
		 "subclass=%d, protocol=%d, maxpacket=%d, len=%d, ls=%d\n", 
		 addr,UGETW(dd->bcdUSB), dd->bDeviceClass, dd->bDeviceSubClass,
		 dd->bDeviceProtocol, dd->bMaxPacketSize, dd->bLength, 
		 dev->lowspeed));

	if (dd->bDescriptorType != UDESC_DEVICE) {
		/* Illegal device descriptor */
		DPRINTFN(-1,("usbd_new_device: illegal descriptor %d\n",
			     dd->bDescriptorType));
		usbd_remove_device(dev, up);
		return (USBD_INVAL);
	}

	if (dd->bLength < USB_DEVICE_DESCRIPTOR_SIZE) {
		DPRINTFN(-1,("usbd_new_device: bad length %d\n", dd->bLength));
		usbd_remove_device(dev, up);
		return (USBD_INVAL);
	}

	USETW(dev->def_ep_desc.wMaxPacketSize, dd->bMaxPacketSize);

	err = usbd_reload_device_desc(dev);
	if (err) {
		DPRINTFN(-1, ("usbd_new_device: addr=%d, getting full desc "
			      "failed\n", addr));
		usbd_remove_device(dev, up);
		return (err);
	}

	/* Set the address */
	err = usbd_set_address(dev, addr);
	DPRINTFN(5,("usbd_new_device: setting device address=%d\n", addr));
	if (err) {
		DPRINTFN(-1,("usb_new_device: set address %d failed\n", addr));
		err = USBD_SET_ADDR_FAILED;
		usbd_remove_device(dev, up);
		return (err);
	}
	/* Allow device time to set new address */
	usbd_delay_ms(dev, USB_SET_ADDRESS_SETTLE);

	dev->address = addr;	/* New device address now */
	bus->devices[addr] = dev;

	/* Assume 100mA bus powered for now. Changed when configured. */
	dev->power = USB_MIN_POWER;
	dev->self_powered = 0;

	DPRINTF(("usbd_new_device: new dev (addr %d), dev=%p, parent=%p\n", 
		 addr, dev, parent));

	err = usbd_probe_and_attach(parent, dev, port, addr);
	if (err) {
		usbd_remove_device(dev, up);
		return (err);
  	}
  
	usbd_add_event(USB_EVENT_CTRLR_ATTACH, dev);

  	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_reload_device_desc(usbd_device_handle dev)
{
	usbd_status err;

	/* Get the full device descriptor. */
	err = usbd_get_device_desc(dev, &dev->ddesc);
	if (err)
		return (err);

	/* Figure out what's wrong with this device. */
	dev->quirks = usbd_find_quirk(&dev->ddesc);

	return (USBD_NORMAL_COMPLETION);
}

void
usbd_remove_device(usbd_device_handle dev, struct usbd_port *up)
{
	DPRINTF(("usbd_remove_device: %p\n", dev));
  
	if (dev->default_pipe != NULL)
		usbd_kill_pipe(dev->default_pipe);
	up->device = 0;
	dev->bus->devices[dev->address] = 0;

	free(dev, M_USB);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
usbd_print(void *aux, const char *pnp)
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
#if 0
	/* 
	 * It gets very crowded with these locators on the attach line.
	 * They are not really needed since they are printed in the clear
	 * by each driver.
	 */
	if (uaa->vendor != UHUB_UNK_VENDOR)
		printf(" vendor 0x%04x", uaa->vendor);
	if (uaa->product != UHUB_UNK_PRODUCT)
		printf(" product 0x%04x", uaa->product);
	if (uaa->release != UHUB_UNK_RELEASE)
		printf(" release 0x%04x", uaa->release);
#endif
	return (UNCONF);
}

#if defined(__NetBSD__)
int
usbd_submatch(struct device *parent, struct cfdata *cf, void *aux)
{
#elif defined(__OpenBSD__)
int
usbd_submatch(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
#endif
	struct usb_attach_arg *uaa = aux;

	DPRINTFN(5,("usbd_submatch port=%d,%d configno=%d,%d "
	    "ifaceno=%d,%d vendor=%d,%d product=%d,%d release=%d,%d\n",
	    uaa->port, cf->uhubcf_port,
	    uaa->configno, cf->uhubcf_configuration,
	    uaa->ifaceno, cf->uhubcf_interface,
	    uaa->vendor, cf->uhubcf_vendor,
	    uaa->product, cf->uhubcf_product,
	    uaa->release, cf->uhubcf_release));
	if (uaa->port != 0 &&	/* root hub has port 0, it should match */
	    ((uaa->port != 0 &&
	      cf->uhubcf_port != UHUB_UNK_PORT &&
	      cf->uhubcf_port != uaa->port) ||
	     (uaa->configno != UHUB_UNK_CONFIGURATION &&
	      cf->uhubcf_configuration != UHUB_UNK_CONFIGURATION &&
	      cf->uhubcf_configuration != uaa->configno) ||
	     (uaa->ifaceno != UHUB_UNK_INTERFACE &&
	      cf->uhubcf_interface != UHUB_UNK_INTERFACE &&
	      cf->uhubcf_interface != uaa->ifaceno) ||
	     (uaa->vendor != UHUB_UNK_VENDOR &&
	      cf->uhubcf_vendor != UHUB_UNK_VENDOR &&
	      cf->uhubcf_vendor != uaa->vendor) ||
	     (uaa->product != UHUB_UNK_PRODUCT &&
	      cf->uhubcf_product != UHUB_UNK_PRODUCT &&
	      cf->uhubcf_product != uaa->product) ||
	     (uaa->release != UHUB_UNK_RELEASE &&
	      cf->uhubcf_release != UHUB_UNK_RELEASE &&
	      cf->uhubcf_release != uaa->release)
	     )
	   )
		return 0;
	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

#endif

void
usbd_fill_deviceinfo(usbd_device_handle dev, struct usb_device_info *di,
		     int usedev)
{
	struct usbd_port *p;
	int i, err, s;

	di->bus = USBDEVUNIT(dev->bus->bdev);
	di->addr = dev->address;
	usbd_devinfo_vp(dev, di->vendor, di->product, usedev);
	usbd_printBCD(di->release, UGETW(dev->ddesc.bcdDevice));
	di->vendorNo = UGETW(dev->ddesc.idVendor);
	di->productNo = UGETW(dev->ddesc.idProduct);
	di->releaseNo = UGETW(dev->ddesc.bcdDevice);
	di->class = dev->ddesc.bDeviceClass;
	di->subclass = dev->ddesc.bDeviceSubClass;
	di->protocol = dev->ddesc.bDeviceProtocol;
	di->config = dev->config;
	di->power = dev->self_powered ? 0 : dev->power;
	di->lowspeed = dev->lowspeed;

	if (dev->subdevs != NULL) {
		for (i = 0; dev->subdevs[i] &&
			     i < USB_MAX_DEVNAMES; i++) {
			strncpy(di->devnames[i], USBDEVPTRNAME(dev->subdevs[i]),
				USB_MAX_DEVNAMELEN);
			di->devnames[i][USB_MAX_DEVNAMELEN-1] = '\0';
                }
        } else {
                i = 0;
        }
        for (/*i is set */; i < USB_MAX_DEVNAMES; i++)
                di->devnames[i][0] = 0;                 /* empty */

	if (dev->hub) {
		for (i = 0; 
		     i < sizeof(di->ports) / sizeof(di->ports[0]) &&
			     i < dev->hub->hubdesc.bNbrPorts;
		     i++) {
			p = &dev->hub->ports[i];
			if (p->device)
				err = p->device->address;
			else {
				s = UGETW(p->status.wPortStatus);
				if (s & UPS_PORT_ENABLED)
					err = USB_PORT_ENABLED;
				else if (s & UPS_SUSPEND)
					err = USB_PORT_SUSPENDED;
				else if (s & UPS_PORT_POWER)
					err = USB_PORT_POWERED;
				else
					err = USB_PORT_DISABLED;
			}
			di->ports[i] = err;
		}
		di->nports = dev->hub->hubdesc.bNbrPorts;
	} else
		di->nports = 0;
}

void
usb_free_device(usbd_device_handle dev)
{
	int ifcidx, nifc;

	if (dev->default_pipe != NULL)
		usbd_kill_pipe(dev->default_pipe);
	if (dev->ifaces != NULL) {
		nifc = dev->cdesc->bNumInterface;
		for (ifcidx = 0; ifcidx < nifc; ifcidx++)
			usbd_free_iface_data(dev, ifcidx);
		free(dev->ifaces, M_USB);
	}
	if (dev->cdesc != NULL)
		free(dev->cdesc, M_USB);
	if (dev->subdevs != NULL)
		free(dev->subdevs, M_USB);
	free(dev, M_USB);
}

/*
 * The general mechanism for detaching drivers works as follows: Each
 * driver is responsible for maintaining a reference count on the
 * number of outstanding references to its softc (e.g.  from
 * processing hanging in a read or write).  The detach method of the
 * driver decrements this counter and flags in the softc that the
 * driver is dying and then wakes any sleepers.  It then sleeps on the
 * softc.  Each place that can sleep must maintain the reference
 * count.  When the reference count drops to -1 (0 is the normal value
 * of the reference count) the a wakeup on the softc is performed
 * signaling to the detach waiter that all references are gone.
 */

/*
 * Called from process context when we discover that a port has
 * been disconnected.
 */
void
usb_disconnect_port(struct usbd_port *up, device_ptr_t parent)
{
	usbd_device_handle dev = up->device;
	const char *hubname = USBDEVPTRNAME(parent);
	int i;

	DPRINTFN(3,("uhub_disconnect: up=%p dev=%p port=%d\n", 
		    up, dev, up->portno));

#ifdef DIAGNOSTIC
	if (dev == NULL) {
		printf("usb_disconnect_port: no device\n");
		return;
	}
#endif

	if (dev->subdevs != NULL) {
		DPRINTFN(3,("usb_disconnect_port: disconnect subdevs\n"));
		for (i = 0; dev->subdevs[i]; i++) {
			printf("%s: at %s", USBDEVPTRNAME(dev->subdevs[i]), 
			       hubname);
			if (up->portno != 0)
				printf(" port %d", up->portno);
			printf(" (addr %d) disconnected\n", dev->address);
#if defined(__NetBSD__) || defined(__OpenBSD__)
			config_detach(dev->subdevs[i], DETACH_FORCE);
#elif defined(__FreeBSD__)
                        device_delete_child(device_get_parent(dev->subdevs[i]),
					    dev->subdevs[i]);
#endif

		}
	}

	/*usbd_add_event(USB_EVENT_DETACH, dev);*/
	dev->bus->devices[dev->address] = NULL;
	up->device = NULL;
	usb_free_device(dev);
}

#ifdef __OpenBSD__
void *usb_realloc(void *p, u_int size, int pool, int flags)
{
	void *q;

	q = malloc(size, pool, flags);
	if (q == NULL)
		return (NULL);
	bcopy(p, q, size);
	free(p, pool);
	return (q);
}
#endif
