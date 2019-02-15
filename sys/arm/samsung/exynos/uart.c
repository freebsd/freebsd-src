/*
 * Copyright (c) 2003 Marcel Moolenaar
 * Copyright (c) 2007-2009 Andrew Turner
 * Copyright (c) 2013 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/tty.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_bus.h>

#include <arm/samsung/exynos/uart.h>

#include "uart_if.h"

#define	DEF_CLK		100000000

static int sscomspeed(long, long);
static int s3c24x0_uart_param(struct uart_bas *, int, int, int, int);

/*
 * Low-level UART interface.
 */
static int s3c2410_probe(struct uart_bas *bas);
static void s3c2410_init(struct uart_bas *bas, int, int, int, int);
static void s3c2410_term(struct uart_bas *bas);
static void s3c2410_putc(struct uart_bas *bas, int);
static int s3c2410_rxready(struct uart_bas *bas);
static int s3c2410_getc(struct uart_bas *bas, struct mtx *mtx);

extern SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs;

static int
sscomspeed(long speed, long frequency)
{
	int x;

	if (speed <= 0 || frequency <= 0)
		return (-1);
	x = (frequency / 16) / speed;
	return (x-1);
}

static int
s3c24x0_uart_param(struct uart_bas *bas, int baudrate, int databits,
    int stopbits, int parity)
{
	int brd, ulcon;

	ulcon = 0;

	switch(databits) {
	case 5:
		ulcon |= ULCON_LENGTH_5;
		break;
	case 6:
		ulcon |= ULCON_LENGTH_6;
		break;
	case 7:
		ulcon |= ULCON_LENGTH_7;
		break;
	case 8:
		ulcon |= ULCON_LENGTH_8;
		break;
	default:
		return (EINVAL);
	}

	switch (parity) {
	case UART_PARITY_NONE:
		ulcon |= ULCON_PARITY_NONE;
		break;
	case UART_PARITY_ODD:
		ulcon |= ULCON_PARITY_ODD;
		break;
	case UART_PARITY_EVEN:
		ulcon |= ULCON_PARITY_EVEN;
		break;
	case UART_PARITY_MARK:
	case UART_PARITY_SPACE:
	default:
		return (EINVAL);
	}

	if (stopbits == 2)
		ulcon |= ULCON_STOP;

	uart_setreg(bas, SSCOM_ULCON, ulcon);

	brd = sscomspeed(baudrate, bas->rclk);
	uart_setreg(bas, SSCOM_UBRDIV, brd);

	return (0);
}

struct uart_ops uart_s3c2410_ops = {
	.probe = s3c2410_probe,
	.init = s3c2410_init,
	.term = s3c2410_term,
	.putc = s3c2410_putc,
	.rxready = s3c2410_rxready,
	.getc = s3c2410_getc,
};

static int
s3c2410_probe(struct uart_bas *bas)
{

	return (0);
}

static void
s3c2410_init(struct uart_bas *bas, int baudrate, int databits, int stopbits,
    int parity)
{

	if (bas->rclk == 0)
		bas->rclk = DEF_CLK;

	KASSERT(bas->rclk != 0, ("s3c2410_init: Invalid rclk"));

	uart_setreg(bas, SSCOM_UCON, 0);
	uart_setreg(bas, SSCOM_UFCON,
	    UFCON_TXTRIGGER_8 | UFCON_RXTRIGGER_8 |
	    UFCON_TXFIFO_RESET | UFCON_RXFIFO_RESET |
	    UFCON_FIFO_ENABLE);
	s3c24x0_uart_param(bas, baudrate, databits, stopbits, parity);

	/* Enable UART. */
	uart_setreg(bas, SSCOM_UCON, UCON_TXMODE_INT | UCON_RXMODE_INT |
	    UCON_TOINT);
	uart_setreg(bas, SSCOM_UMCON, UMCON_RTS);
}

static void
s3c2410_term(struct uart_bas *bas)
{
	/* XXX */
}

static void
s3c2410_putc(struct uart_bas *bas, int c)
{

	while ((bus_space_read_4(bas->bst, bas->bsh, SSCOM_UFSTAT) &
		UFSTAT_TXFULL) == UFSTAT_TXFULL)
		continue;

	uart_setreg(bas, SSCOM_UTXH, c);
}

static int
s3c2410_rxready(struct uart_bas *bas)
{

	return ((uart_getreg(bas, SSCOM_UTRSTAT) & UTRSTAT_RXREADY) ==
	    UTRSTAT_RXREADY);
}

static int
s3c2410_getc(struct uart_bas *bas, struct mtx *mtx)
{
	int utrstat;

	utrstat = bus_space_read_1(bas->bst, bas->bsh, SSCOM_UTRSTAT);
	while (!(utrstat & UTRSTAT_RXREADY)) {
		utrstat = bus_space_read_1(bas->bst, bas->bsh, SSCOM_UTRSTAT);
		continue;
	}

	return (bus_space_read_1(bas->bst, bas->bsh, SSCOM_URXH));
}

