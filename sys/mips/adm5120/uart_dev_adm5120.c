/* $NetBSD: uart.c,v 1.2 2007/03/23 20:05:47 dogcow Exp $ */

/*-
 * Copyright (c) 2007 Ruslan Ermilov and Vsevolod Lobko.
 * Copyright (c) 2007 Oleksandr Tymoshenko.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <machine/bus.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_bus.h>

#include <mips/adm5120/uart_dev_adm5120.h>

#include "uart_if.h"

/*
 * Low-level UART interface.
 */
static int adm5120_uart_probe(struct uart_bas *bas);
static void adm5120_uart_init(struct uart_bas *bas, int, int, int, int);
static void adm5120_uart_term(struct uart_bas *bas);
static void adm5120_uart_putc(struct uart_bas *bas, int);
static int adm5120_uart_rxready(struct uart_bas *bas);
static int adm5120_uart_getc(struct uart_bas *bas, struct mtx *);

static struct uart_ops uart_adm5120_uart_ops = {
	.probe = adm5120_uart_probe,
	.init = adm5120_uart_init,
	.term = adm5120_uart_term,
	.putc = adm5120_uart_putc,
	.rxready = adm5120_uart_rxready,
	.getc = adm5120_uart_getc,
};

static int
adm5120_uart_probe(struct uart_bas *bas)
{

	return (0);
}

static void
adm5120_uart_init(struct uart_bas *bas, int baudrate, int databits, 
    int stopbits, int parity)
{

	/* TODO: Set parameters for uart, meanwhile stick with 115200N1 */
}

static void
adm5120_uart_term(struct uart_bas *bas)
{

}

static void
adm5120_uart_putc(struct uart_bas *bas, int c)
{
	char chr;
	chr = c;
	while (uart_getreg(bas, UART_FR_REG) & UART_FR_TX_FIFO_FULL)
		;
	uart_setreg(bas, UART_DR_REG, c);
	while (uart_getreg(bas, UART_FR_REG) & UART_FR_BUSY)
		;
	uart_barrier(bas);
}

static int
adm5120_uart_rxready(struct uart_bas *bas)
{
	if (uart_getreg(bas, UART_FR_REG) & UART_FR_RX_FIFO_EMPTY)
		return (0);

	return (1);
}

static int
adm5120_uart_getc(struct uart_bas *bas, struct mtx *hwmtx)
{
	int c;

	uart_lock(hwmtx);

	while (uart_getreg(bas, UART_FR_REG) & UART_FR_RX_FIFO_EMPTY) {
		uart_unlock(hwmtx);
		DELAY(10);
		uart_lock(hwmtx);
	}

	c = uart_getreg(bas, UART_DR_REG);

	uart_unlock(hwmtx);

	return (c);
}

/*
 * High-level UART interface.
 */
struct adm5120_uart_softc {
	struct uart_softc base;
};

static int adm5120_uart_bus_attach(struct uart_softc *);
static int adm5120_uart_bus_detach(struct uart_softc *);
static int adm5120_uart_bus_flush(struct uart_softc *, int);
static int adm5120_uart_bus_getsig(struct uart_softc *);
static int adm5120_uart_bus_ioctl(struct uart_softc *, int, intptr_t);
static int adm5120_uart_bus_ipend(struct uart_softc *);
static int adm5120_uart_bus_param(struct uart_softc *, int, int, int, int);
static int adm5120_uart_bus_probe(struct uart_softc *);
static int adm5120_uart_bus_receive(struct uart_softc *);
static int adm5120_uart_bus_setsig(struct uart_softc *, int);
static int adm5120_uart_bus_transmit(struct uart_softc *);

static kobj_method_t adm5120_uart_methods[] = {
	KOBJMETHOD(uart_attach,		adm5120_uart_bus_attach),
	KOBJMETHOD(uart_detach,		adm5120_uart_bus_detach),
	KOBJMETHOD(uart_flush,		adm5120_uart_bus_flush),
	KOBJMETHOD(uart_getsig,		adm5120_uart_bus_getsig),
	KOBJMETHOD(uart_ioctl,		adm5120_uart_bus_ioctl),
	KOBJMETHOD(uart_ipend,		adm5120_uart_bus_ipend),
	KOBJMETHOD(uart_param,		adm5120_uart_bus_param),
	KOBJMETHOD(uart_probe,		adm5120_uart_bus_probe),
	KOBJMETHOD(uart_receive,	adm5120_uart_bus_receive),
	KOBJMETHOD(uart_setsig,		adm5120_uart_bus_setsig),
	KOBJMETHOD(uart_transmit,	adm5120_uart_bus_transmit),
	{ 0, 0 }
};

