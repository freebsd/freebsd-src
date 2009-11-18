/*	$OpenBSD: uslcom.c,v 1.17 2007/11/24 10:52:12 jsg Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/linker_set.h>
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
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#define	USB_DEBUG_VAR uslcom_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/serial/usb_serial.h>

#if USB_DEBUG
static int uslcom_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, uslcom, CTLFLAG_RW, 0, "USB uslcom");
SYSCTL_INT(_hw_usb_uslcom, OID_AUTO, debug, CTLFLAG_RW,
    &uslcom_debug, 0, "Debug level");
#endif

#define	USLCOM_BULK_BUF_SIZE		1024
#define	USLCOM_CONFIG_INDEX	0
#define	USLCOM_IFACE_INDEX	0

#define	USLCOM_SET_DATA_BITS(x)	((x) << 8)

#define	USLCOM_WRITE		0x41
#define	USLCOM_READ		0xc1

#define	USLCOM_UART		0x00
#define	USLCOM_BAUD_RATE	0x01	
#define	USLCOM_DATA		0x03
#define	USLCOM_BREAK		0x05
#define	USLCOM_CTRL		0x07

#define	USLCOM_UART_DISABLE	0x00
#define	USLCOM_UART_ENABLE	0x01

#define	USLCOM_CTRL_DTR_ON	0x0001	
#define	USLCOM_CTRL_DTR_SET	0x0100
#define	USLCOM_CTRL_RTS_ON	0x0002
#define	USLCOM_CTRL_RTS_SET	0x0200
#define	USLCOM_CTRL_CTS		0x0010
#define	USLCOM_CTRL_DSR		0x0020
#define	USLCOM_CTRL_DCD		0x0080

#define	USLCOM_BAUD_REF		0x384000

#define	USLCOM_STOP_BITS_1	0x00
#define	USLCOM_STOP_BITS_2	0x02

#define	USLCOM_PARITY_NONE	0x00
#define	USLCOM_PARITY_ODD	0x10
#define	USLCOM_PARITY_EVEN	0x20

#define	USLCOM_PORT_NO		0xFFFF /* XXX think this should be 0 --hps */

#define	USLCOM_BREAK_OFF	0x00
#define	USLCOM_BREAK_ON		0x01

enum {
	USLCOM_BULK_DT_WR,
	USLCOM_BULK_DT_RD,
	USLCOM_N_TRANSFER,
};

struct uslcom_softc {
	struct ucom_super_softc sc_super_ucom;
	struct ucom_softc sc_ucom;

	struct usb_xfer *sc_xfer[USLCOM_N_TRANSFER];
	struct usb_device *sc_udev;
	struct mtx sc_mtx;

	uint8_t		 sc_msr;
	uint8_t		 sc_lsr;
};

static device_probe_t uslcom_probe;
static device_attach_t uslcom_attach;
static device_detach_t uslcom_detach;

static usb_callback_t uslcom_write_callback;
static usb_callback_t uslcom_read_callback;

static void uslcom_open(struct ucom_softc *);
static void uslcom_close(struct ucom_softc *);
static void uslcom_set_dtr(struct ucom_softc *, uint8_t);
static void uslcom_set_rts(struct ucom_softc *, uint8_t);
static void uslcom_set_break(struct ucom_softc *, uint8_t);
static int uslcom_pre_param(struct ucom_softc *, struct termios *);
static void uslcom_param(struct ucom_softc *, struct termios *);
static void uslcom_get_status(struct ucom_softc *, uint8_t *, uint8_t *);
static void uslcom_start_read(struct ucom_softc *);
static void uslcom_stop_read(struct ucom_softc *);
static void uslcom_start_write(struct ucom_softc *);
static void uslcom_stop_write(struct ucom_softc *);
static void uslcom_poll(struct ucom_softc *ucom);

static const struct usb_config uslcom_config[USLCOM_N_TRANSFER] = {

	[USLCOM_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = USLCOM_BULK_BUF_SIZE,
		.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.callback = &uslcom_write_callback,
	},

	[USLCOM_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = USLCOM_BULK_BUF_SIZE,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = &uslcom_read_callback,
	},
};

static struct ucom_callback uslcom_callback = {
	.ucom_cfg_open = &uslcom_open,
	.ucom_cfg_close = &uslcom_close,
	.ucom_cfg_get_status = &uslcom_get_status,
	.ucom_cfg_set_dtr = &uslcom_set_dtr,
	.ucom_cfg_set_rts = &uslcom_set_rts,
	.ucom_cfg_set_break = &uslcom_set_break,
	.ucom_cfg_param = &uslcom_param,
	.ucom_pre_param = &uslcom_pre_param,
	.ucom_start_read = &uslcom_start_read,
	.ucom_stop_read = &uslcom_stop_read,
	.ucom_start_write = &uslcom_start_write,
	.ucom_stop_write = &uslcom_stop_write,
	.ucom_poll = &uslcom_poll,
};

