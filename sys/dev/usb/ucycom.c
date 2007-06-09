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
 *
 * $FreeBSD$
 */

/*
 * Device driver for Cypress CY7C637xx and CY7C640/1xx series USB to
 * RS232 bridges.
 *
 * Normally, a driver for a USB-to-serial chip would hang off the ucom(4)
 * driver, but ucom(4) was written under the assumption that all USB-to-
 * serial chips use bulk pipes for I/O, while the Cypress parts use HID
 * reports.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/tty.h>

#include "usbdevs.h"
#include <dev/usb/usb.h>
#include <dev/usb/usb_port.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/hid.h>

#define UCYCOM_EP_INPUT		 0
#define UCYCOM_EP_OUTPUT	 1

#define UCYCOM_MAX_IOLEN	 32U

struct ucycom_softc {
	device_t		 sc_dev;
	struct tty		*sc_tty;
	int			 sc_error;
	unsigned long		 sc_cintr;
	unsigned long		 sc_cin;
	unsigned long		 sc_clost;
	unsigned long		 sc_cout;

	/* usb parameters */
	usbd_device_handle	 sc_usbdev;
	usbd_interface_handle	 sc_iface;
	usbd_pipe_handle	 sc_pipe;
	uint8_t			 sc_iep; /* input endpoint */
	uint8_t			 sc_fid; /* feature report id*/
	uint8_t			 sc_iid; /* input report id */
	uint8_t			 sc_oid; /* output report id */
	size_t			 sc_flen; /* feature report length */
	size_t			 sc_ilen; /* input report length */
	size_t			 sc_olen; /* output report length */
	uint8_t			 sc_ibuf[UCYCOM_MAX_IOLEN];

	/* model and settings */
	uint32_t		 sc_model;
#define	MODEL_CY7C63743		 0x63743
#define	MODEL_CY7C64013		 0x64013
	uint32_t		 sc_baud;
	uint8_t			 sc_cfg;
#define UCYCOM_CFG_RESET	 0x80
#define UCYCOM_CFG_PARODD	 0x20
#define UCYCOM_CFG_PAREN	 0x10
#define UCYCOM_CFG_STOPB	 0x08
#define UCYCOM_CFG_DATAB	 0x03
	uint8_t			 sc_ist; /* status flags from last input */
	uint8_t			 sc_ost; /* status flags for next output */

	/* flags */
	char			 sc_dying;
};

static int ucycom_probe(device_t);
static int ucycom_attach(device_t);
static int ucycom_detach(device_t);
static t_open_t ucycom_open;
static t_close_t ucycom_close;
static void ucycom_start(struct tty *);
static void ucycom_stop(struct tty *, int);
static int ucycom_param(struct tty *, struct termios *);
static int ucycom_configure(struct ucycom_softc *, uint32_t, uint8_t);
static void ucycom_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);

static device_method_t ucycom_methods[] = {
	DEVMETHOD(device_probe, ucycom_probe),
	DEVMETHOD(device_attach, ucycom_attach),
	DEVMETHOD(device_detach, ucycom_detach),
	{ 0, 0 }
};

static driver_t ucycom_driver = {
	"ucycom",
	ucycom_methods,
	sizeof(struct ucycom_softc),
};

static devclass_t ucycom_devclass;

DRIVER_MODULE(ucycom, uhub, ucycom_driver, ucycom_devclass, usbd_driver_load, 0);
MODULE_VERSION(ucycom, 1);
MODULE_DEPEND(ucycom, usb, 1, 1, 1);

/*
 * Supported devices
 */

static struct ucycom_device {
	uint16_t		 vendor;
	uint16_t		 product;
	uint32_t		 model;
} ucycom_devices[] = {
	{ USB_VENDOR_DELORME, USB_PRODUCT_DELORME_EARTHMATE, MODEL_CY7C64013 },
	{ 0, 0, 0 },
};