struct uart_class uart_adm5120_uart_class = {
	"adm5120",
	adm5120_uart_methods,
	sizeof(struct adm5120_uart_softc),
	.uc_ops = &uart_adm5120_uart_ops,
	.uc_range = 1, /* use hinted range */
	.uc_rclk = 62500000
};

#define	SIGCHG(c, i, s, d)				\
	if (c) {					\
		i |= (i & s) ? s : s | d;		\
	} else {					\
		i = (i & s) ? (i & ~s) | d : i;		\
	}

/*
 * Disable TX interrupt. uart should be locked 
 */ 
static __inline void
adm5120_uart_disable_txintr(struct uart_softc *sc)
{
	uint8_t cr;

	cr = uart_getreg(&sc->sc_bas, UART_CR_REG);
	cr &= ~UART_CR_TX_INT_EN;
	uart_setreg(&sc->sc_bas, UART_CR_REG, cr);
}

/*
 * Enable TX interrupt. uart should be locked 
 */ 
static __inline void
adm5120_uart_enable_txintr(struct uart_softc *sc)
{
	uint8_t cr;

	cr = uart_getreg(&sc->sc_bas, UART_CR_REG);
	cr |= UART_CR_TX_INT_EN;
	uart_setreg(&sc->sc_bas, UART_CR_REG, cr);
}

static int
adm5120_uart_bus_attach(struct uart_softc *sc)
{
	struct uart_bas *bas;
	struct uart_devinfo *di;

	bas = &sc->sc_bas;
	if (sc->sc_sysdev != NULL) {
		di = sc->sc_sysdev;
		/* TODO: set parameters from di */
	} else {
		/* TODO: set parameters 115200, 8N1 */
	}

	(void)adm5120_uart_bus_getsig(sc);

#if 1
	/* Enable FIFO */
	uart_setreg(bas, UART_LCR_H_REG, 
	    uart_getreg(bas, UART_LCR_H_REG) | UART_LCR_H_FEN);
#endif
	/* Enable interrupts */
	uart_setreg(bas, UART_CR_REG,
	    UART_CR_PORT_EN|UART_CR_RX_INT_EN|UART_CR_RX_TIMEOUT_INT_EN|
	    UART_CR_MODEM_STATUS_INT_EN);

	return (0);
}

static int
adm5120_uart_bus_detach(struct uart_softc *sc)
{

	return (0);
}

static int
adm5120_uart_bus_flush(struct uart_softc *sc, int what)
{

	return (0);
}

static int
adm5120_uart_bus_getsig(struct uart_softc *sc)
{
	uint32_t new, old, sig;
	uint8_t bes;

	do {
		old = sc->sc_hwsig;
		sig = old;
		uart_lock(sc->sc_hwmtx);
		bes = uart_getreg(&sc->sc_bas, UART_FR_REG);
		uart_unlock(sc->sc_hwmtx);
		SIGCHG(bes & UART_FR_CTS, sig, SER_CTS, SER_DCTS);
		SIGCHG(bes & UART_FR_DCD, sig, SER_DCD, SER_DDCD);
		SIGCHG(bes & UART_FR_DSR, sig, SER_DSR, SER_DDSR);
		new = sig & ~SER_MASK_DELTA;
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));

	return (sig);
}

static int
adm5120_uart_bus_ioctl(struct uart_softc *sc, int request, intptr_t data)
{
	struct uart_bas *bas;
	int baudrate, divisor, error;

	bas = &sc->sc_bas;
	error = 0;
	uart_lock(sc->sc_hwmtx);
	switch (request) {
	case UART_IOCTL_BREAK:
		/* TODO: Send BREAK */
		break;
	case UART_IOCTL_BAUD:
		divisor = uart_getreg(bas, UART_LCR_M_REG);
		divisor = (divisor << 8) | 
		    uart_getreg(bas, UART_LCR_L_REG);
		baudrate = bas->rclk / 2 / (divisor + 2);
		*(int*)data = baudrate;
		break;
	default:
		error = EINVAL;
		break;
	}
	uart_unlock(sc->sc_hwmtx);
	return (error);
}

