/*	$NetBSD: uplcom.c,v 1.21 2001/11/13 06:24:56 lukem Exp $	*/

/*-
 * Copyright (c) 2001-2002, Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
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
 * Simple datasheet
 * http://www.prolific.com.tw/download/DataSheet/pl2303_ds11.PDF
 * http://www.nisseisg.co.jp/jyouhou/_cp/@gif/2303.pdf
 * 	(english)
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#if __FreeBSD_version >= 500014
#include <sys/selinfo.h>
#else
#include <sys/select.h>
#endif
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ucomvar.h>

#ifdef USB_DEBUG
static int	uplcomdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, uplcom, CTLFLAG_RW, 0, "USB uplcom");
SYSCTL_INT(_hw_usb_uplcom, OID_AUTO, debug, CTLFLAG_RW,
	   &uplcomdebug, 0, "uplcom debug level");

#define DPRINTFN(n, x)	do { \
				if (uplcomdebug > (n)) \
					logprintf x; \
			} while (0)
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define UPLCOM_MODVER			1	/* module version */

#define	UPLCOM_CONFIG_INDEX		0
#define	UPLCOM_IFACE_INDEX		0
#define	UPLCOM_SECOND_IFACE_INDEX	1

#define UPLCOM_INTR_INTERVAL		100	/* ms */

#define	UPLCOM_SET_REQUEST		0x01
#define	UPLCOM_SET_CRTSCTS		0x41
#define RSAQ_STATUS_DSR			0x02
#define RSAQ_STATUS_DCD			0x01

struct	uplcom_softc {
	struct ucom_softc	sc_ucom;

	int			sc_iface_number;	/* interface number */

	usbd_interface_handle	sc_intr_iface;	/* interrupt interface */
	int			sc_intr_number;	/* interrupt number */
	usbd_pipe_handle	sc_intr_pipe;	/* interrupt pipe */
	u_char			*sc_intr_buf;	/* interrupt buffer */
	int			sc_isize;

	usb_cdc_line_state_t	sc_line_state;	/* current line state */
	u_char			sc_dtr;		/* current DTR state */
	u_char			sc_rts;		/* current RTS state */
	u_char			sc_status;

	u_char			sc_lsr;		/* Local status register */
	u_char			sc_msr;		/* uplcom status register */
};

/*
 * These are the maximum number of bytes transferred per frame.
 * The output buffer size cannot be increased due to the size encoding.
 */
#define UPLCOMIBUFSIZE 256
#define UPLCOMOBUFSIZE 256

Static	usbd_status uplcom_reset(struct uplcom_softc *);
Static	usbd_status uplcom_set_line_coding(struct uplcom_softc *,
					   usb_cdc_line_state_t *);
Static	usbd_status uplcom_set_crtscts(struct uplcom_softc *);
Static	void uplcom_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);

Static	void uplcom_set(void *, int, int, int);
Static	void uplcom_dtr(struct uplcom_softc *, int);
Static	void uplcom_rts(struct uplcom_softc *, int);
Static	void uplcom_break(struct uplcom_softc *, int);
Static	void uplcom_set_line_state(struct uplcom_softc *);
Static	void uplcom_get_status(void *, int, u_char *, u_char *);
#if TODO
Static	int  uplcom_ioctl(void *, int, u_long, caddr_t, int, usb_proc_ptr);
#endif
Static	int  uplcom_param(void *, int, struct termios *);
Static	int  uplcom_open(void *, int);
Static	void uplcom_close(void *, int);

struct ucom_callback uplcom_callback = {
	uplcom_get_status,
	uplcom_set,
	uplcom_param,
	NULL, /* uplcom_ioctl, TODO */
	uplcom_open,
	uplcom_close,
	NULL,
	NULL
};

