/*-
 * Copyright (c) 2002, Alexander Kabaev <kan.FreeBSD.org>.
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

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_cdc.h>

#define	USB_DEBUG_VAR ubsa_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_busdma.h>

#include <dev/usb2/serial/usb2_serial.h>

#if USB_DEBUG
static int ubsa_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, ubsa, CTLFLAG_RW, 0, "USB ubsa");
SYSCTL_INT(_hw_usb2_ubsa, OID_AUTO, debug, CTLFLAG_RW,
    &ubsa_debug, 0, "ubsa debug level");
#endif

#define	UBSA_N_TRANSFER           6	/* units */
#define	UBSA_BSIZE             1024	/* bytes */

#define	UBSA_CONFIG_INDEX	1
#define	UBSA_IFACE_INDEX	0

#define	UBSA_REG_BAUDRATE	0x00
#define	UBSA_REG_STOP_BITS	0x01
#define	UBSA_REG_DATA_BITS	0x02
#define	UBSA_REG_PARITY		0x03
#define	UBSA_REG_DTR		0x0A
#define	UBSA_REG_RTS		0x0B
#define	UBSA_REG_BREAK		0x0C
#define	UBSA_REG_FLOW_CTRL	0x10

#define	UBSA_PARITY_NONE	0x00
#define	UBSA_PARITY_EVEN	0x01
#define	UBSA_PARITY_ODD		0x02
#define	UBSA_PARITY_MARK	0x03
#define	UBSA_PARITY_SPACE	0x04

#define	UBSA_FLOW_NONE		0x0000
#define	UBSA_FLOW_OCTS		0x0001
#define	UBSA_FLOW_ODSR		0x0002
#define	UBSA_FLOW_IDSR		0x0004
#define	UBSA_FLOW_IDTR		0x0008
#define	UBSA_FLOW_IRTS		0x0010
#define	UBSA_FLOW_ORTS		0x0020
#define	UBSA_FLOW_UNKNOWN	0x0040
#define	UBSA_FLOW_OXON		0x0080
#define	UBSA_FLOW_IXON		0x0100

/* line status register */
#define	UBSA_LSR_TSRE		0x40	/* Transmitter empty: byte sent */
#define	UBSA_LSR_TXRDY		0x20	/* Transmitter buffer empty */
#define	UBSA_LSR_BI		0x10	/* Break detected */
#define	UBSA_LSR_FE		0x08	/* Framing error: bad stop bit */
#define	UBSA_LSR_PE		0x04	/* Parity error */
#define	UBSA_LSR_OE		0x02	/* Overrun, lost incoming byte */
#define	UBSA_LSR_RXRDY		0x01	/* Byte ready in Receive Buffer */
#define	UBSA_LSR_RCV_MASK	0x1f	/* Mask for incoming data or error */

/* modem status register */
/* All deltas are from the last read of the MSR. */
#define	UBSA_MSR_DCD		0x80	/* Current Data Carrier Detect */
#define	UBSA_MSR_RI		0x40	/* Current Ring Indicator */
#define	UBSA_MSR_DSR		0x20	/* Current Data Set Ready */
#define	UBSA_MSR_CTS		0x10	/* Current Clear to Send */
#define	UBSA_MSR_DDCD		0x08	/* DCD has changed state */
#define	UBSA_MSR_TERI		0x04	/* RI has toggled low to high */
#define	UBSA_MSR_DDSR		0x02	/* DSR has changed state */
#define	UBSA_MSR_DCTS		0x01	/* CTS has changed state */

struct ubsa_softc {
	struct usb2_com_super_softc sc_super_ucom;
	struct usb2_com_softc sc_ucom;

	struct usb2_xfer *sc_xfer[UBSA_N_TRANSFER];
	struct usb2_device *sc_udev;

	uint16_t sc_flag;
#define	UBSA_FLAG_WRITE_STALL   0x0001
#define	UBSA_FLAG_READ_STALL    0x0002
#define	UBSA_FLAG_INTR_STALL    0x0004

	uint8_t	sc_iface_no;		/* interface number */
	uint8_t	sc_iface_index;		/* interface index */
	uint8_t	sc_lsr;			/* local status register */
	uint8_t	sc_msr;			/* UBSA status register */
};

static device_probe_t ubsa_probe;
static device_attach_t ubsa_attach;
static device_detach_t ubsa_detach;

