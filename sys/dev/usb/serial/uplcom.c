/*	$NetBSD: uplcom.c,v 1.21 2001/11/13 06:24:56 lukem Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 2001-2003, 2005 Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ichiro FUKUHARA (ichiro@ichiro.org).
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
 * This driver supports several USB-to-RS232 serial adapters driven by
 * Prolific PL-2303, PL-2303X and probably PL-2303HX USB-to-RS232
 * bridge chip.  The adapters are sold under many different brand
 * names.
 *
 * Datasheets are available at Prolific www site at
 * http://www.prolific.com.tw.  The datasheets don't contain full
 * programming information for the chip.
 *
 * PL-2303HX is probably programmed the same as PL-2303X.
 *
 * There are several differences between PL-2303 and PL-2303(H)X.
 * PL-2303(H)X can do higher bitrate in bulk mode, has _probably_
 * different command for controlling CRTSCTS and needs special
 * sequence of commands for initialization which aren't also
 * documented in the datasheet.
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
#include <dev/usb/usb_cdc.h>
#include "usbdevs.h"

#define	USB_DEBUG_VAR uplcom_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/serial/usb_serial.h>

#if USB_DEBUG
static int uplcom_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, uplcom, CTLFLAG_RW, 0, "USB uplcom");
SYSCTL_INT(_hw_usb_uplcom, OID_AUTO, debug, CTLFLAG_RW,
    &uplcom_debug, 0, "Debug level");
#endif

#define	UPLCOM_MODVER			1	/* module version */

#define	UPLCOM_CONFIG_INDEX		0
#define	UPLCOM_IFACE_INDEX		0
#define	UPLCOM_SECOND_IFACE_INDEX	1

#ifndef UPLCOM_INTR_INTERVAL
#define	UPLCOM_INTR_INTERVAL		0	/* default */
#endif

#define	UPLCOM_BULK_BUF_SIZE 1024	/* bytes */

#define	UPLCOM_SET_REQUEST		0x01
#define	UPLCOM_SET_CRTSCTS		0x41
#define	UPLCOM_SET_CRTSCTS_PL2303X	0x61
#define	RSAQ_STATUS_CTS			0x80
#define	RSAQ_STATUS_DSR			0x02
#define	RSAQ_STATUS_DCD			0x01

#define	TYPE_PL2303			0
#define	TYPE_PL2303X			1

enum {
	UPLCOM_BULK_DT_WR,
	UPLCOM_BULK_DT_RD,
	UPLCOM_INTR_DT_RD,
	UPLCOM_N_TRANSFER,
};

struct uplcom_softc {
	struct ucom_super_softc sc_super_ucom;
	struct ucom_softc sc_ucom;

	struct usb_xfer *sc_xfer[UPLCOM_N_TRANSFER];
	struct usb_device *sc_udev;
	struct mtx sc_mtx;

	uint16_t sc_line;

	uint8_t	sc_lsr;			/* local status register */
	uint8_t	sc_msr;			/* uplcom status register */
	uint8_t	sc_chiptype;		/* type of chip */
	uint8_t	sc_ctrl_iface_no;
	uint8_t	sc_data_iface_no;
	uint8_t	sc_iface_index[2];
};

/* prototypes */

static usb_error_t uplcom_reset(struct uplcom_softc *, struct usb_device *);
static int	uplcom_pl2303x_init(struct usb_device *);
static void	uplcom_cfg_set_dtr(struct ucom_softc *, uint8_t);
static void	uplcom_cfg_set_rts(struct ucom_softc *, uint8_t);
static void	uplcom_cfg_set_break(struct ucom_softc *, uint8_t);
static int	uplcom_pre_param(struct ucom_softc *, struct termios *);
static void	uplcom_cfg_param(struct ucom_softc *, struct termios *);
static void	uplcom_start_read(struct ucom_softc *);
static void	uplcom_stop_read(struct ucom_softc *);
static void	uplcom_start_write(struct ucom_softc *);
static void	uplcom_stop_write(struct ucom_softc *);
static void	uplcom_cfg_get_status(struct ucom_softc *, uint8_t *,
		    uint8_t *);
