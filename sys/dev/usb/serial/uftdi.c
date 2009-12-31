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
#include "usbdevs.h"

#define	USB_DEBUG_VAR uftdi_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/serial/usb_serial.h>
#include <dev/usb/serial/uftdi_reg.h>

#if USB_DEBUG
static int uftdi_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, uftdi, CTLFLAG_RW, 0, "USB uftdi");
SYSCTL_INT(_hw_usb_uftdi, OID_AUTO, debug, CTLFLAG_RW,
    &uftdi_debug, 0, "Debug level");
#endif

#define	UFTDI_CONFIG_INDEX	0
#define	UFTDI_IFACE_INDEX	0

#define	UFTDI_IBUFSIZE 64		/* bytes, maximum number of bytes per
					 * frame */
#define	UFTDI_OBUFSIZE 64		/* bytes, cannot be increased due to
					 * do size encoding */

enum {
	UFTDI_BULK_DT_WR,
	UFTDI_BULK_DT_RD,
	UFTDI_N_TRANSFER,
};

struct uftdi_softc {
	struct ucom_super_softc sc_super_ucom;
	struct ucom_softc sc_ucom;

	struct usb_device *sc_udev;
	struct usb_xfer *sc_xfer[UFTDI_N_TRANSFER];
	device_t sc_dev;
	struct mtx sc_mtx;

	uint32_t sc_unit;
	enum uftdi_type sc_type;

	uint16_t sc_last_lcr;

	uint8_t	sc_iface_index;
	uint8_t	sc_hdrlen;
	uint8_t	sc_msr;
	uint8_t	sc_lsr;

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

static usb_callback_t uftdi_write_callback;
static usb_callback_t uftdi_read_callback;

static void	uftdi_cfg_open(struct ucom_softc *);
static void	uftdi_cfg_set_dtr(struct ucom_softc *, uint8_t);
static void	uftdi_cfg_set_rts(struct ucom_softc *, uint8_t);
static void	uftdi_cfg_set_break(struct ucom_softc *, uint8_t);
static int	uftdi_set_parm_soft(struct termios *,
		    struct uftdi_param_config *, uint8_t);
static int	uftdi_pre_param(struct ucom_softc *, struct termios *);
static void	uftdi_cfg_param(struct ucom_softc *, struct termios *);
static void	uftdi_cfg_get_status(struct ucom_softc *, uint8_t *,
		    uint8_t *);
static void	uftdi_start_read(struct ucom_softc *);
static void	uftdi_stop_read(struct ucom_softc *);
static void	uftdi_start_write(struct ucom_softc *);
static void	uftdi_stop_write(struct ucom_softc *);
static uint8_t	uftdi_8u232am_getrate(uint32_t, uint16_t *);
static void	uftdi_poll(struct ucom_softc *ucom);

static const struct usb_config uftdi_config[UFTDI_N_TRANSFER] = {

	[UFTDI_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = UFTDI_OBUFSIZE,
		.flags = {.pipe_bof = 1,},
		.callback = &uftdi_write_callback,
	},

	[UFTDI_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = UFTDI_IBUFSIZE,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = &uftdi_read_callback,
	},
};

static const struct ucom_callback uftdi_callback = {
	.ucom_cfg_get_status = &uftdi_cfg_get_status,
	.ucom_cfg_set_dtr = &uftdi_cfg_set_dtr,
	.ucom_cfg_set_rts = &uftdi_cfg_set_rts,
	.ucom_cfg_set_break = &uftdi_cfg_set_break,
	.ucom_cfg_param = &uftdi_cfg_param,
	.ucom_cfg_open = &uftdi_cfg_open,
	.ucom_pre_param = &uftdi_pre_param,
	.ucom_start_read = &uftdi_start_read,
	.ucom_stop_read = &uftdi_stop_read,
	.ucom_start_write = &uftdi_start_write,
	.ucom_stop_write = &uftdi_stop_write,
	.ucom_poll = &uftdi_poll,
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

DRIVER_MODULE(uftdi, uhub, uftdi_driver, uftdi_devclass, NULL, 0);
MODULE_DEPEND(uftdi, ucom, 1, 1, 1);
MODULE_DEPEND(uftdi, usb, 1, 1, 1);

static struct usb_device_id uftdi_devs[] = {
	{USB_VPI(USB_VENDOR_ATMEL, USB_PRODUCT_ATMEL_STK541, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_DRESDENELEKTRONIK, USB_PRODUCT_DRESDENELEKTRONIK_SENSORTERMINALBOARD, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_DRESDENELEKTRONIK, USB_PRODUCT_DRESDENELEKTRONIK_WIRELESSHANDHELDTERMINAL, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_8U100AX, UFTDI_TYPE_SIO)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_2232C, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_2232D, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_8U232AM, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_8U232AM4, UFTDI_TYPE_8U232AM)},
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
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MAXSTREAM, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_CTI_USB_NANO_485, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_FTDI, USB_PRODUCT_FTDI_CTI_USB_MINI_485, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_SIIG2, USB_PRODUCT_SIIG2_US2308, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_INTREPIDCS, USB_PRODUCT_INTREPIDCS_VALUECAN, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_INTREPIDCS, USB_PRODUCT_INTREPIDCS_NEOVI, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_BBELECTRONICS, USB_PRODUCT_BBELECTRONICS_USOTL4, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_MARVELL, USB_PRODUCT_MARVELL_SHEEVAPLUG, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_PCOPRS1, UFTDI_TYPE_8U232AM)},
	{USB_VPI(USB_VENDOR_RATOC, USB_PRODUCT_RATOC_REXUSB60F, UFTDI_TYPE_8U232AM)},
};

