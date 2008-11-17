/*
 * ubtbcmfw.c
 */

/*-
 * Copyright (c) 2003 Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: ubtbcmfw.c,v 1.3 2003/10/10 19:15:08 max Exp $
 * $FreeBSD$
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_ioctl.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_parse.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_mbuf.h>
#include <dev/usb2/core/usb2_dev.h>

/*
 * Download firmware to BCM2033.
 */

#define	UBTBCMFW_CONFIG_NO	1	/* Config number */
#define	UBTBCMFW_IFACE_IDX	0	/* Control interface */
#define	UBTBCMFW_T_MAX		4	/* units */

struct ubtbcmfw_softc {
	struct usb2_fifo_sc sc_fifo;
	struct mtx sc_mtx;

	device_t sc_dev;
	struct usb2_device *sc_udev;
	struct usb2_xfer *sc_xfer[UBTBCMFW_T_MAX];

	uint8_t	sc_flags;
#define	UBTBCMFW_FLAG_WRITE_STALL 0x01
#define	UBTBCMFW_FLAG_READ_STALL  0x02
};

#define	UBTBCMFW_BSIZE		1024
#define	UBTBCMFW_IFQ_MAXLEN	2

/* prototypes */

static device_probe_t ubtbcmfw_probe;
static device_attach_t ubtbcmfw_attach;
static device_detach_t ubtbcmfw_detach;

static usb2_callback_t ubtbcmfw_write_callback;
static usb2_callback_t ubtbcmfw_write_clear_stall_callback;
static usb2_callback_t ubtbcmfw_read_callback;
static usb2_callback_t ubtbcmfw_read_clear_stall_callback;

static usb2_fifo_close_t ubtbcmfw_close;
static usb2_fifo_cmd_t ubtbcmfw_start_read;
static usb2_fifo_cmd_t ubtbcmfw_start_write;
static usb2_fifo_cmd_t ubtbcmfw_stop_read;
static usb2_fifo_cmd_t ubtbcmfw_stop_write;
static usb2_fifo_ioctl_t ubtbcmfw_ioctl;
static usb2_fifo_open_t ubtbcmfw_open;

static struct usb2_fifo_methods ubtbcmfw_fifo_methods = {
	.f_close = &ubtbcmfw_close,
	.f_ioctl = &ubtbcmfw_ioctl,
	.f_open = &ubtbcmfw_open,
	.f_start_read = &ubtbcmfw_start_read,
	.f_start_write = &ubtbcmfw_start_write,
	.f_stop_read = &ubtbcmfw_stop_read,
	.f_stop_write = &ubtbcmfw_stop_write,
	.basename[0] = "ubtbcmfw",
	.basename[1] = "ubtbcmfw",
	.basename[2] = "ubtbcmfw",
	.postfix[0] = "",
	.postfix[1] = ".1",
	.postfix[2] = ".2",
};

static const struct usb2_config ubtbcmfw_config[UBTBCMFW_T_MAX] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = 0x02,	/* fixed */
		.direction = UE_DIR_OUT,
		.mh.bufsize = UBTBCMFW_BSIZE,
		.mh.flags = {.pipe_bof = 1,.proxy_buffer = 1,},
		.mh.callback = &ubtbcmfw_write_callback,
	},

	[1] = {
		.type = UE_INTERRUPT,
		.endpoint = 0x01,	/* fixed */
		.direction = UE_DIR_IN,
		.mh.bufsize = UBTBCMFW_BSIZE,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,.proxy_buffer = 1,},
		.mh.callback = &ubtbcmfw_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &ubtbcmfw_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &ubtbcmfw_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

/*
 * Module
 */

static devclass_t ubtbcmfw_devclass;

static device_method_t ubtbcmfw_methods[] = {
	DEVMETHOD(device_probe, ubtbcmfw_probe),
	DEVMETHOD(device_attach, ubtbcmfw_attach),
	DEVMETHOD(device_detach, ubtbcmfw_detach),
	{0, 0}
};

static driver_t ubtbcmfw_driver = {
	.name = "ubtbcmfw",
	.methods = ubtbcmfw_methods,
	.size = sizeof(struct ubtbcmfw_softc),
};

DRIVER_MODULE(ubtbcmfw, ushub, ubtbcmfw_driver, ubtbcmfw_devclass, NULL, 0);
MODULE_DEPEND(ubtbcmfw, usb2_bluetooth, 1, 1, 1);
MODULE_DEPEND(ubtbcmfw, usb2_core, 1, 1, 1);

/*
 * Probe for a USB Bluetooth device
 */

static int
ubtbcmfw_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != 0)
		return (ENXIO);

	/* Match the boot device. */
	if (uaa->info.idVendor == USB_VENDOR_BROADCOM &&
	    uaa->info.idProduct == USB_PRODUCT_BROADCOM_BCM2033)
		return (0);

	return (ENXIO);
}

/*
 * Attach the device
 */

