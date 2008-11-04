/*	$NetBSD: uscanner.c,v 1.30 2002/07/11 21:14:36 augustss Exp$	*/

/* Also already merged from NetBSD:
 *	$NetBSD: uscanner.c,v 1.33 2002/09/23 05:51:24 simonb Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology
 * and Nick Hibma (n_hibma@qubesoft.com).
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

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	USB_DEBUG_VAR uscanner_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_mbuf.h>
#include <dev/usb2/core/usb2_dev.h>

#if USB_DEBUG
static int uscanner_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, uscanner, CTLFLAG_RW, 0, "USB uscanner");
SYSCTL_INT(_hw_usb2_uscanner, OID_AUTO, uscanner, CTLFLAG_RW, &uscanner_debug,
    0, "uscanner debug level");
#endif

/*
 * uscanner transfers macros definition.
 */
#define	USCANNER_BSIZE			(1 << 15)
#define	USCANNER_IFQ_MAXLEN		2
#define	USCANNER_N_TRANSFER		4

/*
 * Transfers stallings handling flags definition.
 */
#define	USCANNER_FLAG_READ_STALL	0x01
#define	USCANNER_FLAG_WRITE_STALL	0x02

/*
 * uscanner_info flags definition.
 */
#define	USCANNER_FLAG_KEEP_OPEN		0x04

struct uscanner_softc {
	struct usb2_fifo_sc sc_fifo;
	struct mtx sc_mtx;

	struct usb2_xfer *sc_xfer[USCANNER_N_TRANSFER];

	uint8_t	sc_flags;		/* Used to prevent stalls */
};

/*
 * Prototypes for driver handling routines (sorted by use).
 */
static device_probe_t uscanner_probe;
static device_attach_t uscanner_attach;
static device_detach_t uscanner_detach;

/*
 * Prototypes for xfer transfer callbacks.
 */
static usb2_callback_t uscanner_read_callback;
static usb2_callback_t uscanner_read_clear_stall_callback;
static usb2_callback_t uscanner_write_callback;
static usb2_callback_t uscanner_write_clear_stall_callback;

/*
 * Prototypes for the character device handling routines.
 */
static usb2_fifo_close_t uscanner_close;
static usb2_fifo_cmd_t uscanner_start_read;
static usb2_fifo_cmd_t uscanner_start_write;
static usb2_fifo_cmd_t uscanner_stop_read;
static usb2_fifo_cmd_t uscanner_stop_write;
static usb2_fifo_open_t uscanner_open;

static struct usb2_fifo_methods uscanner_fifo_methods = {
	.f_close = &uscanner_close,
	.f_open = &uscanner_open,
	.f_start_read = &uscanner_start_read,
	.f_start_write = &uscanner_start_write,
	.f_stop_read = &uscanner_stop_read,
	.f_stop_write = &uscanner_stop_write,
	.basename[0] = "uscanner",
};

/*
 * xfer transfers array.  Resolve-stalling callbacks are marked as control
 * transfers.
 */
static const struct usb2_config uscanner_config[USCANNER_N_TRANSFER] = {
	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = USCANNER_BSIZE,
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,.proxy_buffer = 1,.force_short_xfer = 1,},
		.mh.callback = &uscanner_write_callback,
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = USCANNER_BSIZE,
		.mh.flags = {.pipe_bof = 1,.proxy_buffer = 1,.short_xfer_ok = 1,},
		.mh.callback = &uscanner_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &uscanner_write_clear_stall_callback,
		.mh.timeout = 1000,
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &uscanner_read_clear_stall_callback,
		.mh.timeout = 1000,
		.mh.interval = 50,	/* 50ms */
	},
};

static devclass_t uscanner_devclass;

static device_method_t uscanner_methods[] = {
	DEVMETHOD(device_probe, uscanner_probe),
	DEVMETHOD(device_attach, uscanner_attach),
	DEVMETHOD(device_detach, uscanner_detach),
	{0, 0}
};

static driver_t uscanner_driver = {
	.name = "uscanner",
	.methods = uscanner_methods,
	.size = sizeof(struct uscanner_softc),
};

