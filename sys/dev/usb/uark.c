/*	$OpenBSD: uark.c,v 1.1 2006/08/14 08:30:22 jsg Exp $	*/

/*
 * Copyright (c) 2006 Jonathan Gray <jsg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#include <dev/usb/ucomvar.h>

#ifdef UARK_DEBUG
#define DPRINTFN(n, x)	do {	\
	if (uarkdebug > (n))	\
		printf x;	\
} while (0)
int	uarkebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define UARKBUFSZ		256
#define UARK_CONFIG_NO		0
#define UARK_IFACE_NO		0

#define UARK_SET_DATA_BITS(x)	(x - 5)

#define UARK_PARITY_NONE	0x00
#define UARK_PARITY_ODD		0x08
#define UARK_PARITY_EVEN	0x18

#define UARK_STOP_BITS_1	0x00
#define UARK_STOP_BITS_2	0x04

#define UARK_BAUD_REF		3000000

#define UARK_WRITE		0x40
#define UARK_READ		0xc0

#define UARK_REQUEST		0xfe

#define UARK_CONFIG_INDEX	0
#define UARK_IFACE_INDEX	0

struct uark_softc {
	struct ucom_softc	sc_ucom;
	usbd_interface_handle	sc_iface;

	u_char			sc_msr;
	u_char			sc_lsr;
};

static void	uark_get_status(void *, int portno, u_char *lsr, u_char *msr);
static void	uark_set(void *, int, int, int);
static int	uark_param(void *, int, struct termios *);
static void	uark_break(void *, int, int);
static int	uark_cmd(struct uark_softc *, uint16_t, uint16_t);

struct ucom_callback uark_callback = {
	uark_get_status,
	uark_set,
	uark_param,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static const struct uark_product {
	uint16_t	vendor;
	uint16_t	product;
} uark_products[] = {
	{ USB_VENDOR_ARKMICRO, USB_PRODUCT_ARKMICRO_ARK3116 },
	{ 0, 0 }
};

static int
uark_match(device_t self)
{
        struct usb_attach_arg *uaa = device_get_ivars(self);
	int i;

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	for (i = 0; uark_products[i].vendor != 0; i++) {
		if (uark_products[i].vendor == uaa->vendor &&
		    uark_products[i].product == uaa->product) {
			return (UMATCH_VENDOR_PRODUCT);
		}
	}

	return (UMATCH_NONE);
}

static int
uark_attach(device_t self)
{
	struct uark_softc *sc = device_get_softc(self);
        struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status error;
	int i;
	struct ucom_softc *ucom = &sc->sc_ucom;

	ucom->sc_dev = self;
	ucom->sc_udev = dev;

	if (uaa->iface == NULL) {
		/* Move the device into the configured state. */
		error = usbd_set_config_index(dev, UARK_CONFIG_INDEX, 1);
		if (error) {
			device_printf(ucom->sc_dev,
			    "failed to set configuration, err=%s\n",
			    usbd_errstr(error));
			goto bad;
		}
		error =
		    usbd_device2interface_handle(dev, UARK_IFACE_INDEX, &iface);
		if (error) {
			device_printf(ucom->sc_dev,
			    "failed to get interface, err=%s\n",
			    usbd_errstr(error));
			goto bad;
		}
	} else
		iface = uaa->iface;

	id = usbd_get_interface_descriptor(iface);
	ucom->sc_iface = iface;

	ucom->sc_bulkin_no = ucom->sc_bulkout_no = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			device_printf(ucom->sc_dev,
			    "could not read endpoint descriptor\n");
 			goto bad;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			ucom->sc_bulkin_no = ed->bEndpointAddress;
		else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			ucom->sc_bulkout_no = ed->bEndpointAddress;
	}
	if (ucom->sc_bulkin_no == -1 || ucom->sc_bulkout_no == -1) {
 		device_printf(ucom->sc_dev, "missing endpoint\n");
 		goto bad;
	}
	ucom->sc_parent = sc;
  	ucom->sc_ibufsize = UARKBUFSZ;
	ucom->sc_obufsize = UARKBUFSZ;
	ucom->sc_ibufsizepad = UARKBUFSZ;
	ucom->sc_opkthdrlen = 0;

	ucom->sc_callback = &uark_callback;

	DPRINTF(("uark: in=0x%x out=0x%x\n", ucom->sc_bulkin_no, ucom->sc_bulkout_no));
	ucom_attach(&sc->sc_ucom);
	return 0;