static int
ubtbcmfw_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct ubtbcmfw_softc *sc = device_get_softc(dev);
	int32_t err;
	uint8_t iface_index;

	if (sc == NULL) {
		return (ENOMEM);
	}
	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;

	device_set_usb2_desc(dev);

	mtx_init(&sc->sc_mtx, "ubtbcmfw lock", NULL, MTX_DEF | MTX_RECURSE);

	iface_index = UBTBCMFW_IFACE_IDX;
	err = usb2_transfer_setup(uaa->device,
	    &iface_index, sc->sc_xfer, ubtbcmfw_config,
	    UBTBCMFW_T_MAX, sc, &sc->sc_mtx);
	if (err) {
		device_printf(dev, "allocating USB transfers "
		    "failed, err=%s\n", usb2_errstr(err));
		goto detach;
	}
	/* set interface permissions */
	usb2_set_iface_perm(uaa->device, uaa->info.bIfaceIndex,
	    UID_ROOT, GID_OPERATOR, 0644);

	err = usb2_fifo_attach(uaa->device, sc, &sc->sc_mtx,
	    &ubtbcmfw_fifo_methods, &sc->sc_fifo,
	    device_get_unit(dev), 0 - 1, uaa->info.bIfaceIndex);
	if (err) {
		goto detach;
	}
	return (0);			/* success */

detach:
	ubtbcmfw_detach(dev);
	return (ENOMEM);		/* failure */
}

/*
 * Detach the device
 */

static int
ubtbcmfw_detach(device_t dev)
{
	struct ubtbcmfw_softc *sc = device_get_softc(dev);

	usb2_fifo_detach(&sc->sc_fifo);

	usb2_transfer_unsetup(sc->sc_xfer, UBTBCMFW_T_MAX);

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
ubtbcmfw_write_callback(struct usb2_xfer *xfer)
{
	struct ubtbcmfw_softc *sc = xfer->priv_sc;
	struct usb2_fifo *f = sc->sc_fifo.fp[USB_FIFO_RX];
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
		if (sc->sc_flags & UBTBCMFW_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
			return;
		}
		if (usb2_fifo_get_data(f, xfer->frbuffers, 0,
		    UBTBCMFW_BSIZE, &actlen, 0)) {

			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= UBTBCMFW_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		return;
	}
}

static void
ubtbcmfw_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ubtbcmfw_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UBTBCMFW_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ubtbcmfw_read_callback(struct usb2_xfer *xfer)
{
	struct ubtbcmfw_softc *sc = xfer->priv_sc;
	struct usb2_fifo *f = sc->sc_fifo.fp[USB_FIFO_RX];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usb2_fifo_put_data(f, xfer->frbuffers,
		    0, xfer->actlen, 1);

	case USB_ST_SETUP:
		if (sc->sc_flags & UBTBCMFW_FLAG_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer[3]);
			return;
		}
		if (usb2_fifo_put_bytes_max(f) != 0) {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= UBTBCMFW_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		return;
	}
}

static void
ubtbcmfw_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ubtbcmfw_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UBTBCMFW_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ubtbcmfw_start_read(struct usb2_fifo *fifo)
{
	struct ubtbcmfw_softc *sc = fifo->priv_sc0;

	usb2_transfer_start(sc->sc_xfer[1]);
	return;
}

static void
ubtbcmfw_stop_read(struct usb2_fifo *fifo)
{
	struct ubtbcmfw_softc *sc = fifo->priv_sc0;

	usb2_transfer_stop(sc->sc_xfer[3]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	return;
}

static void
ubtbcmfw_start_write(struct usb2_fifo *fifo)
{
	struct ubtbcmfw_softc *sc = fifo->priv_sc0;

	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
ubtbcmfw_stop_write(struct usb2_fifo *fifo)
{
	struct ubtbcmfw_softc *sc = fifo->priv_sc0;

	usb2_transfer_stop(sc->sc_xfer[2]);
	usb2_transfer_stop(sc->sc_xfer[0]);
	return;
}

static int
ubtbcmfw_open(struct usb2_fifo *fifo, int fflags, struct thread *td)
{
	struct ubtbcmfw_softc *sc = fifo->priv_sc0;

	if (fflags & FREAD) {
		if (usb2_fifo_alloc_buffer(fifo,
		    sc->sc_xfer[1]->max_data_length,
		    UBTBCMFW_IFQ_MAXLEN)) {
			return (ENOMEM);
		}
	}
	if (fflags & FWRITE) {
		/* clear stall first */
		mtx_lock(&sc->sc_mtx);
		sc->sc_flags |= UBTBCMFW_FLAG_WRITE_STALL;
		mtx_unlock(&sc->sc_mtx);
		if (usb2_fifo_alloc_buffer(fifo,
		    sc->sc_xfer[0]->max_data_length,
		    UBTBCMFW_IFQ_MAXLEN)) {
			return (ENOMEM);
		}
	}
	return (0);
}

static void
ubtbcmfw_close(struct usb2_fifo *fifo, int fflags, struct thread *td)
{
	if (fflags & (FREAD | FWRITE)) {
		usb2_fifo_free_buffer(fifo);
	}
	return;
}

static int
ubtbcmfw_ioctl(struct usb2_fifo *fifo, u_long cmd, void *data,
    int fflags, struct thread *td)
{
	struct ubtbcmfw_softc *sc = fifo->priv_sc0;
	int error = 0;

	switch (cmd) {
	case USB_GET_DEVICE_DESC:
		*(struct usb2_device_descriptor *)data =
		    *usb2_get_device_descriptor(sc->sc_udev);
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}