static void	uplcom_poll(struct ucom_softc *ucom);

static device_probe_t uplcom_probe;
static device_attach_t uplcom_attach;
static device_detach_t uplcom_detach;

static usb_callback_t uplcom_intr_callback;
static usb_callback_t uplcom_write_callback;
static usb_callback_t uplcom_read_callback;

static const struct usb_config uplcom_config_data[UPLCOM_N_TRANSFER] = {

	[UPLCOM_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = UPLCOM_BULK_BUF_SIZE,
		.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.callback = &uplcom_write_callback,
		.if_index = 0,
	},

	[UPLCOM_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = UPLCOM_BULK_BUF_SIZE,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = &uplcom_read_callback,
		.if_index = 0,
	},

	[UPLCOM_INTR_DT_RD] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.bufsize = 0,	/* use wMaxPacketSize */
		.callback = &uplcom_intr_callback,
		.if_index = 1,
	},
};

static struct ucom_callback uplcom_callback = {
	.ucom_cfg_get_status = &uplcom_cfg_get_status,
	.ucom_cfg_set_dtr = &uplcom_cfg_set_dtr,
	.ucom_cfg_set_rts = &uplcom_cfg_set_rts,
	.ucom_cfg_set_break = &uplcom_cfg_set_break,
	.ucom_cfg_param = &uplcom_cfg_param,
	.ucom_pre_param = &uplcom_pre_param,
	.ucom_start_read = &uplcom_start_read,
	.ucom_stop_read = &uplcom_stop_read,
	.ucom_start_write = &uplcom_start_write,
	.ucom_stop_write = &uplcom_stop_write,
	.ucom_poll = &uplcom_poll,
};