static const struct uplcom_product {
	uint16_t	vendor;
	uint16_t	product;
} uplcom_products [] = {
	/* I/O DATA USB-RSAQ */
	{ USB_VENDOR_IODATA, USB_PRODUCT_IODATA_USBRSAQ },
	/* I/O DATA USB-RSAQ2 */
	{ USB_VENDOR_PROLIFIC, USB_PRODUCT_PROLIFIC_RSAQ2 },
	/* PLANEX USB-RS232 URS-03 */
	{ USB_VENDOR_ATEN, USB_PRODUCT_ATEN_UC232A },
	/* IOGEAR/ATEN UC-232A */
	{ USB_VENDOR_PROLIFIC, USB_PRODUCT_PROLIFIC_PL2303 },
	/* TDK USB-PHS Adapter UHA6400 */
	{ USB_VENDOR_TDK, USB_PRODUCT_TDK_UHA6400 },
	/* RATOC REX-USB60 */
	{ USB_VENDOR_RATOC, USB_PRODUCT_RATOC_REXUSB60 },
	/* ELECOM UC-SGT */
	{ USB_VENDOR_ELECOM, USB_PRODUCT_ELECOM_UCSGT },
	/* SOURCENEXT KeikaiDenwa 8 */
	{ USB_VENDOR_SOURCENEXT, USB_PRODUCT_SOURCENEXT_KEIKAI8 },
	/* SOURCENEXT KeikaiDenwa 8 with chager */
	{ USB_VENDOR_SOURCENEXT, USB_PRODUCT_SOURCENEXT_KEIKAI8_CHG },
	/* HAL Corporation Crossam2+USB */
	{ USB_VENDOR_HAL, USB_PRODUCT_HAL_IMR001 },
	{ 0, 0 }
};

Static device_probe_t uplcom_match;
Static device_attach_t uplcom_attach;
Static device_detach_t uplcom_detach;

Static device_method_t uplcom_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uplcom_match),
	DEVMETHOD(device_attach, uplcom_attach),
	DEVMETHOD(device_detach, uplcom_detach),
	{ 0, 0 }
};

Static driver_t uplcom_driver = {
	"ucom",
	uplcom_methods,
	sizeof (struct uplcom_softc)
};

