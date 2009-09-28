/* $FreeBSD$ */
/*-
 * Copyright (c) 2007 Frank A Kingswood. All rights reserved.
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The ChipHead 341 programming details were taken from the ch341.c
 * driver written by Frank A Kingswood.
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

#define	USB_DEBUG_VAR uch341_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/serial/usb_serial.h>

#if USB_DEBUG
static int uch341_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, uch341, CTLFLAG_RW, 0, "USB CH341");
SYSCTL_INT(_hw_usb_uch341, OID_AUTO, debug, CTLFLAG_RW,
    &uch341_debug, 0, "Debug level");
#endif

#define	UCH341_CONFIG_INDEX	0
#define	UCH341_IFACE_INDEX	0
#define	UCH341_BUFSIZE		1024

enum {
	UCH341_BULK_DT_WR,
	UCH341_BULK_DT_RD,
	UCH341_N_TRANSFER,
};

struct uch341_softc {
	struct ucom_super_softc sc_super_ucom;
	struct ucom_softc sc_ucom;

	struct usb_device *sc_udev;
	struct usb_xfer *sc_xfer[UCH341_N_TRANSFER];
	device_t sc_dev;
	struct mtx sc_mtx;

	uint8_t sc_dtr;
	uint8_t sc_rts;
	uint8_t	sc_iface_index;
	uint8_t	sc_hdrlen;
	uint8_t	sc_msr;
	uint8_t	sc_lsr;

	uint8_t	sc_name[16];
};

/* prototypes */

static device_probe_t uch341_probe;
static device_attach_t uch341_attach;
static device_detach_t uch341_detach;

static usb_callback_t uch341_write_callback;
static usb_callback_t uch341_read_callback;

static void	uch341_cfg_open(struct ucom_softc *);
static void	uch341_cfg_set_dtr(struct ucom_softc *, uint8_t);
static void	uch341_cfg_set_rts(struct ucom_softc *, uint8_t);
static void	uch341_cfg_set_break(struct ucom_softc *, uint8_t);
static int	uch341_pre_param(struct ucom_softc *, struct termios *);
static void	uch341_cfg_param(struct ucom_softc *, struct termios *);
static void	uch341_cfg_get_status(struct ucom_softc *, uint8_t *,
		    uint8_t *);
static void	uch341_start_read(struct ucom_softc *);
static void	uch341_stop_read(struct ucom_softc *);
static void	uch341_start_write(struct ucom_softc *);
static void	uch341_stop_write(struct ucom_softc *);
static void	uch341_poll(struct ucom_softc *ucom);

static const struct usb_config uch341_config[UCH341_N_TRANSFER] = {

	[UCH341_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = UCH341_BUFSIZE,
		.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.callback = &uch341_write_callback,
	},

	[UCH341_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = UCH341_BUFSIZE,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = &uch341_read_callback,
	},
};

static const struct ucom_callback uch341_callback = {
	.ucom_cfg_get_status = &uch341_cfg_get_status,
	.ucom_cfg_set_dtr = &uch341_cfg_set_dtr,
	.ucom_cfg_set_rts = &uch341_cfg_set_rts,
	.ucom_cfg_set_break = &uch341_cfg_set_break,
	.ucom_cfg_param = &uch341_cfg_param,
	.ucom_cfg_open = &uch341_cfg_open,
	.ucom_pre_param = &uch341_pre_param,
	.ucom_start_read = &uch341_start_read,
	.ucom_stop_read = &uch341_stop_read,
	.ucom_start_write = &uch341_start_write,
	.ucom_stop_write = &uch341_stop_write,
	.ucom_poll = &uch341_poll,
};

static device_method_t uch341_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uch341_probe),
	DEVMETHOD(device_attach, uch341_attach),
	DEVMETHOD(device_detach, uch341_detach),

	{0, 0}
};

static devclass_t uch341_devclass;

static driver_t uch341_driver = {
	.name = "uch341",
	.methods = uch341_methods,
	.size = sizeof(struct uch341_softc),
};

