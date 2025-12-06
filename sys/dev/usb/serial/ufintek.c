/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Diane Bruce
 * All rights reserved
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
 * Device driver for Fintek F81232 USB to UART bridge.
 */

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
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

#define	USB_DEBUG_VAR ufintek_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/serial/usb_serial.h>

#ifdef USB_DEBUG
static int ufintek_debug = 0;

static SYSCTL_NODE(_hw_usb, OID_AUTO, ufintek, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "USB ufintek");
SYSCTL_INT(_hw_usb_ufintek, OID_AUTO, debug, CTLFLAG_RWTUN,
    &ufintek_debug, 1, "Debug level");
#endif

#define	UFINTEK_BUFSIZE		256
#define	UFINTEK_CONFIG_INDEX	0

#define UFINTEK_REGISTER_REQUEST	0xA0	/* uint8_t request */
#define UFINTEK_READ			0xC0	/* uint8_t requesttype */
#define UFINTEK_WRITE			0x40	/* uint8_t requesttype */

#define	UFINTEK_IFACE_INDEX  	0

#define UFINTEK_UART_REG	0x120

/*
 * It appears the FINTEK serial port looks much like a 16550
 */

#define UFINTEK_RXBUF		0x00		/* Receiver Buffer Read only */ 
#define UFINTEK_TXBUF		0x00		/* Transmitter Holding Register Write only*/
#define UFINTEK_IER		0x01		/* Interrupt Enable Read/Write */
#define UFINTEK_IIR		0x02		/* Interrupt Identification Read only */
#define UFINTEK_FCR		0x02		/* FIFO control Write only */
#define UFINTEK_LCR		0x03		/* Line Control Register Read/Write */
#define UFINTEK_MCR		0x04		/* Modem Control Write only */
#define	UFINTEK_LSR		0x05		/* Line Status Read only */
#define	UFINTEK_MSR		0x06		/* Modem Status Read only */

#define	UFINTEK_BAUDLO		0x00	/* Same as UFINTEK_TXBUF and UFINTEK_RXBUF */
#define	UFINTEK_BAUDHI		0x01	/* Same as UFINTEK_IER */

/* From uart16550.readthedocs.io */
#define	UFINTEK_IER_RXEN	0x01	/* Received data available interrupt */
#define	UFINTEK_IER_TXEN	0x02
#define	UFINTEK_IER_RSEN	0x04	/* Receiver line status interrupt */
#define	UFINTEK_IER_MSI		0x08
#define	UFINTEK_IER_SLEEP	0x10
#define	UFINTEK_IER_XOFF	0x20
#define	UFINTEK_IER_RTS		0x40

#define	UFINTEK_FCR_EN		0x01
#define	UFINTEK_FCR_RXCLR	0x02
#define	UFINTEK_FCR_TXCLR	0x04
#define	UFINTEK_FCR_DMA_BLK	0x08
#define	UFINTEK_FCR_TXLVL_MASK	0x30
#define	UFINTEK_FCR_TRIGGER_8	0x80

#define	UFINTEK_ISR_NONE	0x01
#define	UFINTEK_ISR_TX		0x02
#define	UFINTEK_ISR_RX		0x04
#define	UFINTEK_ISR_LINE	0x06
#define	UFINTEK_ISR_MDM		0x08
#define	UFINTEK_ISR_RXTIMEOUT	0x0C
#define	UFINTEK_ISR_RX_XOFF	0x10
#define	UFINTEK_ISR_RTSCTS	0x20
#define	UFINTEK_ISR_FIFOEN	0xC0

#define UFINTEK_LCR_WORDLEN_5	0x00
#define UFINTEK_LCR_WORDLEN_6	0x01
#define UFINTEK_LCR_WORDLEN_7	0x02
#define	UFINTEK_LCR_WORDLEN_8	0x03

/* As with ns16550 if word length is 5 then 2 stops is 1.5 stop bits.
 * This mimics the ancient TTY15/19 (Murray code/Baudot) printers that
 * needed an extra half stop bit to stop the mechanical clutch!
*/
#define	UFINTEK_LCR_STOP_BITS_1	0x00
#define	UFINTEK_LCR_STOP_BITS_2	0x04

