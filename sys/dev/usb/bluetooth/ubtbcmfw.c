/*
 * ubtbcmfw.c
 */

/*-
 * Copyright (c) 2003-2009 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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

#include "usbdevs.h"
#include <dev/usb/usb.h>
#include <dev/usb/usb_mfunc.h>
#include <dev/usb/usb_error.h>
#include <dev/usb/usb_ioctl.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_parse.h>
#include <dev/usb/usb_lookup.h>
#include <dev/usb/usb_util.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_mbuf.h>
#include <dev/usb/usb_dev.h>

/*
 * Download firmware to BCM2033.
 */

#define	UBTBCMFW_CONFIG_NO	1	/* Config number */
#define	UBTBCMFW_IFACE_IDX	0	/* Control interface */

#define	UBTBCMFW_BSIZE		1024
#define	UBTBCMFW_IFQ_MAXLEN	2

enum {
	UBTBCMFW_BULK_DT_WR = 0,
	UBTBCMFW_INTR_DT_RD,
	UBTBCMFW_N_TRANSFER,
};

struct ubtbcmfw_softc {
	struct usb2_device	*sc_udev;
	struct mtx		sc_mtx;
	struct usb2_xfer	*sc_xfer[UBTBCMFW_N_TRANSFER];
	struct usb2_fifo_sc	sc_fifo;
};

/*
 * Prototypes
 */

static device_probe_t		ubtbcmfw_probe;
static device_attach_t		ubtbcmfw_attach;
static device_detach_t		ubtbcmfw_detach;

static usb2_callback_t		ubtbcmfw_write_callback;
static usb2_callback_t		ubtbcmfw_read_callback;

static usb2_fifo_close_t	ubtbcmfw_close;
static usb2_fifo_cmd_t		ubtbcmfw_start_read;
static usb2_fifo_cmd_t		ubtbcmfw_start_write;
static usb2_fifo_cmd_t		ubtbcmfw_stop_read;
static usb2_fifo_cmd_t		ubtbcmfw_stop_write;
static usb2_fifo_ioctl_t	ubtbcmfw_ioctl;
static usb2_fifo_open_t		ubtbcmfw_open;

static struct usb2_fifo_methods	ubtbcmfw_fifo_methods = 
{
	.f_close =		&ubtbcmfw_close,
	.f_ioctl =		&ubtbcmfw_ioctl,
	.f_open =		&ubtbcmfw_open,
	.f_start_read =		&ubtbcmfw_start_read,
	.f_start_write =	&ubtbcmfw_start_write,
	.f_stop_read =		&ubtbcmfw_stop_read,
	.f_stop_write =		&ubtbcmfw_stop_write,
	.basename[0] =		"ubtbcmfw",
	.basename[1] =		"ubtbcmfw",
	.basename[2] =		"ubtbcmfw",
	.postfix[0] =		"",
	.postfix[1] =		".1",
	.postfix[2] =		".2",
};

/*
 * Device's config structure
 */

static const struct usb2_config	ubtbcmfw_config[UBTBCMFW_N_TRANSFER] =
{
	[UBTBCMFW_BULK_DT_WR] = {
		.type =		UE_BULK,
		.endpoint =	0x02,	/* fixed */
		.direction =	UE_DIR_OUT,
		.if_index =	UBTBCMFW_IFACE_IDX,
		.mh.bufsize =	UBTBCMFW_BSIZE,
		.mh.flags =	{ .pipe_bof = 1, .force_short_xfer = 1,
				  .proxy_buffer = 1, },
		.mh.callback =	&ubtbcmfw_write_callback,
	},

	[UBTBCMFW_INTR_DT_RD] = {
		.type =		UE_INTERRUPT,
		.endpoint =	0x01,	/* fixed */
		.direction =	UE_DIR_IN,
		.if_index =	UBTBCMFW_IFACE_IDX,
		.mh.bufsize =	UBTBCMFW_BSIZE,
		.mh.flags =	{ .pipe_bof = 1, .short_xfer_ok = 1,
				  .proxy_buffer = 1, },
		.mh.callback =	&ubtbcmfw_read_callback,
	},
};

