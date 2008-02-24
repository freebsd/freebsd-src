/*	$NetBSD: usb_subr.c,v 1.99 2002/07/11 21:14:34 augustss Exp $	*/

/* Also already have from NetBSD:
 *	$NetBSD: usb_subr.c,v 1.102 2003/01/01 16:21:50 augustss Exp $
 *	$NetBSD: usb_subr.c,v 1.103 2003/01/10 11:19:13 augustss Exp $
 *	$NetBSD: usb_subr.c,v 1.111 2004/03/15 10:35:04 augustss Exp $
 *	$NetBSD: usb_subr.c,v 1.114 2004/06/23 02:30:52 mycroft Exp $
 *	$NetBSD: usb_subr.c,v 1.115 2004/06/23 05:23:19 mycroft Exp $
 *	$NetBSD: usb_subr.c,v 1.116 2004/06/23 06:27:54 mycroft Exp $
 *	$NetBSD: usb_subr.c,v 1.119 2004/10/23 13:26:33 augustss Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/usb/usb_subr.c,v 1.95 2007/06/30 20:18:44 imp Exp $");

/*-
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

#include "opt_usb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include "usbdevs.h"
#include <dev/usb/usb_quirks.h>

#define delay(d)         DELAY(d)

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) printf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) printf x
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

static usbd_status usbd_set_config(usbd_device_handle, int);
static void usbd_devinfo_vp(usbd_device_handle, char *, char *, int);
static int usbd_getnewaddr(usbd_bus_handle bus);
static void usbd_free_iface_data(usbd_device_handle dev, int ifcno);
static void usbd_kill_pipe(usbd_pipe_handle);
static usbd_status usbd_probe_and_attach(device_t parent,
				 usbd_device_handle dev, int port, int addr);

static u_int32_t usb_cookie_no = 0;

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

#include "usbdevs_data.h"
#endif /* USBVERBOSE */

static const char * const usbd_error_strs[] = {
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
		     usb_string_descriptor_t *sdesc, int *sizep)
{
	usb_device_request_t req;
	usbd_status err;
	int actlen;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, UDESC_STRING, sindex);
	USETW(req.wIndex, langid);
	USETW(req.wLength, 2);	/* only size byte first */
	err = usbd_do_request_flags(dev, &req, sdesc, USBD_SHORT_XFER_OK,
		&actlen, USBD_DEFAULT_TIMEOUT);
	if (err)
		return (err);

	if (actlen < 2)
		return (USBD_SHORT_XFER);

	USETW(req.wLength, sdesc->bLength);	/* the whole string */
	err = usbd_do_request_flags(dev, &req, sdesc, USBD_SHORT_XFER_OK,
		&actlen, USBD_DEFAULT_TIMEOUT);
	if (err)
		return (err);

	if (actlen != sdesc->bLength) {
		DPRINTFN(-1, ("usbd_get_string_desc: expected %d, got %d\n",
		    sdesc->bLength, actlen));
	}

	*sizep = actlen;
	return (USBD_NORMAL_COMPLETION);
}

static void
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

