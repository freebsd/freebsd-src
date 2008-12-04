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

/*
 * NOTE: all function names beginning like "uark_cfg_" can only
 * be called from within the config thread function !
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_cdc.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/serial/usb2_serial.h>

#define	UARK_BUF_SIZE		1024	/* bytes */

#define	UARK_N_TRANSFER	4		/* units */

#define	UARK_SET_DATA_BITS(x)	((x) - 5)

#define	UARK_PARITY_NONE	0x00
#define	UARK_PARITY_ODD		0x08
#define	UARK_PARITY_EVEN	0x18

#define	UARK_STOP_BITS_1	0x00
#define	UARK_STOP_BITS_2	0x04

#define	UARK_BAUD_REF		3000000

#define	UARK_WRITE		0x40
#define	UARK_READ		0xc0

#define	UARK_REQUEST		0xfe

#define	UARK_CONFIG_INDEX	0
#define	UARK_IFACE_INDEX	0

struct uark_softc {
	struct usb2_com_super_softc sc_super_ucom;
	struct usb2_com_softc sc_ucom;

	struct usb2_xfer *sc_xfer[UARK_N_TRANSFER];
	struct usb2_device *sc_udev;

	uint8_t	sc_flags;
#define	UARK_FLAG_BULK_READ_STALL	0x01
#define	UARK_FLAG_BULK_WRITE_STALL	0x02
	uint8_t	sc_msr;
	uint8_t	sc_lsr;
};

/* prototypes */

static device_probe_t uark_probe;
static device_attach_t uark_attach;
static device_detach_t uark_detach;

static usb2_callback_t uark_bulk_write_callback;
static usb2_callback_t uark_bulk_write_clear_stall_callback;
static usb2_callback_t uark_bulk_read_callback;
static usb2_callback_t uark_bulk_read_clear_stall_callback;

static void uark_start_read(struct usb2_com_softc *ucom);
static void uark_stop_read(struct usb2_com_softc *ucom);
static void uark_start_write(struct usb2_com_softc *ucom);
static void uark_stop_write(struct usb2_com_softc *ucom);

static int uark_pre_param(struct usb2_com_softc *ucom, struct termios *t);
static void uark_cfg_param(struct usb2_com_softc *ucom, struct termios *t);
static void uark_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *lsr, uint8_t *msr);
static void uark_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff);
static void uark_cfg_write(struct uark_softc *sc, uint16_t index, uint16_t value);

static const struct usb2_config
	uark_xfer_config[UARK_N_TRANSFER] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = UARK_BUF_SIZE,
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &uark_bulk_write_callback,
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = UARK_BUF_SIZE,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &uark_bulk_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &uark_bulk_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &uark_bulk_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static const struct usb2_com_callback uark_callback = {
	.usb2_com_cfg_get_status = &uark_cfg_get_status,
	.usb2_com_cfg_set_break = &uark_cfg_set_break,
	.usb2_com_cfg_param = &uark_cfg_param,
	.usb2_com_pre_param = &uark_pre_param,
	.usb2_com_start_read = &uark_start_read,
	.usb2_com_stop_read = &uark_stop_read,
	.usb2_com_start_write = &uark_start_write,
	.usb2_com_stop_write = &uark_stop_write,
};

static device_method_t uark_methods[] = {
	/* Device methods */
	DEVMETHOD(device_probe, uark_probe),
	DEVMETHOD(device_attach, uark_attach),
	DEVMETHOD(device_detach, uark_detach),
	{0, 0}
};

static devclass_t uark_devclass;

static driver_t uark_driver = {
	.name = "uark",
	.methods = uark_methods,
	.size = sizeof(struct uark_softc),
};

DRIVER_MODULE(uark, ushub, uark_driver, uark_devclass, NULL, 0);
MODULE_DEPEND(uark, usb2_serial, 1, 1, 1);
MODULE_DEPEND(uark, usb2_core, 1, 1, 1);

static const struct usb2_device_id uark_devs[] = {
	{USB_VPI(USB_VENDOR_ARKMICRO, USB_PRODUCT_ARKMICRO_ARK3116, 0)},
};

static int
uark_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != 0) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != UARK_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(uark_devs, sizeof(uark_devs), uaa));
}

static int
uark_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct uark_softc *sc = device_get_softc(dev);
	int32_t error;
	uint8_t iface_index;

	if (sc == NULL) {
		return (ENOMEM);
	}
	device_set_usb2_desc(dev);

	sc->sc_udev = uaa->device;

	iface_index = UARK_IFACE_INDEX;
	error = usb2_transfer_setup
	    (uaa->device, &iface_index, sc->sc_xfer,
	    uark_xfer_config, UARK_N_TRANSFER, sc, &Giant);

	if (error) {
		device_printf(dev, "allocating control USB "
		    "transfers failed!\n");
		goto detach;
	}
	/* clear stall at first run */
	sc->sc_flags |= (UARK_FLAG_BULK_WRITE_STALL |
	    UARK_FLAG_BULK_READ_STALL);

	error = usb2_com_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &uark_callback, &Giant);
	if (error) {
		DPRINTF("usb2_com_attach failed\n");
		goto detach;
	}
	return (0);			/* success */

