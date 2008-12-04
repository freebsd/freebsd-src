/* $FreeBSD$ */
/*	$OpenBSD: umoscom.c,v 1.2 2006/10/26 06:02:43 jsg Exp $	*/

/*
 * Copyright (c) 2006 Jonathan Gray <jsg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_cdc.h>

#define	USB_DEBUG_VAR umoscom_debug

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
static int umoscom_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, umoscom, CTLFLAG_RW, 0, "USB umoscom");
SYSCTL_INT(_hw_usb2_umoscom, OID_AUTO, debug, CTLFLAG_RW,
    &umoscom_debug, 0, "Debug level");
#endif

#define	UMOSCOM_BUFSIZE	       1024	/* bytes */
#define	UMOSCOM_N_DATA_TRANSFER   6	/* units */

#define	UMOSCOM_CONFIG_INDEX	0
#define	UMOSCOM_IFACE_INDEX	0

/* interrupt packet */
#define	UMOSCOM_IIR_RLS		0x06
#define	UMOSCOM_IIR_RDA		0x04
#define	UMOSCOM_IIR_CTI		0x0c
#define	UMOSCOM_IIR_THR		0x02
#define	UMOSCOM_IIR_MS		0x00

/* registers */
#define	UMOSCOM_READ		0x0d
#define	UMOSCOM_WRITE		0x0e
#define	UMOSCOM_UART_REG	0x0300
#define	UMOSCOM_VEND_REG	0x0000

#define	UMOSCOM_TXBUF		0x00	/* Write */
#define	UMOSCOM_RXBUF		0x00	/* Read */
#define	UMOSCOM_INT		0x01
#define	UMOSCOM_FIFO		0x02	/* Write */
#define	UMOSCOM_ISR		0x02	/* Read */
#define	UMOSCOM_LCR		0x03
#define	UMOSCOM_MCR		0x04
#define	UMOSCOM_LSR		0x05
#define	UMOSCOM_MSR		0x06
#define	UMOSCOM_SCRATCH		0x07
#define	UMOSCOM_DIV_LO		0x08
#define	UMOSCOM_DIV_HI		0x09
#define	UMOSCOM_EFR		0x0a
#define	UMOSCOM_XON1		0x0b
#define	UMOSCOM_XON2		0x0c
#define	UMOSCOM_XOFF1		0x0d
#define	UMOSCOM_XOFF2		0x0e

#define	UMOSCOM_BAUDLO		0x00
#define	UMOSCOM_BAUDHI		0x01

#define	UMOSCOM_INT_RXEN	0x01
#define	UMOSCOM_INT_TXEN	0x02
#define	UMOSCOM_INT_RSEN	0x04
#define	UMOSCOM_INT_MDMEM	0x08
#define	UMOSCOM_INT_SLEEP	0x10
#define	UMOSCOM_INT_XOFF	0x20
#define	UMOSCOM_INT_RTS		0x40

#define	UMOSCOM_FIFO_EN		0x01
#define	UMOSCOM_FIFO_RXCLR	0x02
#define	UMOSCOM_FIFO_TXCLR	0x04
#define	UMOSCOM_FIFO_DMA_BLK	0x08
#define	UMOSCOM_FIFO_TXLVL_MASK	0x30
#define	UMOSCOM_FIFO_TXLVL_8	0x00
#define	UMOSCOM_FIFO_TXLVL_16	0x10
#define	UMOSCOM_FIFO_TXLVL_32	0x20
#define	UMOSCOM_FIFO_TXLVL_56	0x30
#define	UMOSCOM_FIFO_RXLVL_MASK	0xc0
#define	UMOSCOM_FIFO_RXLVL_8	0x00
#define	UMOSCOM_FIFO_RXLVL_16	0x40
#define	UMOSCOM_FIFO_RXLVL_56	0x80
#define	UMOSCOM_FIFO_RXLVL_80	0xc0

