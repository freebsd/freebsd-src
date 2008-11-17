#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 2003 Scott Long
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
 *
 */

/*
 * Driver for the MCT (Magic Control Technology) USB-RS232 Converter.
 * Based on the superb documentation from the linux mct_u232 driver by
 * Wolfgang Grandeggar <wolfgang@cec.ch>.
 * This device smells a lot like the Belkin F5U103, except that it has
 * suffered some mild brain-damage.  This driver is based off of the ubsa.c
 * driver from Alexander Kabaev <kan@freebsd.org>.  Merging the two together
 * might be useful, though the subtle differences might lead to lots of
 * #ifdef's.
 */

/*
 * NOTE: all function names beginning like "umct_cfg_" can only
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
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_device.h>

#include <dev/usb2/serial/usb2_serial.h>

/* The UMCT advertises the standard 8250 UART registers */
#define	UMCT_GET_MSR		2	/* Get Modem Status Register */
#define	UMCT_GET_MSR_SIZE	1
#define	UMCT_GET_LCR		6	/* Get Line Control Register */
#define	UMCT_GET_LCR_SIZE	1
#define	UMCT_SET_BAUD		5	/* Set the Baud Rate Divisor */
#define	UMCT_SET_BAUD_SIZE	4
#define	UMCT_SET_LCR		7	/* Set Line Control Register */
#define	UMCT_SET_LCR_SIZE	1
#define	UMCT_SET_MCR		10	/* Set Modem Control Register */
#define	UMCT_SET_MCR_SIZE	1

#define	UMCT_INTR_INTERVAL	100
#define	UMCT_IFACE_INDEX	0
#define	UMCT_CONFIG_INDEX	1

#define	UMCT_ENDPT_MAX		6	/* units */

struct umct_softc {
	struct usb2_com_super_softc sc_super_ucom;
	struct usb2_com_softc sc_ucom;

	struct usb2_device *sc_udev;
	struct usb2_xfer *sc_xfer[UMCT_ENDPT_MAX];

	uint32_t sc_unit;

	uint16_t sc_obufsize;

	uint8_t	sc_lsr;
	uint8_t	sc_msr;
	uint8_t	sc_lcr;
	uint8_t	sc_mcr;

	uint8_t	sc_name[16];
	uint8_t	sc_flags;
#define	UMCT_FLAG_READ_STALL    0x01
#define	UMCT_FLAG_WRITE_STALL   0x02
#define	UMCT_FLAG_INTR_STALL    0x04
	uint8_t	sc_iface_no;
};

/* prototypes */

static device_probe_t umct_probe;
static device_attach_t umct_attach;
static device_detach_t umct_detach;

static usb2_callback_t umct_intr_clear_stall_callback;
static usb2_callback_t umct_intr_callback;
static usb2_callback_t umct_write_callback;
static usb2_callback_t umct_write_clear_stall_callback;
static usb2_callback_t umct_read_callback;
static usb2_callback_t umct_read_clear_stall_callback;

static void umct_cfg_do_request(struct umct_softc *sc, uint8_t request, uint16_t len, uint32_t value);
static void umct_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *lsr, uint8_t *msr);
static void umct_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff);
static void umct_cfg_set_dtr(struct usb2_com_softc *ucom, uint8_t onoff);
static void umct_cfg_set_rts(struct usb2_com_softc *ucom, uint8_t onoff);
static uint8_t umct_calc_baud(uint32_t baud);
static int umct_pre_param(struct usb2_com_softc *ucom, struct termios *ti);
static void umct_cfg_param(struct usb2_com_softc *ucom, struct termios *ti);
static void umct_start_read(struct usb2_com_softc *ucom);
static void umct_stop_read(struct usb2_com_softc *ucom);
static void umct_start_write(struct usb2_com_softc *ucom);
static void umct_stop_write(struct usb2_com_softc *ucom);

static const struct usb2_config umct_config[UMCT_ENDPT_MAX] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = 0,	/* use wMaxPacketSize */
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &umct_write_callback,
	},

	[1] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = 0,	/* use wMaxPacketSize */
		.mh.callback = &umct_read_callback,
		.ep_index = 0,		/* first interrupt endpoint */
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &umct_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &umct_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[4] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = 0,	/* use wMaxPacketSize */
		.mh.callback = &umct_intr_callback,
		.ep_index = 1,		/* second interrupt endpoint */
	},

	[5] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &umct_intr_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static const struct usb2_com_callback umct_callback = {
	.usb2_com_cfg_get_status = &umct_cfg_get_status,
	.usb2_com_cfg_set_dtr = &umct_cfg_set_dtr,
	.usb2_com_cfg_set_rts = &umct_cfg_set_rts,
	.usb2_com_cfg_set_break = &umct_cfg_set_break,
	.usb2_com_cfg_param = &umct_cfg_param,
	.usb2_com_pre_param = &umct_pre_param,
	.usb2_com_start_read = &umct_start_read,
	.usb2_com_stop_read = &umct_stop_read,
	.usb2_com_start_write = &umct_start_write,
	.usb2_com_stop_write = &umct_stop_write,
};

