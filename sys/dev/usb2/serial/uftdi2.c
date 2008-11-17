/*	$NetBSD: uftdi.c,v 1.13 2002/09/23 05:51:23 simonb Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * NOTE: all function names beginning like "uftdi_cfg_" can only
 * be called from within the config thread function !
 */

/*
 * FTDI FT8U100AX serial adapter driver
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_cdc.h>

#define	USB_DEBUG_VAR uftdi_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_busdma.h>

#include <dev/usb2/serial/usb2_serial.h>
#include <dev/usb2/serial/uftdi2_reg.h>

#if USB_DEBUG
static int uftdi_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, uftdi, CTLFLAG_RW, 0, "USB uftdi");
SYSCTL_INT(_hw_usb2_uftdi, OID_AUTO, debug, CTLFLAG_RW,
    &uftdi_debug, 0, "Debug level");
#endif

#define	UFTDI_CONFIG_INDEX	0
#define	UFTDI_IFACE_INDEX	0
#define	UFTDI_ENDPT_MAX		4

#define	UFTDI_IBUFSIZE 64		/* bytes, maximum number of bytes per
					 * frame */
#define	UFTDI_OBUFSIZE 64		/* bytes, cannot be increased due to
					 * do size encoding */

struct uftdi_softc {
	struct usb2_com_super_softc sc_super_ucom;
	struct usb2_com_softc sc_ucom;

	struct usb2_device *sc_udev;
	struct usb2_xfer *sc_xfer[UFTDI_ENDPT_MAX];
	device_t sc_dev;

	uint32_t sc_unit;
	enum uftdi_type sc_type;

	uint16_t sc_last_lcr;

	uint8_t	sc_iface_index;
	uint8_t	sc_hdrlen;

	uint8_t	sc_msr;
	uint8_t	sc_lsr;

	uint8_t	sc_flag;
#define	UFTDI_FLAG_WRITE_STALL  0x01
#define	UFTDI_FLAG_READ_STALL   0x02

	uint8_t	sc_name[16];
};

struct uftdi_param_config {
	uint16_t rate;
	uint16_t lcr;
	uint8_t	v_start;
	uint8_t	v_stop;
	uint8_t	v_flow;
};

/* prototypes */

static device_probe_t uftdi_probe;
static device_attach_t uftdi_attach;
static device_detach_t uftdi_detach;

static usb2_callback_t uftdi_write_callback;
static usb2_callback_t uftdi_write_clear_stall_callback;
static usb2_callback_t uftdi_read_callback;
static usb2_callback_t uftdi_read_clear_stall_callback;

static void uftdi_cfg_do_request(struct uftdi_softc *sc, struct usb2_device_request *req, void *data);
static void uftdi_cfg_open(struct usb2_com_softc *ucom);
static void uftdi_cfg_set_dtr(struct usb2_com_softc *ucom, uint8_t onoff);
static void uftdi_cfg_set_rts(struct usb2_com_softc *ucom, uint8_t onoff);
static void uftdi_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff);
static int uftdi_set_parm_soft(struct termios *t, struct uftdi_param_config *cfg, uint8_t type);
static int uftdi_pre_param(struct usb2_com_softc *ucom, struct termios *t);
static void uftdi_cfg_param(struct usb2_com_softc *ucom, struct termios *t);
static void uftdi_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *lsr, uint8_t *msr);
static void uftdi_start_read(struct usb2_com_softc *ucom);
static void uftdi_stop_read(struct usb2_com_softc *ucom);
static void uftdi_start_write(struct usb2_com_softc *ucom);
static void uftdi_stop_write(struct usb2_com_softc *ucom);
static uint8_t uftdi_8u232am_getrate(uint32_t speed, uint16_t *rate);

static const struct usb2_config uftdi_config[UFTDI_ENDPT_MAX] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = UFTDI_OBUFSIZE,
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &uftdi_write_callback,
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = UFTDI_IBUFSIZE,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &uftdi_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &uftdi_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &uftdi_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static const struct usb2_com_callback uftdi_callback = {
	.usb2_com_cfg_get_status = &uftdi_cfg_get_status,
	.usb2_com_cfg_set_dtr = &uftdi_cfg_set_dtr,
	.usb2_com_cfg_set_rts = &uftdi_cfg_set_rts,
	.usb2_com_cfg_set_break = &uftdi_cfg_set_break,
	.usb2_com_cfg_param = &uftdi_cfg_param,
	.usb2_com_cfg_open = &uftdi_cfg_open,
	.usb2_com_pre_param = &uftdi_pre_param,
	.usb2_com_start_read = &uftdi_start_read,
	.usb2_com_stop_read = &uftdi_stop_read,
	.usb2_com_start_write = &uftdi_start_write,
	.usb2_com_stop_write = &uftdi_stop_write,
};