DRIVER_MODULE(uscanner, ushub, uscanner_driver, uscanner_devclass, NULL, 0);
MODULE_DEPEND(uscanner, usb2_image, 1, 1, 1);
MODULE_DEPEND(uscanner, usb2_core, 1, 1, 1);

/*
 * USB scanners device IDs
 */
static const struct usb2_device_id uscanner_devs[] = {
	/* Acer */
	{USB_VPI(USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_320U, 0)},
	{USB_VPI(USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_640U, 0)},
	{USB_VPI(USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_640BT, 0)},
	{USB_VPI(USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_620U, 0)},
	{USB_VPI(USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_1240U, 0)},
	{USB_VPI(USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_C310U, 0)},
	{USB_VPI(USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_4300U, 0)},
	/* AGFA */
	{USB_VPI(USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCAN1236U, 0)},
	{USB_VPI(USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCAN1212U, 0)},
	{USB_VPI(USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCAN1212U2, 0)},
	{USB_VPI(USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANTOUCH, 0)},
	{USB_VPI(USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE40, 0)},
	{USB_VPI(USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE50, 0)},
	{USB_VPI(USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE20, 0)},
	{USB_VPI(USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE25, 0)},
	{USB_VPI(USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE26, 0)},
	{USB_VPI(USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE52, 0)},
	/* Avision */
	{USB_VPI(USB_VENDOR_AVISION, USB_PRODUCT_AVISION_1200U, 0)},
	/* Canon */
	{USB_VPI(USB_VENDOR_CANON, USB_PRODUCT_CANON_N656U, 0)},
	{USB_VPI(USB_VENDOR_CANON, USB_PRODUCT_CANON_N676U, 0)},
	{USB_VPI(USB_VENDOR_CANON, USB_PRODUCT_CANON_N1220U, 0)},
	{USB_VPI(USB_VENDOR_CANON, USB_PRODUCT_CANON_D660U, 0)},
	{USB_VPI(USB_VENDOR_CANON, USB_PRODUCT_CANON_N1240U, 0)},
	{USB_VPI(USB_VENDOR_CANON, USB_PRODUCT_CANON_LIDE25, 0)},
	/* Epson */
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_636, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_610, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1200, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1240, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1250, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1270, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1600, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1640, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_640U, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1650, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1660, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1670, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1260, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_RX425, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_3200, USCANNER_FLAG_KEEP_OPEN)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_GT9700F, USCANNER_FLAG_KEEP_OPEN)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_CX5400, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_DX7400, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_GT9300UF, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_2480, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_3500, USCANNER_FLAG_KEEP_OPEN)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_3590, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_4200, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_4800, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_4990, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_5000, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_6000, 0)},
	{USB_VPI(USB_VENDOR_EPSON, USB_PRODUCT_EPSON_DX8400, 0)},
	/* HP */
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_2200C, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_3300C, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_3400CSE, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_4100C, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_4200C, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_4300C, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_4670V, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_S20, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_5200C, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_5300C, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_5400C, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_6200C, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_6300C, 0)},
	{USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_82x0C, 0)},
	/* Kye */
	{USB_VPI(USB_VENDOR_KYE, USB_PRODUCT_KYE_VIVIDPRO, 0)},
	/* Microtek */
	{USB_VPI(USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_X6U, 0)},
	{USB_VPI(USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_336CX, 0)},
	{USB_VPI(USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_336CX2, 0)},
	{USB_VPI(USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_C6, 0)},
	{USB_VPI(USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_V6USL, 0)},
	{USB_VPI(USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_V6USL2, 0)},
	{USB_VPI(USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_V6UL, 0)},
	/* Minolta */
	{USB_VPI(USB_VENDOR_MINOLTA, USB_PRODUCT_MINOLTA_5400, 0)},
	/* Mustek */
	{USB_VPI(USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_1200CU, 0)},
	{USB_VPI(USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_BEARPAW1200F, 0)},
	{USB_VPI(USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_BEARPAW1200TA, 0)},
	{USB_VPI(USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_600USB, 0)},
	{USB_VPI(USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_600CU, 0)},
	{USB_VPI(USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_1200USB, 0)},
	{USB_VPI(USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_1200UB, 0)},
	{USB_VPI(USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_1200USBPLUS, 0)},
	{USB_VPI(USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_1200CUPLUS, 0)},
	/* National */
	{USB_VPI(USB_VENDOR_NATIONAL, USB_PRODUCT_NATIONAL_BEARPAW1200, 0)},
	{USB_VPI(USB_VENDOR_NATIONAL, USB_PRODUCT_NATIONAL_BEARPAW2400, 0)},
	/* Nikon */
	{USB_VPI(USB_VENDOR_NIKON, USB_PRODUCT_NIKON_LS40, 0)},
	/* Primax */
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2X300, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2E300, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2300, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2E3002, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_9600, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_600U, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_6200, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_19200, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_1200U, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G600, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_636I, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2600, 0)},
	{USB_VPI(USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2E600, 0)},
	/* Scanlogic */
	{USB_VPI(USB_VENDOR_SCANLOGIC, USB_PRODUCT_SCANLOGIC_336CX, 0)},
	/* Ultima */
	{USB_VPI(USB_VENDOR_ULTIMA, USB_PRODUCT_ULTIMA_1200UBPLUS, 0)},
	/* UMAX */
	{USB_VPI(USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA1220U, 0)},
	{USB_VPI(USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA1236U, 0)},
	{USB_VPI(USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA2000U, 0)},
	{USB_VPI(USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA2100U, 0)},
	{USB_VPI(USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA2200U, 0)},
	{USB_VPI(USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA3400, 0)},
	/* Visioneer */
	{USB_VPI(USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_3000, 0)},
	{USB_VPI(USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_5300, 0)},
	{USB_VPI(USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_7600, 0)},
	{USB_VPI(USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_6100, 0)},
	{USB_VPI(USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_6200, 0)},
	{USB_VPI(USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_8100, 0)},
	{USB_VPI(USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_8600, 0)}
};

