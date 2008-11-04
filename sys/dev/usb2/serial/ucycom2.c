#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 2004 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Device driver for Cypress CY7C637xx and CY7C640/1xx series USB to
 * RS232 bridges.
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_cdc.h>
#include <dev/usb2/include/usb2_ioctl.h>
#include <dev/usb2/include/usb2_hid.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_hid.h>

#include <dev/usb2/serial/usb2_serial.h>

#define	UCYCOM_MAX_IOLEN	(1024 + 2)	/* bytes */

#define	UCYCOM_ENDPT_MAX	3	/* units */
#define	UCYCOM_IFACE_INDEX	0

struct ucycom_softc {
	struct usb2_com_super_softc sc_super_ucom;
	struct usb2_com_softc sc_ucom;

	struct usb2_device *sc_udev;
	struct usb2_xfer *sc_xfer[UCYCOM_ENDPT_MAX];

	uint32_t sc_model;
#define	MODEL_CY7C63743		0x63743
#define	MODEL_CY7C64013		0x64013

	uint16_t sc_flen;		/* feature report length */
	uint16_t sc_ilen;		/* input report length */
	uint16_t sc_olen;		/* output report length */

	uint8_t	sc_fid;			/* feature report id */
	uint8_t	sc_iid;			/* input report id */
	uint8_t	sc_oid;			/* output report id */
	uint8_t	sc_cfg;
#define	UCYCOM_CFG_RESET	0x80
#define	UCYCOM_CFG_PARODD	0x20
#define	UCYCOM_CFG_PAREN	0x10
#define	UCYCOM_CFG_STOPB	0x08
#define	UCYCOM_CFG_DATAB	0x03
	uint8_t	sc_ist;			/* status flags from last input */
	uint8_t	sc_flags;
#define	UCYCOM_FLAG_INTR_STALL     0x01
	uint8_t	sc_name[16];
	uint8_t	sc_iface_no;
	uint8_t	sc_temp_cfg[32];
};

/* prototypes */

static device_probe_t ucycom_probe;
static device_attach_t ucycom_attach;
static device_detach_t ucycom_detach;

static usb2_callback_t ucycom_ctrl_write_callback;
static usb2_callback_t ucycom_intr_read_clear_stall_callback;
static usb2_callback_t ucycom_intr_read_callback;

static void ucycom_cfg_open(struct usb2_com_softc *ucom);
static void ucycom_start_read(struct usb2_com_softc *ucom);
static void ucycom_stop_read(struct usb2_com_softc *ucom);
static void ucycom_start_write(struct usb2_com_softc *ucom);
static void ucycom_stop_write(struct usb2_com_softc *ucom);
static void ucycom_cfg_write(struct ucycom_softc *sc, uint32_t baud, uint8_t cfg);
static int ucycom_pre_param(struct usb2_com_softc *ucom, struct termios *t);
static void ucycom_cfg_param(struct usb2_com_softc *ucom, struct termios *t);

static const struct usb2_config ucycom_config[UCYCOM_ENDPT_MAX] = {

	[0] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = (sizeof(struct usb2_device_request) + UCYCOM_MAX_IOLEN),
		.mh.flags = {},
		.mh.callback = &ucycom_ctrl_write_callback,
		.mh.timeout = 1000,	/* 1 second */
	},

	[1] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = UCYCOM_MAX_IOLEN,
		.mh.callback = &ucycom_intr_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.flags = {},
		.mh.callback = &ucycom_intr_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static const struct usb2_com_callback ucycom_callback = {
	.usb2_com_cfg_param = &ucycom_cfg_param,
	.usb2_com_cfg_open = &ucycom_cfg_open,
	.usb2_com_pre_param = &ucycom_pre_param,
	.usb2_com_start_read = &ucycom_start_read,
	.usb2_com_stop_read = &ucycom_stop_read,
	.usb2_com_start_write = &ucycom_start_write,
	.usb2_com_stop_write = &ucycom_stop_write,
};

static device_method_t ucycom_methods[] = {
	DEVMETHOD(device_probe, ucycom_probe),
	DEVMETHOD(device_attach, ucycom_attach),
	DEVMETHOD(device_detach, ucycom_detach),
	{0, 0}
};

static devclass_t ucycom_devclass;

static driver_t ucycom_driver = {
	.name = "ucycom",
	.methods = ucycom_methods,
	.size = sizeof(struct ucycom_softc),
};

DRIVER_MODULE(ucycom, ushub, ucycom_driver, ucycom_devclass, NULL, 0);
MODULE_DEPEND(ucycom, usb2_serial, 1, 1, 1);
MODULE_DEPEND(ucycom, usb2_core, 1, 1, 1);
MODULE_DEPEND(ucycom, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);

/*
 * Supported devices
 */
static const struct usb2_device_id ucycom_devs[] = {
	{USB_VPI(USB_VENDOR_DELORME, USB_PRODUCT_DELORME_EARTHMATE, MODEL_CY7C64013)},
};

#define	UCYCOM_DEFAULT_RATE	 4800
#define	UCYCOM_DEFAULT_CFG	 0x03	/* N-8-1 */