#define	UMOSCOM_ISR_MDM		0x00
#define	UMOSCOM_ISR_NONE	0x01
#define	UMOSCOM_ISR_TX		0x02
#define	UMOSCOM_ISR_RX		0x04
#define	UMOSCOM_ISR_LINE	0x06
#define	UMOSCOM_ISR_RXTIMEOUT	0x0c
#define	UMOSCOM_ISR_RX_XOFF	0x10
#define	UMOSCOM_ISR_RTSCTS	0x20
#define	UMOSCOM_ISR_FIFOEN	0xc0

#define	UMOSCOM_LCR_DBITS(x)	((x) - 5)
#define	UMOSCOM_LCR_STOP_BITS_1	0x00
#define	UMOSCOM_LCR_STOP_BITS_2	0x04	/* 2 if 6-8 bits/char or 1.5 if 5 */
#define	UMOSCOM_LCR_PARITY_NONE	0x00
#define	UMOSCOM_LCR_PARITY_ODD	0x08
#define	UMOSCOM_LCR_PARITY_EVEN	0x18
#define	UMOSCOM_LCR_BREAK	0x40
#define	UMOSCOM_LCR_DIVLATCH_EN	0x80

#define	UMOSCOM_MCR_DTR		0x01
#define	UMOSCOM_MCR_RTS		0x02
#define	UMOSCOM_MCR_LOOP	0x04
#define	UMOSCOM_MCR_INTEN	0x08
#define	UMOSCOM_MCR_LOOPBACK	0x10
#define	UMOSCOM_MCR_XONANY	0x20
#define	UMOSCOM_MCR_IRDA_EN	0x40
#define	UMOSCOM_MCR_BAUD_DIV4	0x80

#define	UMOSCOM_LSR_RXDATA	0x01
#define	UMOSCOM_LSR_RXOVER	0x02
#define	UMOSCOM_LSR_RXPAR_ERR	0x04
#define	UMOSCOM_LSR_RXFRM_ERR	0x08
#define	UMOSCOM_LSR_RXBREAK	0x10
#define	UMOSCOM_LSR_TXEMPTY	0x20
#define	UMOSCOM_LSR_TXALLEMPTY	0x40
#define	UMOSCOM_LSR_TXFIFO_ERR	0x80

#define	UMOSCOM_MSR_CTS_CHG	0x01
#define	UMOSCOM_MSR_DSR_CHG	0x02
#define	UMOSCOM_MSR_RI_CHG	0x04
#define	UMOSCOM_MSR_CD_CHG	0x08
#define	UMOSCOM_MSR_CTS		0x10
#define	UMOSCOM_MSR_RTS		0x20
#define	UMOSCOM_MSR_RI		0x40
#define	UMOSCOM_MSR_CD		0x80

#define	UMOSCOM_BAUD_REF	115200

struct umoscom_softc {
	struct usb2_com_super_softc sc_super_ucom;
	struct usb2_com_softc sc_ucom;

	struct usb2_xfer *sc_xfer_data[UMOSCOM_N_DATA_TRANSFER];
	struct usb2_device *sc_udev;

	uint8_t	sc_mcr;
	uint8_t	sc_lcr;
	uint8_t	sc_flags;
#define	UMOSCOM_FLAG_READ_STALL  0x01
#define	UMOSCOM_FLAG_WRITE_STALL 0x02
#define	UMOSCOM_FLAG_INTR_STALL  0x04
};

/* prototypes */

static device_probe_t umoscom_probe;
static device_attach_t umoscom_attach;
static device_detach_t umoscom_detach;

static usb2_callback_t umoscom_write_callback;
static usb2_callback_t umoscom_write_clear_stall_callback;
static usb2_callback_t umoscom_read_callback;
static usb2_callback_t umoscom_read_clear_stall_callback;
static usb2_callback_t umoscom_intr_callback;
static usb2_callback_t umoscom_intr_clear_stall_callback;