#define	UFINTEK_LCR_PARITY_NONE	0x00
#define	UFINTEK_LCR_PARITY_ODD	0x08
#define	UFINTEK_LCR_PARITY_EVEN	0x18
#define	UFINTEK_LCR_BREAK	0x40
#define	UFINTEK_LCR_DLAB	0x80

#define	UFINTEK_MCR_DTR		0x01
#define	UFINTEK_MCR_RTS		0x02
#define	UFINTEK_MCR_LOOP	0x04
#define	UFINTEK_MCR_INTEN	0x08
#define	UFINTEK_MCR_LOOPBACK	0x10
#define	UFINTEK_MCR_XONANY	0x20
#define	UFINTEK_MCR_IRDA_EN	0x40
#define	UFINTEK_MCR_BAUD_DIV4	0x80

#define	UFINTEK_LSR_RXDATA	0x01
#define	UFINTEK_LSR_RXOVER	0x02
#define	UFINTEK_LSR_RXPAR_ERR	0x04
#define	UFINTEK_LSR_RXFRM_ERR	0x08
#define	UFINTEK_LSR_RXBREAK	0x10
#define	UFINTEK_LSR_TXEMPTY	0x20
#define	UFINTEK_LSR_TXALLEMPTY	0x40
#define	UFINTEK_LSR_TXFIFO_ERR	0x80

#define	UFINTEK_MSR_CTS_CHG	0x01
#define	UFINTEK_MSR_DSR_CHG	0x02
#define	UFINTEK_MSR_RI_CHG	0x04
#define	UFINTEK_MSR_CD_CHG	0x08
#define	UFINTEK_MSR_CTS		0x10
#define	UFINTEK_MSR_RTS		0x20
#define	UFINTEK_MSR_RI		0x40
#define	UFINTEK_MSR_CD		0x80

#define UFINTEK_BAUD_REF	115200
#define UFINTEK_DEF_SPEED	115200

/*
 * XXX Future use
 * Baud rate multiplier clock
 * On other similar FINTEK chips the divider clock can be modified
 * via another port. This will be easy to add later.
 *
 * 0x00	1.846MHz
 * 0x01	18.46MHz
 * 0x10	24.00MHz
 * 0x11	14.77MHz
 */
#define	UFINTEK_CLK_REG		0x106
#define	UFINTEK_CLK_MSK		0x3
#define	UFINTEK_CLK_1_846	0x00
#define	UFINTEK_CLK_18_46	0x01
#define UFINTEK_CLK_24_00	0x10
#define	UFINTEK_CLK_14_77	0x11

enum {
	UFINTEK_BULK_DT_WR,
	UFINTEK_BULK_DT_RD,
	UFINTEK_N_TRANSFER,
};

struct ufintek_softc {
	struct ucom_super_softc sc_super_ucom;
	struct ucom_softc sc_ucom;
	
	struct usb_xfer *sc_xfer[UFINTEK_N_TRANSFER];
	struct usb_device *sc_udev;
	struct mtx sc_mtx;
	
	uint32_t	sc_model;
	uint8_t	sc_lcr;
	uint8_t	sc_mcr;
};

/* prototypes */
static device_probe_t ufintek_probe;
static device_attach_t ufintek_attach;
static device_detach_t ufintek_detach;
static void ufintek_free_softc(struct ufintek_softc *);