static int
ucycom_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != 0) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != UCYCOM_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(ucycom_devs, sizeof(ucycom_devs), uaa));
}

static int
ucycom_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct ucycom_softc *sc = device_get_softc(dev);
	void *urd_ptr = NULL;
	int32_t error;
	uint16_t urd_len;
	uint8_t iface_index;

	if (sc == NULL) {
		return (ENOMEM);
	}
	sc->sc_udev = uaa->device;

	device_set_usb2_desc(dev);

	snprintf(sc->sc_name, sizeof(sc->sc_name),
	    "%s", device_get_nameunit(dev));

	DPRINTF("\n");

	/* get chip model */
	sc->sc_model = USB_GET_DRIVER_INFO(uaa);
	if (sc->sc_model == 0) {
		device_printf(dev, "unsupported device\n");
		goto detach;
	}
	device_printf(dev, "Cypress CY7C%X USB to RS232 bridge\n", sc->sc_model);

	/* get report descriptor */

	error = usb2_req_get_hid_desc
	    (uaa->device, &Giant,
	    &urd_ptr, &urd_len, M_USBDEV,
	    UCYCOM_IFACE_INDEX);

	if (error) {
		device_printf(dev, "failed to get report "
		    "descriptor: %s\n",
		    usb2_errstr(error));
		goto detach;
	}
	/* get report sizes */

	sc->sc_flen = hid_report_size(urd_ptr, urd_len, hid_feature, &sc->sc_fid);
	sc->sc_ilen = hid_report_size(urd_ptr, urd_len, hid_input, &sc->sc_iid);
	sc->sc_olen = hid_report_size(urd_ptr, urd_len, hid_output, &sc->sc_oid);

	if ((sc->sc_ilen > UCYCOM_MAX_IOLEN) || (sc->sc_ilen < 1) ||
	    (sc->sc_olen > UCYCOM_MAX_IOLEN) || (sc->sc_olen < 2) ||
	    (sc->sc_flen > UCYCOM_MAX_IOLEN) || (sc->sc_flen < 5)) {
		device_printf(dev, "invalid report size i=%d, o=%d, f=%d, max=%d\n",
		    sc->sc_ilen, sc->sc_olen, sc->sc_flen,
		    UCYCOM_MAX_IOLEN);
		goto detach;
	}
	sc->sc_iface_no = uaa->info.bIfaceNum;

	iface_index = UCYCOM_IFACE_INDEX;
	error = usb2_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, ucycom_config, UCYCOM_ENDPT_MAX,
	    sc, &Giant);
	if (error) {
		device_printf(dev, "allocating USB "
		    "transfers failed!\n");
		goto detach;
	}
	error = usb2_com_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &ucycom_callback, &Giant);

	if (error) {
		goto detach;
	}
	if (urd_ptr) {
		free(urd_ptr, M_USBDEV);
	}
	return (0);			/* success */

detach:
	if (urd_ptr) {
		free(urd_ptr, M_USBDEV);
	}
	ucycom_detach(dev);
	return (ENXIO);
}

static int
ucycom_detach(device_t dev)
{
	struct ucycom_softc *sc = device_get_softc(dev);

	usb2_com_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);

	usb2_transfer_unsetup(sc->sc_xfer, UCYCOM_ENDPT_MAX);

	return (0);
}

static void
ucycom_cfg_open(struct usb2_com_softc *ucom)
{
	struct ucycom_softc *sc = ucom->sc_parent;

	/* set default configuration */
	ucycom_cfg_write(sc, UCYCOM_DEFAULT_RATE, UCYCOM_DEFAULT_CFG);
	return;
}

static void
ucycom_start_read(struct usb2_com_softc *ucom)
{
	struct ucycom_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer[1]);
	return;
}

static void
ucycom_stop_read(struct usb2_com_softc *ucom)
{
	struct ucycom_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer[2]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	return;
}

static void
ucycom_start_write(struct usb2_com_softc *ucom)
{
	struct ucycom_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
ucycom_stop_write(struct usb2_com_softc *ucom)
{
	struct ucycom_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer[0]);
	return;
}

static void
ucycom_ctrl_write_callback(struct usb2_xfer *xfer)
{
	struct ucycom_softc *sc = xfer->priv_sc;
	struct usb2_device_request req;
	uint8_t data[2];
	uint8_t offset;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
tr_transferred:
	case USB_ST_SETUP:

		switch (sc->sc_model) {
		case MODEL_CY7C63743:
			offset = 1;
			break;
		case MODEL_CY7C64013:
			offset = 2;
			break;
		default:
			offset = 0;
			break;
		}

		if (usb2_com_get_data(&sc->sc_ucom, xfer->frbuffers + 1, offset,
		    sc->sc_olen - offset, &actlen)) {

			req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
			req.bRequest = UR_SET_REPORT;
			USETW2(req.wValue, UHID_OUTPUT_REPORT, sc->sc_oid);
			req.wIndex[0] = sc->sc_iface_no;
			req.wIndex[1] = 0;
			USETW(req.wLength, sc->sc_olen);

			switch (sc->sc_model) {
			case MODEL_CY7C63743:
				data[0] = actlen;
				break;
			case MODEL_CY7C64013:
				data[0] = 0;
				data[1] = actlen;
				break;
			default:
				break;
			}

			usb2_copy_in(xfer->frbuffers, 0, &req, sizeof(req));
			usb2_copy_in(xfer->frbuffers + 1, 0, data, offset);

			xfer->frlengths[0] = sizeof(req);
			xfer->frlengths[1] = sc->sc_olen;
			xfer->nframes = xfer->frlengths[1] ? 2 : 1;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			return;
		}
		DPRINTF("error=%s\n",
		    usb2_errstr(xfer->error));
		goto tr_transferred;
	}
}