/*
 * uscanner device probing method.
 */
static int
uscanner_probe(device_t dev)
{
	struct usb2_attach_arg *uaa;

	DPRINTFN(11, "\n");

	uaa = device_get_ivars(dev);
	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	/* Give other class drivers a chance for multifunctional scanners. */
	if (uaa->use_generic == 0) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(uscanner_devs, sizeof(uscanner_devs), uaa));
}

/*
 * uscanner device attaching method.
 */
static int
uscanner_attach(device_t dev)
{
	struct usb2_attach_arg *uaa;
	struct uscanner_softc *sc;
	int unit;
	int error;

	uaa = device_get_ivars(dev);
	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	/*
	 * A first path softc structure filling.  sc_fifo and
	 * sc_xfer are initialised later.
	 */
	sc->sc_flags = USB_GET_DRIVER_INFO(uaa);
	mtx_init(&sc->sc_mtx, "uscanner mutex", NULL, MTX_DEF | MTX_RECURSE);

	/*
	 * Announce the device:
	 */
	device_set_usb2_desc(dev);

	/*
	 * Setup the transfer.
	 */
	if ((error = usb2_transfer_setup(uaa->device, &uaa->info.bIfaceIndex, sc->sc_xfer,
	    uscanner_config, USCANNER_N_TRANSFER, sc, &sc->sc_mtx))) {
		device_printf(dev, "could not setup transfers, "
		    "error=%s\n", usb2_errstr(error));
		goto detach;
	}
	/* set interface permissions */
	usb2_set_iface_perm(uaa->device, uaa->info.bIfaceIndex,
	    UID_ROOT, GID_OPERATOR, 0644);

	error = usb2_fifo_attach(uaa->device, sc, &sc->sc_mtx,
	    &uscanner_fifo_methods, &sc->sc_fifo,
	    unit, 0 - 1, uaa->info.bIfaceIndex);
	if (error) {
		goto detach;
	}
	return (0);

detach:
	uscanner_detach(dev);
	return (ENOMEM);
}

/*
 * uscanner device detaching method.
 */