static void
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
		if (usbd_get_string(dev, udd->iManufacturer, v,
		    USB_MAX_STRING_LEN))
			vendor = NULL;
		else
			vendor = v;
		usbd_trim_spaces(vendor);
		if (usbd_get_string(dev, udd->iProduct, p,
		    USB_MAX_STRING_LEN))
			product = NULL;
		else
			product = p;
		usbd_trim_spaces(product);
		if (vendor && !*vendor)
			vendor = NULL;
		if (product && !*product)
			product = NULL;
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
	usbd_interface_handle iface;
	char vendor[USB_MAX_STRING_LEN];
	char product[USB_MAX_STRING_LEN];
	int bcdDevice, bcdUSB;
	usb_interface_descriptor_t *id;

	usbd_devinfo_vp(dev, vendor, product, 1);
	cp += sprintf(cp, "%s %s", vendor, product);
	if (showclass & USBD_SHOW_DEVICE_CLASS)
		cp += sprintf(cp, ", class %d/%d",
		    udd->bDeviceClass, udd->bDeviceSubClass);
	bcdUSB = UGETW(udd->bcdUSB);
	bcdDevice = UGETW(udd->bcdDevice);
	cp += sprintf(cp, ", rev ");
	cp += usbd_printBCD(cp, bcdUSB);
	*cp++ = '/';
	cp += usbd_printBCD(cp, bcdDevice);
	cp += sprintf(cp, ", addr %d", dev->address);
	if (showclass & USBD_SHOW_INTERFACE_CLASS)
	{
		/* fetch the interface handle for the first interface */
		(void)usbd_device2interface_handle(dev, 0, &iface);
		id = usbd_get_interface_descriptor(iface);
		cp += sprintf(cp, ", iclass %d/%d",
		    id->bInterfaceClass, id->bInterfaceSubClass);
	}
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
		pause("usbdly", (ms*hz+999)/1000 + 1);
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
		/* If the device disappeared, just give up. */
		if (!(UGETW(ps->wPortStatus) & UPS_CURRENT_CONNECT_STATUS))
			return (USBD_NORMAL_COMPLETION);
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
		printf("usbd_fill_iface_data: bad descriptor(s): %s\n",
		       ed->bLength == 0 ? "0 length" :
		       ed->bDescriptorType == UDESC_INTERFACE ? "iface desc":
		       "out of data");
		goto bad;
	found:
		ifc->endpoints[endpt].edesc = ed;
		if (dev->speed == USB_SPEED_HIGH) {
			u_int mps;
			/* Control and bulk endpoints have max packet limits. */
			switch (UE_GET_XFERTYPE(ed->bmAttributes)) {
			case UE_CONTROL:
				mps = USB_2_MAX_CTRL_PACKET;
				goto check;
			case UE_BULK:
				mps = USB_2_MAX_BULK_PACKET;
			check:
				if (UGETW(ed->wMaxPacketSize) != mps) {
					USETW(ed->wMaxPacketSize, mps);
#ifdef DIAGNOSTIC
					printf("usbd_fill_iface_data: bad max "
					       "packet size\n");
#endif
				}
				break;
			default:
				break;
			}
		}
		ifc->endpoints[endpt].refcnt = 0;
		ifc->endpoints[endpt].savedtoggle = 0;
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