#define UCYCOM_DEFAULT_RATE	 4800
#define UCYCOM_DEFAULT_CFG	 0x03 /* N-8-1 */

/*****************************************************************************
 *
 * Driver interface
 *
 */

static int
ucycom_probe(device_t dev)
{
	struct usb_attach_arg *uaa;
	struct ucycom_device *ud;

	uaa = device_get_ivars(dev);
	if (uaa->iface != NULL)
		return (UMATCH_NONE);
	for (ud = ucycom_devices; ud->model != 0; ++ud)
		if (ud->vendor == uaa->vendor && ud->product == uaa->product)
			return (UMATCH_VENDOR_PRODUCT);
	return (UMATCH_NONE);
}

static int
ucycom_attach(device_t dev)
{
	struct usb_attach_arg *uaa;
	struct ucycom_softc *sc;
	struct ucycom_device *ud;
	usb_endpoint_descriptor_t *ued;
	void *urd;
	int error, urdlen;

	/* get arguments and softc */
	uaa = device_get_ivars(dev);
	sc = device_get_softc(dev);
	bzero(sc, sizeof *sc);
	sc->sc_dev = dev;
	sc->sc_usbdev = uaa->device;

	/* get chip model */
	for (ud = ucycom_devices; ud->model != 0; ++ud)
		if (ud->vendor == uaa->vendor && ud->product == uaa->product)
			sc->sc_model = ud->model;
	if (sc->sc_model == 0) {
		device_printf(dev, "unsupported device\n");
		return (ENXIO);
	}
	device_printf(dev, "Cypress CY7C%X USB to RS232 bridge\n", sc->sc_model);

	/* select configuration */
	error = usbd_set_config_index(sc->sc_usbdev, 0, 1 /* verbose */);
	if (error != 0) {
		device_printf(dev, "failed to select configuration: %s\n",
		    usbd_errstr(error));
		return (ENXIO);
	}

	/* get first interface handle */
	error = usbd_device2interface_handle(sc->sc_usbdev, 0, &sc->sc_iface);
	if (error != 0) {
		device_printf(dev, "failed to get interface handle: %s\n",
		    usbd_errstr(error));
		return (ENXIO);
	}

	/* get report descriptor */
	error = usbd_read_report_desc(sc->sc_iface, &urd, &urdlen, M_USBDEV);
	if (error != 0) {
		device_printf(dev, "failed to get report descriptor: %s\n",
		    usbd_errstr(error));
		return (ENXIO);
	}

	/* get report sizes */
	sc->sc_flen = hid_report_size(urd, urdlen, hid_feature, &sc->sc_fid);
	sc->sc_ilen = hid_report_size(urd, urdlen, hid_input, &sc->sc_iid);
	sc->sc_olen = hid_report_size(urd, urdlen, hid_output, &sc->sc_oid);

	if (sc->sc_ilen > UCYCOM_MAX_IOLEN || sc->sc_olen > UCYCOM_MAX_IOLEN) {
		device_printf(dev, "I/O report size too big (%zu, %zu, %u)\n",
		    sc->sc_ilen, sc->sc_olen, UCYCOM_MAX_IOLEN);
		return (ENXIO);
	}

	/* get and verify input endpoint descriptor */
	ued = usbd_interface2endpoint_descriptor(sc->sc_iface, UCYCOM_EP_INPUT);
	if (ued == NULL) {
		device_printf(dev, "failed to get input endpoint descriptor\n");
		return (ENXIO);
	}
	if (UE_GET_DIR(ued->bEndpointAddress) != UE_DIR_IN) {
		device_printf(dev, "not an input endpoint\n");
		return (ENXIO);
	}
	if ((ued->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		device_printf(dev, "not an interrupt endpoint\n");
		return (ENXIO);
	}
	sc->sc_iep = ued->bEndpointAddress;

	/* set up tty */
	sc->sc_tty = ttyalloc();
	sc->sc_tty->t_sc = sc;
	sc->sc_tty->t_oproc = ucycom_start;
	sc->sc_tty->t_stop = ucycom_stop;
	sc->sc_tty->t_param = ucycom_param;
	sc->sc_tty->t_open = ucycom_open;
	sc->sc_tty->t_close = ucycom_close;

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "intr", CTLFLAG_RD, &sc->sc_cintr, 0,
	    "interrupt count");
	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "in", CTLFLAG_RD, &sc->sc_cin, 0,
	    "input bytes read");
	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "lost", CTLFLAG_RD, &sc->sc_clost, 0,
	    "input bytes lost");
	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "out", CTLFLAG_RD, &sc->sc_cout, 0,
	    "output bytes");

	/* create character device node */
	ttycreate(sc->sc_tty, 0, "y%r", device_get_unit(sc->sc_dev));

	return (0);
}