detach:
	uark_detach(dev);
	return (ENXIO);			/* failure */
}

static int
uark_detach(device_t dev)
{
	struct uark_softc *sc = device_get_softc(dev);

	usb2_com_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);

	usb2_transfer_unsetup(sc->sc_xfer, UARK_N_TRANSFER);

	return (0);
}

static void
uark_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct uark_softc *sc = xfer->priv_sc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
		if (sc->sc_flags & UARK_FLAG_BULK_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
			return;
		}
		if (usb2_com_get_data(&sc->sc_ucom, xfer->frbuffers, 0,
		    UARK_BUF_SIZE, &actlen)) {
			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flags |= UARK_FLAG_BULK_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		return;

	}
}

static void
uark_bulk_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct uark_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UARK_FLAG_BULK_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
uark_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct uark_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usb2_com_put_data(&sc->sc_ucom, xfer->frbuffers, 0,
		    xfer->actlen);

	case USB_ST_SETUP:
		if (sc->sc_flags & UARK_FLAG_BULK_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer[3]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flags |= UARK_FLAG_BULK_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		return;

	}
}

static void
uark_bulk_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct uark_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UARK_FLAG_BULK_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
uark_start_read(struct usb2_com_softc *ucom)
{
	struct uark_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer[1]);
	return;
}

static void
uark_stop_read(struct usb2_com_softc *ucom)
{
	struct uark_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer[3]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	return;
}

static void
uark_start_write(struct usb2_com_softc *ucom)
{
	struct uark_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
uark_stop_write(struct usb2_com_softc *ucom)
{
	struct uark_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer[2]);
	usb2_transfer_stop(sc->sc_xfer[0]);
	return;
}

static int
uark_pre_param(struct usb2_com_softc *ucom, struct termios *t)
{
	if ((t->c_ospeed < 300) || (t->c_ospeed > 115200))
		return (EINVAL);
	return (0);
}

static void
uark_cfg_param(struct usb2_com_softc *ucom, struct termios *t)
{
	struct uark_softc *sc = ucom->sc_parent;
	uint32_t speed = t->c_ospeed;
	uint16_t data;

	/*
	 * NOTE: When reverse computing the baud rate from the "data" all
	 * allowed baud rates are within 3% of the initial baud rate.
	 */
	data = (UARK_BAUD_REF + (speed / 2)) / speed;

	uark_cfg_write(sc, 3, 0x83);
	uark_cfg_write(sc, 0, data & 0xFF);
	uark_cfg_write(sc, 1, data >> 8);
	uark_cfg_write(sc, 3, 0x03);

	if (t->c_cflag & CSTOPB)
		data = UARK_STOP_BITS_2;
	else
		data = UARK_STOP_BITS_1;

	if (t->c_cflag & PARENB) {
		if (t->c_cflag & PARODD)
			data |= UARK_PARITY_ODD;
		else
			data |= UARK_PARITY_EVEN;
	} else
		data |= UARK_PARITY_NONE;

	switch (t->c_cflag & CSIZE) {
	case CS5:
		data |= UARK_SET_DATA_BITS(5);
		break;
	case CS6:
		data |= UARK_SET_DATA_BITS(6);
		break;
	case CS7:
		data |= UARK_SET_DATA_BITS(7);
		break;
	default:
	case CS8:
		data |= UARK_SET_DATA_BITS(8);
		break;
	}
	uark_cfg_write(sc, 3, 0x00);
	uark_cfg_write(sc, 3, data);
	return;
}

static void
uark_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *lsr, uint8_t *msr)
{
	struct uark_softc *sc = ucom->sc_parent;

	*lsr = sc->sc_lsr;
	*msr = sc->sc_msr;
	return;
}

static void
uark_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct uark_softc *sc = ucom->sc_parent;

	DPRINTF("onoff=%d\n", onoff);

	uark_cfg_write(sc, 4, onoff ? 0x01 : 0x00);
	return;
}

static void
uark_cfg_write(struct uark_softc *sc, uint16_t index, uint16_t value)
{
	struct usb2_device_request req;
	usb2_error_t err;

	if (usb2_com_cfg_is_gone(&sc->sc_ucom)) {
		return;
	}
	req.bmRequestType = UARK_WRITE;
	req.bRequest = UARK_REQUEST;
	USETW(req.wValue, value);
	USETW(req.wIndex, index);
	USETW(req.wLength, 0);

	err = usb2_do_request_flags
	    (sc->sc_udev, &Giant, &req, NULL, 0, NULL, 1000);

	if (err) {
		DPRINTFN(0, "device request failed, err=%s "
		    "(ignored)\n", usb2_errstr(err));
	}
	return;
}