DRIVER_MODULE(uplcom, uhub, uplcom_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(uplcom, usb, 1, 1, 1);
MODULE_DEPEND(uplcom, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);
MODULE_VERSION(uplcom, UPLCOM_MODVER);

USB_MATCH(uplcom)
{
	USB_MATCH_START(uplcom, uaa);
	int i;

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	for (i = 0; uplcom_products[i].vendor != 0; i++) {
		if (uplcom_products[i].vendor == uaa->vendor &&
		    uplcom_products[i].product == uaa->product) {
			return (UMATCH_VENDOR_PRODUCT);
		}
	}
	return (UMATCH_NONE);
}

USB_ATTACH(uplcom)
{
	USB_ATTACH_START(uplcom, sc, uaa);
	usbd_device_handle dev = uaa->device;
	struct ucom_softc *ucom;
	usb_config_descriptor_t *cdesc;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfo;
	const char *devname;
	usbd_status err;
	int i;

	devinfo = malloc(1024, M_USBDEV, M_WAITOK);
	ucom = &sc->sc_ucom;

	bzero(sc, sizeof (struct uplcom_softc));

	usbd_devinfo(dev, 0, devinfo);
	/* USB_ATTACH_SETUP; */
	ucom->sc_dev = self;
	device_set_desc_copy(self, devinfo);
	/* USB_ATTACH_SETUP; */

	ucom->sc_udev = dev;
	ucom->sc_iface = uaa->iface;

	devname = USBDEVNAME(ucom->sc_dev);
	printf("%s: %s\n", devname, devinfo);

	DPRINTF(("uplcom attach: sc = %p\n", sc));

	/* initialize endpoints */
	ucom->sc_bulkin_no = ucom->sc_bulkout_no = -1;
	sc->sc_intr_number = -1;
	sc->sc_intr_pipe = NULL;

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, UPLCOM_CONFIG_INDEX, 1);
	if (err) {
		printf("%s: failed to set configuration: %s\n",
			devname, usbd_errstr(err));
		ucom->sc_dying = 1;
		goto error;
	}

	/* get the config descriptor */
	cdesc = usbd_get_config_descriptor(ucom->sc_udev);

	if (cdesc == NULL) {
		printf("%s: failed to get configuration descriptor\n",
			USBDEVNAME(ucom->sc_dev));
		ucom->sc_dying = 1;
		goto error;
	}

	/* get the (first/common) interface */
	err = usbd_device2interface_handle(dev, UPLCOM_IFACE_INDEX,
					   &ucom->sc_iface);
	if (err) {
		printf("%s: failed to get interface: %s\n",
			devname, usbd_errstr(err));
		ucom->sc_dying = 1;
		goto error;
	}

	/* Find the interrupt endpoints */

	id = usbd_get_interface_descriptor(ucom->sc_iface);
	sc->sc_iface_number = id->bInterfaceNumber;

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(ucom->sc_iface, i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor for %d\n",
				USBDEVNAME(ucom->sc_dev), i);
			ucom->sc_dying = 1;
			goto error;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_intr_number = ed->bEndpointAddress;
			sc->sc_isize = UGETW(ed->wMaxPacketSize);
		}
	}

	if (sc->sc_intr_number == -1) {
		printf("%s: Could not find interrupt in\n",
			USBDEVNAME(ucom->sc_dev));
		ucom->sc_dying = 1;
		goto error;
	}

	/* keep interface for interrupt */
	sc->sc_intr_iface = ucom->sc_iface;

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
	if (cdesc->bNumInterface == 2) {
		err = usbd_device2interface_handle(dev,
						   UPLCOM_SECOND_IFACE_INDEX,
						   &ucom->sc_iface);
		if (err) {
			printf("%s: failed to get second interface: %s\n",
				devname, usbd_errstr(err));
			ucom->sc_dying = 1;
			goto error;
		}
	}

	/* Find the bulk{in,out} endpoints */

	id = usbd_get_interface_descriptor(ucom->sc_iface);
	sc->sc_iface_number = id->bInterfaceNumber;

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(ucom->sc_iface, i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor for %d\n",
				USBDEVNAME(ucom->sc_dev), i);
			ucom->sc_dying = 1;
			goto error;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ucom->sc_bulkin_no = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ucom->sc_bulkout_no = ed->bEndpointAddress;
		}
	}

	if (ucom->sc_bulkin_no == -1) {
		printf("%s: Could not find data bulk in\n",
			USBDEVNAME(ucom->sc_dev));
		ucom->sc_dying = 1;
		goto error;
	}

	if (ucom->sc_bulkout_no == -1) {
		printf("%s: Could not find data bulk out\n",
			USBDEVNAME(ucom->sc_dev));
		ucom->sc_dying = 1;
		goto error;
	}

	sc->sc_dtr = sc->sc_rts = -1;
	ucom->sc_parent = sc;
	ucom->sc_portno = UCOM_UNK_PORTNO;
	/* bulkin, bulkout set above */
	ucom->sc_ibufsize = UPLCOMIBUFSIZE;
	ucom->sc_obufsize = UPLCOMOBUFSIZE;
	ucom->sc_ibufsizepad = UPLCOMIBUFSIZE;
	ucom->sc_opkthdrlen = 0;
	ucom->sc_callback = &uplcom_callback;

	err = uplcom_reset(sc);

	if (err) {
		printf("%s: reset failed: %s\n",
		       USBDEVNAME(ucom->sc_dev), usbd_errstr(err));
		ucom->sc_dying = 1;
		goto error;
	}

	DPRINTF(("uplcom: in = 0x%x, out = 0x%x, intr = 0x%x\n",
		 ucom->sc_bulkin_no, ucom->sc_bulkout_no, sc->sc_intr_number));

	ucom_attach(&sc->sc_ucom);

	free(devinfo, M_USBDEV);
	USB_ATTACH_SUCCESS_RETURN;

error:
	free(devinfo, M_USBDEV);
	USB_ATTACH_ERROR_RETURN;
}

USB_DETACH(uplcom)
{
	USB_DETACH_START(uplcom, sc);
	int rv = 0;

	DPRINTF(("uplcom_detach: sc = %p\n", sc));

	if (sc->sc_intr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_intr_pipe);
		usbd_close_pipe(sc->sc_intr_pipe);
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}

	sc->sc_ucom.sc_dying = 1;

	rv = ucom_detach(&sc->sc_ucom);

	return (rv);
}

Static usbd_status
uplcom_reset(struct uplcom_softc *sc)
{
	usb_device_request_t req;
	usbd_status err;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UPLCOM_SET_REQUEST;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_iface_number);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, 0);
	if (err) {
		printf("%s: uplcom_reset: %s\n",
		       USBDEVNAME(sc->sc_ucom.sc_dev), usbd_errstr(err));
		return (EIO);
	}

	return (0);
}