static usb_callback_t ufintek_write_callback;
static usb_callback_t ufintek_read_callback;
static void	ufintek_free(struct ucom_softc *);
static void	ufintek_cfg_open(struct ucom_softc *);
static void	ufintek_cfg_close(struct ucom_softc *);
static void	ufintek_cfg_set_dtr(struct ucom_softc *, uint8_t);
static void	ufintek_cfg_set_rts(struct ucom_softc *, uint8_t);
static void	ufintek_cfg_set_break(struct ucom_softc *, uint8_t);
static void	ufintek_cfg_param(struct ucom_softc *, struct termios *);
static void	ufintek_cfg_get_status(struct ucom_softc *, uint8_t *, uint8_t *);
static int	ufintek_pre_param(struct ucom_softc *, struct termios *);
static void	ufintek_cfg_write(struct ufintek_softc *, uint16_t, uint8_t);
static uint8_t	ufintek_cfg_read(struct ufintek_softc *, uint16_t);
static void	ufintek_start_read(struct ucom_softc *);
static void	ufintek_stop_read(struct ucom_softc *);
static void	ufintek_start_write(struct ucom_softc *);
static void	ufintek_stop_write(struct ucom_softc *);
static void	ufintek_poll(struct ucom_softc *ucom);

static const struct usb_config ufintek_config[UFINTEK_N_TRANSFER] = {
	[UFINTEK_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = UFINTEK_BUFSIZE,
		.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.callback = &ufintek_write_callback,
	},
	[UFINTEK_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = UFINTEK_BUFSIZE,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = &ufintek_read_callback,
	},
};

static const struct ucom_callback ufintek_callback = {
	/* configuration callbacks */
  	.ucom_cfg_get_status = &ufintek_cfg_get_status,
  	.ucom_cfg_set_dtr = &ufintek_cfg_set_dtr,
	.ucom_cfg_set_rts = &ufintek_cfg_set_rts,
	.ucom_cfg_set_break = &ufintek_cfg_set_break,
	.ucom_cfg_open = &ufintek_cfg_open,
	.ucom_cfg_close = &ufintek_cfg_close,
	.ucom_cfg_param = &ufintek_cfg_param,
	.ucom_pre_param = &ufintek_pre_param,
	.ucom_start_read = &ufintek_start_read,
	.ucom_stop_read = &ufintek_stop_read,
	.ucom_start_write = &ufintek_start_write,
	.ucom_stop_write = &ufintek_stop_write,
	.ucom_poll = &ufintek_poll,
	.ucom_free = &ufintek_free,
};

static device_method_t ufintek_methods[] = {
	DEVMETHOD(device_probe, ufintek_probe),
	DEVMETHOD(device_attach, ufintek_attach),
	DEVMETHOD(device_detach, ufintek_detach),
	DEVMETHOD_END
};

static driver_t ufintek_driver = {
	.name = "ufintek",
	.methods = ufintek_methods,
	.size = sizeof(struct ufintek_softc),
};


/*
 * The following VENDOR PRODUCT definitions should be moved to usbdevs
 * 
 * vendor  FINTEK		0x1934
 * product FINTEK	SERIAL	0x0706
 *
 */
#ifndef USB_VENDOR_FINTEK
#define USB_VENDOR_FINTEK		0x1934
#endif
#ifndef USB_PRODUCT_FINTEK_SERIAL
#define USB_PRODUCT_FINTEK_SERIAL	0x0706
#endif
#ifndef FINTEK_MODEL_F81232
#define FINTEK_MODEL_F81232		0x02
#endif

static const STRUCT_USB_HOST_ID ufintek_devs[] = {
	{USB_VPI(USB_VENDOR_FINTEK, USB_PRODUCT_FINTEK_SERIAL, FINTEK_MODEL_F81232)}
};

DRIVER_MODULE(ufintek, uhub, ufintek_driver, NULL, NULL);
MODULE_DEPEND(ufintek, ucom, 1, 1, 1);
MODULE_DEPEND(ufintek, usb, 1, 1, 1);
MODULE_VERSION(ufintek, 1);
USB_PNP_HOST_INFO(ufintek_devs);

#define	UFINTEK_DEFAULT_BAUD	 115200
#define	UFINTEK_DEFAULT_LCR	 UFINTEK_LCR_WORDLEN_8|UFINTEK_LCR_STOP_BITS_1| \
					UFINTEK_LCR_PARITY_NONE

static int
ufintek_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bConfigIndex != UFINTEK_CONFIG_INDEX) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != UFINTEK_IFACE_INDEX) {
		return (ENXIO);
	}
	return (usbd_lookup_id_by_uaa(ufintek_devs, sizeof(ufintek_devs), uaa));
}

