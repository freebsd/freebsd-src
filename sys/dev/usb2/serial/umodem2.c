/*	$NetBSD: umodem.c,v 1.45 2002/09/23 05:51:23 simonb Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 2003, M. Warner Losh <imp@freebsd.org>.
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
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 * Comm Class spec:  http://www.usb.org/developers/devclass_docs/usbccs10.pdf
 *                   http://www.usb.org/developers/devclass_docs/usbcdc11.pdf
 */

/*
 * TODO:
 * - Add error recovery in various places; the big problem is what
 *   to do in a callback if there is an error.
 * - Implement a Call Device for modems without multiplexed commands.
 *
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_cdc.h>
#include <dev/usb2/include/usb2_ioctl.h>
#include <dev/usb2/include/usb2_defs.h>

#define	USB_DEBUG_VAR umodem_debug

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

#if USB_DEBUG
static int umodem_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, umodem, CTLFLAG_RW, 0, "USB umodem");
SYSCTL_INT(_hw_usb2_umodem, OID_AUTO, debug, CTLFLAG_RW,
    &umodem_debug, 0, "Debug level");
#endif

static const struct usb2_device_id umodem_devs[] = {
	/* Generic Modem class match */
	{USB_IFACE_CLASS(UICLASS_CDC),
		USB_IFACE_SUBCLASS(UISUBCLASS_ABSTRACT_CONTROL_MODEL),
	USB_IFACE_PROTOCOL(UIPROTO_CDC_AT)},
	/* Kyocera AH-K3001V */
	{USB_VPI(USB_VENDOR_KYOCERA, USB_PRODUCT_KYOCERA_AHK3001V, 1)},
	{USB_VPI(USB_VENDOR_SIERRA, USB_PRODUCT_SIERRA_MC5720, 1)},
	{USB_VPI(USB_VENDOR_CURITEL, USB_PRODUCT_CURITEL_PC5740, 1)},
};

/*
 * As speeds for umodem deivces increase, these numbers will need to
 * be increased. They should be good for G3 speeds and below.
 *
 * TODO: The TTY buffers should be increased!
 */
#define	UMODEM_BUF_SIZE 1024
#define	UMODEM_N_DATA_TRANSFER 4
#define	UMODEM_N_INTR_TRANSFER 2

#define	UMODEM_MODVER			1	/* module version */

struct umodem_softc {
	struct usb2_com_super_softc sc_super_ucom;
	struct usb2_com_softc sc_ucom;

	struct usb2_xfer *sc_xfer_data[UMODEM_N_DATA_TRANSFER];
	struct usb2_xfer *sc_xfer_intr[UMODEM_N_INTR_TRANSFER];
	struct usb2_device *sc_udev;

	uint16_t sc_line;

	uint8_t	sc_lsr;			/* local status register */
	uint8_t	sc_msr;			/* modem status register */
	uint8_t	sc_ctrl_iface_no;
	uint8_t	sc_ctrl_iface_index;
	uint8_t	sc_data_iface_no;
	uint8_t	sc_data_iface_index;
	uint8_t	sc_cm_over_data;
	uint8_t	sc_cm_cap;		/* CM capabilities */
	uint8_t	sc_acm_cap;		/* ACM capabilities */
	uint8_t	sc_flag;
#define	UMODEM_FLAG_READ_STALL  0x01
#define	UMODEM_FLAG_WRITE_STALL 0x02
#define	UMODEM_FLAG_INTR_STALL  0x04
};

static device_probe_t umodem_probe;
static device_attach_t umodem_attach;
static device_detach_t umodem_detach;

static usb2_callback_t umodem_intr_callback;
static usb2_callback_t umodem_intr_clear_stall_callback;
static usb2_callback_t umodem_write_callback;
static usb2_callback_t umodem_read_callback;
static usb2_callback_t umodem_write_clear_stall_callback;
static usb2_callback_t umodem_read_clear_stall_callback;