static void umoscom_cfg_open(struct usb2_com_softc *ucom);
static void umoscom_cfg_close(struct usb2_com_softc *ucom);
static void umoscom_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff);
static void umoscom_cfg_set_dtr(struct usb2_com_softc *ucom, uint8_t onoff);
static void umoscom_cfg_set_rts(struct usb2_com_softc *ucom, uint8_t onoff);
static int umoscom_pre_param(struct usb2_com_softc *ucom, struct termios *t);
static void umoscom_cfg_param(struct usb2_com_softc *ucom, struct termios *t);
static void umoscom_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *lsr, uint8_t *msr);
static void umoscom_cfg_write(struct umoscom_softc *sc, uint16_t reg, uint16_t val);
static uint8_t umoscom_cfg_read(struct umoscom_softc *sc, uint16_t reg);
static void umoscom_cfg_do_request(struct umoscom_softc *sc, struct usb2_device_request *req, void *data);

static void umoscom_start_read(struct usb2_com_softc *ucom);
static void umoscom_stop_read(struct usb2_com_softc *ucom);
static void umoscom_start_write(struct usb2_com_softc *ucom);
static void umoscom_stop_write(struct usb2_com_softc *ucom);

static const struct usb2_config umoscom_config_data[UMOSCOM_N_DATA_TRANSFER] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = UMOSCOM_BUFSIZE,
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = &umoscom_write_callback,
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = UMOSCOM_BUFSIZE,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &umoscom_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &umoscom_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &umoscom_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[4] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = 0,	/* use wMaxPacketSize */
		.mh.callback = &umoscom_intr_callback,
	},

	[5] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &umoscom_intr_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

static const struct usb2_com_callback umoscom_callback = {
	/* configuration callbacks */
	.usb2_com_cfg_get_status = &umoscom_cfg_get_status,
	.usb2_com_cfg_set_dtr = &umoscom_cfg_set_dtr,
	.usb2_com_cfg_set_rts = &umoscom_cfg_set_rts,
	.usb2_com_cfg_set_break = &umoscom_cfg_set_break,
	.usb2_com_cfg_param = &umoscom_cfg_param,
	.usb2_com_cfg_open = &umoscom_cfg_open,
	.usb2_com_cfg_close = &umoscom_cfg_close,

	/* other callbacks */
	.usb2_com_pre_param = &umoscom_pre_param,
	.usb2_com_start_read = &umoscom_start_read,
	.usb2_com_stop_read = &umoscom_stop_read,
	.usb2_com_start_write = &umoscom_start_write,
	.usb2_com_stop_write = &umoscom_stop_write,
};

static device_method_t umoscom_methods[] = {
	DEVMETHOD(device_probe, umoscom_probe),
	DEVMETHOD(device_attach, umoscom_attach),
	DEVMETHOD(device_detach, umoscom_detach),
	{0, 0}
};

static devclass_t umoscom_devclass;

static driver_t umoscom_driver = {
	.name = "umoscom",
	.methods = umoscom_methods,
	.size = sizeof(struct umoscom_softc),
};

DRIVER_MODULE(umoscom, ushub, umoscom_driver, umoscom_devclass, NULL, 0);
MODULE_DEPEND(umoscom, usb2_serial, 1, 1, 1);
MODULE_DEPEND(umoscom, usb2_core, 1, 1, 1);

static const struct usb2_device_id umoscom_devs[] = {
	{USB_VPI(USB_VENDOR_MOSCHIP, USB_PRODUCT_MOSCHIP_MCS7703, 0)}
};

static int
umoscom_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != UMOSCOM_CONFIG_INDEX) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != UMOSCOM_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(umoscom_devs, sizeof(umoscom_devs), uaa));
}

