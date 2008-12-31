/*	$NetBSD: uipaq.c,v 1.4 2006/11/16 01:33:27 christos Exp $	*/
/*	$OpenBSD: uipaq.c,v 1.1 2005/06/17 23:50:33 deraadt Exp $	*/

/*
 * Copyright (c) 2000-2005 The NetBSD Foundation, Inc.
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

/*
 * iPAQ driver
 * 
 * 19 July 2003:	Incorporated changes suggested by Sam Lawrance from
 * 			the uppc module
 *
 *
 * Contact isis@cs.umd.edu if you have any questions/comments about this driver
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/usb/uipaq.c,v 1.7.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/module.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbcdc.h>	/*UCDC_* stuff */

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#include <dev/usb/ucomvar.h>

#ifdef UIPAQ_DEBUG
#define DPRINTF(x)	if (uipaqdebug) printf x
#define DPRINTFN(n,x)	if (uipaqdebug>(n)) printf x
int uipaqdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UIPAQ_CONFIG_NO		1
#define UIPAQ_IFACE_INDEX	0

#define UIPAQIBUFSIZE 1024
#define UIPAQOBUFSIZE 1024

struct uipaq_softc {
	struct ucom_softc       sc_ucom;
	u_int16_t		sc_lcr;		/* state for DTR/RTS */
	u_int16_t		sc_flags;

};

/* Callback routines */
static void	uipaq_set(void *, int, int, int);

/* Support routines. */
/* based on uppc module by Sam Lawrance */
static void	uipaq_dtr(struct uipaq_softc *sc, int onoff);
static void	uipaq_rts(struct uipaq_softc *sc, int onoff);
static void	uipaq_break(struct uipaq_softc* sc, int onoff);

int uipaq_detach(device_t self);

struct ucom_callback uipaq_callback = {
	NULL,
	uipaq_set,
	NULL,
	NULL,
	NULL,	/*open*/
	NULL,	/*close*/
	NULL,
	NULL
};

struct uipaq_type {
	struct usb_devno	uv_dev;
	u_int16_t		uv_flags;
};

static const struct uipaq_type uipaq_devs[] = {
	{{ USB_VENDOR_HP, USB_PRODUCT_HP_2215 }, 0 },
	{{ USB_VENDOR_HP, USB_PRODUCT_HP_568J }, 0},
	{{ USB_VENDOR_HTC, USB_PRODUCT_HTC_WINMOBILE }, 0},
	{{ USB_VENDOR_HTC, USB_PRODUCT_HTC_PPC6700MODEM }, 0},
	{{ USB_VENDOR_HTC, USB_PRODUCT_HTC_SMARTPHONE }, 0},
	{{ USB_VENDOR_COMPAQ, USB_PRODUCT_COMPAQ_IPAQPOCKETPC } , 0},
	{{ USB_VENDOR_CASIO, USB_PRODUCT_CASIO_BE300 } , 0},
	{{ USB_VENDOR_SHARP, USB_PRODUCT_SHARP_WZERO3ES }, 0},
	{{ USB_VENDOR_ASUS, USB_PRODUCT_ASUS_P535 }, 0},
};

#define uipaq_lookup(v, p) ((const struct uipaq_type *)usb_lookup(uipaq_devs, v, p))

static int
uipaq_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	DPRINTFN(20,("uipaq: vendor=0x%x, product=0x%x\n",
	    uaa->vendor, uaa->product));

	return (uipaq_lookup(uaa->vendor, uaa->product) != NULL ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

static int
uipaq_attach(device_t self)
{
	struct uipaq_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int i;
	usbd_status err;
	struct ucom_softc *ucom = &sc->sc_ucom;

	ucom->sc_dev = self;
	ucom->sc_udev = dev;

	DPRINTFN(10,("\nuipaq_attach: sc=%p\n", sc));

	/* Move the device into the configured state. */
	err = usbd_set_config_no(dev, UIPAQ_CONFIG_NO, 1);
	if (err) {
		device_printf(ucom->sc_dev,
		    "failed to set configuration: %s\n", usbd_errstr(err));
		goto bad;
	}

	err = usbd_device2interface_handle(dev, UIPAQ_IFACE_INDEX, &iface);
	if (err) {
		device_printf(ucom->sc_dev, "failed to get interface: %s\n",
		    usbd_errstr(err));
		goto bad;
	}

	sc->sc_flags = uipaq_lookup(uaa->vendor, uaa->product)->uv_flags;
	id = usbd_get_interface_descriptor(iface);
	ucom->sc_iface = iface;
	ucom->sc_ibufsize = UIPAQIBUFSIZE;
	ucom->sc_obufsize = UIPAQOBUFSIZE;
	ucom->sc_ibufsizepad = UIPAQIBUFSIZE;
	ucom->sc_opkthdrlen = 0;
	ucom->sc_callback = &uipaq_callback;
	ucom->sc_parent = sc;
	ucom->sc_bulkin_no = ucom->sc_bulkout_no = -1;
	for (i=0; i<id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			device_printf(ucom->sc_dev, 
			    "no endpoint descriptor for %d\n", i);
			goto bad;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
		       ucom->sc_bulkin_no = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
		       ucom->sc_bulkout_no = ed->bEndpointAddress;
		}
	}
	if (ucom->sc_bulkin_no == -1 || ucom->sc_bulkout_no == -1) {
		device_printf(ucom->sc_dev,
		    "no proper endpoints found (%d,%d)\n",
		    ucom->sc_bulkin_no, ucom->sc_bulkout_no);
		return (ENXIO);
	}
	
	ucom_attach(&sc->sc_ucom);
	return (0);
bad:
	DPRINTF(("uipaq_attach: ATTACH ERROR\n"));
	ucom->sc_dying = 1;
	return (ENXIO);
}