static int
ufintek_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct ufintek_softc *sc = device_get_softc(dev);
	int error;
	uint8_t iface_index;

	sc->sc_udev = uaa->device;
	
	device_set_usb_desc(dev);

	device_printf(dev, "<FINTEK USB Serial Port Adapter>\n");

	mtx_init(&sc->sc_mtx, "ufintek", NULL, MTX_DEF);
	ucom_ref(&sc->sc_super_ucom);
	
	DPRINTF("\n");

	/* get chip model */
	sc->sc_model = USB_GET_DRIVER_INFO(uaa);
	if (sc->sc_model == 0) {
		device_printf(dev, "unsupported device\n");
		goto detach;
	}
	device_printf(dev, "FINTEK F8123%X USB to RS232 bridge\n", sc->sc_model);
	iface_index = UFINTEK_IFACE_INDEX;
	error = usbd_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, ufintek_config,
	    UFINTEK_N_TRANSFER, sc, &sc->sc_mtx);

	if (error) {
		device_printf(dev, "allocating USB transfers failed\n");
		goto detach;
	}

	/* clear stall at first run */
	mtx_lock(&sc->sc_mtx);
	usbd_xfer_set_stall(sc->sc_xfer[UFINTEK_BULK_DT_WR]);
	usbd_xfer_set_stall(sc->sc_xfer[UFINTEK_BULK_DT_RD]);
	mtx_unlock(&sc->sc_mtx);

	error = ucom_attach(&sc->sc_super_ucom, &sc->sc_ucom, 1, sc,
	    &ufintek_callback, &sc->sc_mtx);
	if (error) {
		goto detach;
	}
	ucom_set_pnpinfo_usb(&sc->sc_super_ucom, dev);

	return (0);			/* success */

detach:
	ufintek_detach(dev);
	return (ENXIO);
}

static int
ufintek_detach(device_t dev)
{
	struct ufintek_softc *sc = device_get_softc(dev);

	ucom_detach(&sc->sc_super_ucom, &sc->sc_ucom);
	usbd_transfer_unsetup(sc->sc_xfer, UFINTEK_N_TRANSFER);

	device_claim_softc(dev);

	ufintek_free_softc(sc);

	return (0);
}

UCOM_UNLOAD_DRAIN(ufintek);

static void
ufintek_free_softc(struct ufintek_softc *sc)
{
	if (ucom_unref(&sc->sc_super_ucom)) {
		mtx_destroy(&sc->sc_mtx);
		device_free_softc(sc);
	}
}

static void
ufintek_free(struct ucom_softc *ucom)
{
	ufintek_free_softc(ucom->sc_parent);
}

static void
ufintek_cfg_open(struct ucom_softc *ucom)
{
	struct ufintek_softc *sc = ucom->sc_parent;

	/* Enable FIFO, trigger8 */
	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_FCR,
			  UFINTEK_FCR_EN | UFINTEK_FCR_TRIGGER_8);

	sc->sc_mcr =  UFINTEK_MCR_DTR|UFINTEK_MCR_RTS;
	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_MCR, sc->sc_mcr);
	
	/* Enable interrupts */
	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_IER,
			  UFINTEK_IER_MSI );
}

static void
ufintek_cfg_close(struct ucom_softc *ucom)
{
	return;
}

static void
ufintek_cfg_set_break(struct ucom_softc *ucom, uint8_t onoff)
{
	struct ufintek_softc *sc = ucom->sc_parent;

	if (onoff)
		sc->sc_lcr |= UFINTEK_LCR_BREAK;
	else
		sc->sc_lcr &= ~UFINTEK_LCR_BREAK;

	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_LCR, sc->sc_lcr);
}

static void
ufintek_cfg_set_dtr(struct ucom_softc *ucom, uint8_t onoff)
{
	struct ufintek_softc *sc = ucom->sc_parent;

	if (onoff)
		sc->sc_mcr |= UFINTEK_MCR_DTR;
	else
		sc->sc_mcr &= ~UFINTEK_MCR_DTR;

	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_MCR, sc->sc_mcr);
}

