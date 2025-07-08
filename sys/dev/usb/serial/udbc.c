/*-
 * SPDX-License-Identifier: BSD-2-Clause-NetBSD
 *
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * Copyright (c) 2016-2024 Hiroki Sato <hrs@FreeBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/queue.h>
#include <sys/stddef.h>
#include <sys/stdint.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_core.h>

#include "usbdevs.h"

#define USB_DEBUG_VAR udbc_debug
#include <dev/usb/usb_process.h>
#include <dev/usb/serial/usb_serial.h>
#include <dev/usb/usb_debug.h>

static SYSCTL_NODE(_hw_usb, OID_AUTO, udbc, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "USB DbC Client");

#ifdef USB_DEBUG
static int udbc_debug = 0;
SYSCTL_INT(_hw_usb_udbc, OID_AUTO, debug, CTLFLAG_RWTUN, &udbc_debug, 0,
    "Debug level");
#endif

#define UDBC_CONFIG_INDEX 0

#define UDBC_IBUFSIZE	  1024
#define UDBC_OBUFSIZE	  1024

enum {
	UDBC_BULK_DT_WR,
	UDBC_BULK_DT_RD,
	UDBC_N_TRANSFER, /* n of EP */
};

struct udbc_softc {
	struct ucom_super_softc sc_super_ucom;
	struct ucom_softc sc_ucom;

	struct usb_device *sc_udev;
	struct usb_xfer *sc_xfer[UDBC_N_TRANSFER];
	device_t sc_dev;
	struct mtx sc_mtx;

	uint32_t sc_unit;
};

/* prototypes */

static device_probe_t udbc_probe;
static device_attach_t udbc_attach;
static device_detach_t udbc_detach;
static void udbc_free_softc(struct udbc_softc *);

static usb_callback_t udbc_write_callback;
static usb_callback_t udbc_read_callback;

static void udbc_free(struct ucom_softc *);
static void udbc_cfg_open(struct ucom_softc *);
static void udbc_cfg_close(struct ucom_softc *);
static int udbc_pre_param(struct ucom_softc *, struct termios *);
static int udbc_ioctl(struct ucom_softc *, uint32_t, caddr_t, int,
    struct thread *);
static void udbc_start_read(struct ucom_softc *);
static void udbc_stop_read(struct ucom_softc *);
static void udbc_start_write(struct ucom_softc *);
static void udbc_stop_write(struct ucom_softc *);
static void udbc_poll(struct ucom_softc *ucom);

static const struct usb_config udbc_config[UDBC_N_TRANSFER] = {
	[UDBC_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = UDBC_OBUFSIZE,
		.flags = {.pipe_bof = 1,},
		.callback = &udbc_write_callback,
	},

	[UDBC_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = UDBC_IBUFSIZE,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = &udbc_read_callback,
	},
};

static const struct ucom_callback udbc_callback = {
	.ucom_cfg_open = &udbc_cfg_open,
	.ucom_cfg_close = &udbc_cfg_close,
	.ucom_pre_param = &udbc_pre_param,
	.ucom_ioctl = &udbc_ioctl,
	.ucom_start_read = &udbc_start_read,
	.ucom_stop_read = &udbc_stop_read,
	.ucom_start_write = &udbc_start_write,
	.ucom_stop_write = &udbc_stop_write,
	.ucom_poll = &udbc_poll,
	.ucom_free = &udbc_free,
};

static device_method_t udbc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, udbc_probe),
	DEVMETHOD(device_attach, udbc_attach),
	DEVMETHOD(device_detach, udbc_detach),
	DEVMETHOD_END
};

static int
udbc_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != UDBC_CONFIG_INDEX)
		return (ENXIO);
	if (uaa->info.bInterfaceClass != UICLASS_DIAGNOSTIC)
		return (ENXIO);
	if (uaa->info.bDeviceProtocol != 0x00) /* GNU GDB == 1 */
		return (ENXIO);

	return (BUS_PROBE_SPECIFIC);
}

static int
udbc_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct udbc_softc *sc = device_get_softc(dev);
	int error;

	DPRINTF("\n");

	sc->sc_udev = uaa->device;
	sc->sc_dev = dev;
	sc->sc_unit = device_get_unit(dev);

	device_set_usb_desc(dev);
	mtx_init(&sc->sc_mtx, "udbc", NULL, MTX_DEF);
	ucom_ref(&sc->sc_super_ucom);

	sc->sc_ucom.sc_portno = 0;

	error = usbd_transfer_setup(uaa->device, &uaa->info.bIfaceIndex,
	    sc->sc_xfer, udbc_config, UDBC_N_TRANSFER, sc, &sc->sc_mtx);

	if (error) {
		device_printf(dev,
		    "allocating USB transfers failed\n");
		goto detach;
	}
	/* clear stall at first run */
	mtx_lock(&sc->sc_mtx);
	usbd_xfer_set_stall(sc->sc_xfer[UDBC_BULK_DT_WR]);
	usbd_xfer_set_stall(sc->sc_xfer[UDBC_BULK_DT_RD]);
	mtx_unlock(&sc->sc_mtx);

	error = ucom_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &udbc_callback, &sc->sc_mtx);
	if (error)
		goto detach;
	ucom_set_pnpinfo_usb(&sc->sc_super_ucom, dev);

	return (0); /* success */