static usbd_status
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
	int i, ifcidx, nifc, len, selfpowered, power;

	DPRINTFN(5,("usbd_set_config_index: dev=%p index=%d\n", dev, index));

	if (dev->config != USB_UNCONFIG_NO) {
		nifc = dev->cdesc->bNumInterface;

		/* Check that all interfaces are idle */
		for (ifcidx = 0; ifcidx < nifc; ifcidx++) {
			if (LIST_EMPTY(&dev->ifaces[ifcidx].pipes))
				continue;
			DPRINTF(("usbd_set_config_index: open pipes exist\n"));
			return (USBD_IN_USE);
		}

		DPRINTF(("usbd_set_config_index: free old config\n"));
		/* Free all configuration data structures. */
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

	/* Get the full descriptor.  Try a few times for slow devices. */
	for (i = 0; i < 3; i++) {
		err = usbd_get_desc(dev, UDESC_CONFIG, index, len, cdp);
		if (!err)
			break;
		usbd_delay_ms(dev, 200);
	}
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
			if (dev->quirks->uq_flags & UQ_POWER_CLAIM) {
				/*
				 * Hub claims to be self powered, but isn't.
				 * It seems that the power status can be
				 * determined by the hub characteristics.
				 */
				usb_hub_descriptor_t hd;
				usb_device_request_t req;
				req.bmRequestType = UT_READ_CLASS_DEVICE;
				req.bRequest = UR_GET_DESCRIPTOR;
				USETW(req.wValue, 0);
				USETW(req.wIndex, 0);
				USETW(req.wLength, USB_HUB_DESCRIPTOR_SIZE);
				err = usbd_do_request(dev, &req, &hd);
				if (!err &&
				    (UGETW(hd.wHubCharacteristics) &
				     UHD_PWR_INDIVIDUAL))
					selfpowered = 1;
				DPRINTF(("usbd_set_config_index: charac=0x%04x"
				    ", error=%s\n",
				    UGETW(hd.wHubCharacteristics),
				    usbd_errstr(err)));
			} else {
				err = usbd_get_device_status(dev, &ds);
				if (!err &&
				    (UGETW(ds.wStatus) & UDS_SELF_POWERED))
					selfpowered = 1;
				DPRINTF(("usbd_set_config_index: status=0x%04x"
				    ", error=%s\n",
				    UGETW(ds.wStatus), usbd_errstr(err)));
			}
		} else
			selfpowered = 1;
	}
	DPRINTF(("usbd_set_config_index: (addr %d) cno=%d attr=0x%02x, "
		 "selfpowered=%d, power=%d\n",
		 cdp->bConfigurationValue, dev->address, cdp->bmAttributes,
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
			       device_get_nameunit(dev->bus->bdev), dev->address,
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
	p->aborting = 0;
	p->repeat = 0;
	p->interval = ival;
	STAILQ_INIT(&p->queue);
	err = dev->bus->methods->open_pipe(p);
	if (err) {
		DPRINTFN(-1,("usbd_setup_pipe: endpoint=0x%x failed, error="
			 "%s\n",
			 ep->edesc->bEndpointAddress, usbd_errstr(err)));
		free(p, M_USB);
		return (err);
	}

	if (dev->quirks->uq_flags & UQ_OPEN_CLEARSTALL) {
		/* Clear any stall and make sure DATA0 toggle will be used next. */
		if (UE_GET_ADDR(ep->edesc->bEndpointAddress) != USB_CONTROL_ENDPOINT) {
			err = usbd_clear_endpoint_stall(p);
			if (err && err != USBD_STALLED && err != USBD_TIMEOUT) {
				printf("usbd_setup_pipe: failed to start "
				    "endpoint, %s\n", usbd_errstr(err));
				return (err);
			}
		}
	}

	*pipe = p;
	return (USBD_NORMAL_COMPLETION);
}

/* Abort the device control pipe. */
void
usbd_kill_pipe(usbd_pipe_handle pipe)
{
	usbd_abort_pipe(pipe);
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
usbd_probe_and_attach(device_t parent, usbd_device_handle dev,
		      int port, int addr)
{
	struct usb_attach_arg uaa;
	usb_device_descriptor_t *dd = &dev->ddesc;
	int found, i, confi, nifaces;
	usbd_status err;
	device_t *tmpdv;
	usbd_interface_handle ifaces[256]; /* 256 is the absolute max */
	char *devinfo;

	/* XXX FreeBSD may leak resources on failure cases -- fixme */
 	device_t bdev;
	struct usb_attach_arg *uaap;

	devinfo = malloc(1024, M_USB, M_NOWAIT);
	if (devinfo == NULL) {
		device_printf(parent, "Can't allocate memory for probe string\n");
		return (USBD_NOMEM);
	}
	bdev = device_add_child(parent, NULL, -1);
	if (!bdev) {
		free(devinfo, M_USB);
		device_printf(parent, "Device creation failed\n");
		return (USBD_INVAL);
	}
	uaap = malloc(sizeof(uaa), M_USB, M_NOWAIT);
	if (uaap == NULL) {
		free(devinfo, M_USB);
		return (USBD_INVAL);
	}
	device_set_ivars(bdev, uaap);
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
	uaa.matchlvl = 0;

	/* First try with device specific drivers. */
	DPRINTF(("usbd_probe_and_attach: trying device specific drivers\n"));

	dev->ifacenums = NULL;
	dev->subdevs = malloc(2 * sizeof(device_t), M_USB, M_NOWAIT);
	if (dev->subdevs == NULL) {
		free(devinfo, M_USB);
		return (USBD_NOMEM);
	}
	dev->subdevs[0] = bdev;
	dev->subdevs[1] = 0;
	*uaap = uaa;
	usbd_devinfo(dev, 1, devinfo);
	device_set_desc_copy(bdev, devinfo);
	if (device_probe_and_attach(bdev) == 0) {
		free(devinfo, M_USB);
		return (USBD_NORMAL_COMPLETION);
	}

	/*
	 * Free subdevs so we can reallocate it larger for the number of
	 * interfaces 
	 */
	tmpdv = dev->subdevs;
	dev->subdevs = NULL;
	free(tmpdv, M_USB);
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
				 "error=%s\n", device_get_nameunit(parent), port,
				 addr, usbd_errstr(err)));
#else
			printf("%s: port %d, set config at addr %d failed\n",
			       device_get_nameunit(parent), port, addr);
#endif
			free(devinfo, M_USB);
 			return (err);
		}
		nifaces = dev->cdesc->bNumInterface;
		uaa.configno = dev->cdesc->bConfigurationValue;
		for (i = 0; i < nifaces; i++)
			ifaces[i] = &dev->ifaces[i];
		uaa.ifaces = ifaces;
		uaa.nifaces = nifaces;
		dev->subdevs = malloc((nifaces+1) * sizeof(device_t), M_USB,M_NOWAIT);
		if (dev->subdevs == NULL) {
			free(devinfo, M_USB);
			return (USBD_NOMEM);
		}
		dev->ifacenums = malloc((nifaces) * sizeof(*dev->ifacenums),
		    M_USB,M_NOWAIT);
		if (dev->ifacenums == NULL) {
			free(devinfo, M_USB);
			return (USBD_NOMEM);
		}

		found = 0;
		for (i = 0; i < nifaces; i++) {
			if (ifaces[i] == NULL)
				continue; /* interface already claimed */
			uaa.iface = ifaces[i];
			uaa.ifaceno = ifaces[i]->idesc->bInterfaceNumber;
			dev->subdevs[found] = bdev;
			dev->subdevs[found + 1] = 0;
			dev->ifacenums[found] = i;
			*uaap = uaa;
			usbd_devinfo(dev, 1, devinfo);
			device_set_desc_copy(bdev, devinfo);
			if (device_probe_and_attach(bdev) == 0) {
				ifaces[i] = 0; /* consumed */
				found++;
				/* create another child for the next iface */
				bdev = device_add_child(parent, NULL, -1);
				if (!bdev) {
					device_printf(parent,
					    "Device add failed\n");
					free(devinfo, M_USB);
					return (USBD_NORMAL_COMPLETION);
				}
				uaap = malloc(sizeof(uaa), M_USB, M_NOWAIT);
				if (uaap == NULL) {
					free(devinfo, M_USB);
					return (USBD_NOMEM);
				}
				device_set_ivars(bdev, uaap);
			} else {
				dev->subdevs[found] = 0;
			}
		}
		if (found != 0) {
			/* remove the last created child.  It is unused */
			free(uaap, M_USB);
			free(devinfo, M_USB);
			device_delete_child(parent, bdev);
			/* free(uaap, M_USB); */ /* May be needed? xxx */
			return (USBD_NORMAL_COMPLETION);
		}
		tmpdv = dev->subdevs;
		dev->subdevs = NULL;
		free(tmpdv, M_USB);
		free(dev->ifacenums, M_USB);
		dev->ifacenums = NULL;
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
	dev->subdevs = malloc(2 * sizeof(device_t), M_USB, M_NOWAIT);
	if (dev->subdevs == 0) {
		free(devinfo, M_USB);
		return (USBD_NOMEM);
	}
	dev->subdevs[0] = bdev;
	dev->subdevs[1] = 0;
	*uaap = uaa;
	usbd_devinfo(dev, 1, devinfo);
	device_set_desc_copy(bdev, devinfo);
	free(devinfo, M_USB);
	if (device_probe_and_attach(bdev) == 0)
		return (USBD_NORMAL_COMPLETION);

	/*
	 * The generic attach failed, but leave the device as it is.
	 * We just did not find any drivers, that's all.  The device is
	 * fully operational and not harming anyone.
	 */
	DPRINTF(("usbd_probe_and_attach: generic attach failed\n"));
 	return (USBD_NORMAL_COMPLETION);
}