Static void
uplcom_set_line_state(struct uplcom_softc *sc)
{
	usb_device_request_t req;
	int ls;
	usbd_status err;

	ls = (sc->sc_dtr ? UCDC_LINE_DTR : 0) |
		(sc->sc_rts ? UCDC_LINE_RTS : 0);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, ls);
	USETW(req.wIndex, sc->sc_iface_number);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, 0);
	if (err)
		printf("%s: uplcom_set_line_status: %s\n",
		       USBDEVNAME(sc->sc_ucom.sc_dev), usbd_errstr(err));
}

Static void
uplcom_set(void *addr, int portno, int reg, int onoff)
{
	struct uplcom_softc *sc = addr;

	switch (reg) {
	case UCOM_SET_DTR:
		uplcom_dtr(sc, onoff);
		break;
	case UCOM_SET_RTS:
		uplcom_rts(sc, onoff);
		break;
	case UCOM_SET_BREAK:
		uplcom_break(sc, onoff);
		break;
	default:
		break;
	}
}

Static void
uplcom_dtr(struct uplcom_softc *sc, int onoff)
{
	DPRINTF(("uplcom_dtr: onoff = %d\n", onoff));

	if (sc->sc_dtr == onoff)
		return;
	sc->sc_dtr = onoff;

	uplcom_set_line_state(sc);
}

Static void
uplcom_rts(struct uplcom_softc *sc, int onoff)
{
	DPRINTF(("uplcom_rts: onoff = %d\n", onoff));

	if (sc->sc_rts == onoff)
		return;
	sc->sc_rts = onoff;

	uplcom_set_line_state(sc);
}

Static void
uplcom_break(struct uplcom_softc *sc, int onoff)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("uplcom_break: onoff = %d\n", onoff));

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_BREAK;
	USETW(req.wValue, onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);
	USETW(req.wIndex, sc->sc_iface_number);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, 0);
	if (err)
		printf("%s: uplcom_break: %s\n",
		       USBDEVNAME(sc->sc_ucom.sc_dev), usbd_errstr(err));
}

Static usbd_status
uplcom_set_crtscts(struct uplcom_softc *sc)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("uplcom_set_crtscts: on\n"));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UPLCOM_SET_REQUEST;
	USETW(req.wValue, 0);
	USETW(req.wIndex, UPLCOM_SET_CRTSCTS);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, 0);
	if (err) {
		printf("%s: uplcom_set_crtscts: %s\n",
		       USBDEVNAME(sc->sc_ucom.sc_dev), usbd_errstr(err));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static usbd_status
uplcom_set_line_coding(struct uplcom_softc *sc, usb_cdc_line_state_t *state)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF((
"uplcom_set_line_coding: rate = %d, fmt = %d, parity = %d bits = %d\n",
		 UGETDW(state->dwDTERate), state->bCharFormat,
		 state->bParityType, state->bDataBits));

	if (memcmp(state, &sc->sc_line_state, UCDC_LINE_STATE_LENGTH) == 0) {
		DPRINTF(("uplcom_set_line_coding: already set\n"));
		return (USBD_NORMAL_COMPLETION);
	}

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_LINE_CODING;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_iface_number);
	USETW(req.wLength, UCDC_LINE_STATE_LENGTH);

	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, state);
	if (err) {
		printf("%s: uplcom_set_line_coding: %s\n",
		       USBDEVNAME(sc->sc_ucom.sc_dev), usbd_errstr(err));
		return (err);
	}

	sc->sc_line_state = *state;

	return (USBD_NORMAL_COMPLETION);
}

Static int
uplcom_param(void *addr, int portno, struct termios *t)
{
	struct uplcom_softc *sc = addr;
	usbd_status err;
	usb_cdc_line_state_t ls;

	DPRINTF(("uplcom_param: sc = %p\n", sc));

	USETDW(ls.dwDTERate, t->c_ospeed);
	if (ISSET(t->c_cflag, CSTOPB))
		ls.bCharFormat = UCDC_STOP_BIT_2;
	else
		ls.bCharFormat = UCDC_STOP_BIT_1;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			ls.bParityType = UCDC_PARITY_ODD;
		else
			ls.bParityType = UCDC_PARITY_EVEN;
	} else
		ls.bParityType = UCDC_PARITY_NONE;
	switch (ISSET(t->c_cflag, CSIZE)) {
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

	err = uplcom_set_line_coding(sc, &ls);
	if (err)
		return (EIO);

	if (ISSET(t->c_cflag, CRTSCTS)) {
		err = uplcom_set_crtscts(sc);
		if (err)
			return (EIO);
	}

	return (0);
}