static int
uftdi_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != UFTDI_CONFIG_INDEX) {
		return (ENXIO);
	}
	/* attach to all present interfaces */

	return (usbd_lookup_id_by_uaa(uftdi_devs, sizeof(uftdi_devs), uaa));
}

static int
uftdi_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct uftdi_softc *sc = device_get_softc(dev);
	int error;

	sc->sc_udev = uaa->device;
	sc->sc_dev = dev;
	sc->sc_unit = device_get_unit(dev);

	device_set_usb_desc(dev);
	mtx_init(&sc->sc_mtx, "uftdi", NULL, MTX_DEF);

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

	error = usbd_transfer_setup(uaa->device,
	    &sc->sc_iface_index, sc->sc_xfer, uftdi_config,
	    UFTDI_N_TRANSFER, sc, &sc->sc_mtx);

	if (error) {
		device_printf(dev, "allocating USB "
		    "transfers failed\n");
		goto detach;
	}
	sc->sc_ucom.sc_portno = FTDI_PIT_SIOA + uaa->info.bIfaceNum;

	/* clear stall at first run */
	mtx_lock(&sc->sc_mtx);
	usbd_xfer_set_stall(sc->sc_xfer[UFTDI_BULK_DT_WR]);
	usbd_xfer_set_stall(sc->sc_xfer[UFTDI_BULK_DT_RD]);
	mtx_unlock(&sc->sc_mtx);

	/* set a valid "lcr" value */

	sc->sc_last_lcr =
	    (FTDI_SIO_SET_DATA_STOP_BITS_2 |
	    FTDI_SIO_SET_DATA_PARITY_NONE |
	    FTDI_SIO_SET_DATA_BITS(8));

	error = ucom_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &uftdi_callback, &sc->sc_mtx);
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

	ucom_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);
	usbd_transfer_unsetup(sc->sc_xfer, UFTDI_N_TRANSFER);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
uftdi_cfg_open(struct ucom_softc *ucom)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	uint16_t wIndex = ucom->sc_portno;
	struct usb_device_request req;

	DPRINTF("");

	/* perform a full reset on the device */

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_RESET;
	USETW(req.wValue, FTDI_SIO_RESET_SIO);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, 0);
	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);

	/* turn on RTS/CTS flow control */

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_FLOW_CTRL;
	USETW(req.wValue, 0);
	USETW2(req.wIndex, FTDI_SIO_RTS_CTS_HS, wIndex);
	USETW(req.wLength, 0);
	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);

	/*
	 * NOTE: with the new UCOM layer there will always be a
	 * "uftdi_cfg_param()" call after "open()", so there is no need for
	 * "open()" to configure anything
	 */
}

static void
uftdi_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uftdi_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint32_t actlen;
	uint8_t buf[1];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