static int
uscanner_detach(device_t dev)
{
	struct uscanner_softc *sc;

	sc = device_get_softc(dev);

	usb2_fifo_detach(&sc->sc_fifo);
	usb2_transfer_unsetup(sc->sc_xfer, USCANNER_N_TRANSFER);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

/*
 * Reading callback.  Implemented as an "in" bulk transfer.
 */
static void
uscanner_read_callback(struct usb2_xfer *xfer)
{
	struct uscanner_softc *sc;
	struct usb2_fifo *f;

	sc = xfer->priv_sc;
	f = sc->sc_fifo.fp[USB_FIFO_RX];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usb2_fifo_put_data(f, xfer->frbuffers, 0,
		    xfer->actlen, 1);

	case USB_ST_SETUP:
		/*
		 * If reading is in stall, just jump to clear stall callback and
		 * solve the situation.
		 */
		if (sc->sc_flags & USCANNER_FLAG_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer[3]);
			break;
		}
		if (usb2_fifo_put_bytes_max(f) != 0) {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flags |= USCANNER_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		break;
	}
	return;
}

/*
 * Removing stall on reading callback.
 */
static void
uscanner_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct uscanner_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~USCANNER_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

/*
 * Writing callback.  Implemented as an "out" bulk transfer.
 */
static void
uscanner_write_callback(struct usb2_xfer *xfer)
{
	struct uscanner_softc *sc;
	struct usb2_fifo *f;
	uint32_t actlen;

	sc = xfer->priv_sc;
	f = sc->sc_fifo.fp[USB_FIFO_TX];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
		/*
		 * If writing is in stall, just jump to clear stall callback and
		 * solve the situation.
		 */
		if (sc->sc_flags & USCANNER_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
			break;
		}
		/*
		 * Write datas, setup and perform hardware transfer.
		 */
		if (usb2_fifo_get_data(f, xfer->frbuffers, 0,
		    xfer->max_data_length, &actlen, 0)) {
			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flags |= USCANNER_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		break;
	}
	return;
}

/*
 * Removing stall on writing callback.
 */
static void
uscanner_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct uscanner_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~USCANNER_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

/*
 * uscanner character device opening method.
 */
static int
uscanner_open(struct usb2_fifo *fifo, int fflags, struct thread *td)
{
	struct uscanner_softc *sc;

	sc = fifo->priv_sc0;

	if (!(sc->sc_flags & USCANNER_FLAG_KEEP_OPEN)) {
		if (fflags & FWRITE) {
			sc->sc_flags |= USCANNER_FLAG_WRITE_STALL;
		}
		if (fflags & FREAD) {
			sc->sc_flags |= USCANNER_FLAG_READ_STALL;
		}
	}
	if (fflags & FREAD) {
		if (usb2_fifo_alloc_buffer(fifo,
		    sc->sc_xfer[1]->max_data_length,
		    USCANNER_IFQ_MAXLEN)) {
			return (ENOMEM);
		}
	}
	if (fflags & FWRITE) {
		if (usb2_fifo_alloc_buffer(fifo,
		    sc->sc_xfer[0]->max_data_length,
		    USCANNER_IFQ_MAXLEN)) {
			return (ENOMEM);
		}
	}
	return (0);
}

static void
uscanner_close(struct usb2_fifo *fifo, int fflags, struct thread *td)
{
	if (fflags & (FREAD | FWRITE)) {
		usb2_fifo_free_buffer(fifo);
	}
	return;
}

/*
 * uscanner character device start reading method.
 */
static void
uscanner_start_read(struct usb2_fifo *fifo)
{
	struct uscanner_softc *sc;

	sc = fifo->priv_sc0;
	usb2_transfer_start(sc->sc_xfer[1]);
}

/*
 * uscanner character device start writing method.
 */
static void
uscanner_start_write(struct usb2_fifo *fifo)
{
	struct uscanner_softc *sc;

	sc = fifo->priv_sc0;
	usb2_transfer_start(sc->sc_xfer[0]);
}

/*
 * uscanner character device stop reading method.
 */
static void
uscanner_stop_read(struct usb2_fifo *fifo)
{
	struct uscanner_softc *sc;

	sc = fifo->priv_sc0;
	usb2_transfer_stop(sc->sc_xfer[3]);
	usb2_transfer_stop(sc->sc_xfer[1]);
}

/*
 * uscanner character device stop writing method.
 */
static void
uscanner_stop_write(struct usb2_fifo *fifo)
{
	struct uscanner_softc *sc;

	sc = fifo->priv_sc0;
	usb2_transfer_stop(sc->sc_xfer[2]);
	usb2_transfer_stop(sc->sc_xfer[0]);
}