static usb2_callback_t ubsa_write_callback;
static usb2_callback_t ubsa_write_clear_stall_callback;
static usb2_callback_t ubsa_read_callback;
static usb2_callback_t ubsa_read_clear_stall_callback;
static usb2_callback_t ubsa_intr_callback;
static usb2_callback_t ubsa_intr_clear_stall_callback;

static void ubsa_cfg_request(struct ubsa_softc *sc, uint8_t index, uint16_t value);
static void ubsa_cfg_set_dtr(struct usb2_com_softc *ucom, uint8_t onoff);
static void ubsa_cfg_set_rts(struct usb2_com_softc *ucom, uint8_t onoff);
static void ubsa_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff);
static int ubsa_pre_param(struct usb2_com_softc *ucom, struct termios *t);
static void ubsa_cfg_param(struct usb2_com_softc *ucom, struct termios *t);
static void ubsa_start_read(struct usb2_com_softc *ucom);
static void ubsa_stop_read(struct usb2_com_softc *ucom);
static void ubsa_start_write(struct usb2_com_softc *ucom);
static void ubsa_stop_write(struct usb2_com_softc *ucom);
static void ubsa_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *lsr, uint8_t *msr);

static const struct usb2_config ubsa_config[UBSA_N_TRANSFER] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = UBSA_BSIZE,	/* bytes */
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &ubsa_write_callback,
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = UBSA_BSIZE,	/* bytes */
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &ubsa_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &ubsa_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &ubsa_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[4] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = 0,	/* use wMaxPacketSize */
		.mh.callback = &ubsa_intr_callback,
	},

	[5] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &ubsa_intr_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static const struct usb2_com_callback ubsa_callback = {
	.usb2_com_cfg_get_status = &ubsa_cfg_get_status,
	.usb2_com_cfg_set_dtr = &ubsa_cfg_set_dtr,
	.usb2_com_cfg_set_rts = &ubsa_cfg_set_rts,
	.usb2_com_cfg_set_break = &ubsa_cfg_set_break,
	.usb2_com_cfg_param = &ubsa_cfg_param,
	.usb2_com_pre_param = &ubsa_pre_param,
	.usb2_com_start_read = &ubsa_start_read,
	.usb2_com_stop_read = &ubsa_stop_read,
	.usb2_com_start_write = &ubsa_start_write,
	.usb2_com_stop_write = &ubsa_stop_write,
};

static const struct usb2_device_id ubsa_devs[] = {
	/* AnyData ADU-500A */
	{USB_VPI(USB_VENDOR_ANYDATA, USB_PRODUCT_ANYDATA_ADU_500A, 0)},
	/* AnyData ADU-E100A/H */
	{USB_VPI(USB_VENDOR_ANYDATA, USB_PRODUCT_ANYDATA_ADU_E100X, 0)},
	/* Axesstel MV100H */
	{USB_VPI(USB_VENDOR_AXESSTEL, USB_PRODUCT_AXESSTEL_DATAMODEM, 0)},
	/* BELKIN F5U103 */
	{USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5U103, 0)},
	/* BELKIN F5U120 */
	{USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5U120, 0)},
	/* GoHubs GO-COM232 */
	{USB_VPI(USB_VENDOR_ETEK, USB_PRODUCT_ETEK_1COM, 0)},
	/* GoHubs GO-COM232 */
	{USB_VPI(USB_VENDOR_GOHUBS, USB_PRODUCT_GOHUBS_GOCOM232, 0)},
	/* Peracom */
	{USB_VPI(USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_SERIAL1, 0)},
};

static device_method_t ubsa_methods[] = {
	DEVMETHOD(device_probe, ubsa_probe),
	DEVMETHOD(device_attach, ubsa_attach),
	DEVMETHOD(device_detach, ubsa_detach),
	{0, 0}
};

static devclass_t ubsa_devclass;

static driver_t ubsa_driver = {
	.name = "ubsa",
	.methods = ubsa_methods,
	.size = sizeof(struct ubsa_softc),
};

DRIVER_MODULE(ubsa, ushub, ubsa_driver, ubsa_devclass, NULL, 0);
MODULE_DEPEND(ubsa, usb2_serial, 1, 1, 1);
MODULE_DEPEND(ubsa, usb2_core, 1, 1, 1);

static int
ubsa_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != UBSA_CONFIG_INDEX) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != UBSA_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(ubsa_devs, sizeof(ubsa_devs), uaa));
}