Static int
uplcom_open(void *addr, int portno)
{
	struct uplcom_softc *sc = addr;
	int err;

	if (sc->sc_ucom.sc_dying)
		return (ENXIO);

	DPRINTF(("uplcom_open: sc = %p\n", sc));

	if (sc->sc_intr_number != -1 && sc->sc_intr_pipe == NULL) {
		sc->sc_status = 0; /* clear status bit */
		sc->sc_intr_buf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);
		err = usbd_open_pipe_intr(sc->sc_intr_iface,
					  sc->sc_intr_number,
					  USBD_SHORT_XFER_OK,
					  &sc->sc_intr_pipe,
					  sc,
					  sc->sc_intr_buf,
					  sc->sc_isize,
					  uplcom_intr,
					  UPLCOM_INTR_INTERVAL);
		if (err) {
			printf("%s: cannot open interrupt pipe (addr %d)\n",
			       USBDEVNAME(sc->sc_ucom.sc_dev),
			       sc->sc_intr_number);
			return (EIO);
		}
	}

	return (0);
}

Static void
uplcom_close(void *addr, int portno)
{
	struct uplcom_softc *sc = addr;
	int err;

	if (sc->sc_ucom.sc_dying)
		return;

	DPRINTF(("uplcom_close: close\n"));

	if (sc->sc_intr_pipe != NULL) {
		err = usbd_abort_pipe(sc->sc_intr_pipe);
		if (err)
			printf("%s: abort interrupt pipe failed: %s\n",
			       USBDEVNAME(sc->sc_ucom.sc_dev),
			       usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_intr_pipe);
		if (err)
			printf("%s: close interrupt pipe failed: %s\n",
			       USBDEVNAME(sc->sc_ucom.sc_dev),
			       usbd_errstr(err));
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}
}

Static void
uplcom_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct uplcom_softc *sc = priv;
	u_char *buf = sc->sc_intr_buf;
	u_char pstatus;

	if (sc->sc_ucom.sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		DPRINTF(("%s: uplcom_intr: abnormal status: %s\n",
			USBDEVNAME(sc->sc_ucom.sc_dev),
			usbd_errstr(status)));
		usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);
		return;
	}

	DPRINTF(("%s: uplcom status = %02x\n",
		 USBDEVNAME(sc->sc_ucom.sc_dev), buf[8]));

	sc->sc_lsr = sc->sc_msr = 0;
	pstatus = buf[8];
	if (ISSET(pstatus, RSAQ_STATUS_DSR))
		sc->sc_msr |= UMSR_DSR;
	if (ISSET(pstatus, RSAQ_STATUS_DCD))
		sc->sc_msr |= UMSR_DCD;
	ucom_status_change(&sc->sc_ucom);
}

Static void
uplcom_get_status(void *addr, int portno, u_char *lsr, u_char *msr)
{
	struct uplcom_softc *sc = addr;

	DPRINTF(("uplcom_get_status:\n"));

	if (lsr != NULL)
		*lsr = sc->sc_lsr;
	if (msr != NULL)
		*msr = sc->sc_msr;
}

#if TODO
Static int
uplcom_ioctl(void *addr, int portno, u_long cmd, caddr_t data, int flag,
	     usb_proc_ptr p)
{
	struct uplcom_softc *sc = addr;
	int error = 0;

	if (sc->sc_ucom.sc_dying)
		return (EIO);

	DPRINTF(("uplcom_ioctl: cmd = 0x%08lx\n", cmd));

	switch (cmd) {
	case TIOCNOTTY:
	case TIOCMGET:
	case TIOCMSET:
	case USB_GET_CM_OVER_DATA:
	case USB_SET_CM_OVER_DATA:
		break;

	default:
		DPRINTF(("uplcom_ioctl: unknown\n"));
		error = ENOTTY;
		break;
	}

	return (error);
}
#endif