#define	UPLCOM_DEV(v,p,rl,rh,t)				\
  { USB_VENDOR(USB_VENDOR_##v), USB_PRODUCT(USB_PRODUCT_##v##_##p), \
    USB_DEV_BCD_GTEQ(rl), USB_DEV_BCD_LTEQ(rh), USB_DRIVER_INFO(TYPE_##t) }

static const struct usb_device_id uplcom_devs[] = {
	/* Belkin F5U257 */
	UPLCOM_DEV(BELKIN, F5U257, 0, 0xFFFF, PL2303X),
	/* I/O DATA USB-RSAQ */
	UPLCOM_DEV(IODATA, USBRSAQ, 0, 0xFFFF, PL2303),
	/* I/O DATA USB-RSAQ2 */
	UPLCOM_DEV(PROLIFIC, RSAQ2, 0, 0xFFFF, PL2303),
	/* I/O DATA USB-RSAQ3 */
	UPLCOM_DEV(PROLIFIC, RSAQ3, 0, 0xFFFF, PL2303X),
	/* PLANEX USB-RS232 URS-03 */
	UPLCOM_DEV(ATEN, UC232A, 0, 0xFFFF, PL2303),
	/* TrendNet TU-S9 */
	UPLCOM_DEV(PROLIFIC, PL2303, 0x0400, 0xFFFF, PL2303X),
	/* ST Lab USB-SERIAL-4 */
	UPLCOM_DEV(PROLIFIC, PL2303, 0x0300, 0x03FF, PL2303X),
	/* IOGEAR/ATEN UC-232A (also ST Lab USB-SERIAL-1) */
	UPLCOM_DEV(PROLIFIC, PL2303, 0, 0x02FF, PL2303),
	/* TDK USB-PHS Adapter UHA6400 */
	UPLCOM_DEV(TDK, UHA6400, 0, 0xFFFF, PL2303),
	/* RATOC REX-USB60 */
	UPLCOM_DEV(RATOC, REXUSB60, 0, 0xFFFF, PL2303),
	/* ELECOM UC-SGT */
	UPLCOM_DEV(ELECOM, UCSGT, 0, 0xFFFF, PL2303),
	UPLCOM_DEV(ELECOM, UCSGT0, 0, 0xFFFF, PL2303),
	/* Sagem USB-Serial Controller */
	UPLCOM_DEV(SAGEM, USBSERIAL, 0, 0xFFFF, PL2303X),
	/* Sony Ericsson USB Cable */
	UPLCOM_DEV(SONYERICSSON, DCU10, 0, 0xFFFF, PL2303),
	/* SOURCENEXT KeikaiDenwa 8 */
	UPLCOM_DEV(SOURCENEXT, KEIKAI8, 0, 0xFFFF, PL2303),
	/* SOURCENEXT KeikaiDenwa 8 with charger */
	UPLCOM_DEV(SOURCENEXT, KEIKAI8_CHG, 0, 0, PL2303),
	/* HAL Corporation Crossam2+USB */
	UPLCOM_DEV(HAL, IMR001, 0, 0xFFFF, PL2303),
	/* Sitecom USB to Serial */
	UPLCOM_DEV(SITECOM, SERIAL, 0, 0xFFFF, PL2303),
	/* Tripp-Lite U209-000-R */
	UPLCOM_DEV(TRIPPLITE, U209, 0, 0xFFFF, PL2303X),
	UPLCOM_DEV(RADIOSHACK, USBCABLE, 0, 0xFFFF, PL2303),
	/* Prolific Pharos */
	UPLCOM_DEV(PROLIFIC, PHAROS, 0, 0xFFFF, PL2303),
	/* Willcom W-SIM */
	UPLCOM_DEV(PROLIFIC2, WSIM, 0, 0xFFFF, PL2303X),
	/* Mobile Action MA-620 Infrared Adapter */
	UPLCOM_DEV(MOBILEACTION, MA620, 0, 0xFFFF, PL2303X),
};
#undef UPLCOM_DEV

static device_method_t uplcom_methods[] = {
	DEVMETHOD(device_probe, uplcom_probe),
	DEVMETHOD(device_attach, uplcom_attach),
	DEVMETHOD(device_detach, uplcom_detach),
	{0, 0}
};

static devclass_t uplcom_devclass;

static driver_t uplcom_driver = {
	.name = "uplcom",
	.methods = uplcom_methods,
	.size = sizeof(struct uplcom_softc),
};

DRIVER_MODULE(uplcom, uhub, uplcom_driver, uplcom_devclass, NULL, 0);
MODULE_DEPEND(uplcom, ucom, 1, 1, 1);
MODULE_DEPEND(uplcom, usb, 1, 1, 1);
MODULE_VERSION(uplcom, UPLCOM_MODVER);

static int
uplcom_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	DPRINTFN(11, "\n");

	if (uaa->usb_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != UPLCOM_CONFIG_INDEX) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != UPLCOM_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usbd_lookup_id_by_uaa(uplcom_devs, sizeof(uplcom_devs), uaa));
}

static int
uplcom_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct uplcom_softc *sc = device_get_softc(dev);
	struct usb_interface *iface;
	struct usb_interface_descriptor *id;
	int error;

	DPRINTFN(11, "\n");

	device_set_usb_desc(dev);
	mtx_init(&sc->sc_mtx, "uplcom", NULL, MTX_DEF);

	DPRINTF("sc = %p\n", sc);

	sc->sc_chiptype = USB_GET_DRIVER_INFO(uaa);
	sc->sc_udev = uaa->device;

	DPRINTF("chiptype: %s\n",
	    (sc->sc_chiptype == TYPE_PL2303X) ?
	    "2303X" : "2303");

	/*
	 * USB-RSAQ1 has two interface
	 *
	 *  USB-RSAQ1       | USB-RSAQ2
	 * -----------------+-----------------
	 * Interface 0      |Interface 0
	 *  Interrupt(0x81) | Interrupt(0x81)
	 * -----------------+ BulkIN(0x02)
	 * Interface 1	    | BulkOUT(0x83)
	 *   BulkIN(0x02)   |
	 *   BulkOUT(0x83)  |
	 */

	sc->sc_ctrl_iface_no = uaa->info.bIfaceNum;
	sc->sc_iface_index[1] = UPLCOM_IFACE_INDEX;

	iface = usbd_get_iface(uaa->device, UPLCOM_SECOND_IFACE_INDEX);
	if (iface) {
		id = usbd_get_interface_descriptor(iface);
		if (id == NULL) {
			device_printf(dev, "no interface descriptor (2)\n");
			goto detach;
		}
		sc->sc_data_iface_no = id->bInterfaceNumber;
		sc->sc_iface_index[0] = UPLCOM_SECOND_IFACE_INDEX;
		usbd_set_parent_iface(uaa->device,
		    UPLCOM_SECOND_IFACE_INDEX, uaa->info.bIfaceIndex);
	} else {
		sc->sc_data_iface_no = sc->sc_ctrl_iface_no;
		sc->sc_iface_index[0] = UPLCOM_IFACE_INDEX;
	}

	error = usbd_transfer_setup(uaa->device,
	    sc->sc_iface_index, sc->sc_xfer, uplcom_config_data,
	    UPLCOM_N_TRANSFER, sc, &sc->sc_mtx);
	if (error) {
		DPRINTF("one or more missing USB endpoints, "
		    "error=%s\n", usbd_errstr(error));
		goto detach;
	}
	error = uplcom_reset(sc, uaa->device);
	if (error) {
		device_printf(dev, "reset failed, error=%s\n",
		    usbd_errstr(error));
		goto detach;
	}
	/* clear stall at first run */
	mtx_lock(&sc->sc_mtx);
	usbd_xfer_set_stall(sc->sc_xfer[UPLCOM_BULK_DT_WR]);
	usbd_xfer_set_stall(sc->sc_xfer[UPLCOM_BULK_DT_RD]);
	mtx_unlock(&sc->sc_mtx);

	error = ucom_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &uplcom_callback, &sc->sc_mtx);
	if (error) {
		goto detach;
	}
	/*
	 * do the initialization during attach so that the system does not
	 * sleep during open:
	 */
	if (sc->sc_chiptype == TYPE_PL2303X) {
		if (uplcom_pl2303x_init(uaa->device)) {
			device_printf(dev, "init failed\n");
			goto detach;
		}
	}
	return (0);

