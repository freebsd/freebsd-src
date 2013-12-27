/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Oleksandr Rybalko under sponsorship
 * from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.	Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2.	Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kdb.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_bus.h>

#include <dev/uart/uart_dev_imx5xx.h>

#include "uart_if.h"
/*
 * Low-level UART interface.
 */
static int imx_uart_probe(struct uart_bas *bas);
static void imx_uart_init(struct uart_bas *bas, int, int, int, int);
static void imx_uart_term(struct uart_bas *bas);
static void imx_uart_putc(struct uart_bas *bas, int);
static int imx_uart_rxready(struct uart_bas *bas);
static int imx_uart_getc(struct uart_bas *bas, struct mtx *);

static struct uart_ops uart_imx_uart_ops = {
	.probe = imx_uart_probe,
	.init = imx_uart_init,
	.term = imx_uart_term,
	.putc = imx_uart_putc,
	.rxready = imx_uart_rxready,
	.getc = imx_uart_getc,
};

static int
imx_uart_probe(struct uart_bas *bas)
{

	return (0);
}

static void
imx_uart_init(struct uart_bas *bas, int baudrate, int databits, 
    int stopbits, int parity)
{

}

static void
imx_uart_term(struct uart_bas *bas)
{

}

static void
imx_uart_putc(struct uart_bas *bas, int c)
{

	while (!(IS(bas, USR2, TXFE)))
		;
	SETREG(bas, REG(UTXD), c);
}

static int
imx_uart_rxready(struct uart_bas *bas)
{

	return ((IS(bas, USR2, RDR)) ? 1 : 0);
}

static int
imx_uart_getc(struct uart_bas *bas, struct mtx *hwmtx)
{
	int c;

	uart_lock(hwmtx);
	while (!(IS(bas, USR2, RDR)))
		;

	c = GETREG(bas, REG(URXD));
	uart_unlock(hwmtx);
#if defined(KDB)
	if (c & FLD(URXD, BRK)) {
		if (kdb_break())
			return (0);
	}
#endif
	return (c & 0xff);
}

/*
 * High-level UART interface.
 */
struct imx_uart_softc {
	struct uart_softc base;
};

static int imx_uart_bus_attach(struct uart_softc *);
static int imx_uart_bus_detach(struct uart_softc *);
static int imx_uart_bus_flush(struct uart_softc *, int);
static int imx_uart_bus_getsig(struct uart_softc *);
static int imx_uart_bus_ioctl(struct uart_softc *, int, intptr_t);
static int imx_uart_bus_ipend(struct uart_softc *);
static int imx_uart_bus_param(struct uart_softc *, int, int, int, int);
static int imx_uart_bus_probe(struct uart_softc *);
static int imx_uart_bus_receive(struct uart_softc *);
static int imx_uart_bus_setsig(struct uart_softc *, int);
static int imx_uart_bus_transmit(struct uart_softc *);

static kobj_method_t imx_uart_methods[] = {
	KOBJMETHOD(uart_attach,		imx_uart_bus_attach),
	KOBJMETHOD(uart_detach,		imx_uart_bus_detach),
	KOBJMETHOD(uart_flush,		imx_uart_bus_flush),
	KOBJMETHOD(uart_getsig,		imx_uart_bus_getsig),
	KOBJMETHOD(uart_ioctl,		imx_uart_bus_ioctl),
	KOBJMETHOD(uart_ipend,		imx_uart_bus_ipend),
	KOBJMETHOD(uart_param,		imx_uart_bus_param),
	KOBJMETHOD(uart_probe,		imx_uart_bus_probe),
	KOBJMETHOD(uart_receive,	imx_uart_bus_receive),
	KOBJMETHOD(uart_setsig,		imx_uart_bus_setsig),
	KOBJMETHOD(uart_transmit,	imx_uart_bus_transmit),
	{ 0, 0 }
};

struct uart_class uart_imx_class = {
	"imx",
	imx_uart_methods,
	sizeof(struct imx_uart_softc),
	.uc_ops = &uart_imx_uart_ops,
	.uc_range = 0x100,
	.uc_rclk = 24000000 /* TODO: get value from CCM */
};