static int
ubsa_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct ubsa_softc *sc = device_get_softc(dev);
	int error;

	DPRINTF("sc=%p\n", sc);

	if (sc == NULL) {
		return (ENOMEM);
	}
	device_set_usb2_desc(dev);

	sc->sc_udev = uaa->device;
	sc->sc_iface_no = uaa->info.bIfaceNum;
	sc->sc_iface_index = UBSA_IFACE_INDEX;

	error = usb2_transfer_setup(uaa->device, &sc->sc_iface_index,
	    sc->sc_xfer, ubsa_config, UBSA_N_TRANSFER, sc, &Giant);

	if (error) {
		DPRINTF("could not allocate all pipes\n");
		goto detach;
	}
	/* clear stall at first run */
	sc->sc_flag |= (UBSA_FLAG_WRITE_STALL |
	    UBSA_FLAG_READ_STALL);

	error = usb2_com_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &ubsa_callback, &Giant);
	if (error) {
		DPRINTF("usb2_com_attach failed\n");
		goto detach;
	}
	return (0);

detach:
	ubsa_detach(dev);
	return (ENXIO);
}

static int
ubsa_detach(device_t dev)
{
	struct ubsa_softc *sc = device_get_softc(dev);

	DPRINTF("sc=%p\n", sc);

	usb2_com_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);

	usb2_transfer_unsetup(sc->sc_xfer, UBSA_N_TRANSFER);

	return (0);
}

static void
ubsa_cfg_request(struct ubsa_softc *sc, uint8_t index, uint16_t value)
{
	struct usb2_device_request req;
	usb2_error_t err;

	if (usb2_com_cfg_is_gone(&sc->sc_ucom)) {
		return;
	}
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = index;
	USETW(req.wValue, value);
	req.wIndex[0] = sc->sc_iface_no;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);

	err = usb2_do_request_flags
	    (sc->sc_udev, &Giant, &req, NULL, 0, NULL, 1000);

	if (err) {
		DPRINTFN(0, "device request failed, err=%s "
		    "(ignored)\n", usb2_errstr(err));
	}
	return;
}

static void
ubsa_cfg_set_dtr(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct ubsa_softc *sc = ucom->sc_parent;

	DPRINTF("onoff = %d\n", onoff);

	ubsa_cfg_request(sc, UBSA_REG_DTR, onoff ? 1 : 0);
	return;
}

static void
ubsa_cfg_set_rts(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct ubsa_softc *sc = ucom->sc_parent;

	DPRINTF("onoff = %d\n", onoff);

	ubsa_cfg_request(sc, UBSA_REG_RTS, onoff ? 1 : 0);
	return;
}

static void
ubsa_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct ubsa_softc *sc = ucom->sc_parent;

	DPRINTF("onoff = %d\n", onoff);

	ubsa_cfg_request(sc, UBSA_REG_BREAK, onoff ? 1 : 0);
	return;
}