detach:
	uplcom_detach(dev);
	return (ENXIO);
}

static int
uplcom_detach(device_t dev)
{
	struct uplcom_softc *sc = device_get_softc(dev);

	DPRINTF("sc=%p\n", sc);

	ucom_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);
	usbd_transfer_unsetup(sc->sc_xfer, UPLCOM_N_TRANSFER);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static usb_error_t
uplcom_reset(struct uplcom_softc *sc, struct usb_device *udev)
{
	struct usb_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UPLCOM_SET_REQUEST;
	USETW(req.wValue, 0);
	req.wIndex[0] = sc->sc_data_iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);

	return (usbd_do_request(udev, NULL, &req, NULL));
}

struct pl2303x_init {
	uint8_t	req_type;
	uint8_t	request;
	uint16_t value;
	uint16_t index;
	uint16_t length;
};

static const struct pl2303x_init pl2303x[] = {
	{UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8484, 0, 1},
	{UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x0404, 0, 0},
	{UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8484, 0, 1},
	{UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8383, 0, 1},
	{UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8484, 0, 1},
	{UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x0404, 1, 0},
	{UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8484, 0, 1},
	{UT_READ_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0x8383, 0, 1},
	{UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 0, 1, 0},
	{UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 1, 0, 0},
	{UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 2, 0x44, 0},
	{UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 8, 0, 0},
	{UT_WRITE_VENDOR_DEVICE, UPLCOM_SET_REQUEST, 9, 0, 0},
};

#define	N_PL2302X_INIT	(sizeof(pl2303x)/sizeof(pl2303x[0]))

static int
uplcom_pl2303x_init(struct usb_device *udev)
{
	struct usb_device_request req;
	usb_error_t err;
	uint8_t buf[4];
	uint8_t i;

	for (i = 0; i != N_PL2302X_INIT; i++) {
		req.bmRequestType = pl2303x[i].req_type;
		req.bRequest = pl2303x[i].request;
		USETW(req.wValue, pl2303x[i].value);
		USETW(req.wIndex, pl2303x[i].index);
		USETW(req.wLength, pl2303x[i].length);

		err = usbd_do_request(udev, NULL, &req, buf);
		if (err) {
			DPRINTF("error=%s\n", usbd_errstr(err));
			return (EIO);
		}
	}
	return (0);
}