static device_method_t uftdi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uftdi_probe),
	DEVMETHOD(device_attach, uftdi_attach),
	DEVMETHOD(device_detach, uftdi_detach),

	{0, 0}
};

static devclass_t uftdi_devclass;

static driver_t uftdi_driver = {
	.name = "uftdi",
	.methods = uftdi_methods,
	.size = sizeof(struct uftdi_softc),
};

DRIVER_MODULE(uftdi, ushub, uftdi_driver, uftdi_devclass, NULL, 0);
MODULE_DEPEND(uftdi, usb2_serial, 1, 1, 1);
MODULE_DEPEND(uftdi, usb2_core, 1, 1, 1);

static struct usb2_device_id uftdi_devs[] = {
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_8U100AX, UFTDI_TYPE_SIO)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_2232C, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_8U232AM, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SEMC_DSS20, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_CFA_631, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_CFA_632, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_CFA_633, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_CFA_634, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_CFA_635, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_USBSERIAL, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MX2_3, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MX4_5, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_LK202, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_LK204, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_TACTRIX_OPENPORT_13M, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_TACTRIX_OPENPORT_13S, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_TACTRIX_OPENPORT_13U, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_EISCOU, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_UOPTBR, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_EMCU2D, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_PCMSFU, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_EMCU2H, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_SIIG2, USB_PRODUCT_SIIG2_US2308, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_INTREPIDCS, USB_PRODUCT_INTREPIDCS_VALUECAN, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_INTREPIDCS, USB_PRODUCT_INTREPIDCS_NEOVI, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_BBELECTRONICS, USB_PRODUCT_BBELECTRONICS_USOTL4, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_PCOPRS1, UFTDI_TYPE_8U232AM)},
};

static int
uftdi_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != UFTDI_CONFIG_INDEX) {
		return (ENXIO);
	}
	/* attach to all present interfaces */

	return (usb2_lookup_id_by_uaa(uftdi_devs, sizeof(uftdi_devs), uaa));
}

static int
uftdi_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct uftdi_softc *sc = device_get_softc(dev);
	int error;

	if (sc == NULL) {
		return (ENOMEM);
	}
	sc->sc_udev = uaa->device;
	sc->sc_dev = dev;
	sc->sc_unit = device_get_unit(dev);

	device_set_usb2_desc(dev);

	snprintf(sc->sc_name, sizeof(sc->sc_name),
	    "%s", device_get_nameunit(dev));

	DPRINTF("\n");

	sc->sc_iface_index = uaa->info.bIfaceIndex;
	sc->sc_type = USB_GET_DRIVER_INFO(uaa);

	switch (sc->sc_type) {
	case UFTDI_TYPE_SIO:
		sc->sc_hdrlen = 1;
		break;
	case UFTDI_TYPE_8U232AM:
	default:
		sc->sc_hdrlen = 0;
		break;
	}

	error = usb2_transfer_setup(uaa->device,
	    &sc->sc_iface_index, sc->sc_xfer, uftdi_config,
	    UFTDI_ENDPT_MAX, sc, &Giant);

	if (error) {
		device_printf(dev, "allocating USB "
		    "transfers failed!\n");
		goto detach;
	}
	sc->sc_ucom.sc_portno = FTDI_PIT_SIOA + uaa->info.bIfaceNum;

	/* clear stall at first run */

	sc->sc_flag |= (UFTDI_FLAG_WRITE_STALL |
	    UFTDI_FLAG_READ_STALL);

	/* set a valid "lcr" value */

	sc->sc_last_lcr =
	    (FTDI_SIO_SET_DATA_STOP_BITS_2 |
	    FTDI_SIO_SET_DATA_PARITY_NONE |
	    FTDI_SIO_SET_DATA_BITS(8));

	error = usb2_com_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &uftdi_callback, &Giant);
	if (error) {
		goto detach;
	}
	return (0);			/* success */