static const struct usb_device_id uslcom_devs[] = {
    { USB_VPI(USB_VENDOR_BALTECH,	USB_PRODUCT_BALTECH_CARDREADER, 0) },
    { USB_VPI(USB_VENDOR_DYNASTREAM,	USB_PRODUCT_DYNASTREAM_ANTDEVBOARD, 0) },
    { USB_VPI(USB_VENDOR_JABLOTRON,	USB_PRODUCT_JABLOTRON_PC60B, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_ARGUSISP, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_CRUMB128, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_DEGREE, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_BURNSIDE, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_HELICOM, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_LIPOWSKY_HARP, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_LIPOWSKY_JTAG, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_LIPOWSKY_LIN, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_POLOLU, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_CP2102, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_CP210X_2, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_SUUNTO, 0) },
    { USB_VPI(USB_VENDOR_SILABS,	USB_PRODUCT_SILABS_TRAQMATE, 0) },
    { USB_VPI(USB_VENDOR_SILABS2,	USB_PRODUCT_SILABS2_DCU11CLONE, 0) },
    { USB_VPI(USB_VENDOR_USI,		USB_PRODUCT_USI_MC60, 0) },
};

static device_method_t uslcom_methods[] = {
	DEVMETHOD(device_probe, uslcom_probe),
	DEVMETHOD(device_attach, uslcom_attach),
	DEVMETHOD(device_detach, uslcom_detach),
	{0, 0}
};

static devclass_t uslcom_devclass;

static driver_t uslcom_driver = {
	.name = "uslcom",
	.methods = uslcom_methods,
	.size = sizeof(struct uslcom_softc),
};

DRIVER_MODULE(uslcom, uhub, uslcom_driver, uslcom_devclass, NULL, 0);
MODULE_DEPEND(uslcom, ucom, 1, 1, 1);
MODULE_DEPEND(uslcom, usb, 1, 1, 1);
MODULE_VERSION(uslcom, 1);

static int
uslcom_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	DPRINTFN(11, "\n");

	if (uaa->usb_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != USLCOM_CONFIG_INDEX) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != USLCOM_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usbd_lookup_id_by_uaa(uslcom_devs, sizeof(uslcom_devs), uaa));
}

static int
uslcom_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct uslcom_softc *sc = device_get_softc(dev);
	int error;

	DPRINTFN(11, "\n");

	device_set_usb_desc(dev);
	mtx_init(&sc->sc_mtx, "uslcom", NULL, MTX_DEF);

	sc->sc_udev = uaa->device;

	error = usbd_transfer_setup(uaa->device,
	    &uaa->info.bIfaceIndex, sc->sc_xfer, uslcom_config,
	    USLCOM_N_TRANSFER, sc, &sc->sc_mtx);
	if (error) {
		DPRINTF("one or more missing USB endpoints, "
		    "error=%s\n", usbd_errstr(error));
		goto detach;
	}
	/* clear stall at first run */
	mtx_lock(&sc->sc_mtx);
	usbd_xfer_set_stall(sc->sc_xfer[USLCOM_BULK_DT_WR]);
	usbd_xfer_set_stall(sc->sc_xfer[USLCOM_BULK_DT_RD]);
	mtx_unlock(&sc->sc_mtx);

	error = ucom_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &uslcom_callback, &sc->sc_mtx);
	if (error) {
		goto detach;
	}
	return (0);

detach:
	uslcom_detach(dev);
	return (ENXIO);
}

static int
uslcom_detach(device_t dev)
{
	struct uslcom_softc *sc = device_get_softc(dev);

	DPRINTF("sc=%p\n", sc);

	ucom_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);
	usbd_transfer_unsetup(sc->sc_xfer, USLCOM_N_TRANSFER);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
uslcom_open(struct ucom_softc *ucom)
{
	struct uslcom_softc *sc = ucom->sc_parent;
	struct usb_device_request req;

	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_UART;
	USETW(req.wValue, USLCOM_UART_ENABLE);
	USETW(req.wIndex, USLCOM_PORT_NO);
	USETW(req.wLength, 0);

        if (ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000)) {
		DPRINTF("UART enable failed (ignored)\n");
	}
}

static void
uslcom_close(struct ucom_softc *ucom)
{
	struct uslcom_softc *sc = ucom->sc_parent;
	struct usb_device_request req;

	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_UART;
	USETW(req.wValue, USLCOM_UART_DISABLE);
	USETW(req.wIndex, USLCOM_PORT_NO);
	USETW(req.wLength, 0);

        if (ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000)) {
		DPRINTF("UART disable failed (ignored)\n");
	}
}

static void
uslcom_set_dtr(struct ucom_softc *ucom, uint8_t onoff)
{
        struct uslcom_softc *sc = ucom->sc_parent;
	struct usb_device_request req;
	uint16_t ctl;

        DPRINTF("onoff = %d\n", onoff);

	ctl = onoff ? USLCOM_CTRL_DTR_ON : 0;
	ctl |= USLCOM_CTRL_DTR_SET;

	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_CTRL;
	USETW(req.wValue, ctl);
	USETW(req.wIndex, USLCOM_PORT_NO);
	USETW(req.wLength, 0);

        if (ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000)) {
		DPRINTF("Setting DTR failed (ignored)\n");
	}
}