static void
ucycom_cfg_write(struct ucycom_softc *sc, uint32_t baud, uint8_t cfg)
{
	struct usb2_device_request req;
	uint16_t len;
	usb2_error_t err;

	len = sc->sc_flen;
	if (len > sizeof(sc->sc_temp_cfg)) {
		len = sizeof(sc->sc_temp_cfg);
	}
	sc->sc_cfg = cfg;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_REPORT;
	USETW2(req.wValue, UHID_FEATURE_REPORT, sc->sc_fid);
	req.wIndex[0] = sc->sc_iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, len);

	sc->sc_temp_cfg[0] = (baud & 0xff);
	sc->sc_temp_cfg[1] = (baud >> 8) & 0xff;
	sc->sc_temp_cfg[2] = (baud >> 16) & 0xff;
	sc->sc_temp_cfg[3] = (baud >> 24) & 0xff;
	sc->sc_temp_cfg[4] = cfg;

	if (usb2_com_cfg_is_gone(&sc->sc_ucom)) {
		return;
	}
	err = usb2_do_request_flags
	    (sc->sc_udev, &Giant, &req, sc->sc_temp_cfg, 0, NULL, 1000);

	if (err) {
		DPRINTFN(0, "device request failed, err=%s "
		    "(ignored)\n", usb2_errstr(err));
	}
	return;
}

static int
ucycom_pre_param(struct usb2_com_softc *ucom, struct termios *t)
{
	switch (t->c_ospeed) {
		case 600:
		case 1200:
		case 2400:
		case 4800:
		case 9600:
		case 19200:
		case 38400:
		case 57600:
#if 0
		/*
		 * Stock chips only support standard baud rates in the 600 - 57600
		 * range, but higher rates can be achieved using custom firmware.
		 */
		case 115200:
		case 153600:
		case 192000:
#endif
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static void
ucycom_cfg_param(struct usb2_com_softc *ucom, struct termios *t)
{
	struct ucycom_softc *sc = ucom->sc_parent;
	uint8_t cfg;

	DPRINTF("\n");

	if (t->c_cflag & CIGNORE) {
		cfg = sc->sc_cfg;
	} else {
		cfg = 0;
		switch (t->c_cflag & CSIZE) {
		default:
		case CS8:
			++cfg;
		case CS7:
			++cfg;
		case CS6:
			++cfg;
		case CS5:
			break;
		}

		if (t->c_cflag & CSTOPB)
			cfg |= UCYCOM_CFG_STOPB;
		if (t->c_cflag & PARENB)
			cfg |= UCYCOM_CFG_PAREN;
		if (t->c_cflag & PARODD)
			cfg |= UCYCOM_CFG_PARODD;
	}

	ucycom_cfg_write(sc, t->c_ospeed, cfg);
	return;
}

static void
ucycom_intr_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ucycom_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UCYCOM_FLAG_INTR_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ucycom_intr_read_callback(struct usb2_xfer *xfer)
{
	struct ucycom_softc *sc = xfer->priv_sc;
	uint8_t buf[2];
	uint32_t offset;
	uint32_t len;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		switch (sc->sc_model) {
		case MODEL_CY7C63743:
			if (xfer->actlen < 1) {
				goto tr_setup;
			}
			usb2_copy_out(xfer->frbuffers, 0, buf, 1);

			sc->sc_ist = buf[0] & ~0x07;
			len = buf[0] & 0x07;

			(xfer->actlen)--;

			offset = 1;

			break;

		case MODEL_CY7C64013:
			if (xfer->actlen < 2) {
				goto tr_setup;
			}
			usb2_copy_out(xfer->frbuffers, 0, buf, 2);

			sc->sc_ist = buf[0] & ~0x07;
			len = buf[1];

			(xfer->actlen) -= 2;

			offset = 2;

			break;

		default:
			DPRINTFN(0, "unsupported model number!\n");
			goto tr_setup;
		}

		if (len > xfer->actlen) {
			len = xfer->actlen;
		}
		if (len) {
			usb2_com_put_data(&sc->sc_ucom, xfer->frbuffers,
			    offset, len);
		}
	case USB_ST_SETUP:
tr_setup:
		if (sc->sc_flags & UCYCOM_FLAG_INTR_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
		} else {
			xfer->frlengths[0] = sc->sc_ilen;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flags |= UCYCOM_FLAG_INTR_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		return;

	}
}