static void
ufintek_cfg_set_rts(struct ucom_softc *ucom, uint8_t onoff)
{
	struct ufintek_softc *sc = ucom->sc_parent;

	if (onoff)
		sc->sc_mcr |= UFINTEK_MCR_RTS;
	else
		sc->sc_mcr &= ~UFINTEK_MCR_RTS;

	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_MCR, sc->sc_mcr);
}

static int
ufintek_pre_param(struct ucom_softc *ucom, struct termios *t)
{
	struct ufintek_softc *sc = ucom->sc_parent;
	uint16_t divisor;

	if ((t->c_ospeed <= 1) || (t->c_ospeed > 115200))
		return (EINVAL);

	sc->sc_mcr = UFINTEK_MCR_DTR|UFINTEK_MCR_RTS;
	sc->sc_lcr = UFINTEK_DEFAULT_LCR;
	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_LCR, sc->sc_lcr);

	DPRINTF("baud=%d\n", t->c_ospeed);
	
	divisor = ((uint32_t)UFINTEK_BAUD_REF) / ((uint32_t)t->c_ospeed);

	if (divisor == 0) {
		DPRINTF("invalid baud rate!\n");
		return (1);
	}

	/* Flip RXBUF and TXBUF to BAUDLO and BAUDHI */
	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_LCR, UFINTEK_LCR_DLAB);
        ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_BAUDLO,
			  (divisor & 0xFF));
	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_BAUDHI,
			  ((divisor >> 8) & 0xFF));
	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_LCR, sc->sc_lcr);
	
	return (0);
}

static void
ufintek_cfg_param(struct ucom_softc *ucom, struct termios *t)
{
	struct ufintek_softc *sc = ucom->sc_parent;
	uint8_t lcr=0;
	uint16_t divisor;
	
	DPRINTF("baud=%d\n", t->c_ospeed);
	
	divisor = ((uint32_t)UFINTEK_BAUD_REF) / ((uint32_t)t->c_ospeed);

	if (divisor == 0) {
		DPRINTF("invalid baud rate!\n");
		return;
	}

	/* Flip RXBUF and TXBUF to BAUDLO and BAUDHI */
	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_LCR, UFINTEK_LCR_DLAB);
        ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_BAUDLO,
			  (divisor & 0xFF));
	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_BAUDHI,
			  ((divisor >> 8) & 0xFF));

	if (!(t->c_cflag & CIGNORE)) {
		lcr = 0;
		switch (t->c_cflag & CSIZE) {
		case CS8:
			lcr |= UFINTEK_LCR_WORDLEN_8;
			break;
		case CS7:
			lcr |= UFINTEK_LCR_WORDLEN_7;
			break;
		case CS6:
			lcr |= UFINTEK_LCR_WORDLEN_6;
			break;
		case CS5:
			break;
		default:
			break;
		}

		if (t->c_cflag & CSTOPB)
			lcr |= UFINTEK_LCR_STOP_BITS_1;
		if (t->c_cflag & PARODD)
			lcr |= UFINTEK_LCR_PARITY_ODD;
		else if (t->c_cflag & PARENB)
			lcr |= UFINTEK_LCR_PARITY_EVEN;
	}
	sc->sc_lcr = lcr;
	ufintek_cfg_write(sc, UFINTEK_UART_REG | UFINTEK_LCR, sc->sc_lcr);
}