static const struct usb2_device_id umct_devs[] = {
	{USB_VPI(USB_VENDOR_MCT, USB_PRODUCT_MCT_USB232, 0)},
	{USB_VPI(USB_VENDOR_MCT, USB_PRODUCT_MCT_SITECOM_USB232, 0)},
	{USB_VPI(USB_VENDOR_MCT, USB_PRODUCT_MCT_DU_H3SP_USB232, 0)},
	{USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5U109, 0)},
	{USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5U409, 0)},
};

static device_method_t umct_methods[] = {
	DEVMETHOD(device_probe, umct_probe),
	DEVMETHOD(device_attach, umct_attach),
	DEVMETHOD(device_detach, umct_detach),
	{0, 0}
};

static devclass_t umct_devclass;

static driver_t umct_driver = {
	.name = "umct",
	.methods = umct_methods,
	.size = sizeof(struct umct_softc),
};

DRIVER_MODULE(umct, ushub, umct_driver, umct_devclass, NULL, 0);
MODULE_DEPEND(umct, usb2_serial, 1, 1, 1);
MODULE_DEPEND(umct, usb2_core, 1, 1, 1);

static int
umct_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != UMCT_CONFIG_INDEX) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != UMCT_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(umct_devs, sizeof(umct_devs), uaa));
}

static int
umct_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct umct_softc *sc = device_get_softc(dev);
	int32_t error;
	uint16_t maxp;
	uint8_t iface_index;

	if (sc == NULL) {
		return (ENOMEM);
	}
	sc->sc_udev = uaa->device;
	sc->sc_unit = device_get_unit(dev);

	device_set_usb2_desc(dev);

	snprintf(sc->sc_name, sizeof(sc->sc_name),
	    "%s", device_get_nameunit(dev));

	sc->sc_iface_no = uaa->info.bIfaceNum;

	iface_index = UMCT_IFACE_INDEX;
	error = usb2_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, umct_config, UMCT_ENDPT_MAX, sc, &Giant);

	if (error) {
		device_printf(dev, "allocating USB "
		    "transfers failed!\n");
		goto detach;
	}
	/*
	 * The real bulk-in endpoint is also marked as an interrupt.
	 * The only way to differentiate it from the real interrupt
	 * endpoint is to look at the wMaxPacketSize field.
	 */
	maxp = UGETW(sc->sc_xfer[1]->pipe->edesc->wMaxPacketSize);
	if (maxp == 0x2) {

		/* guessed wrong - switch around endpoints */

		struct usb2_xfer *temp = sc->sc_xfer[4];

		sc->sc_xfer[4] = sc->sc_xfer[1];
		sc->sc_xfer[1] = temp;

		sc->sc_xfer[1]->callback = &umct_read_callback;
		sc->sc_xfer[4]->callback = &umct_intr_callback;
	}
	sc->sc_obufsize = sc->sc_xfer[0]->max_data_length;

	if (uaa->info.idProduct == USB_PRODUCT_MCT_SITECOM_USB232) {
		if (sc->sc_obufsize > 16) {
			sc->sc_obufsize = 16;
		}
	}
	error = usb2_com_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &umct_callback, &Giant);
	if (error) {
		goto detach;
	}
	return (0);			/* success */

detach:
	umct_detach(dev);
	return (ENXIO);			/* failure */
}

static int
umct_detach(device_t dev)
{
	struct umct_softc *sc = device_get_softc(dev);

	usb2_com_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);

	usb2_transfer_unsetup(sc->sc_xfer, UMCT_ENDPT_MAX);

	return (0);
}

static void
umct_cfg_do_request(struct umct_softc *sc, uint8_t request,
    uint16_t len, uint32_t value)
{
	struct usb2_device_request req;
	usb2_error_t err;
	uint8_t temp[4];

	if (usb2_com_cfg_is_gone(&sc->sc_ucom)) {
		goto done;
	}
	if (len > 4) {
		len = 4;
	}
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = request;
	USETW(req.wValue, 0);
	req.wIndex[0] = sc->sc_iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, len);
	USETDW(temp, value);

	err = usb2_do_request_flags(sc->sc_udev, &Giant, &req,
	    temp, 0, NULL, 1000);

	if (err) {
		DPRINTFN(0, "device request failed, err=%s "
		    "(ignored)\n", usb2_errstr(err));
	}
done:
	return;
}

static void
umct_intr_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct umct_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[4];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UMCT_FLAG_INTR_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
umct_intr_callback(struct usb2_xfer *xfer)
{
	struct umct_softc *sc = xfer->priv_sc;
	uint8_t buf[2];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (xfer->actlen < 2) {
			DPRINTF("too short message\n");
			goto tr_setup;
		}
		usb2_copy_out(xfer->frbuffers, 0, buf, sizeof(buf));

		sc->sc_msr = buf[0];
		sc->sc_lsr = buf[1];

		usb2_com_status_change(&sc->sc_ucom);

	case USB_ST_SETUP:
tr_setup:
		if (sc->sc_flags & UMCT_FLAG_INTR_STALL) {
			usb2_transfer_start(sc->sc_xfer[5]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* start clear stall */
			sc->sc_flags |= UMCT_FLAG_INTR_STALL;
			usb2_transfer_start(sc->sc_xfer[5]);
		}
		return;

	}
}