#define	SIGCHG(c, i, s, d)				\
	if (c) {					\
		i |= (i & s) ? s : s | d;		\
	} else {					\
		i = (i & s) ? (i & ~s) | d : i;		\
	}

static int
imx_uart_bus_attach(struct uart_softc *sc)
{
	struct uart_bas *bas;
	struct uart_devinfo *di;

	bas = &sc->sc_bas;
	if (sc->sc_sysdev != NULL) {
		di = sc->sc_sysdev;
		imx_uart_init(bas, di->baudrate, di->databits, di->stopbits,
		    di->parity);
	} else {
		imx_uart_init(bas, 115200, 8, 1, 0);
	}

	(void)imx_uart_bus_getsig(sc);

	/* XXX workaround to have working console on mount prompt */
	if (sc->sc_sysdev != NULL && sc->sc_sysdev->type == UART_DEV_CONSOLE){
		DIS(bas, UCR4, DREN);
	} else {
		ENA(bas, UCR4, DREN);
	}
	DIS(bas, UCR1, RRDYEN);
	DIS(bas, UCR1, IDEN);
	DIS(bas, UCR3, RXDSEN);
	DIS(bas, UCR2, ATEN);
	DIS(bas, UCR1, TXMPTYEN);
	DIS(bas, UCR1, TRDYEN);
	DIS(bas, UCR4, TCEN);
	DIS(bas, UCR4, OREN);
	ENA(bas, UCR4, BKEN);
	DIS(bas, UCR4, WKEN);
	DIS(bas, UCR1, ADEN);
	DIS(bas, UCR3, ACIEN);
	DIS(bas, UCR2, ESCI);
	DIS(bas, UCR4, ENIRI);
	DIS(bas, UCR3, AIRINTEN);
	DIS(bas, UCR3, AWAKEN);
	DIS(bas, UCR3, FRAERREN);
	DIS(bas, UCR3, PARERREN);
	DIS(bas, UCR1, RTSDEN);
	DIS(bas, UCR2, RTSEN);
	DIS(bas, UCR3, DTREN);
	DIS(bas, UCR3, RI);
	DIS(bas, UCR3, DCD);
	DIS(bas, UCR3, DTRDEN);

	/* ACK all interrupts */
	SETREG(bas, REG(USR1), 0xffff);
	SETREG(bas, REG(USR2), 0xffff);
	return (0);
}

static int
imx_uart_bus_detach(struct uart_softc *sc)
{

	SETREG(&sc->sc_bas, REG(UCR4), 0);

	return (0);
}

static int
imx_uart_bus_flush(struct uart_softc *sc, int what)
{

	/* TODO */
	return (0);
}

static int
imx_uart_bus_getsig(struct uart_softc *sc)
{
	uint32_t new, old, sig;
	uint8_t bes;

	do {
		old = sc->sc_hwsig;
		sig = old;
		uart_lock(sc->sc_hwmtx);
		bes = GETREG(&sc->sc_bas, REG(USR2));
		uart_unlock(sc->sc_hwmtx);
		/* XXX: chip can show delta */
		SIGCHG(bes & FLD(USR2, DCDIN), sig, SER_DCD, SER_DDCD);
		new = sig & ~SER_MASK_DELTA;
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));

	return (sig);
}

static int
imx_uart_bus_ioctl(struct uart_softc *sc, int request, intptr_t data)
{
	struct uart_bas *bas;
	int error;

	bas = &sc->sc_bas;
	error = 0;
	uart_lock(sc->sc_hwmtx);
	switch (request) {
	case UART_IOCTL_BREAK:
		/* TODO */
		break;
	case UART_IOCTL_BAUD:
		/* TODO */
		*(int*)data = 115200;
		break;
	default:
		error = EINVAL;
		break;
	}
	uart_unlock(sc->sc_hwmtx);

	return (error);
}