detach:
	uftdi_detach(dev);
	return (ENXIO);
}

static int
uftdi_detach(device_t dev)
{
	struct uftdi_softc *sc = device_get_softc(dev);

	usb2_com_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);

	usb2_transfer_unsetup(sc->sc_xfer, UFTDI_ENDPT_MAX);

	return (0);
}

static void
uftdi_cfg_do_request(struct uftdi_softc *sc, struct usb2_device_request *req,
    void *data)
{
	uint16_t length;
	usb2_error_t err;

	if (usb2_com_cfg_is_gone(&sc->sc_ucom)) {
		goto error;
	}
	err = usb2_do_request_flags
	    (sc->sc_udev, &Giant, req, data, 0, NULL, 1000);

	if (err) {

		DPRINTFN(0, "device request failed, err=%s "
		    "(ignored)\n", usb2_errstr(err));

error:
		length = UGETW(req->wLength);

		if ((req->bmRequestType & UT_READ) && length) {
			bzero(data, length);
		}
	}
	return;
}

static void
uftdi_cfg_open(struct usb2_com_softc *ucom)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	uint16_t wIndex = ucom->sc_portno;
	struct usb2_device_request req;

	DPRINTF("");

	/* perform a full reset on the device */

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_RESET;
	USETW(req.wValue, FTDI_SIO_RESET_SIO);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, 0);
	uftdi_cfg_do_request(sc, &req, NULL);

	/* turn on RTS/CTS flow control */

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_FLOW_CTRL;
	USETW(req.wValue, 0);
	USETW2(req.wIndex, FTDI_SIO_RTS_CTS_HS, wIndex);
	USETW(req.wLength, 0);
	uftdi_cfg_do_request(sc, &req, NULL);

	/*
	 * NOTE: with the new UCOM layer there will always be a
	 * "uftdi_cfg_param()" call after "open()", so there is no need for
	 * "open()" to configure anything
	 */
	return;
}

static void
uftdi_write_callback(struct usb2_xfer *xfer)
{
	struct uftdi_softc *sc = xfer->priv_sc;
	uint32_t actlen;
	uint8_t buf[1];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
		if (sc->sc_flag & UFTDI_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
			return;
		}
		if (usb2_com_get_data(&sc->sc_ucom, xfer->frbuffers,
		    sc->sc_hdrlen, UFTDI_OBUFSIZE - sc->sc_hdrlen,
		    &actlen)) {

			if (sc->sc_hdrlen > 0) {
				buf[0] =
				    FTDI_OUT_TAG(actlen, sc->sc_ucom.sc_portno);
				usb2_copy_in(xfer->frbuffers, 0, buf, 1);
			}
			xfer->frlengths[0] = actlen + sc->sc_hdrlen;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flag |= UFTDI_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		return;

	}
}

static void
uftdi_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct uftdi_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flag &= ~UFTDI_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
uftdi_read_callback(struct usb2_xfer *xfer)
{
	struct uftdi_softc *sc = xfer->priv_sc;
	uint8_t buf[2];
	uint8_t msr;
	uint8_t lsr;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		if (xfer->actlen < 2) {
			goto tr_setup;
		}
		usb2_copy_out(xfer->frbuffers, 0, buf, 2);

		msr = FTDI_GET_MSR(buf);
		lsr = FTDI_GET_LSR(buf);

		if ((sc->sc_msr != msr) ||
		    ((sc->sc_lsr & FTDI_LSR_MASK) != (lsr & FTDI_LSR_MASK))) {
			DPRINTF("status change msr=0x%02x (0x%02x) "
			    "lsr=0x%02x (0x%02x)\n", msr, sc->sc_msr,
			    lsr, sc->sc_lsr);

			sc->sc_msr = msr;
			sc->sc_lsr = lsr;

			usb2_com_status_change(&sc->sc_ucom);
		}
		xfer->actlen -= 2;

		if (xfer->actlen > 0) {
			usb2_com_put_data(&sc->sc_ucom, xfer->frbuffers, 2,
			    xfer->actlen);
		}
	case USB_ST_SETUP:
tr_setup:
		if (sc->sc_flag & UFTDI_FLAG_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer[3]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flag |= UFTDI_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		return;

	}
}