bad:
	DPRINTF(("uftdi_attach: ATTACH ERROR\n"));
	ucom->sc_dying = 1;
	return ENXIO;
}

static int
uark_detach(device_t self)
{
	struct uark_softc *sc = device_get_softc(self);
	int rv = 0;

	DPRINTF(("uark_detach: sc=%p\n", sc));
	sc->sc_ucom.sc_dying = 1;
	rv = ucom_detach(&sc->sc_ucom);

	return (rv);
}

static void
uark_set(void *vsc, int portno, int reg, int onoff)
{
	struct uark_softc *sc = vsc;

	switch (reg) {
	case UCOM_SET_BREAK:
		uark_break(sc, portno, onoff);
		return;
		/* NOTREACHED */
	case UCOM_SET_DTR:
	case UCOM_SET_RTS:
	default:
		return;
		/* NOTREACHED */
	}
}

static int
uark_param(void *vsc, int portno, struct termios *t)
{
	struct uark_softc *sc = (struct uark_softc *)vsc;
	int data;

	switch (t->c_ospeed) {
	case 300:
	case 600:
	case 1200:
	case 1800:
	case 2400:
	case 4800:
	case 9600:
	case 19200:
	case 38400:
	case 57600:
	case 115200:
		uark_cmd(sc, 3, 0x83);
		uark_cmd(sc, 0, (UARK_BAUD_REF / t->c_ospeed) & 0xFF);
		uark_cmd(sc, 1, (UARK_BAUD_REF / t->c_ospeed) >> 8);
		uark_cmd(sc, 3, 0x03);
		break;
	default:
		return (EINVAL);
		/* NOTREACHED */
	}
	if (ISSET(t->c_cflag, CSTOPB))
		data = UARK_STOP_BITS_2;
	else
		data = UARK_STOP_BITS_1;

	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			data |= UARK_PARITY_ODD;
		else
			data |= UARK_PARITY_EVEN;
	} else
		data |= UARK_PARITY_NONE;

	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		data |= UARK_SET_DATA_BITS(5);
		break;
	case CS6:
		data |= UARK_SET_DATA_BITS(6);
		break;
	case CS7:
		data |= UARK_SET_DATA_BITS(7);
		break;
	case CS8:
		data |= UARK_SET_DATA_BITS(8);
		break;
	}
	uark_cmd(sc, 3, 0x00);
	uark_cmd(sc, 3, data);

	return (0);
}

void
uark_get_status(void *vsc, int portno, u_char *lsr, u_char *msr)
{
	struct uark_softc *sc = vsc;

	if (msr != NULL)
		*msr = sc->sc_msr;
	if (lsr != NULL)
		*lsr = sc->sc_lsr;
}

void
uark_break(void *vsc, int portno, int onoff)
{
#ifdef UARK_DEBUG
	struct uark_softc *sc = vsc;

	device_printf(sc->sc_dev, "%s: break %s!\n", onoff ? "on" : "off");
	if (onoff)
		/* break on */
		uark_cmd(sc, 4, 0x01);
	else
		uark_cmd(sc, 4, 0x00);
#endif
}

int
uark_cmd(struct uark_softc *sc, uint16_t index, uint16_t value)
{
	usb_device_request_t req;
	usbd_status err;
	struct ucom_softc *ucom = &sc->sc_ucom;

	req.bmRequestType = UARK_WRITE;
	req.bRequest = UARK_REQUEST;
	USETW(req.wValue, value);
	USETW(req.wIndex, index);
	USETW(req.wLength, 0);
	err = usbd_do_request(ucom->sc_udev, &req, NULL);

	if (err)
		return (EIO);

	return (0);
}

static device_method_t uark_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uark_match),
	DEVMETHOD(device_attach, uark_attach),
	DEVMETHOD(device_detach, uark_detach),

	{ 0, 0 }
};

static driver_t uark_driver = {
	"ucom",
	uark_methods,
	sizeof (struct uark_softc)
};

DRIVER_MODULE(uark, uhub, uark_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(uark, usb, 1, 1, 1);
MODULE_DEPEND(uark, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);