static void umodem_start_read(struct usb2_com_softc *ucom);
static void umodem_stop_read(struct usb2_com_softc *ucom);
static void umodem_start_write(struct usb2_com_softc *ucom);
static void umodem_stop_write(struct usb2_com_softc *ucom);
static void umodem_get_caps(struct usb2_attach_arg *uaa, uint8_t *cm, uint8_t *acm);
static void umodem_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *lsr, uint8_t *msr);
static int umodem_pre_param(struct usb2_com_softc *ucom, struct termios *t);
static void umodem_cfg_param(struct usb2_com_softc *ucom, struct termios *t);
static int umodem_ioctl(struct usb2_com_softc *ucom, uint32_t cmd, caddr_t data, int flag, struct thread *td);
static void umodem_cfg_set_dtr(struct usb2_com_softc *ucom, uint8_t onoff);
static void umodem_cfg_set_rts(struct usb2_com_softc *ucom, uint8_t onoff);
static void umodem_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff);
static void *umodem_get_desc(struct usb2_attach_arg *uaa, uint8_t type, uint8_t subtype);
static usb2_error_t umodem_set_comm_feature(struct usb2_device *udev, uint8_t iface_no, uint16_t feature, uint16_t state);
static void umodem_cfg_do_request(struct umodem_softc *sc, struct usb2_device_request *req, void *data);

static const struct usb2_config umodem_config_data[UMODEM_N_DATA_TRANSFER] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = UMODEM_BUF_SIZE,
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &umodem_write_callback,
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = UMODEM_BUF_SIZE,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &umodem_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &umodem_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &umodem_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static const struct usb2_config umodem_config_intr[UMODEM_N_INTR_TRANSFER] = {
	[0] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = 0,	/* use wMaxPacketSize */
		.mh.callback = &umodem_intr_callback,
	},

	[1] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &umodem_intr_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static const struct usb2_com_callback umodem_callback = {
	.usb2_com_cfg_get_status = &umodem_cfg_get_status,
	.usb2_com_cfg_set_dtr = &umodem_cfg_set_dtr,
	.usb2_com_cfg_set_rts = &umodem_cfg_set_rts,
	.usb2_com_cfg_set_break = &umodem_cfg_set_break,
	.usb2_com_cfg_param = &umodem_cfg_param,
	.usb2_com_pre_param = &umodem_pre_param,
	.usb2_com_ioctl = &umodem_ioctl,
	.usb2_com_start_read = &umodem_start_read,
	.usb2_com_stop_read = &umodem_stop_read,
	.usb2_com_start_write = &umodem_start_write,
	.usb2_com_stop_write = &umodem_stop_write,
};

static device_method_t umodem_methods[] = {
	DEVMETHOD(device_probe, umodem_probe),
	DEVMETHOD(device_attach, umodem_attach),
	DEVMETHOD(device_detach, umodem_detach),
	{0, 0}
};

static devclass_t umodem_devclass;

static driver_t umodem_driver = {
	.name = "umodem",
	.methods = umodem_methods,
	.size = sizeof(struct umodem_softc),
};

DRIVER_MODULE(umodem, ushub, umodem_driver, umodem_devclass, NULL, 0);
MODULE_DEPEND(umodem, usb2_serial, 1, 1, 1);
MODULE_DEPEND(umodem, usb2_core, 1, 1, 1);
MODULE_VERSION(umodem, UMODEM_MODVER);

static int
umodem_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	uint8_t cm;
	uint8_t acm;
	int error;

	DPRINTFN(11, "\n");

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	error = usb2_lookup_id_by_uaa(umodem_devs, sizeof(umodem_devs), uaa);
	if (error) {
		return (error);
	}
	if (uaa->driver_info == NULL) {
		/* some modems do not have any capabilities */
		return (error);
	}
	umodem_get_caps(uaa, &cm, &acm);
	if (!(cm & USB_CDC_CM_DOES_CM) ||
	    !(cm & USB_CDC_CM_OVER_DATA) ||
	    !(acm & USB_CDC_ACM_HAS_LINE)) {
		error = ENXIO;
	}
	return (error);
}