static int
umoscom_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct umoscom_softc *sc = device_get_softc(dev);
	int error;
	uint8_t iface_index;

	if (sc == NULL) {
		return (ENOMEM);
	}
	sc->sc_udev = uaa->device;
	sc->sc_mcr = 0x08;		/* enable interrupts */

	/* XXX the device doesn't provide any ID string, so set a static one */
	device_set_desc(dev, "MOSCHIP USB Serial Port Adapter");
	device_printf(dev, "<MOSCHIP USB Serial Port Adapter>\n");

	iface_index = UMOSCOM_IFACE_INDEX;
	error = usb2_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer_data, umoscom_config_data,
	    UMOSCOM_N_DATA_TRANSFER, sc, &Giant);

	if (error) {
		goto detach;
	}
	/* clear stall at first run */
	sc->sc_flags |= (UMOSCOM_FLAG_READ_STALL |
	    UMOSCOM_FLAG_WRITE_STALL);

	error = usb2_com_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &umoscom_callback, &Giant);
	if (error) {
		goto detach;
	}
	return (0);

detach:
	device_printf(dev, "attach error: %s\n", usb2_errstr(error));
	umoscom_detach(dev);
	return (ENXIO);
}

static int
umoscom_detach(device_t dev)
{
	struct umoscom_softc *sc = device_get_softc(dev);

	mtx_lock(&Giant);

	mtx_unlock(&Giant);

	usb2_com_detach(&sc->sc_super_ucom, &sc->sc_ucom, 1);

	usb2_transfer_unsetup(sc->sc_xfer_data, UMOSCOM_N_DATA_TRANSFER);

	return (0);
}

static void
umoscom_cfg_open(struct usb2_com_softc *ucom)
{
	struct umoscom_softc *sc = ucom->sc_parent;

	DPRINTF("\n");

	/* Purge FIFOs or odd things happen */
	umoscom_cfg_write(sc, UMOSCOM_FIFO, 0x00 | UMOSCOM_UART_REG);

	/* Enable FIFO */
	umoscom_cfg_write(sc, UMOSCOM_FIFO, UMOSCOM_FIFO_EN |
	    UMOSCOM_FIFO_RXCLR | UMOSCOM_FIFO_TXCLR |
	    UMOSCOM_FIFO_DMA_BLK | UMOSCOM_FIFO_RXLVL_MASK |
	    UMOSCOM_UART_REG);

	/* Enable Interrupt Registers */
	umoscom_cfg_write(sc, UMOSCOM_INT, 0x0C | UMOSCOM_UART_REG);

	/* Magic */
	umoscom_cfg_write(sc, 0x01, 0x08);

	/* Magic */
	umoscom_cfg_write(sc, 0x00, 0x02);

	return;
}

static void
umoscom_cfg_close(struct usb2_com_softc *ucom)
{
	return;
}

static void
umoscom_cfg_set_break(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct umoscom_softc *sc = ucom->sc_parent;
	uint16_t val;

	val = sc->sc_lcr;
	if (onoff)
		val |= UMOSCOM_LCR_BREAK;

	umoscom_cfg_write(sc, UMOSCOM_LCR, val | UMOSCOM_UART_REG);
	return;
}

static void
umoscom_cfg_set_dtr(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct umoscom_softc *sc = ucom->sc_parent;

	if (onoff)
		sc->sc_mcr |= UMOSCOM_MCR_DTR;
	else
		sc->sc_mcr &= ~UMOSCOM_MCR_DTR;

	umoscom_cfg_write(sc, UMOSCOM_MCR, sc->sc_mcr | UMOSCOM_UART_REG);
	return;
}

static void
umoscom_cfg_set_rts(struct usb2_com_softc *ucom, uint8_t onoff)
{
	struct umoscom_softc *sc = ucom->sc_parent;

	if (onoff)
		sc->sc_mcr |= UMOSCOM_MCR_RTS;
	else
		sc->sc_mcr &= ~UMOSCOM_MCR_RTS;

	umoscom_cfg_write(sc, UMOSCOM_MCR, sc->sc_mcr | UMOSCOM_UART_REG);
	return;
}