tr_setup:
		pc = usbd_xfer_get_frame(xfer, 0);
		if (ucom_get_data(&sc->sc_ucom, pc,
		    sc->sc_hdrlen, UFTDI_OBUFSIZE - sc->sc_hdrlen,
		    &actlen)) {

			if (sc->sc_hdrlen > 0) {
				buf[0] =
				    FTDI_OUT_TAG(actlen, sc->sc_ucom.sc_portno);
				usbd_copy_in(pc, 0, buf, 1);
			}
			usbd_xfer_set_frame_len(xfer, 0, actlen + sc->sc_hdrlen);
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
uftdi_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uftdi_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint8_t buf[2];
	uint8_t ftdi_msr;
	uint8_t msr;
	uint8_t lsr;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		if (actlen < 2) {
			goto tr_setup;
		}
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, 0, buf, 2);

		ftdi_msr = FTDI_GET_MSR(buf);
		lsr = FTDI_GET_LSR(buf);

		msr = 0;
		if (ftdi_msr & FTDI_SIO_CTS_MASK)
			msr |= SER_CTS;
		if (ftdi_msr & FTDI_SIO_DSR_MASK)
			msr |= SER_DSR;
		if (ftdi_msr & FTDI_SIO_RI_MASK)
			msr |= SER_RI;
		if (ftdi_msr & FTDI_SIO_RLSD_MASK)
			msr |= SER_DCD;

		if ((sc->sc_msr != msr) ||
		    ((sc->sc_lsr & FTDI_LSR_MASK) != (lsr & FTDI_LSR_MASK))) {
			DPRINTF("status change msr=0x%02x (0x%02x) "
			    "lsr=0x%02x (0x%02x)\n", msr, sc->sc_msr,
			    lsr, sc->sc_lsr);

			sc->sc_msr = msr;
			sc->sc_lsr = lsr;

			ucom_status_change(&sc->sc_ucom);
		}
		actlen -= 2;

		if (actlen > 0) {
			ucom_put_data(&sc->sc_ucom, pc, 2, actlen);
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
uftdi_cfg_set_dtr(struct ucom_softc *ucom, uint8_t onoff)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	uint16_t wIndex = ucom->sc_portno;
	uint16_t wValue;
	struct usb_device_request req;

	wValue = onoff ? FTDI_SIO_SET_DTR_HIGH : FTDI_SIO_SET_DTR_LOW;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_MODEM_CTRL;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, 0);
	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);
}

static void
uftdi_cfg_set_rts(struct ucom_softc *ucom, uint8_t onoff)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	uint16_t wIndex = ucom->sc_portno;
	uint16_t wValue;
	struct usb_device_request req;

	wValue = onoff ? FTDI_SIO_SET_RTS_HIGH : FTDI_SIO_SET_RTS_LOW;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_MODEM_CTRL;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, 0);
	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);
}

static void
uftdi_cfg_set_break(struct ucom_softc *ucom, uint8_t onoff)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	uint16_t wIndex = ucom->sc_portno;
	uint16_t wValue;
	struct usb_device_request req;

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
	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);
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
uftdi_pre_param(struct ucom_softc *ucom, struct termios *t)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	struct uftdi_param_config cfg;

	DPRINTF("\n");

	return (uftdi_set_parm_soft(t, &cfg, sc->sc_type));
}

static void
uftdi_cfg_param(struct ucom_softc *ucom, struct termios *t)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	uint16_t wIndex = ucom->sc_portno;
	struct uftdi_param_config cfg;
	struct usb_device_request req;

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
	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_DATA;
	USETW(req.wValue, cfg.lcr);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, 0);
	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_FLOW_CTRL;
	USETW2(req.wValue, cfg.v_stop, cfg.v_start);
	USETW2(req.wIndex, cfg.v_flow, wIndex);
	USETW(req.wLength, 0);
	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, NULL, 0, 1000);
}

static void
uftdi_cfg_get_status(struct ucom_softc *ucom, uint8_t *lsr, uint8_t *msr)
{
	struct uftdi_softc *sc = ucom->sc_parent;

	DPRINTF("msr=0x%02x lsr=0x%02x\n",
	    sc->sc_msr, sc->sc_lsr);

	*msr = sc->sc_msr;
	*lsr = sc->sc_lsr;
}

static void
uftdi_start_read(struct ucom_softc *ucom)
{
	struct uftdi_softc *sc = ucom->sc_parent;

	usbd_transfer_start(sc->sc_xfer[UFTDI_BULK_DT_RD]);
}

static void
uftdi_stop_read(struct ucom_softc *ucom)
{
	struct uftdi_softc *sc = ucom->sc_parent;

	usbd_transfer_stop(sc->sc_xfer[UFTDI_BULK_DT_RD]);
}

static void
uftdi_start_write(struct ucom_softc *ucom)
{
	struct uftdi_softc *sc = ucom->sc_parent;

	usbd_transfer_start(sc->sc_xfer[UFTDI_BULK_DT_WR]);
}

static void
uftdi_stop_write(struct ucom_softc *ucom)
{
	struct uftdi_softc *sc = ucom->sc_parent;

	usbd_transfer_stop(sc->sc_xfer[UFTDI_BULK_DT_WR]);
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

static void
uftdi_poll(struct ucom_softc *ucom)
{
	struct uftdi_softc *sc = ucom->sc_parent;
	usbd_transfer_poll(sc->sc_xfer, UFTDI_N_TRANSFER);
}