DRIVER_MODULE(uch341, uhub, uch341_driver, uch341_devclass, NULL, 0);
MODULE_DEPEND(uch341, ucom, 1, 1, 1);
MODULE_DEPEND(uch341, usb, 1, 1, 1);

static struct usb_device_id uch341_devs[] = {
	{USB_VPI(0x4348, 0x5523, 0)},
	{USB_VPI(0x1a86, 0x7523, 0)},
};

static int
uch341_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != UCH341_CONFIG_INDEX) {
		return (ENXIO);
	}
	/* attach to all present interfaces */

	return (usbd_lookup_id_by_uaa(uch341_devs, sizeof(uch341_devs), uaa));
}

static int
uch341_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct uch341_softc *sc = device_get_softc(dev);
	int error;

	sc->sc_udev = uaa->device;
	sc->sc_dev = dev;

	device_set_usb_desc(dev);
	mtx_init(&sc->sc_mtx, "uch341", NULL, MTX_DEF);

	snprintf(sc->sc_name, sizeof(sc->sc_name),
	    "%s", device_get_nameunit(dev));

	DPRINTF("\n");

	sc->sc_iface_index = uaa->info.bIfaceIndex;

	error = usbd_transfer_setup(uaa->device,
	    &sc->sc_iface_index, sc->sc_xfer, uch341_config,
	    UCH341_N_TRANSFER, sc, &sc->sc_mtx);

	if (error) {
		device_printf(dev, "allocating USB "
		    "transfers failed!\n");
		goto detach;
	}

	/* clear stall at first run */
	mtx_lock(&sc->sc_mtx);
	usbd_xfer_set_stall(sc->sc_xfer[UCH341_BULK_DT_WR]);
	usbd_xfer_set_stall(sc->sc_xfer[UCH341_BULK_DT_RD]);
	mtx_unlock(&sc->sc_mtx);

	error = ucom_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &uch341_callback, &sc->sc_mtx);
	if (error) {
		goto detach;
	}
	return (0);			/* success */

detach:
	uch341_detach(dev);
	return (ENXIO);
}

static int
uch341_detach(device_t dev)
{
	struct uch341_softc *sc = device_get_softc(dev);

	ucom_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);
	usbd_transfer_unsetup(sc->sc_xfer, UCH341_N_TRANSFER);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
uch341_cfg_open(struct ucom_softc *ucom)
{
}

static void
uch341_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uch341_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
tr_setup:
		pc = usbd_xfer_get_frame(xfer, 0);
		if (ucom_get_data(&sc->sc_ucom, pc,
		    0, usbd_xfer_max_len(xfer), &actlen)) {

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
uch341_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uch341_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		if (actlen > 0) {
			pc = usbd_xfer_get_frame(xfer, 0);
			ucom_put_data(&sc->sc_ucom, pc, 0, actlen);
		}
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
uch341_control_out(struct uch341_softc *sc, uint8_t bRequest,
    uint16_t wValue, uint16_t wIndex)
{
	struct usb_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = bRequest;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, 0);
	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);
}

static void
uch341_control_in(struct uch341_softc *sc, uint8_t bRequest,
    uint16_t wValue, uint16_t wIndex, void *buf, uint16_t wLength)
{
	struct usb_device_request req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = bRequest;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, wLength);
	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, buf, USB_SHORT_XFER_OK, 1000);
}

static void
uch341_cfg_set_rts_dtr(struct uch341_softc *sc)
{
	uch341_control_out(sc, 0xa4, ~((sc->sc_dtr ? (1<<5) : 0) |
	    (sc->sc_rts? (1<<6) : 0)), 0);
}

static void
uch341_cfg_set_dtr(struct ucom_softc *ucom, uint8_t onoff)
{
	struct uch341_softc *sc = ucom->sc_parent;

	sc->sc_dtr = onoff;

	uch341_cfg_set_rts_dtr(sc);
}

static void
uch341_cfg_set_rts(struct ucom_softc *ucom, uint8_t onoff)
{
	struct uch341_softc *sc = ucom->sc_parent;

	sc->sc_rts = onoff;

	uch341_cfg_set_rts_dtr(sc);
}