static int
umodem_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct umodem_softc *sc = device_get_softc(dev);
	struct usb2_cdc_cm_descriptor *cmd;
	uint8_t i;
	int error;

	if (sc == NULL) {
		return (ENOMEM);
	}
	device_set_usb2_desc(dev);

	sc->sc_ctrl_iface_no = uaa->info.bIfaceNum;
	sc->sc_ctrl_iface_index = uaa->info.bIfaceIndex;
	sc->sc_udev = uaa->device;

	umodem_get_caps(uaa, &sc->sc_cm_cap, &sc->sc_acm_cap);

	/* get the data interface number */

	cmd = umodem_get_desc(uaa, UDESC_CS_INTERFACE, UDESCSUB_CDC_CM);

	if ((cmd == NULL) || (cmd->bLength < sizeof(*cmd))) {
		device_printf(dev, "no CM descriptor!\n");
		goto detach;
	}
	sc->sc_data_iface_no = cmd->bDataInterface;

	device_printf(dev, "data interface %d, has %sCM over "
	    "data, has %sbreak\n",
	    sc->sc_data_iface_no,
	    sc->sc_cm_cap & USB_CDC_CM_OVER_DATA ? "" : "no ",
	    sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK ? "" : "no ");

	/* get the data interface too */

	for (i = 0;; i++) {
		struct usb2_interface *iface;
		struct usb2_interface_descriptor *id;

		iface = usb2_get_iface(uaa->device, i);

		if (iface) {

			id = usb2_get_interface_descriptor(iface);

			if (id && (id->bInterfaceNumber == sc->sc_data_iface_no)) {
				sc->sc_data_iface_index = i;
				usb2_set_parent_iface(uaa->device, i, uaa->info.bIfaceIndex);
				break;
			}
		} else {
			device_printf(dev, "no data interface!\n");
			goto detach;
		}
	}

	if (sc->sc_cm_cap & USB_CDC_CM_OVER_DATA) {
		if (sc->sc_acm_cap & USB_CDC_ACM_HAS_FEATURE) {

			error = umodem_set_comm_feature
			    (uaa->device, sc->sc_ctrl_iface_no,
			    UCDC_ABSTRACT_STATE, UCDC_DATA_MULTIPLEXED);

			/* ignore any errors */
		}
		sc->sc_cm_over_data = 1;
	}
	error = usb2_transfer_setup(uaa->device,
	    &sc->sc_data_iface_index, sc->sc_xfer_data,
	    umodem_config_data, UMODEM_N_DATA_TRANSFER,
	    sc, &Giant);
	if (error) {
		goto detach;
	}
	error = usb2_transfer_setup(uaa->device,
	    &sc->sc_ctrl_iface_index, sc->sc_xfer_intr,
	    umodem_config_intr, UMODEM_N_INTR_TRANSFER,
	    sc, &Giant);

	if (error == 0) {
		device_printf(dev, "status change "
		    "notification available\n");
	}
	/* clear stall at first run */
	sc->sc_flag |= (UMODEM_FLAG_READ_STALL |
	    UMODEM_FLAG_WRITE_STALL);

	error = usb2_com_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &umodem_callback, &Giant);
	if (error) {
		goto detach;
	}
	return (0);

detach:
	umodem_detach(dev);
	return (ENXIO);
}

static void
umodem_start_read(struct usb2_com_softc *ucom)
{
	struct umodem_softc *sc = ucom->sc_parent;

	if (sc->sc_xfer_intr[0]) {
		/* start interrupt endpoint */
		usb2_transfer_start(sc->sc_xfer_intr[0]);
	}
	/* start read endpoint */
	usb2_transfer_start(sc->sc_xfer_data[1]);
	return;
}

static void
umodem_stop_read(struct usb2_com_softc *ucom)
{
	struct umodem_softc *sc = ucom->sc_parent;

	if (sc->sc_xfer_intr[0]) {
		/* stop interrupt endpoint */
		usb2_transfer_stop(sc->sc_xfer_intr[0]);
	}
	/* stop read endpoint */
	usb2_transfer_stop(sc->sc_xfer_data[3]);
	usb2_transfer_stop(sc->sc_xfer_data[1]);
	return;
}

static void
umodem_start_write(struct usb2_com_softc *ucom)
{
	struct umodem_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer_data[0]);
	return;
}

static void
umodem_stop_write(struct usb2_com_softc *ucom)
{
	struct umodem_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer_data[2]);
	usb2_transfer_stop(sc->sc_xfer_data[0]);
	return;
}

static void
umodem_get_caps(struct usb2_attach_arg *uaa, uint8_t *cm, uint8_t *acm)
{
	struct usb2_cdc_cm_descriptor *cmd;
	struct usb2_cdc_acm_descriptor *cad;

	*cm = *acm = 0;

	cmd = umodem_get_desc(uaa, UDESC_CS_INTERFACE, UDESCSUB_CDC_CM);
	if ((cmd == NULL) || (cmd->bLength < sizeof(*cmd))) {
		DPRINTF("no CM desc\n");
		return;
	}
	*cm = cmd->bmCapabilities;

	cad = umodem_get_desc(uaa, UDESC_CS_INTERFACE, UDESCSUB_CDC_ACM);
	if ((cad == NULL) || (cad->bLength < sizeof(*cad))) {
		DPRINTF("no ACM desc\n");
		return;
	}
	*acm = cad->bmCapabilities;

	return;
}