static int
ucycom_detach(device_t dev)
{
	struct ucycom_softc *sc;

	sc = device_get_softc(dev);

	ttyfree(sc->sc_tty);

	return (0);
}

/*****************************************************************************
 *
 * Device interface
 *
 */

static int
ucycom_open(struct tty *tp, struct cdev *cdev)
{
	struct ucycom_softc *sc = tp->t_sc;
	int error;

	/* set default configuration */
	ucycom_configure(sc, UCYCOM_DEFAULT_RATE, UCYCOM_DEFAULT_CFG);

	/* open interrupt pipe */
	error = usbd_open_pipe_intr(sc->sc_iface, sc->sc_iep, 0,
	    &sc->sc_pipe, sc, sc->sc_ibuf, sc->sc_ilen,
	    ucycom_intr, USBD_DEFAULT_INTERVAL);
	if (error != 0) {
		device_printf(sc->sc_dev, "failed to open interrupt pipe: %s\n",
		    usbd_errstr(error));
		return (ENXIO);
	}

	if (bootverbose)
		device_printf(sc->sc_dev, "%s bypass l_rint()\n",
		    (sc->sc_tty->t_state & TS_CAN_BYPASS_L_RINT) ?
		    "can" : "can't");

	/* done! */
	return (0);
}

static void
ucycom_close(struct tty *tp)
{
	struct ucycom_softc *sc = tp->t_sc;

	/* stop interrupts and close the interrupt pipe */
	usbd_abort_pipe(sc->sc_pipe);
	usbd_close_pipe(sc->sc_pipe);
	sc->sc_pipe = 0;

	return;
}

/*****************************************************************************
 *
 * TTY interface
 *
 */

static void
ucycom_start(struct tty *tty)
{
	struct ucycom_softc *sc = tty->t_sc;
	uint8_t report[sc->sc_olen];
	int error, len;

	while (sc->sc_error == 0 && sc->sc_tty->t_outq.c_cc > 0) {
		switch (sc->sc_model) {
		case MODEL_CY7C63743:
			len = q_to_b(&sc->sc_tty->t_outq,
			    report + 1, sc->sc_olen - 1);
			sc->sc_cout += len;
			report[0] = len;
			len += 1;
			break;
		case MODEL_CY7C64013:
			len = q_to_b(&sc->sc_tty->t_outq,
			    report + 2, sc->sc_olen - 2);
			sc->sc_cout += len;
			report[0] = 0;
			report[1] = len;
			len += 2;
			break;
		default:
			panic("unsupported model (driver error)");
		}

		while (len < sc->sc_olen)
			report[len++] = 0;
		error = usbd_set_report(sc->sc_iface, UHID_OUTPUT_REPORT,
		    sc->sc_oid, report, sc->sc_olen);
#if 0
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "failed to set output report: %s\n",
			    usbd_errstr(error));
			sc->sc_error = error;
		}
#endif
	}
}

static void
ucycom_stop(struct tty *tty, int flags)
{
	struct ucycom_softc *sc;

	sc = tty->t_sc;
	if (bootverbose)
		device_printf(sc->sc_dev, "%s()\n", __func__);
}