static void
uch341_cfg_set_break(struct ucom_softc *ucom, uint8_t onoff)
{
	/* TODO */
}

static int
uch341_pre_param(struct ucom_softc *ucom, struct termios *t)
{
	DPRINTF("\n");

	switch (t->c_ospeed) {
	case 2400:
	case 4800:
	case 9600:
	case 19200:
	case 38400:
	case 115200:
		return (0);
	default:
		return (EINVAL);
	}
}

static void
uch341_set_baudrate(struct uch341_softc *sc, uint32_t rate)
{
	uint16_t a;
	uint16_t b;

	DPRINTF("Baud = %d\n", rate);

	switch (rate) {
	case 2400:
		a = 0xd901;
		b = 0x0038;
		break;
	case 4800:
		a = 0x6402;
		b = 0x001f;
		break;
	case 9600:
		a = 0xb202;
		b = 0x0013;
		break;
	case 19200:
		a = 0xd902;
		b = 0x000d;
		break;
	case 38400:
		a = 0x6403;
		b = 0x000a;
		break;
	case 115200:
		a = 0xcc03;
		b = 0x0008;
		break;
	default:
		/* should not happen */
		a = 0;
		b = 0;
		break;
	}

	uch341_control_out(sc, 0x9a, 0x1312, a);
	uch341_control_out(sc, 0x9a, 0x0f2c, b);
}

static void
uch341_get_status(struct uch341_softc *sc)
{
	uint8_t buf[8];
	uch341_control_in(sc, 0x95, 0x0706, 0, buf, sizeof(buf));
}

static void
uch341_cfg_param(struct ucom_softc *ucom, struct termios *t)
{
	struct uch341_softc *sc = ucom->sc_parent;

	uint8_t buf[8];

	DPRINTF("\n");

	uch341_control_in(sc, 0x5f, 0, 0, buf, sizeof(buf));
	uch341_control_out(sc, 0xa1, 0, 0);
	uch341_set_baudrate(sc, t->c_ospeed);
	uch341_control_in(sc, 0x95, 0x2518, 0, buf, sizeof(buf));
	uch341_control_out(sc, 0x9a, 0x2518, 0x0050);
	uch341_get_status(sc);
	uch341_control_out(sc, 0xa1, 0x501f, 0xd90a);
	uch341_set_baudrate(sc, t->c_ospeed);
	uch341_cfg_set_rts_dtr(sc);
	uch341_get_status(sc);
}

static void
uch341_cfg_get_status(struct ucom_softc *ucom, uint8_t *lsr, uint8_t *msr)
{
	struct uch341_softc *sc = ucom->sc_parent;

	DPRINTF("msr=0x%02x lsr=0x%02x\n",
	    sc->sc_msr, sc->sc_lsr);

	*msr = sc->sc_msr;
	*lsr = sc->sc_lsr;
}

static void
uch341_start_read(struct ucom_softc *ucom)
{
	struct uch341_softc *sc = ucom->sc_parent;

	usbd_transfer_start(sc->sc_xfer[UCH341_BULK_DT_RD]);
}

static void
uch341_stop_read(struct ucom_softc *ucom)
{
	struct uch341_softc *sc = ucom->sc_parent;

	usbd_transfer_stop(sc->sc_xfer[UCH341_BULK_DT_RD]);
}

static void
uch341_start_write(struct ucom_softc *ucom)
{
	struct uch341_softc *sc = ucom->sc_parent;

	usbd_transfer_start(sc->sc_xfer[UCH341_BULK_DT_WR]);
}

static void
uch341_stop_write(struct ucom_softc *ucom)
{
	struct uch341_softc *sc = ucom->sc_parent;

	usbd_transfer_stop(sc->sc_xfer[UCH341_BULK_DT_WR]);
}

static void
uch341_poll(struct ucom_softc *ucom)
{
	struct uch341_softc *sc = ucom->sc_parent;
	usbd_transfer_poll(sc->sc_xfer, UCH341_N_TRANSFER);
}