static void
uftdi_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct uftdi_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flag &= ~UFTDI_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
uftdi_cfg_set_dtr(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	uint16_t wIndex = ucom->sc_portno;
	uint16_t wValue;
	struct usb2_device_request req;

	wValue = onoff ? FTDI_SIO_SET_DTR_HIGH : FTDI_SIO_SET_DTR_LOW;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_MODEM_CTRL;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, 0);
	uftdi_cfg_do_request(sc, &req, NULL);
	return;
}

static void
uftdi_cfg_set_rts(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	uint16_t wIndex = ucom->sc_portno;
	uint16_t wValue;
	struct usb2_device_request req;

	wValue = onoff ? FTDI_SIO_SET_RTS_HIGH : FTDI_SIO_SET_RTS_LOW;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_MODEM_CTRL;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, 0);
	uftdi_cfg_do_request(sc, &req, NULL);
	return;
}

static void
uftdi_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	uint16_t wIndex = ucom->sc_portno;
	uint16_t wValue;
	struct usb2_device_request req;

	if (onoff) {
		sc->sc_last_lcr |= FTDI_SIO_SET_BREAK;
	} else {
		sc->sc_last_lcr &= ~FTDI_SIO_SET_BREAK;
	}

	wValue = sc->sc_last_lcr;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_DATA;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, 0);
	uftdi_cfg_do_request(sc, &req, NULL);
	return;
}

static int
uftdi_set_parm_soft(struct termios *t,
    struct uftdi_param_config *cfg, uint8_t type)
{
	bzero(cfg, sizeof(*cfg));

	switch (type) {
	case UFTDI_TYPE_SIO:
		switch (t->c_ospeed) {
		case 300:
			cfg->rate = ftdi_sio_b300;
			break;
		case 600:
			cfg->rate = ftdi_sio_b600;
			break;
		case 1200:
			cfg->rate = ftdi_sio_b1200;
			break;
		case 2400:
			cfg->rate = ftdi_sio_b2400;
			break;
		case 4800:
			cfg->rate = ftdi_sio_b4800;
			break;
		case 9600:
			cfg->rate = ftdi_sio_b9600;
			break;
		case 19200:
			cfg->rate = ftdi_sio_b19200;
			break;
		case 38400:
			cfg->rate = ftdi_sio_b38400;
			break;
		case 57600:
			cfg->rate = ftdi_sio_b57600;
			break;
		case 115200:
			cfg->rate = ftdi_sio_b115200;
			break;
		default:
			return (EINVAL);
		}
		break;

	case UFTDI_TYPE_8U232AM:
		if (uftdi_8u232am_getrate(t->c_ospeed, &cfg->rate)) {
			return (EINVAL);
		}
		break;
	}

	if (t->c_cflag & CSTOPB)
		cfg->lcr = FTDI_SIO_SET_DATA_STOP_BITS_2;
	else
		cfg->lcr = FTDI_SIO_SET_DATA_STOP_BITS_1;

	if (t->c_cflag & PARENB) {
		if (t->c_cflag & PARODD) {
			cfg->lcr |= FTDI_SIO_SET_DATA_PARITY_ODD;
		} else {
			cfg->lcr |= FTDI_SIO_SET_DATA_PARITY_EVEN;
		}
	} else {
		cfg->lcr |= FTDI_SIO_SET_DATA_PARITY_NONE;
	}

	switch (t->c_cflag & CSIZE) {
	case CS5:
		cfg->lcr |= FTDI_SIO_SET_DATA_BITS(5);
		break;

	case CS6:
		cfg->lcr |= FTDI_SIO_SET_DATA_BITS(6);
		break;

	case CS7:
		cfg->lcr |= FTDI_SIO_SET_DATA_BITS(7);
		break;

	case CS8:
		cfg->lcr |= FTDI_SIO_SET_DATA_BITS(8);
		break;
	}

	if (t->c_cflag & CRTSCTS) {
		cfg->v_flow = FTDI_SIO_RTS_CTS_HS;
	} else if (t->c_iflag & (IXON | IXOFF)) {
		cfg->v_flow = FTDI_SIO_XON_XOFF_HS;
		cfg->v_start = t->c_cc[VSTART];
		cfg->v_stop = t->c_cc[VSTOP];
	} else {
		cfg->v_flow = FTDI_SIO_DISABLE_FLOW_CTRL;
	}

	return (0);
}