static void
umct_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *lsr, uint8_t *msr)
{
	struct umct_softc *sc = ucom->sc_parent;

	*lsr = sc->sc_lsr;
	*msr = sc->sc_msr;
	return;
}

static void
umct_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct umct_softc *sc = ucom->sc_parent;

	if (onoff)
		sc->sc_lcr |= 0x40;
	else
		sc->sc_lcr &= ~0x40;

	umct_cfg_do_request(sc, UMCT_SET_LCR, UMCT_SET_LCR_SIZE, sc->sc_lcr);
	return;
}

static void
umct_cfg_set_dtr(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct umct_softc *sc = ucom->sc_parent;

	if (onoff)
		sc->sc_mcr |= 0x01;
	else
		sc->sc_mcr &= ~0x01;

	umct_cfg_do_request(sc, UMCT_SET_MCR, UMCT_SET_MCR_SIZE, sc->sc_mcr);
	return;
}

static void
umct_cfg_set_rts(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct umct_softc *sc = ucom->sc_parent;

	if (onoff)
		sc->sc_mcr |= 0x02;
	else
		sc->sc_mcr &= ~0x02;

	umct_cfg_do_request(sc, UMCT_SET_MCR, UMCT_SET_MCR_SIZE, sc->sc_mcr);
	return;
}

static uint8_t
umct_calc_baud(uint32_t baud)
{
	switch (baud) {
		case B300:return (0x1);
	case B600:
		return (0x2);
	case B1200:
		return (0x3);
	case B2400:
		return (0x4);
	case B4800:
		return (0x6);
	case B9600:
		return (0x8);
	case B19200:
		return (0x9);
	case B38400:
		return (0xa);
	case B57600:
		return (0xb);
	case 115200:
		return (0xc);
	case B0:
	default:
		break;
	}
	return (0x0);
}

static int
umct_pre_param(struct usb2_com_softc *ucom, struct termios *t)
{
	return (0);			/* we accept anything */
}

static void
umct_cfg_param(struct usb2_com_softc *ucom, struct termios *t)
{
	struct umct_softc *sc = ucom->sc_parent;
	uint32_t value;

	value = umct_calc_baud(t->c_ospeed);
	umct_cfg_do_request(sc, UMCT_SET_BAUD, UMCT_SET_BAUD_SIZE, value);

	value = (sc->sc_lcr & 0x40);

	switch (t->c_cflag & CSIZE) {
	case CS5:
		value |= 0x0;
		break;
	case CS6:
		value |= 0x1;
		break;
	case CS7:
		value |= 0x2;
		break;
	default:
	case CS8:
		value |= 0x3;
		break;
	}

	value |= (t->c_cflag & CSTOPB) ? 0x4 : 0;
	if (t->c_cflag & PARENB) {
		value |= 0x8;
		value |= (t->c_cflag & PARODD) ? 0x0 : 0x10;
	}
	/*
	 * XXX There doesn't seem to be a way to tell the device
	 * to use flow control.
	 */

	sc->sc_lcr = value;
	umct_cfg_do_request(sc, UMCT_SET_LCR, UMCT_SET_LCR_SIZE, value);
	return;
}

static void
umct_start_read(struct usb2_com_softc *ucom)
{
	struct umct_softc *sc = ucom->sc_parent;

	/* start interrupt endpoint */
	usb2_transfer_start(sc->sc_xfer[4]);

	/* start read endpoint */
	usb2_transfer_start(sc->sc_xfer[1]);
	return;
}

static void
umct_stop_read(struct usb2_com_softc *ucom)
{
	struct umct_softc *sc = ucom->sc_parent;

	/* stop interrupt endpoint */
	usb2_transfer_stop(sc->sc_xfer[5]);
	usb2_transfer_stop(sc->sc_xfer[4]);

	/* stop read endpoint */
	usb2_transfer_stop(sc->sc_xfer[3]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	return;
}

static void
umct_start_write(struct usb2_com_softc *ucom)
{
	struct umct_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
umct_stop_write(struct usb2_com_softc *ucom)
{
	struct umct_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer[2]);
	usb2_transfer_stop(sc->sc_xfer[0]);
	return;
}

static void
umct_write_callback(struct usb2_xfer *xfer)
{
	struct umct_softc *sc = xfer->priv_sc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
		if (sc->sc_flags & UMCT_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
			return;
		}
		if (usb2_com_get_data(&sc->sc_ucom, xfer->frbuffers, 0,
		    sc->sc_obufsize, &actlen)) {

			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flags |= UMCT_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		return;

	}
}

static void
umct_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct umct_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UMCT_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
umct_read_callback(struct usb2_xfer *xfer)
{
	struct umct_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usb2_com_put_data(&sc->sc_ucom, xfer->frbuffers,
		    0, xfer->actlen);

	case USB_ST_SETUP:
		if (sc->sc_flags & UMCT_FLAG_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer[3]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flags |= UMCT_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		return;

	}
}

static void
umct_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct umct_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UMCT_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}