static void
ufintek_cfg_get_status(struct ucom_softc *ucom, uint8_t *p_lsr, uint8_t *p_msr)
{
	struct ufintek_softc *sc = ucom->sc_parent;
	uint8_t lsr;
	uint8_t ufintek_msr;
	
	lsr = ufintek_cfg_read(sc, UFINTEK_UART_REG | UFINTEK_LSR);
	lsr &= 7;	/* Only need bottom bits */
	*p_lsr = lsr;
	
	ufintek_msr = ufintek_cfg_read(sc, UFINTEK_UART_REG | UFINTEK_MSR);

	/* translate bits */

	*p_msr = 0;
	if (ufintek_msr & UFINTEK_MSR_CTS)
		*p_msr |= SER_CTS;

	if (ufintek_msr & UFINTEK_MSR_CD)
		*p_msr |= SER_DCD;

	if (ufintek_msr & UFINTEK_MSR_RI)
		*p_msr |= SER_RI;

	if (ufintek_msr & UFINTEK_MSR_RTS)
		*p_msr |= SER_RTS;
}

static void
ufintek_start_read(struct ucom_softc *ucom)
{
	struct ufintek_softc *sc = ucom->sc_parent;

	usbd_transfer_start(sc->sc_xfer[UFINTEK_BULK_DT_RD]);
}

static void
ufintek_stop_read(struct ucom_softc *ucom)
{
	struct ufintek_softc *sc = ucom->sc_parent;

	usbd_transfer_stop(sc->sc_xfer[UFINTEK_BULK_DT_RD]);
}

static void
ufintek_start_write(struct ucom_softc *ucom)
{
	struct ufintek_softc *sc = ucom->sc_parent;

	usbd_transfer_start(sc->sc_xfer[UFINTEK_BULK_DT_WR]);
}

static void
ufintek_stop_write(struct ucom_softc *ucom)
{
	struct ufintek_softc *sc = ucom->sc_parent;

	usbd_transfer_stop(sc->sc_xfer[UFINTEK_BULK_DT_WR]);
}

static void
ufintek_cfg_write(struct ufintek_softc *sc, uint16_t reg, uint8_t val)
{
	struct usb_device_request req;
	usb_error_t uerr;
	uint8_t data;
	
	req.bmRequestType = UFINTEK_WRITE;
	req.bRequest = UFINTEK_REGISTER_REQUEST;
	USETW(req.wValue, reg);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 1);

	data = val;

	uerr = ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom,
		&req, &data, 0, 1000);
	if (uerr != 0)
		DPRINTF("failed to set ctrl %s\n", usbd_errstr(uerr));
}

static uint8_t
ufintek_cfg_read(struct ufintek_softc *sc, uint16_t reg)
{
	struct usb_device_request req;
	uint8_t val;
	
	req.bmRequestType = UFINTEK_READ;
	req.bRequest = UFINTEK_REGISTER_REQUEST;
	USETW(req.wValue, reg);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 1);

	ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom, 
	    &req, &val, 0, 1000);

	DPRINTF("reg=0x%04x, val=0x%02x\n", reg, val);
	return (val);
}


static void
ufintek_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct ufintek_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint32_t actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:
tr_setup:

		pc = usbd_xfer_get_frame(xfer, 0);
		if (ucom_get_data(&sc->sc_ucom, pc, 0,
			UFINTEK_BUFSIZE, &actlen)) {
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
ufintek_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct ufintek_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int actlen;
	uint8_t buf[UFINTEK_BUFSIZE];
	int i;
	
	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);
	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTF("got %d bytes\n", actlen);
		if ((actlen < 2) || (actlen % 2))
			goto tr_setup;
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, 0, &buf, sizeof(buf));
		/* XXX From Linux driver the very first byte after open
		 * is supposed to be a status which we should ignore.
		 * Why it is 0xFF I don't know TBH. 
		 */
		if (buf[0] == 0xFF) 
			buf[0] = ufintek_cfg_read(sc, UFINTEK_UART_REG);
		else {
		/*
		 * Annoyingly this device presents data as
		 * <LSR><DATA><LSR><DATA> ...
		 */
		for (i = 0; i < actlen; i += 2) {
			ucom_put_data(&sc->sc_ucom, pc, i+1, 1);
		}

		/* FALLTHROUGH */
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
ufintek_poll(struct ucom_softc *ucom)
{
	struct ufintek_softc *sc = ucom->sc_parent;
	usbd_transfer_poll(sc->sc_xfer, UFINTEK_N_TRANSFER);
}