static int
uftdi_pre_param(struct usb2_com_softc *ucom, struct termios *t)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	struct uftdi_param_config cfg;

	DPRINTF("\n");

	return (uftdi_set_parm_soft(t, &cfg, sc->sc_type));
}

static void
uftdi_cfg_param(struct usb2_com_softc *ucom, struct termios *t)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	uint16_t wIndex = ucom->sc_portno;
	struct uftdi_param_config cfg;
	struct usb2_device_request req;

	if (uftdi_set_parm_soft(t, &cfg, sc->sc_type)) {
		/* should not happen */
		return;
	}
	sc->sc_last_lcr = cfg.lcr;

	DPRINTF("\n");

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_BAUD_RATE;
	USETW(req.wValue, cfg.rate);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, 0);
	uftdi_cfg_do_request(sc, &req, NULL);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_DATA;
	USETW(req.wValue, cfg.lcr);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, 0);
	uftdi_cfg_do_request(sc, &req, NULL);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_FLOW_CTRL;
	USETW2(req.wValue, cfg.v_stop, cfg.v_start);
	USETW2(req.wIndex, cfg.v_flow, wIndex);
	USETW(req.wLength, 0);
	uftdi_cfg_do_request(sc, &req, NULL);

	return;
}

static void
uftdi_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *lsr, uint8_t *msr)
{
	struct uftdi_softc *sc = ucom->sc_parent;

	DPRINTF("msr=0x%02x lsr=0x%02x\n",
	    sc->sc_msr, sc->sc_lsr);

	*msr = sc->sc_msr;
	*lsr = sc->sc_lsr;
	return;
}

static void
uftdi_start_read(struct usb2_com_softc *ucom)
{
	struct uftdi_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer[1]);
	return;
}

static void
uftdi_stop_read(struct usb2_com_softc *ucom)
{
	struct uftdi_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer[3]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	return;
}

static void
uftdi_start_write(struct usb2_com_softc *ucom)
{
	struct uftdi_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
uftdi_stop_write(struct usb2_com_softc *ucom)
{
	struct uftdi_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer[2]);
	usb2_transfer_stop(sc->sc_xfer[0]);
	return;
}

/*------------------------------------------------------------------------*
 *	uftdi_8u232am_getrate
 *
 * Return values:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
uftdi_8u232am_getrate(uint32_t speed, uint16_t *rate)
{
	/* Table of the nearest even powers-of-2 for values 0..15. */
	static const uint8_t roundoff[16] = {
		0, 2, 2, 4, 4, 4, 8, 8,
		8, 8, 8, 8, 16, 16, 16, 16,
	};
	uint32_t d;
	uint32_t freq;
	uint16_t result;

	if ((speed < 178) || (speed > ((3000000 * 100) / 97)))
		return (1);		/* prevent numerical overflow */

	/* Special cases for 2M and 3M. */
	if ((speed >= ((3000000 * 100) / 103)) &&
	    (speed <= ((3000000 * 100) / 97))) {
		result = 0;
		goto done;
	}
	if ((speed >= ((2000000 * 100) / 103)) &&
	    (speed <= ((2000000 * 100) / 97))) {
		result = 1;
		goto done;
	}
	d = (FTDI_8U232AM_FREQ << 4) / speed;
	d = (d & ~15) + roundoff[d & 15];

	if (d < FTDI_8U232AM_MIN_DIV)
		d = FTDI_8U232AM_MIN_DIV;
	else if (d > FTDI_8U232AM_MAX_DIV)
		d = FTDI_8U232AM_MAX_DIV;

	/*
	 * Calculate the frequency needed for "d" to exactly divide down to
	 * our target "speed", and check that the actual frequency is within
	 * 3% of this.
	 */
	freq = (speed * d);
	if ((freq < ((FTDI_8U232AM_FREQ * 1600ULL) / 103)) ||
	    (freq > ((FTDI_8U232AM_FREQ * 1600ULL) / 97)))
		return (1);

	/*
	 * Pack the divisor into the resultant value.  The lower 14-bits
	 * hold the integral part, while the upper 2 bits encode the
	 * fractional component: either 0, 0.5, 0.25, or 0.125.
	 */
	result = (d >> 4);
	if (d & 8)
		result |= 0x4000;
	else if (d & 4)
		result |= 0x8000;
	else if (d & 2)
		result |= 0xc000;

done:
	*rate = result;
	return (0);
}