static int
adm5120_uart_bus_ipend(struct uart_softc *sc)
{
	struct uart_bas *bas;
	int ipend;
	uint8_t ir, fr, rsr;

	bas = &sc->sc_bas;
	ipend = 0;

	uart_lock(sc->sc_hwmtx);
	ir = uart_getreg(&sc->sc_bas, UART_IR_REG);
	fr = uart_getreg(&sc->sc_bas, UART_FR_REG);
	rsr = uart_getreg(&sc->sc_bas, UART_RSR_REG);

	if (ir & UART_IR_RX_INT)
		ipend |= SER_INT_RXREADY;

	if (ir & UART_IR_RX_TIMEOUT_INT)
		ipend |= SER_INT_RXREADY;

	if (ir & UART_IR_MODEM_STATUS_INT)
		ipend |= SER_INT_SIGCHG;

	if (rsr & UART_RSR_BE)
		ipend |= SER_INT_BREAK;

	if (rsr & UART_RSR_OE)
		ipend |= SER_INT_OVERRUN;

	if (fr & UART_FR_TX_FIFO_EMPTY) {
		if (ir & UART_IR_TX_INT) {
			adm5120_uart_disable_txintr(sc);
			ipend |= SER_INT_TXIDLE;
		}
	}

	if (ipend)
		uart_setreg(bas, UART_IR_REG, ir | UART_IR_UICR);

	uart_unlock(sc->sc_hwmtx);

	return (ipend);
}

static int
adm5120_uart_bus_param(struct uart_softc *sc, int baudrate, int databits,
    int stopbits, int parity)
{

	/* TODO: Set parameters for uart, meanwhile stick with 115200 8N1 */
	return (0);
}

static int
adm5120_uart_bus_probe(struct uart_softc *sc)
{
	char buf[80];
	int error;
	char ch;

	error = adm5120_uart_probe(&sc->sc_bas);
	if (error)
		return (error);

	sc->sc_rxfifosz = 16;
	sc->sc_txfifosz = 16;

	ch = sc->sc_bas.chan + 'A';

	snprintf(buf, sizeof(buf), "adm5120_uart, channel %c", ch);
	device_set_desc_copy(sc->sc_dev, buf);

	return (0);
}

static int
adm5120_uart_bus_receive(struct uart_softc *sc)
{
	struct uart_bas *bas;
	int xc;
	uint8_t fr, rsr;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);
	fr = uart_getreg(bas, UART_FR_REG);
	while (!(fr & UART_FR_RX_FIFO_EMPTY)) {
		if (uart_rx_full(sc)) {
			sc->sc_rxbuf[sc->sc_rxput] = UART_STAT_OVERRUN;
			break;
		}
		xc = 0;
		rsr = uart_getreg(bas, UART_RSR_REG);
		if (rsr & UART_RSR_FE)
			xc |= UART_STAT_FRAMERR;
		if (rsr & UART_RSR_PE)
			xc |= UART_STAT_PARERR;
		if (rsr & UART_RSR_OE)
			xc |= UART_STAT_OVERRUN;
		xc |= uart_getreg(bas, UART_DR_REG);
		uart_barrier(bas);
		uart_rx_put(sc, xc);
		if (rsr & (UART_RSR_FE | UART_RSR_PE | UART_RSR_OE)) {
			uart_setreg(bas, UART_ECR_REG, UART_ECR_RSR);
			uart_barrier(bas);
		}
		fr = uart_getreg(bas, UART_FR_REG);
	}

	/* Discard everything left in the Rx FIFO. */
	while (!(fr & UART_FR_RX_FIFO_EMPTY)) {
		( void)uart_getreg(bas, UART_DR_REG);
		uart_barrier(bas);
		rsr = uart_getreg(bas, UART_RSR_REG);
		if (rsr & (UART_RSR_FE | UART_RSR_PE | UART_RSR_OE)) {
			uart_setreg(bas, UART_ECR_REG, UART_ECR_RSR);
			uart_barrier(bas);
		}
		fr = uart_getreg(bas, UART_FR_REG);
	}
	uart_unlock(sc->sc_hwmtx);
	return (0);
}

static int
adm5120_uart_bus_setsig(struct uart_softc *sc, int sig)
{

	/* TODO: implement (?) */
	return (0);
}

static int
adm5120_uart_bus_transmit(struct uart_softc *sc)
{
	struct uart_bas *bas;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);
	sc->sc_txbusy = 1;
	for (int i = 0; i < sc->sc_txdatasz; i++) {
		if (uart_getreg(bas, UART_FR_REG) & UART_FR_TX_FIFO_FULL) 
			break;
		uart_setreg(bas, UART_DR_REG, sc->sc_txbuf[i]);
	}

	/* Enable TX interrupt */
	adm5120_uart_enable_txintr(sc);
	uart_unlock(sc->sc_hwmtx);
	return (0);
}