static void
umodem_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *lsr, uint8_t *msr)
{
	struct umodem_softc *sc = ucom->sc_parent;

	DPRINTF("\n");

	*lsr = sc->sc_lsr;
	*msr = sc->sc_msr;
	return;
}

static int
umodem_pre_param(struct usb2_com_softc *ucom, struct termios *t)
{
	return (0);			/* we accept anything */
}

static void
umodem_cfg_param(struct usb2_com_softc *ucom, struct termios *t)
{
	struct umodem_softc *sc = ucom->sc_parent;
	struct usb2_cdc_line_state ls;
	struct usb2_device_request req;

	DPRINTF("sc=%p\n", sc);

	bzero(&ls, sizeof(ls));

	USETDW(ls.dwDTERate, t->c_ospeed);

	ls.bCharFormat = (t->c_cflag & CSTOPB) ?
	    UCDC_STOP_BIT_2 : UCDC_STOP_BIT_1;

	ls.bParityType = (t->c_cflag & PARENB) ?
	    ((t->c_cflag & PARODD) ?
	    UCDC_PARITY_ODD : UCDC_PARITY_EVEN) : UCDC_PARITY_NONE;

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
	req.wIndex[0] = sc->sc_ctrl_iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, sizeof(ls));

	umodem_cfg_do_request(sc, &req, &ls);
	return;
}

static int
umodem_ioctl(struct usb2_com_softc *ucom, uint32_t cmd, caddr_t data,
    int flag, struct thread *td)
{
	struct umodem_softc *sc = ucom->sc_parent;
	int error = 0;

	DPRINTF("cmd=0x%08x\n", cmd);

	switch (cmd) {
	case USB_GET_CM_OVER_DATA:
		*(int *)data = sc->sc_cm_over_data;
		break;

	case USB_SET_CM_OVER_DATA:
		if (*(int *)data != sc->sc_cm_over_data) {
			/* XXX change it */
		}
		break;

	default:
		DPRINTF("unknown\n");
		error = ENOIOCTL;
		break;
	}

	return (error);
}

static void
umodem_cfg_set_dtr(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct umodem_softc *sc = ucom->sc_parent;
	struct usb2_device_request req;

	DPRINTF("onoff=%d\n", onoff);

	if (onoff)
		sc->sc_line |= UCDC_LINE_DTR;
	else
		sc->sc_line &= ~UCDC_LINE_DTR;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, sc->sc_line);
	req.wIndex[0] = sc->sc_ctrl_iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);

	umodem_cfg_do_request(sc, &req, NULL);
	return;
}

static void
umodem_cfg_set_rts(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct umodem_softc *sc = ucom->sc_parent;
	struct usb2_device_request req;

	DPRINTF("onoff=%d\n", onoff);

	if (onoff)
		sc->sc_line |= UCDC_LINE_RTS;
	else
		sc->sc_line &= ~UCDC_LINE_RTS;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, sc->sc_line);
	req.wIndex[0] = sc->sc_ctrl_iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);

	umodem_cfg_do_request(sc, &req, NULL);
	return;
}

static void
umodem_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct umodem_softc *sc = ucom->sc_parent;
	struct usb2_device_request req;
	uint16_t temp;

	DPRINTF("onoff=%d\n", onoff);

	if (sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK) {

		temp = onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF;

		req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
		req.bRequest = UCDC_SEND_BREAK;
		USETW(req.wValue, temp);
		req.wIndex[0] = sc->sc_ctrl_iface_no;
		req.wIndex[1] = 0;
		USETW(req.wLength, 0);

		umodem_cfg_do_request(sc, &req, NULL);
	}
	return;
}