static void
uplcom_cfg_set_dtr(struct ucom_softc *ucom, uint8_t onoff)
{
	struct uplcom_softc *sc = ucom->sc_parent;
	struct usb_device_request req;

	DPRINTF("onoff = %d\n", onoff);

	if (onoff)
		sc->sc_line |= UCDC_LINE_DTR;
	else
		sc->sc_line &= ~UCDC_LINE_DTR;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, sc->sc_line);
	req.wIndex[0] = sc->sc_data_iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);

	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);
}

static void
uplcom_cfg_set_rts(struct ucom_softc *ucom, uint8_t onoff)
{
	struct uplcom_softc *sc = ucom->sc_parent;
	struct usb_device_request req;

	DPRINTF("onoff = %d\n", onoff);

	if (onoff)
		sc->sc_line |= UCDC_LINE_RTS;
	else
		sc->sc_line &= ~UCDC_LINE_RTS;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, sc->sc_line);
	req.wIndex[0] = sc->sc_data_iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);

	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);
}

static void
uplcom_cfg_set_break(struct ucom_softc *ucom, uint8_t onoff)
{
	struct uplcom_softc *sc = ucom->sc_parent;
	struct usb_device_request req;
	uint16_t temp;

	DPRINTF("onoff = %d\n", onoff);

	temp = (onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_BREAK;
	USETW(req.wValue, temp);
	req.wIndex[0] = sc->sc_data_iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);

	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);
}

static const int32_t uplcom_rates[] = {
	75, 150, 300, 600, 1200, 1800, 2400, 3600, 4800, 7200, 9600, 14400,
	19200, 28800, 38400, 57600, 115200,
	/*
	 * Higher speeds are probably possible. PL2303X supports up to
	 * 6Mb and can set any rate
	 */
	230400, 460800, 614400, 921600, 1228800
};

#define	N_UPLCOM_RATES	(sizeof(uplcom_rates)/sizeof(uplcom_rates[0]))

static int
uplcom_pre_param(struct ucom_softc *ucom, struct termios *t)
{
	uint8_t i;

	DPRINTF("\n");

	/* check requested baud rate */

	for (i = 0;; i++) {

		if (i != N_UPLCOM_RATES) {
			if (uplcom_rates[i] == t->c_ospeed) {
				break;
			}
		} else {
			DPRINTF("invalid baud rate (%d)\n", t->c_ospeed);
			return (EIO);
		}
	}

	return (0);
}

static void
uplcom_cfg_param(struct ucom_softc *ucom, struct termios *t)
{
	struct uplcom_softc *sc = ucom->sc_parent;
	struct usb_cdc_line_state ls;
	struct usb_device_request req;

	DPRINTF("sc = %p\n", sc);

	bzero(&ls, sizeof(ls));

	USETDW(ls.dwDTERate, t->c_ospeed);

	if (t->c_cflag & CSTOPB) {
		ls.bCharFormat = UCDC_STOP_BIT_2;
	} else {
		ls.bCharFormat = UCDC_STOP_BIT_1;
	}

	if (t->c_cflag & PARENB) {
		if (t->c_cflag & PARODD) {
			ls.bParityType = UCDC_PARITY_ODD;
		} else {
			ls.bParityType = UCDC_PARITY_EVEN;
		}
	} else {
		ls.bParityType = UCDC_PARITY_NONE;
	}

	switch (t->c_cflag & CSIZE) {
	case CS5:
		ls.bDataBits = 5;
		break;
	case CS6:
		ls.bDataBits = 6;
		break;
	case CS7:
		ls.bDataBits = 7;
		break;
	case CS8:
		ls.bDataBits = 8;
		break;
	}

	DPRINTF("rate=%d fmt=%d parity=%d bits=%d\n",
	    UGETDW(ls.dwDTERate), ls.bCharFormat,
	    ls.bParityType, ls.bDataBits);

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_LINE_CODING;
	USETW(req.wValue, 0);
	req.wIndex[0] = sc->sc_data_iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, UCDC_LINE_STATE_LENGTH);

	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, &ls, 0, 1000);

	if (t->c_cflag & CRTSCTS) {

		DPRINTF("crtscts = on\n");

		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
		req.bRequest = UPLCOM_SET_REQUEST;
		USETW(req.wValue, 0);
		if (sc->sc_chiptype == TYPE_PL2303X)
			USETW(req.wIndex, UPLCOM_SET_CRTSCTS_PL2303X);
		else
			USETW(req.wIndex, UPLCOM_SET_CRTSCTS);
		USETW(req.wLength, 0);

		ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
		    &req, NULL, 0, 1000);
	} else {
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
		req.bRequest = UPLCOM_SET_REQUEST;
		USETW(req.wValue, 0);
		USETW(req.wIndex, 0);
		USETW(req.wLength, 0);
		ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
		    &req, NULL, 0, 1000);
	}
}