static int
imx_uart_bus_ipend(struct uart_softc *sc)
{
	struct uart_bas *bas;
	int ipend;
	uint32_t usr1, usr2;
	uint32_t ucr1, ucr4;

	bas = &sc->sc_bas;
	ipend = 0;

	uart_lock(sc->sc_hwmtx);

	/* Read pending interrupts */
	usr1 = GETREG(bas, REG(USR1));
	usr2 = GETREG(bas, REG(USR2));
	/* ACK interrupts */
	SETREG(bas, REG(USR1), usr1);
	SETREG(bas, REG(USR2), usr2);

	ucr1 = GETREG(bas, REG(UCR1));
	ucr4 = GETREG(bas, REG(UCR4));

	if ((usr2 & FLD(USR2, TXFE)) && (ucr1 & FLD(UCR1, TXMPTYEN))) {
		DIS(bas, UCR1, TXMPTYEN);
		/* Continue TXing */
		ipend |= SER_INT_TXIDLE;
	}
	if ((usr2 & FLD(USR2, RDR)) && (ucr4 & FLD(UCR4, DREN))) {
		DIS(bas, UCR4, DREN);
		/* Wow, new char on input */
		ipend |= SER_INT_RXREADY;
	}
	if ((usr2 & FLD(USR2, BRCD)) && (ucr4 & FLD(UCR4, BKEN)))
		ipend |= SER_INT_BREAK;

	uart_unlock(sc->sc_hwmtx);

	return (ipend);
}

static int
imx_uart_bus_param(struct uart_softc *sc, int baudrate, int databits,
    int stopbits, int parity)
{

	uart_lock(sc->sc_hwmtx);
	imx_uart_init(&sc->sc_bas, baudrate, databits, stopbits, parity);
	uart_unlock(sc->sc_hwmtx);
	return (0);
}

static int
imx_uart_bus_probe(struct uart_softc *sc)
{
	int error;

	error = imx_uart_probe(&sc->sc_bas);
	if (error)
		return (error);

	sc->sc_rxfifosz = 1;
	sc->sc_txfifosz = 1;

	device_set_desc(sc->sc_dev, "imx_uart");
	return (0);
}

static int
imx_uart_bus_receive(struct uart_softc *sc)
{
	struct uart_bas *bas;
	int xc, out;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);

	/* Read while we have anything in FIFO */
	while (IS(bas, USR2, RDR)) {
		if (uart_rx_full(sc)) {
			/* No space left in input buffer */
			sc->sc_rxbuf[sc->sc_rxput] = UART_STAT_OVERRUN;
			break;
		}
		out = 0;
		xc = GETREG(bas, REG(URXD));

		/* We have valid char */
		if (xc & FLD(URXD, CHARRDY))
			out = xc & 0x000000ff;

		if (xc & FLD(URXD, FRMERR))
			out |= UART_STAT_FRAMERR;
		if (xc & FLD(URXD, PRERR))
			out |= UART_STAT_PARERR;
		if (xc & FLD(URXD, OVRRUN))
			out |= UART_STAT_OVERRUN;
		if (xc & FLD(URXD, BRK))
			out |= UART_STAT_BREAK;

		uart_rx_put(sc, out);
	}
	/* Reenable Data Ready interrupt */
	ENA(bas, UCR4, DREN);

	uart_unlock(sc->sc_hwmtx);
	return (0);
}

static int
imx_uart_bus_setsig(struct uart_softc *sc, int sig)
{

	/* TODO: implement (?) */

	/* XXX workaround to have working console on mount prompt */
	/* Enable RX interrupt */
	if (sc->sc_sysdev != NULL && sc->sc_sysdev->type == UART_DEV_CONSOLE)
		if (!IS(&sc->sc_bas, UCR4, DREN))
			ENA(&sc->sc_bas, UCR4, DREN);
	return (0);
}

static int
imx_uart_bus_transmit(struct uart_softc *sc)
{
	struct uart_bas *bas = &sc->sc_bas;
	int i;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);

	/* Fill TX FIFO */
	for (i = 0; i < sc->sc_txdatasz; i++) {
		SETREG(bas, REG(UTXD), sc->sc_txbuf[i] & 0xff);
	}

	sc->sc_txbusy = 1;
	/* Call me when ready */
	ENA(bas, UCR1, TXMPTYEN);

	uart_unlock(sc->sc_hwmtx);

	return (0);
}