static void
umodem_intr_callback(struct usb2_xfer *xfer)
{
	struct usb2_cdc_notification pkt;
	struct umodem_softc *sc = xfer->priv_sc;
	uint16_t wLen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		if (xfer->actlen < 8) {
			DPRINTF("received short packet, "
			    "%d bytes\n", xfer->actlen);
			goto tr_setup;
		}
		if (xfer->actlen > sizeof(pkt)) {
			DPRINTF("truncating message\n");
			xfer->actlen = sizeof(pkt);
		}
		usb2_copy_out(xfer->frbuffers, 0, &pkt, xfer->actlen);

		xfer->actlen -= 8;

		wLen = UGETW(pkt.wLength);
		if (xfer->actlen > wLen) {
			xfer->actlen = wLen;
		}
		if (pkt.bmRequestType != UCDC_NOTIFICATION) {
			DPRINTF("unknown message type, "
			    "0x%02x, on notify pipe!\n",
			    pkt.bmRequestType);
			goto tr_setup;
		}
		switch (pkt.bNotification) {
		case UCDC_N_SERIAL_STATE:
			/*
			 * Set the serial state in ucom driver based on
			 * the bits from the notify message
			 */
			if (xfer->actlen < 2) {
				DPRINTF("invalid notification "
				    "length, %d bytes!\n", xfer->actlen);
				break;
			}
			DPRINTF("notify bytes = %02x%02x\n",
			    pkt.data[0],
			    pkt.data[1]);

			/* Currently, lsr is always zero. */
			sc->sc_lsr = 0;
			sc->sc_msr = 0;

			if (pkt.data[0] & UCDC_N_SERIAL_RI) {
				sc->sc_msr |= SER_RI;
			}
			if (pkt.data[0] & UCDC_N_SERIAL_DSR) {
				sc->sc_msr |= SER_DSR;
			}
			if (pkt.data[0] & UCDC_N_SERIAL_DCD) {
				sc->sc_msr |= SER_DCD;
			}
			usb2_com_status_change(&sc->sc_ucom);
			break;

		default:
			DPRINTF("unknown notify message: 0x%02x\n",
			    pkt.bNotification);
			break;
		}

	case USB_ST_SETUP:
tr_setup:
		if (sc->sc_flag & UMODEM_FLAG_INTR_STALL) {
			usb2_transfer_start(sc->sc_xfer_intr[1]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flag |= UMODEM_FLAG_INTR_STALL;
			usb2_transfer_start(sc->sc_xfer_intr[1]);
		}
		return;

	}
}

static void
umodem_intr_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct umodem_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer_intr[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flag &= ~UMODEM_FLAG_INTR_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
umodem_write_callback(struct usb2_xfer *xfer)
{
	struct umodem_softc *sc = xfer->priv_sc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
		if (sc->sc_flag & UMODEM_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer_data[2]);
			return;
		}
		if (usb2_com_get_data(&sc->sc_ucom, xfer->frbuffers, 0,
		    UMODEM_BUF_SIZE, &actlen)) {

			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flag |= UMODEM_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer_data[2]);
		}
		return;

	}
}

static void
umodem_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct umodem_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer_data[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flag &= ~UMODEM_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
umodem_read_callback(struct usb2_xfer *xfer)
{
	struct umodem_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTF("actlen=%d\n", xfer->actlen);

		usb2_com_put_data(&sc->sc_ucom, xfer->frbuffers, 0,
		    xfer->actlen);

	case USB_ST_SETUP:
		if (sc->sc_flag & UMODEM_FLAG_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer_data[3]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flag |= UMODEM_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer_data[3]);
		}
		return;

	}
}

static void
umodem_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct umodem_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer_data[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flag &= ~UMODEM_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void *
umodem_get_desc(struct usb2_attach_arg *uaa, uint8_t type, uint8_t subtype)
{
	return (usb2_find_descriptor(uaa->device, NULL, uaa->info.bIfaceIndex,
	    type, 0 - 1, subtype, 0 - 1));
}

static usb2_error_t
umodem_set_comm_feature(struct usb2_device *udev, uint8_t iface_no,
    uint16_t feature, uint16_t state)
{
	struct usb2_device_request req;
	struct usb2_cdc_abstract_state ast;

	DPRINTF("feature=%d state=%d\n",
	    feature, state);

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_COMM_FEATURE;
	USETW(req.wValue, feature);
	req.wIndex[0] = iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, UCDC_ABSTRACT_STATE_LENGTH);
	USETW(ast.wState, state);

	return (usb2_do_request(udev, &Giant, &req, &ast));
}

static int
umodem_detach(device_t dev)
{
	struct umodem_softc *sc = device_get_softc(dev);

	DPRINTF("sc=%p\n", sc);

	usb2_com_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);

	usb2_transfer_unsetup(sc->sc_xfer_intr, UMODEM_N_INTR_TRANSFER);

	usb2_transfer_unsetup(sc->sc_xfer_data, UMODEM_N_DATA_TRANSFER);

	return (0);
}

static void
umodem_cfg_do_request(struct umodem_softc *sc, struct usb2_device_request *req,
    void *data)
{
	uint16_t length;
	usb2_error_t err;

	if (usb2_com_cfg_is_gone(&sc->sc_ucom)) {
		goto error;
	}
	err = usb2_do_request_flags(sc->sc_udev, &Giant, req,
	    data, 0, NULL, 1000);

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
