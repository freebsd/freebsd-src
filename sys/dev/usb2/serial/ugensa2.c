/* $FreeBSD$ */
/*	$NetBSD: ugensa.c,v 1.9.2.1 2007/03/24 14:55:50 yamt Exp $	*/

/*
 * Copyright (c) 2004, 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roland C. Dowdeswell <elric@netbsd.org>.
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
 * NOTE: all function names beginning like "ugensa_cfg_" can only
 * be called from within the config thread function !
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_cdc.h>
#include <dev/usb2/include/usb2_defs.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_device.h>

#include <dev/usb2/serial/usb2_serial.h>

#define	UGENSA_BUF_SIZE		2048	/* bytes */
#define	UGENSA_N_TRANSFER	4	/* units */
#define	UGENSA_CONFIG_INDEX	0
#define	UGENSA_IFACE_INDEX	0
#define	UGENSA_IFACE_MAX	8	/* exclusivly */

struct ugensa_sub_softc {
	struct usb2_com_softc *sc_usb2_com_ptr;
	struct usb2_xfer *sc_xfer[UGENSA_N_TRANSFER];

	uint8_t	sc_flags;
#define	UGENSA_FLAG_BULK_READ_STALL	0x01
#define	UGENSA_FLAG_BULK_WRITE_STALL	0x02
};

struct ugensa_softc {
	struct usb2_com_super_softc sc_super_ucom;
	struct usb2_com_softc sc_ucom[UGENSA_IFACE_MAX];
	struct ugensa_sub_softc sc_sub[UGENSA_IFACE_MAX];

	struct mtx sc_mtx;
	uint8_t	sc_niface;
};

/* prototypes */

static device_probe_t ugensa_probe;
static device_attach_t ugensa_attach;
static device_detach_t ugensa_detach;

static usb2_callback_t ugensa_bulk_write_callback;
static usb2_callback_t ugensa_bulk_write_clear_stall_callback;
static usb2_callback_t ugensa_bulk_read_callback;
static usb2_callback_t ugensa_bulk_read_clear_stall_callback;

static void ugensa_start_read(struct usb2_com_softc *ucom);
static void ugensa_stop_read(struct usb2_com_softc *ucom);
static void ugensa_start_write(struct usb2_com_softc *ucom);
static void ugensa_stop_write(struct usb2_com_softc *ucom);

static const struct usb2_config
	ugensa_xfer_config[UGENSA_N_TRANSFER] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = UGENSA_BUF_SIZE,
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &ugensa_bulk_write_callback,
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = UGENSA_BUF_SIZE,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &ugensa_bulk_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &ugensa_bulk_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &ugensa_bulk_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static const struct usb2_com_callback ugensa_callback = {
	.usb2_com_start_read = &ugensa_start_read,
	.usb2_com_stop_read = &ugensa_stop_read,
	.usb2_com_start_write = &ugensa_start_write,
	.usb2_com_stop_write = &ugensa_stop_write,
};

static device_method_t ugensa_methods[] = {
	/* Device methods */
	DEVMETHOD(device_probe, ugensa_probe),
	DEVMETHOD(device_attach, ugensa_attach),
	DEVMETHOD(device_detach, ugensa_detach),
	{0, 0}
};

static devclass_t ugensa_devclass;

static driver_t ugensa_driver = {
	.name = "ugensa",
	.methods = ugensa_methods,
	.size = sizeof(struct ugensa_softc),
};

DRIVER_MODULE(ugensa, ushub, ugensa_driver, ugensa_devclass, NULL, 0);
MODULE_DEPEND(ugensa, usb2_serial, 1, 1, 1);
MODULE_DEPEND(ugensa, usb2_core, 1, 1, 1);

static const struct usb2_device_id ugensa_devs[] = {
	{USB_VPI(USB_VENDOR_AIRPRIME, USB_PRODUCT_AIRPRIME_PC5220, 0)},
	{USB_VPI(USB_VENDOR_CMOTECH, USB_PRODUCT_CMOTECH_CDMA_MODEM1, 0)},
	{USB_VPI(USB_VENDOR_KYOCERA2, USB_PRODUCT_KYOCERA2_CDMA_MSM_K, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_49GPLUS, 0)},
/*	{USB_VPI(USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_E270, 0)}, */
	{USB_VPI(USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_MOBILE, 0)},
	{USB_VPI(USB_VENDOR_MERLIN, USB_PRODUCT_MERLIN_V620, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_CDMA_MODEM, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_ES620, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U720, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U727, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U740, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U740_2, 0)},
/*	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U950D, 0)}, */
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V620, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V640, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V720, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V740, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_X950D, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U870, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_XU870, 0)},
	{USB_VPI(USB_VENDOR_NOVATEL2, USB_PRODUCT_NOVATEL2_FLEXPACKGPS, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD580, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD595, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC595U, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC597E, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_C597, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880E, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC880U, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881E, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC881U, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_EM5625, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720_2, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5725, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MINI5725, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AIRCARD875, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755_2, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8755_3, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8765, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_AC875U, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8775_2, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8780, 0)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC8781, 0)},
};

static int
ugensa_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != UGENSA_CONFIG_INDEX) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != 0) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(ugensa_devs, sizeof(ugensa_devs), uaa));
}