static int s3c2410_bus_probe(struct uart_softc *sc);
static int s3c2410_bus_attach(struct uart_softc *sc);
static int s3c2410_bus_flush(struct uart_softc *, int);
static int s3c2410_bus_getsig(struct uart_softc *);
static int s3c2410_bus_ioctl(struct uart_softc *, int, intptr_t);
static int s3c2410_bus_ipend(struct uart_softc *);
static int s3c2410_bus_param(struct uart_softc *, int, int, int, int);
static int s3c2410_bus_receive(struct uart_softc *);
static int s3c2410_bus_setsig(struct uart_softc *, int);
static int s3c2410_bus_transmit(struct uart_softc *);

static kobj_method_t s3c2410_methods[] = {
	KOBJMETHOD(uart_probe,		s3c2410_bus_probe),
	KOBJMETHOD(uart_attach, 	s3c2410_bus_attach),
	KOBJMETHOD(uart_flush,		s3c2410_bus_flush),
	KOBJMETHOD(uart_getsig,		s3c2410_bus_getsig),
	KOBJMETHOD(uart_ioctl,		s3c2410_bus_ioctl),
	KOBJMETHOD(uart_ipend,		s3c2410_bus_ipend),
	KOBJMETHOD(uart_param,		s3c2410_bus_param),
	KOBJMETHOD(uart_receive,	s3c2410_bus_receive),
	KOBJMETHOD(uart_setsig,		s3c2410_bus_setsig),
	KOBJMETHOD(uart_transmit,	s3c2410_bus_transmit),

	{0, 0 }
};

int
s3c2410_bus_probe(struct uart_softc *sc)
{

	sc->sc_txfifosz = 16;
	sc->sc_rxfifosz = 16;

	return (0);
}

static int
s3c2410_bus_attach(struct uart_softc *sc)
{

	sc->sc_hwiflow = 0;
	sc->sc_hwoflow = 0;

	return (0);
}

static int
s3c2410_bus_transmit(struct uart_softc *sc)
{
	int i;
	int reg;

	uart_lock(sc->sc_hwmtx);

	for (i = 0; i < sc->sc_txdatasz; i++) {
		s3c2410_putc(&sc->sc_bas, sc->sc_txbuf[i]);
		uart_barrier(&sc->sc_bas);
	}

	sc->sc_txbusy = 1;

	uart_unlock(sc->sc_hwmtx);

	/* unmask TX interrupt */
	reg = bus_space_read_4(sc->sc_bas.bst, sc->sc_bas.bsh, SSCOM_UINTM);
	reg &= ~(1 << 2);
	bus_space_write_4(sc->sc_bas.bst, sc->sc_bas.bsh, SSCOM_UINTM, reg);

	return (0);
}

static int
s3c2410_bus_setsig(struct uart_softc *sc, int sig)
{

	return (0);
}

static int
s3c2410_bus_receive(struct uart_softc *sc)
{

	uart_rx_put(sc, uart_getreg(&sc->sc_bas, SSCOM_URXH));
	return (0);
}

static int
s3c2410_bus_param(struct uart_softc *sc, int baudrate, int databits,
    int stopbits, int parity)
{
	int error;

	if (sc->sc_bas.rclk == 0)
		sc->sc_bas.rclk = DEF_CLK;

	KASSERT(sc->sc_bas.rclk != 0, ("s3c2410_init: Invalid rclk"));

	uart_lock(sc->sc_hwmtx);
	error = s3c24x0_uart_param(&sc->sc_bas, baudrate, databits, stopbits,
	    parity);
	uart_unlock(sc->sc_hwmtx);

	return (error);
}

static int
s3c2410_bus_ipend(struct uart_softc *sc)
{
	uint32_t ints;
	uint32_t txempty, rxready;
	int reg;
	int ipend;

	uart_lock(sc->sc_hwmtx);
	ints = bus_space_read_4(sc->sc_bas.bst, sc->sc_bas.bsh, SSCOM_UINTP);
	bus_space_write_4(sc->sc_bas.bst, sc->sc_bas.bsh, SSCOM_UINTP, ints);

	txempty = (1 << 2);
	rxready = (1 << 0);

	ipend = 0;
	if ((ints & txempty) > 0) {
		if (sc->sc_txbusy != 0)
			ipend |= SER_INT_TXIDLE;

		/* mask TX interrupt */
		reg = bus_space_read_4(sc->sc_bas.bst, sc->sc_bas.bsh,
		    SSCOM_UINTM);
		reg |= (1 << 2);
		bus_space_write_4(sc->sc_bas.bst, sc->sc_bas.bsh,
		    SSCOM_UINTM, reg);
	}

	if ((ints & rxready) > 0) {
		ipend |= SER_INT_RXREADY;
	}

	uart_unlock(sc->sc_hwmtx);
	return (ipend);
}

static int
s3c2410_bus_flush(struct uart_softc *sc, int what)
{

	return (0);
}

static int
s3c2410_bus_getsig(struct uart_softc *sc)
{

	return (0);
}

static int
s3c2410_bus_ioctl(struct uart_softc *sc, int request, intptr_t data)
{

	return (EINVAL);
}

struct uart_class uart_s3c2410_class = {
	"s3c2410 class",
	s3c2410_methods,
	1,
	.uc_ops = &uart_s3c2410_ops,
	.uc_range = 8,
	.uc_rclk = 0,
};