static int
umoscom_pre_param(struct usb2_com_softc *ucom, struct termios *t)
{
	if ((t->c_ospeed <= 1) || (t->c_ospeed > 115200))
		return (EINVAL);

	return (0);
}

static void
umoscom_cfg_param(struct usb2_com_softc *ucom, struct termios *t)
{
	struct umoscom_softc *sc = ucom->sc_parent;
	uint16_t data;

	DPRINTF("speed=%d\n", t->c_ospeed);

	data = ((uint32_t)UMOSCOM_BAUD_REF) / ((uint32_t)t->c_ospeed);

	if (data == 0) {
		DPRINTF("invalid baud rate!\n");
		return;
	}
	umoscom_cfg_write(sc, UMOSCOM_LCR,
	    UMOSCOM_LCR_DIVLATCH_EN | UMOSCOM_UART_REG);

	umoscom_cfg_write(sc, UMOSCOM_BAUDLO,
	    (data & 0xFF) | UMOSCOM_UART_REG);

	umoscom_cfg_write(sc, UMOSCOM_BAUDHI,
	    ((data >> 8) & 0xFF) | UMOSCOM_UART_REG);

	if (t->c_cflag & CSTOPB)
		data = UMOSCOM_LCR_STOP_BITS_2;
	else
		data = UMOSCOM_LCR_STOP_BITS_1;

	if (t->c_cflag & PARENB) {
		if (t->c_cflag & PARODD)
			data |= UMOSCOM_LCR_PARITY_ODD;
		else
			data |= UMOSCOM_LCR_PARITY_EVEN;
	} else
		data |= UMOSCOM_LCR_PARITY_NONE;

	switch (t->c_cflag & CSIZE) {
	case CS5:
		data |= UMOSCOM_LCR_DBITS(5);
		break;
	case CS6:
		data |= UMOSCOM_LCR_DBITS(6);
		break;
	case CS7:
		data |= UMOSCOM_LCR_DBITS(7);
		break;
	case CS8:
		data |= UMOSCOM_LCR_DBITS(8);
		break;
	}

	sc->sc_lcr = data;
	umoscom_cfg_write(sc, UMOSCOM_LCR, data | UMOSCOM_UART_REG);

	return;
}

static void
umoscom_cfg_get_status(struct usb2_com_softc *ucom, uint8_t *p_lsr, uint8_t *p_msr)
{
	struct umoscom_softc *sc = ucom->sc_parent;
	uint8_t lsr;
	uint8_t msr;

	DPRINTFN(5, "\n");

	/* read status registers */

	lsr = umoscom_cfg_read(sc, UMOSCOM_LSR);
	msr = umoscom_cfg_read(sc, UMOSCOM_MSR);

	/* translate bits */

	if (msr & UMOSCOM_MSR_CTS)
		*p_msr |= SER_CTS;

	if (msr & UMOSCOM_MSR_CD)
		*p_msr |= SER_DCD;

	if (msr & UMOSCOM_MSR_RI)
		*p_msr |= SER_RI;

	if (msr & UMOSCOM_MSR_RTS)
		*p_msr |= SER_DSR;

	return;
}

static void
umoscom_cfg_write(struct umoscom_softc *sc, uint16_t reg, uint16_t val)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UMOSCOM_WRITE;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	umoscom_cfg_do_request(sc, &req, NULL);
}

static uint8_t
umoscom_cfg_read(struct umoscom_softc *sc, uint16_t reg)
{
	struct usb2_device_request req;
	uint8_t val;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UMOSCOM_READ;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	umoscom_cfg_do_request(sc, &req, &val);

	DPRINTF("reg=0x%04x, val=0x%02x\n", reg, val);

	return (val);
}