static int
ugensa_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct ugensa_softc *sc = device_get_softc(dev);
	struct ugensa_sub_softc *ssc;
	struct usb2_interface *iface;
	int32_t error;
	uint8_t iface_index;
	int x, cnt;

	if (sc == NULL)
		return (ENOMEM);

	device_set_usb2_desc(dev);
	mtx_init(&sc->sc_mtx, "ugensa", NULL, MTX_DEF);

	/* Figure out how many interfaces this device has got */
	for (cnt = 0; cnt < UGENSA_IFACE_MAX; cnt++) {
		if ((usb2_get_pipe(uaa->device, cnt, ugensa_xfer_config + 0) == NULL) ||
		    (usb2_get_pipe(uaa->device, cnt, ugensa_xfer_config + 1) == NULL)) {
			/* we have reached the end */
			break;
		}
	}

	if (cnt == 0) {
		device_printf(dev, "No interfaces!\n");
		goto detach;
	}
	for (x = 0; x < cnt; x++) {
		iface = usb2_get_iface(uaa->device, x);
		if (iface->idesc->bInterfaceClass != UICLASS_VENDOR)
			/* Not a serial port, most likely a SD reader */
			continue;

		ssc = sc->sc_sub + sc->sc_niface;
		ssc->sc_usb2_com_ptr = sc->sc_ucom + sc->sc_niface;

		iface_index = (UGENSA_IFACE_INDEX + x);
		error = usb2_transfer_setup(uaa->device,
		    &iface_index, ssc->sc_xfer, ugensa_xfer_config,
		    UGENSA_N_TRANSFER, ssc, &sc->sc_mtx);

		if (error) {
			device_printf(dev, "allocating USB "
			    "transfers failed!\n");
			goto detach;
		}
		/* clear stall at first run */
		ssc->sc_flags |= (UGENSA_FLAG_BULK_WRITE_STALL |
		    UGENSA_FLAG_BULK_READ_STALL);

		/* initialize port number */
		ssc->sc_usb2_com_ptr->sc_portno = sc->sc_niface;
		sc->sc_niface++;
		if (x != uaa->info.bIfaceIndex)
			usb2_set_parent_iface(uaa->device, x,
			    uaa->info.bIfaceIndex);
	}
	device_printf(dev, "Found %d interfaces.\n", sc->sc_niface);

	error = usb2_com_attach(&sc->sc_super_ucom, sc->sc_ucom, sc->sc_niface, sc,
	    &ugensa_callback, &sc->sc_mtx);
	if (error) {
		DPRINTF("attach failed\n");
		goto detach;
	}
	return (0);			/* success */

detach:
	ugensa_detach(dev);
	return (ENXIO);			/* failure */
}

static int
ugensa_detach(device_t dev)
{
	struct ugensa_softc *sc = device_get_softc(dev);
	uint8_t x;

	usb2_com_detach(&sc->sc_super_ucom, sc->sc_ucom, sc->sc_niface);

	for (x = 0; x < sc->sc_niface; x++) {
		usb2_transfer_unsetup(sc->sc_sub[x].sc_xfer, UGENSA_N_TRANSFER);
	}
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
ugensa_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct ugensa_sub_softc *ssc = xfer->priv_sc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
		if (ssc->sc_flags & UGENSA_FLAG_BULK_WRITE_STALL) {
			usb2_transfer_start(ssc->sc_xfer[2]);
			return;
		}
		if (usb2_com_get_data(ssc->sc_usb2_com_ptr, xfer->frbuffers, 0,
		    UGENSA_BUF_SIZE, &actlen)) {
			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			ssc->sc_flags |= UGENSA_FLAG_BULK_WRITE_STALL;
			usb2_transfer_start(ssc->sc_xfer[2]);
		}
		return;

	}
}

static void
ugensa_bulk_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ugensa_sub_softc *ssc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = ssc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		ssc->sc_flags &= ~UGENSA_FLAG_BULK_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ugensa_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct ugensa_sub_softc *ssc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usb2_com_put_data(ssc->sc_usb2_com_ptr, xfer->frbuffers, 0,
		    xfer->actlen);

	case USB_ST_SETUP:
		if (ssc->sc_flags & UGENSA_FLAG_BULK_READ_STALL) {
			usb2_transfer_start(ssc->sc_xfer[3]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			ssc->sc_flags |= UGENSA_FLAG_BULK_READ_STALL;
			usb2_transfer_start(ssc->sc_xfer[3]);
		}
		return;

	}
}

static void
ugensa_bulk_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ugensa_sub_softc *ssc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = ssc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		ssc->sc_flags &= ~UGENSA_FLAG_BULK_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ugensa_start_read(struct usb2_com_softc *ucom)
{
	struct ugensa_softc *sc = ucom->sc_parent;
	struct ugensa_sub_softc *ssc = sc->sc_sub + ucom->sc_portno;

	usb2_transfer_start(ssc->sc_xfer[1]);
	return;
}

static void
ugensa_stop_read(struct usb2_com_softc *ucom)
{
	struct ugensa_softc *sc = ucom->sc_parent;
	struct ugensa_sub_softc *ssc = sc->sc_sub + ucom->sc_portno;

	usb2_transfer_stop(ssc->sc_xfer[3]);
	usb2_transfer_stop(ssc->sc_xfer[1]);
	return;
}

static void
ugensa_start_write(struct usb2_com_softc *ucom)
{
	struct ugensa_softc *sc = ucom->sc_parent;
	struct ugensa_sub_softc *ssc = sc->sc_sub + ucom->sc_portno;

	usb2_transfer_start(ssc->sc_xfer[0]);
	return;
}

static void
ugensa_stop_write(struct usb2_com_softc *ucom)
{
	struct ugensa_softc *sc = ucom->sc_parent;
	struct ugensa_sub_softc *ssc = sc->sc_sub + ucom->sc_portno;

	usb2_transfer_stop(ssc->sc_xfer[2]);
	usb2_transfer_stop(ssc->sc_xfer[0]);
	return;
}