/*
 * Module
 */

static devclass_t	ubtbcmfw_devclass;

static device_method_t	ubtbcmfw_methods[] =
{
	DEVMETHOD(device_probe, ubtbcmfw_probe),
	DEVMETHOD(device_attach, ubtbcmfw_attach),
	DEVMETHOD(device_detach, ubtbcmfw_detach),
	{0, 0}
};

static driver_t		ubtbcmfw_driver =
{
	.name =		"ubtbcmfw",
	.methods =	ubtbcmfw_methods,
	.size =		sizeof(struct ubtbcmfw_softc),
};

DRIVER_MODULE(ubtbcmfw, uhub, ubtbcmfw_driver, ubtbcmfw_devclass, NULL, 0);
MODULE_DEPEND(ubtbcmfw, usb, 1, 1, 1);

/*
 * Probe for a USB Bluetooth device
 */

static int
ubtbcmfw_probe(device_t dev)
{
	const struct usb2_device_id	devs[] = {
	/* Broadcom BCM2033 devices only */
	{ USB_VPI(USB_VENDOR_BROADCOM, USB_PRODUCT_BROADCOM_BCM2033, 0) },
	};

	struct usb2_attach_arg	*uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST)
		return (ENXIO);

	if (uaa->info.bIfaceIndex != 0)
		return (ENXIO);

	return (usb2_lookup_id_by_uaa(devs, sizeof(devs), uaa));
} /* ubtbcmfw_probe */

/*
 * Attach the device
 */

static int
ubtbcmfw_attach(device_t dev)
{
	struct usb2_attach_arg	*uaa = device_get_ivars(dev);
	struct ubtbcmfw_softc	*sc = device_get_softc(dev);
	uint8_t			iface_index;
	int			error;

	sc->sc_udev = uaa->device;

	device_set_usb2_desc(dev);

	mtx_init(&sc->sc_mtx, "ubtbcmfw lock", NULL, MTX_DEF | MTX_RECURSE);

	iface_index = UBTBCMFW_IFACE_IDX;
	error = usb2_transfer_setup(uaa->device, &iface_index, sc->sc_xfer,
				ubtbcmfw_config, UBTBCMFW_N_TRANSFER,
				sc, &sc->sc_mtx);
	if (error != 0) {
		device_printf(dev, "allocating USB transfers failed. %s\n",
			usb2_errstr(error));
		goto detach;
	}

	error = usb2_fifo_attach(uaa->device, sc, &sc->sc_mtx,
			&ubtbcmfw_fifo_methods, &sc->sc_fifo,
			device_get_unit(dev), 0 - 1, uaa->info.bIfaceIndex,
			UID_ROOT, GID_OPERATOR, 0644);
	if (error != 0) {
		device_printf(dev, "could not attach fifo. %s\n",
			usb2_errstr(error));
		goto detach;
	}

	return (0);	/* success */

detach:
	ubtbcmfw_detach(dev);

	return (ENXIO);	/* failure */
} /* ubtbcmfw_attach */ 

/*
 * Detach the device
 */

static int
ubtbcmfw_detach(device_t dev)
{
	struct ubtbcmfw_softc	*sc = device_get_softc(dev);

	usb2_fifo_detach(&sc->sc_fifo);

	usb2_transfer_unsetup(sc->sc_xfer, UBTBCMFW_N_TRANSFER);

	mtx_destroy(&sc->sc_mtx);

	return (0);
} /* ubtbcmfw_detach */

/*
 * USB write callback
 */