static void
uslcom_set_rts(struct ucom_softc *ucom, uint8_t onoff)
{
        struct uslcom_softc *sc = ucom->sc_parent;
	struct usb_device_request req;
	uint16_t ctl;

        DPRINTF("onoff = %d\n", onoff);

	ctl = onoff ? USLCOM_CTRL_RTS_ON : 0;
	ctl |= USLCOM_CTRL_RTS_SET;

	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_CTRL;
	USETW(req.wValue, ctl);
	USETW(req.wIndex, USLCOM_PORT_NO);
	USETW(req.wLength, 0);

        if (ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000)) {
		DPRINTF("Setting DTR failed (ignored)\n");
	}
}

static int
uslcom_pre_param(struct ucom_softc *ucom, struct termios *t)
{
	if (t->c_ospeed <= 0 || t->c_ospeed > 921600)
		return (EINVAL);
	return (0);
}

static void
uslcom_param(struct ucom_softc *ucom, struct termios *t)
{
	struct uslcom_softc *sc = ucom->sc_parent;
	struct usb_device_request req;
	uint16_t data;

	DPRINTF("\n");

	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_BAUD_RATE;
	USETW(req.wValue, USLCOM_BAUD_REF / t->c_ospeed);
	USETW(req.wIndex, USLCOM_PORT_NO);
	USETW(req.wLength, 0);

        if (ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000)) {
		DPRINTF("Set baudrate failed (ignored)\n");
	}

	if (t->c_cflag & CSTOPB)
		data = USLCOM_STOP_BITS_2;
	else
		data = USLCOM_STOP_BITS_1;
	if (t->c_cflag & PARENB) {
		if (t->c_cflag & PARODD)
			data |= USLCOM_PARITY_ODD;
		else
			data |= USLCOM_PARITY_EVEN;
	} else
		data |= USLCOM_PARITY_NONE;
	switch (t->c_cflag & CSIZE) {
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
	USETW(req.wIndex, USLCOM_PORT_NO);
	USETW(req.wLength, 0);

        if (ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000)) {
		DPRINTF("Set format failed (ignored)\n");
	}
	return;
}

static void
uslcom_get_status(struct ucom_softc *ucom, uint8_t *lsr, uint8_t *msr)
{
	struct uslcom_softc *sc = ucom->sc_parent;

	DPRINTF("\n");

	*lsr = sc->sc_lsr;
	*msr = sc->sc_msr;
}

static void
uslcom_set_break(struct ucom_softc *ucom, uint8_t onoff)
{
        struct uslcom_softc *sc = ucom->sc_parent;
	struct usb_device_request req;
	uint16_t brk = onoff ? USLCOM_BREAK_ON : USLCOM_BREAK_OFF;

	req.bmRequestType = USLCOM_WRITE;
	req.bRequest = USLCOM_BREAK;
	USETW(req.wValue, brk);
	USETW(req.wIndex, USLCOM_PORT_NO);
	USETW(req.wLength, 0);

        if (ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000)) {
		DPRINTF("Set BREAK failed (ignored)\n");
	}
}

static void
uslcom_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uslcom_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
tr_setup:
		pc = usbd_xfer_get_frame(xfer, 0);
		if (ucom_get_data(&sc->sc_ucom, pc, 0,
		    USLCOM_BULK_BUF_SIZE, &actlen)) {

			DPRINTF("actlen = %d\n", actlen);

			usbd_xfer_set_frame_len(xfer, 0, actlen);
			usbd_transfer_submit(xfer);
		}
		return;

	default:			/* Error */
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		return;
	}
}

static void
uslcom_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uslcom_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		pc = usbd_xfer_get_frame(xfer, 0);
		ucom_put_data(&sc->sc_ucom, pc, 0, actlen);

	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		return;

	default:			/* Error */
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		return;
	}
}

static void
uslcom_start_read(struct ucom_softc *ucom)
{
	struct uslcom_softc *sc = ucom->sc_parent;

	/* start read endpoint */
	usbd_transfer_start(sc->sc_xfer[USLCOM_BULK_DT_RD]);
}

static void
uslcom_stop_read(struct ucom_softc *ucom)
{
	struct uslcom_softc *sc = ucom->sc_parent;

	/* stop read endpoint */
	usbd_transfer_stop(sc->sc_xfer[USLCOM_BULK_DT_RD]);
}

static void
uslcom_start_write(struct ucom_softc *ucom)
{
	struct uslcom_softc *sc = ucom->sc_parent;

	usbd_transfer_start(sc->sc_xfer[USLCOM_BULK_DT_WR]);
}

static void
uslcom_stop_write(struct ucom_softc *ucom)
{
	struct uslcom_softc *sc = ucom->sc_parent;

	usbd_transfer_stop(sc->sc_xfer[USLCOM_BULK_DT_WR]);
}

static void
uslcom_poll(struct ucom_softc *ucom)
{
	struct uslcom_softc *sc = ucom->sc_parent;
	usbd_transfer_poll(sc->sc_xfer, USLCOM_N_TRANSFER);
}
