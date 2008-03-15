/*	$FreeBSD$ */
/*	$OpenBSD: uslcom.c,v 1.17 2007/11/24 10:52:12 jsg Exp $	*/

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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/tty.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include "usbdevs.h"
#include <dev/usb/ucomvar.h>

#ifdef USLCOM_DEBUG
#define DPRINTFN(n, x)  do { if (uslcomdebug > (n)) printf x; } while (0)
int	uslcomdebug = 1;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define USLCOMBUFSZ		256
#define USLCOM_CONFIG_NO	0
#define USLCOM_IFACE_NO		0

#define USLCOM_SET_DATA_BITS(x)	(x << 8)

#define USLCOM_WRITE		0x41
#define USLCOM_READ		0xc1

#define USLCOM_UART		0x00
#define USLCOM_BAUD_RATE	0x01	
#define USLCOM_DATA		0x03
#define USLCOM_BREAK		0x05
#define USLCOM_CTRL		0x07

#define USLCOM_UART_DISABLE	0x00
#define USLCOM_UART_ENABLE	0x01

#define USLCOM_CTRL_DTR_ON	0x0001	
#define USLCOM_CTRL_DTR_SET	0x0100
#define USLCOM_CTRL_RTS_ON	0x0002
#define USLCOM_CTRL_RTS_SET	0x0200
#define USLCOM_CTRL_CTS		0x0010
#define USLCOM_CTRL_DSR		0x0020
#define USLCOM_CTRL_DCD		0x0080


#define USLCOM_BAUD_REF		0x384000

#define USLCOM_STOP_BITS_1	0x00
#define USLCOM_STOP_BITS_2	0x02

#define USLCOM_PARITY_NONE	0x00
#define USLCOM_PARITY_ODD	0x10
#define USLCOM_PARITY_EVEN	0x20

#define USLCOM_BREAK_OFF	0x00
#define USLCOM_BREAK_ON		0x01


struct uslcom_softc {
	struct ucom_softc	 sc_ucom;
	device_t		 sc_dev;
	usbd_device_handle	 sc_udev;

	u_char			 sc_msr;
	u_char			 sc_lsr;

	u_char			 sc_dying;
};

void	uslcom_get_status(void *, int portno, u_char *lsr, u_char *msr);
void	uslcom_set(void *, int, int, int);
int	uslcom_param(void *, int, struct termios *);
int	uslcom_open(void *sc, int portno);
void	uslcom_close(void *, int);
void	uslcom_break(void *sc, int portno, int onoff);

struct ucom_callback uslcom_callback = {
	uslcom_get_status,
	uslcom_set,
	uslcom_param,
	NULL,
	uslcom_open,
	uslcom_close,
	NULL,
	NULL,
};

static const struct usb_devno uslcom_devs[] = {
	{ USB_VENDOR_BALTECH,		USB_PRODUCT_BALTECH_CARDREADER },
	{ USB_VENDOR_DYNASTREAM,	USB_PRODUCT_DYNASTREAM_ANTDEVBOARD },
	{ USB_VENDOR_JABLOTRON,		USB_PRODUCT_JABLOTRON_PC60B },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_ARGUSISP },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_CRUMB128 },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_DEGREE },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_BURNSIDE },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_HELICOM },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_LIPOWSKY_HARP },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_LIPOWSKY_JTAG },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_LIPOWSKY_LIN },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_POLOLU },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_CP2102 },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_CP210X_2 },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_SUUNTO },
	{ USB_VENDOR_SILABS,		USB_PRODUCT_SILABS_TRAQMATE },
	{ USB_VENDOR_SILABS2,		USB_PRODUCT_SILABS2_DCU11CLONE },
	{ USB_VENDOR_USI,		USB_PRODUCT_USI_MC60 }
};

static device_probe_t uslcom_match;
static device_attach_t uslcom_attach;
static device_detach_t uslcom_detach;

static device_method_t uslcom_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uslcom_match),
	DEVMETHOD(device_attach, uslcom_attach),
	DEVMETHOD(device_detach, uslcom_detach),
	{ 0, 0 }
};

static driver_t uslcom_driver = {
	"ucom",
	uslcom_methods,
	sizeof (struct uslcom_softc)
};