static void
ubtbcmfw_write_callback(struct usb2_xfer *xfer)
{
	struct ubtbcmfw_softc	*sc = xfer->priv_sc;
	struct usb2_fifo	*f = sc->sc_fifo.fp[USB_FIFO_TX];
	uint32_t		actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
setup_next:
		if (usb2_fifo_get_data(f, xfer->frbuffers, 0,
				xfer->max_data_length, &actlen, 0)) {
			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		break;

	default: /* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto setup_next;
		}
		break;
	}
} /* ubtbcmfw_write_callback */

/*
 * USB read callback
 */

static void
ubtbcmfw_read_callback(struct usb2_xfer *xfer)
{
	struct ubtbcmfw_softc	*sc = xfer->priv_sc;
	struct usb2_fifo	*fifo = sc->sc_fifo.fp[USB_FIFO_RX];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usb2_fifo_put_data(fifo, xfer->frbuffers, 0, xfer->actlen, 1);
		/* FALLTHROUGH */

	case USB_ST_SETUP:
setup_next:
		if (usb2_fifo_put_bytes_max(fifo) > 0) {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		break;

	default: /* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto setup_next;
		}
		break;
	}
} /* ubtbcmfw_read_callback */

/*
 * Called when we about to start read()ing from the device
 */

static void
ubtbcmfw_start_read(struct usb2_fifo *fifo)
{
	struct ubtbcmfw_softc	*sc = fifo->priv_sc0;

	usb2_transfer_start(sc->sc_xfer[UBTBCMFW_INTR_DT_RD]);
} /* ubtbcmfw_start_read */

/*
 * Called when we about to stop reading (i.e. closing fifo)
 */

static void
ubtbcmfw_stop_read(struct usb2_fifo *fifo)
{
	struct ubtbcmfw_softc	*sc = fifo->priv_sc0;

	usb2_transfer_stop(sc->sc_xfer[UBTBCMFW_INTR_DT_RD]);
} /* ubtbcmfw_stop_read */

/*
 * Called when we about to start write()ing to the device, poll()ing
 * for write or flushing fifo
 */

static void
ubtbcmfw_start_write(struct usb2_fifo *fifo)
{
	struct ubtbcmfw_softc	*sc = fifo->priv_sc0;

	usb2_transfer_start(sc->sc_xfer[UBTBCMFW_BULK_DT_WR]);
} /* ubtbcmfw_start_write */

/*
 * Called when we about to stop writing (i.e. closing fifo)
 */

static void
ubtbcmfw_stop_write(struct usb2_fifo *fifo)
{
	struct ubtbcmfw_softc	*sc = fifo->priv_sc0;

	usb2_transfer_stop(sc->sc_xfer[UBTBCMFW_BULK_DT_WR]);
} /* ubtbcmfw_stop_write */

/*
 * Called when fifo is open
 */

static int
ubtbcmfw_open(struct usb2_fifo *fifo, int fflags)
{
	struct ubtbcmfw_softc	*sc = fifo->priv_sc0;
	struct usb2_xfer	*xfer;

	/*
	 * f_open fifo method can only be called with either FREAD
	 * or FWRITE flag set at one time.
	 */

	if (fflags & FREAD)
		xfer = sc->sc_xfer[UBTBCMFW_INTR_DT_RD];
	else if (fflags & FWRITE)
		xfer = sc->sc_xfer[UBTBCMFW_BULK_DT_WR];
	else
		return (EINVAL);	/* should not happen */

	if (usb2_fifo_alloc_buffer(fifo, xfer->max_data_length,
			UBTBCMFW_IFQ_MAXLEN) != 0)
		return (ENOMEM);

	return (0);
} /* ubtbcmfw_open */

/* 
 * Called when fifo is closed
 */

static void
ubtbcmfw_close(struct usb2_fifo *fifo, int fflags)
{
	if (fflags & (FREAD | FWRITE))
		usb2_fifo_free_buffer(fifo);
} /* ubtbcmfw_close */

/*
 * Process ioctl() on USB device
 */

static int
ubtbcmfw_ioctl(struct usb2_fifo *fifo, u_long cmd, void *data,
    int fflags)
{
	struct ubtbcmfw_softc	*sc = fifo->priv_sc0;
	int			error = 0;

	switch (cmd) {
	case USB_GET_DEVICE_DESC:
		memcpy(data, usb2_get_device_descriptor(sc->sc_udev),
			sizeof(struct usb2_device_descriptor));
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
} /* ubtbcmfw_ioctl */