/*
 * Called when a new device has been put in the powered state,
 * but not yet in the addressed state.
 * Get initial descriptor, set the address, get full descriptor,
 * and attach a driver.
 */
usbd_status
usbd_new_device(device_t parent, usbd_bus_handle bus, int depth,
		int speed, int port, struct usbd_port *up)
{
	usbd_device_handle dev, adev;
	struct usbd_device *hub;
	usb_device_descriptor_t *dd;
	usb_port_status_t ps;
	usbd_status err;
	int addr;
	int i;
	int p;

	DPRINTF(("usbd_new_device bus=%p port=%d depth=%d speed=%d\n",
		 bus, port, depth, speed));
	addr = usbd_getnewaddr(bus);
	if (addr < 0) {
		printf("%s: No free USB addresses, new device ignored.\n",
		       device_get_nameunit(bus->bdev));
		return (USBD_NO_ADDR);
	}

	dev = malloc(sizeof *dev, M_USB, M_NOWAIT|M_ZERO);
	if (dev == NULL)
		return (USBD_NOMEM);

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
	dev->depth = depth;
	dev->powersrc = up;
	dev->myhub = up->parent;

	up->device = dev;

	if (up->parent && speed > up->parent->speed) {
#ifdef USB_DEBUG
		printf("%s: maxium speed of attached "
		    "device, %d, is higher than speed "
		    "of parent HUB, %d.\n",
		    __FUNCTION__, speed, up->parent->speed);
#endif
		/*
		 * Reduce the speed, otherwise we won't setup the
		 * proper transfer methods.
		 */
		speed = up->parent->speed;
	}
	
	/* Locate port on upstream high speed hub */
	for (adev = dev, hub = up->parent;
	     hub != NULL && hub->speed != USB_SPEED_HIGH;
	     adev = hub, hub = hub->myhub)
		;
	if (hub) {
		for (p = 0; p < hub->hub->hubdesc.bNbrPorts; p++) {
			if (hub->hub->ports[p].device == adev) {
				dev->myhsport = &hub->hub->ports[p];
				goto found;
			}
		}
		panic("usbd_new_device: cannot find HS port\n");
	found:
		DPRINTFN(1,("usbd_new_device: high speed port %d\n", p));
	} else {
		dev->myhsport = NULL;
	}
	dev->speed = speed;
	dev->langid = USBD_NOLANG;
	dev->cookie.cookie = ++usb_cookie_no;

	/* Establish the default pipe. */
	err = usbd_setup_pipe(dev, 0, &dev->def_ep, USBD_DEFAULT_INTERVAL,
			      &dev->default_pipe);
	if (err) {
		usbd_remove_device(dev, up);
		return (err);
	}

	dd = &dev->ddesc;
	/* Try a few times in case the device is slow (i.e. outside specs.) */
	DPRINTFN(5,("usbd_new_device: setting device address=%d\n", addr));
	for (i = 0; i < 15; i++) {
		/* Get the first 8 bytes of the device descriptor. */
		err = usbd_get_desc(dev, UDESC_DEVICE, 0, USB_MAX_IPACKET, dd);
		if (!err)
			break;
		usbd_delay_ms(dev, 200);
		if ((i & 3) == 3) {
			DPRINTFN(-1,("usb_new_device: set address %d "
			    "failed - trying a port reset\n", addr));
			usbd_reset_port(up->parent, port, &ps);
		}

	}
	if (err) {
		DPRINTFN(-1, ("usbd_new_device: addr=%d, getting first desc "
			      "failed\n", addr));
		usbd_remove_device(dev, up);
		return (err);
	}

	if (speed == USB_SPEED_HIGH) {
		/* Max packet size must be 64 (sec 5.5.3). */
		if (dd->bMaxPacketSize != USB_2_MAX_CTRL_PACKET) {
#ifdef DIAGNOSTIC
			printf("usbd_new_device: addr=%d bad max packet size\n",
			       addr);
#endif
			dd->bMaxPacketSize = USB_2_MAX_CTRL_PACKET;
		}
	}

	DPRINTF(("usbd_new_device: adding unit addr=%d, rev=%02x, class=%d, "
		 "subclass=%d, protocol=%d, maxpacket=%d, len=%d, speed=%d\n",
		 addr,UGETW(dd->bcdUSB), dd->bDeviceClass, dd->bDeviceSubClass,
		 dd->bDeviceProtocol, dd->bMaxPacketSize, dd->bLength,
		 dev->speed));

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

	/* Re-establish the default pipe with the new max packet size. */
	usbd_kill_pipe(dev->default_pipe);
	err = usbd_setup_pipe(dev, 0, &dev->def_ep, USBD_DEFAULT_INTERVAL,
	    &dev->default_pipe);
	if (err) {
		usbd_remove_device(dev, up);
		return (err);
	}

	err = usbd_reload_device_desc(dev);
	if (err) {
		DPRINTFN(-1, ("usbd_new_device: addr=%d, getting full desc "
			      "failed\n", addr));
		usbd_remove_device(dev, up);
		return (err);
	}

	/* Set the address */
	DPRINTFN(5,("usbd_new_device: setting device address=%d\n", addr));
	err = usbd_set_address(dev, addr);
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

	/* Re-establish the default pipe with the new address. */
	usbd_kill_pipe(dev->default_pipe);
	err = usbd_setup_pipe(dev, 0, &dev->def_ep, USBD_DEFAULT_INTERVAL,
	    &dev->default_pipe);
	if (err) {
		usbd_remove_device(dev, up);
		return (err);
	}

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

	usbd_add_dev_event(USB_EVENT_DEVICE_ATTACH, dev);

  	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_reload_device_desc(usbd_device_handle dev)
{
	usbd_status err;
	int i;

	/* Get the full device descriptor. */
	for (i = 0; i < 3; ++i) {
		err = usbd_get_device_desc(dev, &dev->ddesc);
		if (!err)
			break;
 		usbd_delay_ms(dev, 200);
	}
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
	up->device = NULL;
	dev->bus->devices[dev->address] = NULL;

	free(dev, M_USB);
}

void
usbd_fill_deviceinfo(usbd_device_handle dev, struct usb_device_info *di,
		     int usedev)
{
	struct usbd_port *p;
	int i, err, s;

	di->udi_bus = device_get_unit(dev->bus->bdev);
	di->udi_addr = dev->address;
	di->udi_cookie = dev->cookie;
	usbd_devinfo_vp(dev, di->udi_vendor, di->udi_product, usedev);
	usbd_printBCD(di->udi_release, UGETW(dev->ddesc.bcdDevice));
	di->udi_vendorNo = UGETW(dev->ddesc.idVendor);
	di->udi_productNo = UGETW(dev->ddesc.idProduct);
	di->udi_releaseNo = UGETW(dev->ddesc.bcdDevice);
	di->udi_class = dev->ddesc.bDeviceClass;
	di->udi_subclass = dev->ddesc.bDeviceSubClass;
	di->udi_protocol = dev->ddesc.bDeviceProtocol;
	di->udi_config = dev->config;
	di->udi_power = dev->self_powered ? 0 : dev->power;
	di->udi_speed = dev->speed;

	if (dev->subdevs != NULL) {
		for (i = 0; dev->subdevs[i] && i < USB_MAX_DEVNAMES; i++) {
			if (device_is_attached(dev->subdevs[i]))
				strlcpy(di->udi_devnames[i],
				    device_get_nameunit(dev->subdevs[i]),
				    USB_MAX_DEVNAMELEN);
			else
				di->udi_devnames[i][0] = 0;
                }
        } else {
                i = 0;
        }
        for (/*i is set */; i < USB_MAX_DEVNAMES; i++)
                di->udi_devnames[i][0] = 0;                 /* empty */

	if (dev->hub) {
		for (i = 0;
		     i < sizeof(di->udi_ports) / sizeof(di->udi_ports[0]) &&
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
			di->udi_ports[i] = err;
		}
		di->udi_nports = dev->hub->hubdesc.bNbrPorts;
	} else
		di->udi_nports = 0;
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
	if (dev->ifacenums != NULL)
		free(dev->ifacenums, M_USB);
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
usb_disconnect_port(struct usbd_port *up, device_t parent)
{
	usbd_device_handle dev = up->device;
	const char *hubname = device_get_nameunit(parent);
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
			printf("%s: at %s", device_get_nameunit(dev->subdevs[i]),
			       hubname);
			if (up->portno != 0)
				printf(" port %d", up->portno);
			printf(" (addr %d) disconnected\n", dev->address);
			struct usb_attach_arg *uaap =
			    device_get_ivars(dev->subdevs[i]);
			device_detach(dev->subdevs[i]);
			free(uaap, M_USB);
			device_delete_child(device_get_parent(dev->subdevs[i]),
			    dev->subdevs[i]);
			dev->subdevs[i] = NULL;
		}
	}

	usbd_add_dev_event(USB_EVENT_DEVICE_DETACH, dev);
	dev->bus->devices[dev->address] = NULL;
	up->device = NULL;
	usb_free_device(dev);
}