DRIVER_MODULE(uslcom, uhub, uslcom_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(uslcom, usb, 1, 1, 1);
MODULE_DEPEND(uslcom, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);
MODULE_VERSION(uslcom, 1);

static int
uslcom_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->iface != NULL)
		return UMATCH_NONE;

	return (usb_lookup(uslcom_devs, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

static int
uslcom_attach(device_t self)
{
	struct uslcom_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle dev = uaa->device;
	struct ucom_softc* ucom;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status error;
	int i;

	ucom = &sc->sc_ucom;
	ucom->sc_dev = self;
	ucom->sc_udev = dev;
	ucom->sc_iface = uaa->iface;
	
	sc->sc_dev = self;
	sc->sc_udev = uaa->device;

	if (usbd_set_config_index(sc->sc_udev, USLCOM_CONFIG_NO, 1) != 0) {
		device_printf(self, "could not set configuration no\n");
		sc->sc_dying = 1;
		return ENXIO;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(sc->sc_udev, USLCOM_IFACE_NO,
	    &ucom->sc_iface);
	if (error != 0) {
		device_printf(self, "could not get interface handle\n");
		sc->sc_dying = 1;
		return ENXIO;
	}

	id = usbd_get_interface_descriptor(ucom->sc_iface);

	ucom->sc_bulkin_no = ucom->sc_bulkout_no = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(ucom->sc_iface, i);
		if (ed == NULL) {
			device_printf(self, "no endpoint descriptor found for %d\n",
			    i);
			sc->sc_dying = 1;
			return ENXIO;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			ucom->sc_bulkin_no = ed->bEndpointAddress;
		else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			ucom->sc_bulkout_no = ed->bEndpointAddress;
	}

	if (ucom->sc_bulkin_no == -1 || ucom->sc_bulkout_no == -1) {
		device_printf(self, "missing endpoint\n");
		sc->sc_dying = 1;
		return ENXIO;
	}

	ucom->sc_parent = sc;
	ucom->sc_portno = UCOM_UNK_PORTNO;
	/* bulkin, bulkout set above */
	ucom->sc_ibufsize = USLCOMBUFSZ;
	ucom->sc_obufsize = USLCOMBUFSZ;
	ucom->sc_ibufsizepad = USLCOMBUFSZ;
	ucom->sc_opkthdrlen = 0;
	ucom->sc_callback = &uslcom_callback;

        DPRINTF(("uslcom: in = 0x%x, out = 0x%x\n",
                 ucom->sc_bulkin_no, ucom->sc_bulkout_no));

        ucom_attach(&sc->sc_ucom);
        return 0;
}

static int
uslcom_detach(device_t self)
{
	struct uslcom_softc *sc = device_get_softc(self);

	sc->sc_dying = 1;
	return ucom_detach(&sc->sc_ucom);
}

int
uslcom_open(void *vsc, int portno)
{
	struct uslcom_softc *sc = vsc;
	usb_device_request_t req;
	usbd_status err;

	if (sc->sc_dying)
		return (EIO);

	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_UART;
	USETW(req.wValue, USLCOM_UART_ENABLE);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err)
		return (EIO);

	return (0);
}

void
uslcom_close(void *vsc, int portno)
{
	struct uslcom_softc *sc = vsc;
	usb_device_request_t req;

	if (sc->sc_dying)
		return;

	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_UART;
	USETW(req.wValue, USLCOM_UART_DISABLE);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	usbd_do_request(sc->sc_udev, &req, NULL);
}

void
uslcom_set(void *vsc, int portno, int reg, int onoff)
{
	struct uslcom_softc *sc = vsc;
	usb_device_request_t req;
	int ctl;

	switch (reg) {
	case UCOM_SET_DTR:
		ctl = onoff ? USLCOM_CTRL_DTR_ON : 0;
		ctl |= USLCOM_CTRL_DTR_SET;
		break;
	case UCOM_SET_RTS:
		ctl = onoff ? USLCOM_CTRL_RTS_ON : 0;
		ctl |= USLCOM_CTRL_RTS_SET;
		break;
	case UCOM_SET_BREAK:
		uslcom_break(sc, portno, onoff);
		return;
	default:
		return;
	}
	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_CTRL;
	USETW(req.wValue, ctl);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	usbd_do_request(sc->sc_udev, &req, NULL);
}

int
uslcom_param(void *vsc, int portno, struct termios *t)
{
	struct uslcom_softc *sc = (struct uslcom_softc *)vsc;
	usbd_status err;
	usb_device_request_t req;
	int data;

	if (t->c_ospeed <= 0 || t->c_ospeed > 921600)
		return (EINVAL);

	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_BAUD_RATE;
	USETW(req.wValue, USLCOM_BAUD_REF / t->c_ospeed);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err)
		return (EIO);

	if (ISSET(t->c_cflag, CSTOPB))
		data = USLCOM_STOP_BITS_2;
	else
		data = USLCOM_STOP_BITS_1;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			data |= USLCOM_PARITY_ODD;
		else
			data |= USLCOM_PARITY_EVEN;
	} else
		data |= USLCOM_PARITY_NONE;
	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		data |= USLCOM_SET_DATA_BITS(5);
		break;
	case CS6:
		data |= USLCOM_SET_DATA_BITS(6);
		break;
	case CS7:
		data |= USLCOM_SET_DATA_BITS(7);
		break;
	case CS8:
		data |= USLCOM_SET_DATA_BITS(8);
		break;
	}

	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_DATA;
	USETW(req.wValue, data);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err)
		return (EIO);

#if 0
	/* XXX flow control */
	if (ISSET(t->c_cflag, CRTSCTS))
		/*  rts/cts flow ctl */
	} else if (ISSET(t->c_iflag, IXON|IXOFF)) {
		/*  xon/xoff flow ctl */
	} else {
		/* disable flow ctl */
	}
#endif

	return (0);
}

void
uslcom_get_status(void *vsc, int portno, u_char *lsr, u_char *msr)
{
	struct uslcom_softc *sc = vsc;
	
	if (msr != NULL)
		*msr = sc->sc_msr;
	if (lsr != NULL)
		*lsr = sc->sc_lsr;
}

void
uslcom_break(void *vsc, int portno, int onoff)
{
	struct uslcom_softc *sc = vsc;
	usb_device_request_t req;
	int brk = onoff ? USLCOM_BREAK_ON : USLCOM_BREAK_OFF;	

	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_BREAK;
	USETW(req.wValue, brk);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	usbd_do_request(sc->sc_udev, &req, NULL);
}