static int
ucycom_param(struct tty *tty, struct termios *t)
{
	struct ucycom_softc *sc;
	uint32_t baud;
	uint8_t cfg;
	int error;

	sc = tty->t_sc;

	if (t->c_ispeed != t->c_ospeed)
		return (EINVAL);
	baud = t->c_ispeed;

	if (t->c_cflag & CIGNORE) {
		cfg = sc->sc_cfg;
	} else {
		cfg = 0;
		switch (t->c_cflag & CSIZE) {
		case CS8:
			++cfg;
		case CS7:
			++cfg;
		case CS6:
			++cfg;
		case CS5:
			break;
		default:
			return (EINVAL);
		}
		if (t->c_cflag & CSTOPB)
			cfg |= UCYCOM_CFG_STOPB;
		if (t->c_cflag & PARENB)
			cfg |= UCYCOM_CFG_PAREN;
		if (t->c_cflag & PARODD)
			cfg |= UCYCOM_CFG_PARODD;
	}

	error = ucycom_configure(sc, baud, cfg);
	return (error);
}

/*****************************************************************************
 *
 * Hardware interface
 *
 */

static int
ucycom_configure(struct ucycom_softc *sc, uint32_t baud, uint8_t cfg)
{
	uint8_t report[sc->sc_flen];
	int error;

	switch (baud) {
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

	if (bootverbose)
		device_printf(sc->sc_dev, "%d baud, %c-%d-%d\n", baud,
		    (cfg & UCYCOM_CFG_PAREN) ?
		    ((cfg & UCYCOM_CFG_PARODD) ? 'O' : 'E') : 'N',
		    5 + (cfg & UCYCOM_CFG_DATAB),
		    (cfg & UCYCOM_CFG_STOPB) ? 2 : 1);
	report[0] = baud & 0xff;
	report[1] = (baud >> 8) & 0xff;
	report[2] = (baud >> 16) & 0xff;
	report[3] = (baud >> 24) & 0xff;
	report[4] = cfg;
	error = usbd_set_report(sc->sc_iface, UHID_FEATURE_REPORT,
	    sc->sc_fid, report, sc->sc_flen);
	if (error != 0) {
		device_printf(sc->sc_dev, "%s\n", usbd_errstr(error));
		return (EIO);
	}
	sc->sc_baud = baud;
	sc->sc_cfg = cfg;
	return (0);
}

static void
ucycom_intr(usbd_xfer_handle xfer, usbd_private_handle scp, usbd_status status)
{
	struct ucycom_softc *sc = scp;
	uint8_t *data;
	int i, len, lost;

	sc->sc_cintr++;

	switch (sc->sc_model) {
	case MODEL_CY7C63743:
		sc->sc_ist = sc->sc_ibuf[0] & ~0x07;
		len = sc->sc_ibuf[0] & 0x07;
		data = sc->sc_ibuf + 1;
		break;
	case MODEL_CY7C64013:
		sc->sc_ist = sc->sc_ibuf[0] & ~0x07;
		len = sc->sc_ibuf[1];
		data = sc->sc_ibuf + 2;
		break;
	default:
		panic("unsupported model (driver error)");
	}

	switch (status) {
	case USBD_NORMAL_COMPLETION:
		break;
	default:
		/* XXX */
		return;
	}

	if (sc->sc_tty->t_state & TS_CAN_BYPASS_L_RINT) {
		/* XXX flow control! */
		lost = b_to_q(data, len, &sc->sc_tty->t_rawq);
		sc->sc_tty->t_rawcc += len - lost;
		ttwakeup(sc->sc_tty);
	} else {
		for (i = 0, lost = len; i < len; ++i, --lost)
			if (ttyld_rint(sc->sc_tty, data[i]) != 0)
				break;
	}
	sc->sc_cin += len - lost;
	sc->sc_clost += lost;
}