void
uipaq_dtr(struct uipaq_softc* sc, int onoff)
{
	usb_device_request_t req;
	struct ucom_softc *ucom = &sc->sc_ucom;
	usbd_status err;
	int retries = 3;

	DPRINTF(("%s: uipaq_dtr: onoff=%x\n", device_get_nameunit(ucom->sc_dev), onoff));

	/* Avoid sending unnecessary requests */
	if (onoff && (sc->sc_lcr & UCDC_LINE_DTR))
		return;
	if (!onoff && !(sc->sc_lcr & UCDC_LINE_DTR))
		return;

	/* Other parameters depend on reg */
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	sc->sc_lcr = onoff ? sc->sc_lcr | UCDC_LINE_DTR : sc->sc_lcr & ~UCDC_LINE_DTR;
	USETW(req.wValue, sc->sc_lcr);
	USETW(req.wIndex, 0x0);
	USETW(req.wLength, 0);

	/* Fire off the request a few times if necessary */
	while (retries) {
		err = usbd_do_request(ucom->sc_udev, &req, NULL);
		if (!err)
			break;
		retries--;
	}
}

void
uipaq_rts(struct uipaq_softc* sc, int onoff)
{
	usb_device_request_t req;
	struct ucom_softc *ucom = &sc->sc_ucom;
	usbd_status err;
	int retries = 3;

	DPRINTF(("%s: uipaq_rts: onoff=%x\n", device_get_nameunit(ucom->sc_dev), onoff));

	/* Avoid sending unnecessary requests */
	if (onoff && (sc->sc_lcr & UCDC_LINE_RTS)) return;
	if (!onoff && !(sc->sc_lcr & UCDC_LINE_RTS)) return;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	sc->sc_lcr = onoff ? sc->sc_lcr | UCDC_LINE_RTS : sc->sc_lcr & ~UCDC_LINE_RTS;
	USETW(req.wValue, sc->sc_lcr);
	USETW(req.wIndex, 0x0);
	USETW(req.wLength, 0);

	while (retries) {
		err = usbd_do_request(ucom->sc_udev, &req, NULL);
		if (!err)
			break;
		retries--;
	}
}

void
uipaq_break(struct uipaq_softc* sc, int onoff)
{
	usb_device_request_t req;
	struct ucom_softc *ucom = &sc->sc_ucom;
	usbd_status err;
	int retries = 3;

	DPRINTF(("%s: uipaq_break: onoff=%x\n", device_get_nameunit(ucom->sc_dev), onoff));

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_BREAK;

	USETW(req.wValue, onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);
	USETW(req.wIndex, 0x0);
	USETW(req.wLength, 0);

	while (retries) {
		err = usbd_do_request(ucom->sc_udev, &req, NULL);
		if (!err)
			break;
		retries--;
	}
}

void
uipaq_set(void *addr, int portno, int reg, int onoff)
{
	struct uipaq_softc* sc = addr;
	struct ucom_softc *ucom = &sc->sc_ucom;

	switch (reg) {
	case UCOM_SET_DTR:
		uipaq_dtr(addr, onoff);
		break;
	case UCOM_SET_RTS:
		uipaq_rts(addr, onoff);
		break;
	case UCOM_SET_BREAK:
		uipaq_break(addr, onoff);
		break;
	default:
		printf("%s: unhandled set request: reg=%x onoff=%x\n",
		  device_get_nameunit(ucom->sc_dev), reg, onoff);
		return;
	}
}

int
uipaq_detach(device_t self)
{
	struct uipaq_softc *sc = device_get_softc(self);
	struct ucom_softc *ucom = &sc->sc_ucom;
	int rv = 0;

	DPRINTF(("uipaq_detach: sc=%p flags=%d\n", sc, flags));
	ucom->sc_dying = 1;

	rv = ucom_detach(ucom);

	return (rv);
}

static device_method_t uipaq_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uipaq_match),
	DEVMETHOD(device_attach, uipaq_attach),
	DEVMETHOD(device_detach, uipaq_detach),

	{ 0, 0 }
};
static driver_t uipaq_driver = {
	"ucom",
	uipaq_methods,
	sizeof (struct uipaq_softc)
};

DRIVER_MODULE(uipaq, uhub, uipaq_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(uipaq, usb, 1, 1, 1);
MODULE_DEPEND(uipaq, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);