static int
ubsa_pre_param(struct usb2_com_softc *ucom, struct termios *t)
{
	struct ubsa_softc *sc = ucom->sc_parent;

	DPRINTF("sc = %p\n", sc);

	switch (t->c_ospeed) {
	case B0:
	case B300:
	case B600:
	case B1200:
	case B2400:
	case B4800:
	case B9600:
	case B19200:
	case B38400:
	case B57600:
	case B115200:
	case B230400:
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static void
ubsa_cfg_param(struct usb2_com_softc *ucom, struct termios *t)
{
	struct ubsa_softc *sc = ucom->sc_parent;
	uint16_t value = 0;

	DPRINTF("sc = %p\n", sc);

	switch (t->c_ospeed) {
	case B0:
		ubsa_cfg_request(sc, UBSA_REG_FLOW_CTRL, 0);
		ubsa_cfg_set_dtr(&sc->sc_ucom, 0);
		ubsa_cfg_set_rts(&sc->sc_ucom, 0);
		break;
	case B300:
	case B600:
	case B1200:
	case B2400:
	case B4800:
	case B9600:
	case B19200:
	case B38400:
	case B57600:
	case B115200:
	case B230400:
		value = B230400 / t->c_ospeed;
		ubsa_cfg_request(sc, UBSA_REG_BAUDRATE, value);
		break;
	default:
		return;
	}

	if (t->c_cflag & PARENB)
		value = (t->c_cflag & PARODD) ? UBSA_PARITY_ODD : UBSA_PARITY_EVEN;
	else
		value = UBSA_PARITY_NONE;

	ubsa_cfg_request(sc, UBSA_REG_PARITY, value);

	switch (t->c_cflag & CSIZE) {
	case CS5:
		value = 0;
		break;
	case CS6:
		value = 1;
		break;
	case CS7:
		value = 2;
		break;
	default:
	case CS8:
		value = 3;
		break;
	}

	ubsa_cfg_request(sc, UBSA_REG_DATA_BITS, value);

	value = (t->c_cflag & CSTOPB) ? 1 : 0;

	ubsa_cfg_request(sc, UBSA_REG_STOP_BITS, value);

	value = 0;
	if (t->c_cflag & CRTSCTS)
		value |= UBSA_FLOW_OCTS | UBSA_FLOW_IRTS;

	if (t->c_iflag & (IXON | IXOFF))
		value |= UBSA_FLOW_OXON | UBSA_FLOW_IXON;

	ubsa_cfg_request(sc, UBSA_REG_FLOW_CTRL, value);
	return;
}

static void
ubsa_start_read(struct usb2_com_softc *ucom)
{
	struct ubsa_softc *sc = ucom->sc_parent;

	/* start interrupt endpoint */
	usb2_transfer_start(sc->sc_xfer[4]);

	/* start read endpoint */
	usb2_transfer_start(sc->sc_xfer[1]);
	return;
}

static void
ubsa_stop_read(struct usb2_com_softc *ucom)
{
	struct ubsa_softc *sc = ucom->sc_parent;

	/* stop interrupt endpoint */
	usb2_transfer_stop(sc->sc_xfer[5]);
	usb2_transfer_stop(sc->sc_xfer[4]);

	/* stop read endpoint */
	usb2_transfer_stop(sc->sc_xfer[3]);
	usb2_transfer_stop(sc->sc_xfer[1]);
	return;
}

static void
ubsa_start_write(struct usb2_com_softc *ucom)
{
	struct ubsa_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer[0]);
	return;
}

static void
ubsa_stop_write(struct usb2_com_softc *ucom)
{
	struct ubsa_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer[2]);
	usb2_transfer_stop(sc->sc_xfer[0]);
	return;
}

static void
ubsa_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *lsr, uint8_t *msr)
{
	struct ubsa_softc *sc = ucom->sc_parent;

	DPRINTF("\n");

	*lsr = sc->sc_lsr;
	*msr = sc->sc_msr;
	return;
}

static void
ubsa_write_callback(struct usb2_xfer *xfer)
{
	struct ubsa_softc *sc = xfer->priv_sc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
		if (sc->sc_flag & UBSA_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer[2]);
			return;
		}
		if (usb2_com_get_data(&sc->sc_ucom, xfer->frbuffers, 0,
		    UBSA_BSIZE, &actlen)) {

			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flag |= UBSA_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer[2]);
		}
		return;

	}
}

static void
ubsa_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ubsa_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flag &= ~UBSA_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ubsa_read_callback(struct usb2_xfer *xfer)
{
	struct ubsa_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usb2_com_put_data(&sc->sc_ucom, xfer->frbuffers, 0, xfer->actlen);

	case USB_ST_SETUP:
		if (sc->sc_flag & UBSA_FLAG_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer[3]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flag |= UBSA_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer[3]);
		}
		return;

	}
}

static void
ubsa_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ubsa_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flag &= ~UBSA_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ubsa_intr_callback(struct usb2_xfer *xfer)
{
	struct ubsa_softc *sc = xfer->priv_sc;
	uint8_t buf[4];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		if (xfer->actlen >= sizeof(buf)) {

			usb2_copy_out(xfer->frbuffers, 0, buf, sizeof(buf));

			/*
			 * incidentally, Belkin adapter status bits match
			 * UART 16550 bits
			 */
			sc->sc_lsr = buf[2];
			sc->sc_msr = buf[3];

			DPRINTF("lsr = 0x%02x, msr = 0x%02x\n",
			    sc->sc_lsr, sc->sc_msr);

			usb2_com_status_change(&sc->sc_ucom);
		} else {
			DPRINTF("ignoring short packet, %d bytes\n",
			    xfer->actlen);
		}

	case USB_ST_SETUP:
		if (sc->sc_flag & UBSA_FLAG_INTR_STALL) {
			usb2_transfer_start(sc->sc_xfer[5]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			sc->sc_flag |= UBSA_FLAG_INTR_STALL;
			usb2_transfer_start(sc->sc_xfer[5]);
		}
		return;

	}
}

static void
ubsa_intr_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ubsa_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer[4];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flag &= ~UBSA_FLAG_INTR_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}