static void
umoscom_cfg_do_request(struct umoscom_softc *sc, struct usb2_device_request *req,
    void *data)
{
	uint16_t length;
	usb2_error_t err;

	if (usb2_com_cfg_is_gone(&sc->sc_ucom))
		goto error;

	err = usb2_do_request_flags
	    (sc->sc_udev, &Giant, req, data, 0, NULL, 1000);

	if (err) {
		DPRINTFN(0, "control request failed: %s\n",
		    usb2_errstr(err));
error:
		length = UGETW(req->wLength);

		if ((req->bmRequestType & UT_READ) && length) {
			bzero(data, length);
		}
	}
	return;
}

static void
umoscom_start_read(struct usb2_com_softc *ucom)
{
	struct umoscom_softc *sc = ucom->sc_parent;

#if 0
	/* start interrupt endpoint */
	usb2_transfer_start(sc->sc_xfer_data[4]);
#endif
	/* start read endpoint */
	usb2_transfer_start(sc->sc_xfer_data[1]);
	return;
}

static void
umoscom_stop_read(struct usb2_com_softc *ucom)
{
	struct umoscom_softc *sc = ucom->sc_parent;

	/* stop interrupt transfer */
	usb2_transfer_stop(sc->sc_xfer_data[5]);
	usb2_transfer_stop(sc->sc_xfer_data[4]);

	/* stop read endpoint */
	usb2_transfer_stop(sc->sc_xfer_data[3]);
	usb2_transfer_stop(sc->sc_xfer_data[1]);
	return;
}

static void
umoscom_start_write(struct usb2_com_softc *ucom)
{
	struct umoscom_softc *sc = ucom->sc_parent;

	usb2_transfer_start(sc->sc_xfer_data[0]);
	return;
}

static void
umoscom_stop_write(struct usb2_com_softc *ucom)
{
	struct umoscom_softc *sc = ucom->sc_parent;

	usb2_transfer_stop(sc->sc_xfer_data[2]);
	usb2_transfer_stop(sc->sc_xfer_data[0]);
	return;
}

static void
umoscom_write_callback(struct usb2_xfer *xfer)
{
	struct umoscom_softc *sc = xfer->priv_sc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
		DPRINTF("\n");

		if (sc->sc_flags & UMOSCOM_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer_data[2]);
			return;
		}
		if (usb2_com_get_data(&sc->sc_ucom, xfer->frbuffers, 0,
		    UMOSCOM_BUFSIZE, &actlen)) {

			xfer->frlengths[0] = actlen;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			DPRINTFN(0, "transfer failed\n");
			sc->sc_flags |= UMOSCOM_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer_data[2]);
		}
		return;
	}
}

static void
umoscom_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct umoscom_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer_data[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UMOSCOM_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
umoscom_read_callback(struct usb2_xfer *xfer)
{
	struct umoscom_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTF("got %d bytes\n", xfer->actlen);
		usb2_com_put_data(&sc->sc_ucom, xfer->frbuffers, 0, xfer->actlen);

	case USB_ST_SETUP:
		DPRINTF("\n");

		if (sc->sc_flags & UMOSCOM_FLAG_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer_data[3]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			DPRINTFN(0, "transfer failed\n");
			sc->sc_flags |= UMOSCOM_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer_data[3]);
		}
		return;

	}
}

static void
umoscom_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct umoscom_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer_data[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UMOSCOM_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
umoscom_intr_callback(struct usb2_xfer *xfer)
{
	struct umoscom_softc *sc = xfer->priv_sc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (xfer->actlen < 2) {
			DPRINTF("too short message\n");
			goto tr_setup;
		}
		usb2_com_status_change(&sc->sc_ucom);

	case USB_ST_SETUP:
tr_setup:
		if (sc->sc_flags & UMOSCOM_FLAG_INTR_STALL) {
			usb2_transfer_start(sc->sc_xfer_data[5]);
		} else {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			DPRINTFN(0, "transfer failed\n");
			sc->sc_flags |= UMOSCOM_FLAG_INTR_STALL;
			usb2_transfer_start(sc->sc_xfer_data[5]);
		}
		return;
	}
}

static void
umoscom_intr_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct umoscom_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer_data[4];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UMOSCOM_FLAG_INTR_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}