static void
uplcom_start_read(struct ucom_softc *ucom)
{
	struct uplcom_softc *sc = ucom->sc_parent;

	/* start interrupt endpoint */
	usbd_transfer_start(sc->sc_xfer[UPLCOM_INTR_DT_RD]);

	/* start read endpoint */
	usbd_transfer_start(sc->sc_xfer[UPLCOM_BULK_DT_RD]);
}

static void
uplcom_stop_read(struct ucom_softc *ucom)
{
	struct uplcom_softc *sc = ucom->sc_parent;

	/* stop interrupt endpoint */
	usbd_transfer_stop(sc->sc_xfer[UPLCOM_INTR_DT_RD]);

	/* stop read endpoint */
	usbd_transfer_stop(sc->sc_xfer[UPLCOM_BULK_DT_RD]);
}

static void
uplcom_start_write(struct ucom_softc *ucom)
{
	struct uplcom_softc *sc = ucom->sc_parent;

	usbd_transfer_start(sc->sc_xfer[UPLCOM_BULK_DT_WR]);
}

static void
uplcom_stop_write(struct ucom_softc *ucom)
{
	struct uplcom_softc *sc = ucom->sc_parent;

	usbd_transfer_stop(sc->sc_xfer[UPLCOM_BULK_DT_WR]);
}

static void
uplcom_cfg_get_status(struct ucom_softc *ucom, uint8_t *lsr, uint8_t *msr)
{
	struct uplcom_softc *sc = ucom->sc_parent;

	DPRINTF("\n");

	*lsr = sc->sc_lsr;
	*msr = sc->sc_msr;
}

static void
uplcom_intr_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uplcom_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint8_t buf[9];
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTF("actlen = %u\n", actlen);

		if (actlen >= 9) {

			pc = usbd_xfer_get_frame(xfer, 0);
			usbd_copy_out(pc, 0, buf, sizeof(buf));

			DPRINTF("status = 0x%02x\n", buf[8]);

			sc->sc_lsr = 0;
			sc->sc_msr = 0;

			if (buf[8] & RSAQ_STATUS_CTS) {
				sc->sc_msr |= SER_CTS;
			}
			if (buf[8] & RSAQ_STATUS_DSR) {
				sc->sc_msr |= SER_DSR;
			}
			if (buf[8] & RSAQ_STATUS_DCD) {
				sc->sc_msr |= SER_DCD;
			}
			ucom_status_change(&sc->sc_ucom);
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
uplcom_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uplcom_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
tr_setup:
		pc = usbd_xfer_get_frame(xfer, 0);
		if (ucom_get_data(&sc->sc_ucom, pc, 0,
		    UPLCOM_BULK_BUF_SIZE, &actlen)) {

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
uplcom_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uplcom_softc *sc = usbd_xfer_softc(xfer);
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
uplcom_poll(struct ucom_softc *ucom)
{
	struct uplcom_softc *sc = ucom->sc_parent;
	usbd_transfer_poll(sc->sc_xfer, UPLCOM_N_TRANSFER);
}