detach:
	udbc_detach(dev);
	return (ENXIO);
}

static int
udbc_detach(device_t dev)
{
	struct udbc_softc *sc = device_get_softc(dev);

	ucom_detach(&sc->sc_super_ucom, &sc->sc_ucom);
	usbd_transfer_unsetup(sc->sc_xfer, UDBC_N_TRANSFER);

	device_claim_softc(dev);

	udbc_free_softc(sc);

	return (0);
}

UCOM_UNLOAD_DRAIN(udbc);

static void
udbc_free_softc(struct udbc_softc *sc)
{
	if (ucom_unref(&sc->sc_super_ucom)) {
		mtx_destroy(&sc->sc_mtx);
		device_free_softc(sc);
	}
}

static void
udbc_free(struct ucom_softc *ucom)
{
	udbc_free_softc(ucom->sc_parent);
}

static void
udbc_cfg_open(struct ucom_softc *ucom)
{
	/*
	 * This do-nothing open routine exists for the sole purpose of this
	 * DPRINTF() so that you can see the point at which open gets called
	 * when debugging is enabled.
	 */
	DPRINTF("\n");
}

static void
udbc_cfg_close(struct ucom_softc *ucom)
{
	/*
	 * This do-nothing close routine exists for the sole purpose of this
	 * DPRINTF() so that you can see the point at which close gets called
	 * when debugging is enabled.
	 */
	DPRINTF("\n");
}

static void
udbc_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct udbc_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint32_t buflen;

	DPRINTFN(3, "\n");

	switch (USB_GET_STATE(xfer)) {
	default: /* Error */
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
		}
		/* FALLTHROUGH */
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
		pc = usbd_xfer_get_frame(xfer, 0);
		if (ucom_get_data(&sc->sc_ucom, pc, 0, UDBC_OBUFSIZE,
			&buflen) == 0)
			break;
		if (buflen != 0) {
			usbd_xfer_set_frame_len(xfer, 0, buflen);
			usbd_transfer_submit(xfer);
		}
		break;
	}
}

static void
udbc_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct udbc_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int buflen;

	DPRINTFN(3, "\n");

	usbd_xfer_status(xfer, &buflen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		pc = usbd_xfer_get_frame(xfer, 0);
		ucom_put_data(&sc->sc_ucom, pc, 0, buflen);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		return;

	default: /* Error */
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		return;
	}
}

static int
udbc_pre_param(struct ucom_softc *ucom, struct termios *t)
{
	DPRINTF("\n");

	return (0);
}

static int
udbc_ioctl(struct ucom_softc *ucom, uint32_t cmd, caddr_t data, int flag,
    struct thread *td)
{
	return (ENOIOCTL);
}

static void
udbc_start_read(struct ucom_softc *ucom)
{
	struct udbc_softc *sc = ucom->sc_parent;

	usbd_transfer_start(sc->sc_xfer[UDBC_BULK_DT_RD]);
}

static void
udbc_stop_read(struct ucom_softc *ucom)
{
	struct udbc_softc *sc = ucom->sc_parent;

	usbd_transfer_stop(sc->sc_xfer[UDBC_BULK_DT_RD]);
}

static void
udbc_start_write(struct ucom_softc *ucom)
{
	struct udbc_softc *sc = ucom->sc_parent;

	usbd_transfer_start(sc->sc_xfer[UDBC_BULK_DT_WR]);
}

static void
udbc_stop_write(struct ucom_softc *ucom)
{
	struct udbc_softc *sc = ucom->sc_parent;

	usbd_transfer_stop(sc->sc_xfer[UDBC_BULK_DT_WR]);
}

static void
udbc_poll(struct ucom_softc *ucom)
{
	struct udbc_softc *sc = ucom->sc_parent;

	usbd_transfer_poll(sc->sc_xfer, UDBC_N_TRANSFER);
}

static driver_t udbc_driver = {
	.name = "udbc",
	.methods = udbc_methods,
	.size = sizeof(struct udbc_softc),
};

DRIVER_MODULE(udbc, uhub, udbc_driver, NULL, NULL);
MODULE_DEPEND(udbc, ucom, 1, 1, 1);
MODULE_DEPEND(udbc, usb, 1, 1, 1);
MODULE_VERSION(udbc, 1);
