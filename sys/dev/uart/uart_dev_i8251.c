/*
 * Copyright (c) 2003 Marcel Moolenaar
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
#include <machine/bus.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_dev_i8251.h>

#include "uart_if.h"

#define	DEFAULT_RCLK	1843200

/*
 * Clear pending interrupts. THRE is cleared by reading IIR. Data
 * that may have been received gets lost here.
 */
static void
i8251_clrint(struct uart_bas *bas)
{
	uint8_t iir;

	iir = uart_getreg(bas, REG_IIR);
	while ((iir & IIR_NOPEND) == 0) {
		iir &= IIR_IMASK;
		if (iir == IIR_RLS)
			(void)uart_getreg(bas, REG_LSR);
		else if (iir == IIR_RXRDY || iir == IIR_RXTOUT)
			(void)uart_getreg(bas, REG_DATA);
		else if (iir == IIR_MLSC)
			(void)uart_getreg(bas, REG_MSR);
		uart_barrier(bas);
		iir = uart_getreg(bas, REG_IIR);
	}
}

static int
i8251_delay(struct uart_bas *bas)
{
	int divisor;
	u_char lcr;

	lcr = uart_getreg(bas, REG_LCR);
	uart_setreg(bas, REG_LCR, lcr | LCR_DLAB);
	uart_barrier(bas);
	divisor = uart_getdreg(bas, REG_DL);
	uart_barrier(bas);
	uart_setreg(bas, REG_LCR, lcr);
	uart_barrier(bas);

	/* 1/10th the time to transmit 1 character (estimate). */
	return (16000000 * divisor / bas->rclk);
}

static int
i8251_divisor(int rclk, int baudrate)
{
	int actual_baud, divisor;
	int error;

	if (baudrate == 0)
		return (0);

	divisor = (rclk / (baudrate << 3) + 1) >> 1;
	if (divisor == 0 || divisor >= 65536)
		return (0);
	actual_baud = rclk / (divisor << 4);

	/* 10 times error in percent: */
	error = ((actual_baud - baudrate) * 2000 / baudrate + 1) >> 1;

	/* 3.0% maximum error tolerance: */
	if (error < -30 || error > 30)
		return (0);

	return (divisor);
}

static int
i8251_drain(struct uart_bas *bas, int what)
{
	int delay, limit;

	delay = i8251_delay(bas);

	if (what & UART_DRAIN_TRANSMITTER) {
		/*
		 * Pick an arbitrary high limit to avoid getting stuck in
		 * an infinite loop when the hardware is broken. Make the
		 * limit high enough to handle large FIFOs.
		 */
		limit = 10*1024;
		while ((uart_getreg(bas, REG_LSR) & LSR_TEMT) == 0 && --limit)
			DELAY(delay);
		if (limit == 0) {
			/* printf("i8251: transmitter appears stuck... "); */
			return (EIO);
		}
	}

	if (what & UART_DRAIN_RECEIVER) {
		/*
		 * Pick an arbitrary high limit to avoid getting stuck in
		 * an infinite loop when the hardware is broken. Make the
		 * limit high enough to handle large FIFOs and integrated
		 * UARTs. The HP rx2600 for example has 3 UARTs on the
		 * management board that tend to get a lot of data send
		 * to it when the UART is first activated.
		 */
		limit=10*4096;
		while ((uart_getreg(bas, REG_LSR) & LSR_RXRDY) && --limit) {
			(void)uart_getreg(bas, REG_DATA);
			uart_barrier(bas);
			DELAY(delay << 2);
		}
		if (limit == 0) {
			/* printf("i8251: receiver appears broken... "); */
			return (EIO);
		}
	}

	return (0);
}

/*
 * We can only flush UARTs with FIFOs. UARTs without FIFOs should be
 * drained. WARNING: this function clobbers the FIFO setting!
 */
static void
i8251_flush(struct uart_bas *bas, int what)
{
	uint8_t fcr;

	fcr = FCR_ENABLE;
	if (what & UART_FLUSH_TRANSMITTER)
		fcr |= FCR_XMT_RST;
	if (what & UART_FLUSH_RECEIVER)
		fcr |= FCR_RCV_RST;
	uart_setreg(bas, REG_FCR, fcr);
	uart_barrier(bas);
}

static int
i8251_param(struct uart_bas *bas, int baudrate, int databits, int stopbits,
    int parity)
{
	int divisor;
	uint8_t lcr;

	lcr = 0;
	if (databits >= 8)
		lcr |= LCR_8BITS;
	else if (databits == 7)
		lcr |= LCR_7BITS;
	else if (databits == 6)
		lcr |= LCR_6BITS;
	else
		lcr |= LCR_5BITS;
	if (stopbits > 1)
		lcr |= LCR_STOPB;
	lcr |= parity << 3;

	/* Set baudrate. */
	if (baudrate > 0) {
		uart_setreg(bas, REG_LCR, lcr | LCR_DLAB);
		uart_barrier(bas);
		divisor = i8251_divisor(bas->rclk, baudrate);
		if (divisor == 0)
			return (EINVAL);
		uart_setdreg(bas, REG_DL, divisor);
		uart_barrier(bas);
	}

	/* Set LCR and clear DLAB. */
	uart_setreg(bas, REG_LCR, lcr);
	uart_barrier(bas);
	return (0);
}

/*
 * Low-level UART interface.
 */
static int i8251_probe(struct uart_bas *bas);
static void i8251_init(struct uart_bas *bas, int, int, int, int);
static void i8251_term(struct uart_bas *bas);
static void i8251_putc(struct uart_bas *bas, int);
static int i8251_poll(struct uart_bas *bas);
static int i8251_getc(struct uart_bas *bas);

struct uart_ops uart_i8251_ops = {
	.probe = i8251_probe,
	.init = i8251_init,
	.term = i8251_term,
	.putc = i8251_putc,
	.poll = i8251_poll,
	.getc = i8251_getc,
};

static int
i8251_probe(struct uart_bas *bas)
{
	u_char lcr, val;

	/* Check known 0 bits that don't depend on DLAB. */
	val = uart_getreg(bas, REG_IIR);
	if (val & 0x30)
		return (ENXIO);
	val = uart_getreg(bas, REG_MCR);
	if (val & 0xe0)
		return (ENXIO);

	lcr = uart_getreg(bas, REG_LCR);
	uart_setreg(bas, REG_LCR, lcr & ~LCR_DLAB);
	uart_barrier(bas);

	/* Check known 0 bits that depend on !DLAB. */
	val = uart_getreg(bas, REG_IER);
	if (val & 0xf0)
		goto fail;

	uart_setreg(bas, REG_LCR, lcr);
	uart_barrier(bas);
	return (0);

 fail:
	uart_setreg(bas, REG_LCR, lcr);
	uart_barrier(bas);
	return (ENXIO);
}

static void
i8251_init(struct uart_bas *bas, int baudrate, int databits, int stopbits,
    int parity)
{

	if (bas->rclk == 0)
		bas->rclk = DEFAULT_RCLK;
	i8251_param(bas, baudrate, databits, stopbits, parity);

	/* Disable all interrupt sources. */
	uart_setreg(bas, REG_IER, 0);
	uart_barrier(bas);

	/* Disable the FIFO (if present). */
	uart_setreg(bas, REG_FCR, 0);
	uart_barrier(bas);

	/* Set RTS & DTR. */
	uart_setreg(bas, REG_MCR, MCR_IE | MCR_RTS | MCR_DTR);
	uart_barrier(bas);

	i8251_clrint(bas);
}

static void
i8251_term(struct uart_bas *bas)
{

	/* Clear RTS & DTR. */
	uart_setreg(bas, REG_MCR, MCR_IE);
	uart_barrier(bas);
}

static void
i8251_putc(struct uart_bas *bas, int c)
{
	int delay, limit;

	/* 1/10th the time to transmit 1 character (estimate). */
	delay = i8251_delay(bas);

	limit = 20;
	while ((uart_getreg(bas, REG_LSR) & LSR_THRE) == 0 && --limit)
		DELAY(delay);
	uart_setreg(bas, REG_DATA, c);
	limit = 40;
	while ((uart_getreg(bas, REG_LSR) & LSR_TEMT) == 0 && --limit)
		DELAY(delay);
}

static int
i8251_poll(struct uart_bas *bas)
{

	if (uart_getreg(bas, REG_LSR) & LSR_RXRDY)
		return (uart_getreg(bas, REG_DATA));
	return (-1);
}

static int
i8251_getc(struct uart_bas *bas)
{
	int delay;

	/* 1/10th the time to transmit 1 character (estimate). */
	delay = i8251_delay(bas);

	while ((uart_getreg(bas, REG_LSR) & LSR_RXRDY) == 0)
		DELAY(delay);
	return (uart_getreg(bas, REG_DATA));
}

/*
 * High-level UART interface.
 */
struct i8251_softc {
	struct uart_softc base;
	uint8_t		fcr;
	uint8_t		ier;
	uint8_t		mcr;
};

static int i8251_bus_attach(struct uart_softc *);
static int i8251_bus_detach(struct uart_softc *);
static int i8251_bus_flush(struct uart_softc *, int);
static int i8251_bus_getsig(struct uart_softc *);
static int i8251_bus_ioctl(struct uart_softc *, int, intptr_t);
static int i8251_bus_ipend(struct uart_softc *);
static int i8251_bus_param(struct uart_softc *, int, int, int, int);
static int i8251_bus_probe(struct uart_softc *);
static int i8251_bus_receive(struct uart_softc *);
static int i8251_bus_setsig(struct uart_softc *, int);
static int i8251_bus_transmit(struct uart_softc *);

static kobj_method_t i8251_methods[] = {
	KOBJMETHOD(uart_attach,		i8251_bus_attach),
	KOBJMETHOD(uart_detach,		i8251_bus_detach),
	KOBJMETHOD(uart_flush,		i8251_bus_flush),
	KOBJMETHOD(uart_getsig,		i8251_bus_getsig),
	KOBJMETHOD(uart_ioctl,		i8251_bus_ioctl),
	KOBJMETHOD(uart_ipend,		i8251_bus_ipend),
	KOBJMETHOD(uart_param,		i8251_bus_param),
	KOBJMETHOD(uart_probe,		i8251_bus_probe),
	KOBJMETHOD(uart_receive,	i8251_bus_receive),
	KOBJMETHOD(uart_setsig,		i8251_bus_setsig),
	KOBJMETHOD(uart_transmit,	i8251_bus_transmit),
	{ 0, 0 }
};

struct uart_class uart_i8251_class = {
	"i8251 class",
	i8251_methods,
	sizeof(struct i8251_softc),
	.uc_range = 8,
	.uc_rclk = DEFAULT_RCLK
};

#define	SIGCHG(c, i, s, d)				\
	if (c) {					\
		i |= (i & s) ? s : s | d;		\
	} else {					\
		i = (i & s) ? (i & ~s) | d : i;		\
	}

static int
i8251_bus_attach(struct uart_softc *sc)
{
	struct i8251_softc *i8251 = (struct i8251_softc*)sc;
	struct uart_bas *bas;

	bas = &sc->sc_bas;

	i8251->mcr = uart_getreg(bas, REG_MCR);
	i8251->fcr = FCR_ENABLE | FCR_RX_MEDH;
	uart_setreg(bas, REG_FCR, i8251->fcr);
	uart_barrier(bas);
	i8251_bus_flush(sc, UART_FLUSH_RECEIVER|UART_FLUSH_TRANSMITTER);

	if (i8251->mcr & MCR_DTR)
		sc->sc_hwsig |= SER_DTR;
	if (i8251->mcr & MCR_RTS)
		sc->sc_hwsig |= SER_RTS;
	i8251_bus_getsig(sc);

	i8251_clrint(bas);
	i8251->ier = IER_EMSC | IER_ERLS | IER_ERXRDY;
	uart_setreg(bas, REG_IER, i8251->ier);
	uart_barrier(bas);
	return (0);
}

static int
i8251_bus_detach(struct uart_softc *sc)
{
	struct uart_bas *bas;

	bas = &sc->sc_bas;
	uart_setreg(bas, REG_IER, 0);
	uart_barrier(bas);
	i8251_clrint(bas);
	return (0);
}

static int
i8251_bus_flush(struct uart_softc *sc, int what)
{
	struct i8251_softc *i8251 = (struct i8251_softc*)sc;
	struct uart_bas *bas;
	int error;

	bas = &sc->sc_bas;
	mtx_lock_spin(&sc->sc_hwmtx);
	if (sc->sc_hasfifo) {
		i8251_flush(bas, what);
		uart_setreg(bas, REG_FCR, i8251->fcr);
		uart_barrier(bas);
		error = 0;
	} else
		error = i8251_drain(bas, what);
	mtx_unlock_spin(&sc->sc_hwmtx);
	return (error);
}

static int
i8251_bus_getsig(struct uart_softc *sc)
{
	uint32_t new, old, sig;
	uint8_t msr;

	do {
		old = sc->sc_hwsig;
		sig = old;
		mtx_lock_spin(&sc->sc_hwmtx);
		msr = uart_getreg(&sc->sc_bas, REG_MSR);
		mtx_unlock_spin(&sc->sc_hwmtx);
		SIGCHG(msr & MSR_DSR, sig, SER_DSR, SER_DDSR);
		SIGCHG(msr & MSR_CTS, sig, SER_CTS, SER_DCTS);
		SIGCHG(msr & MSR_DCD, sig, SER_DCD, SER_DDCD);
		SIGCHG(msr & MSR_RI,  sig, SER_RI,  SER_DRI);
		new = sig & ~UART_SIGMASK_DELTA;
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));
	return (sig);
}

static int
i8251_bus_ioctl(struct uart_softc *sc, int request, intptr_t data)
{
	struct uart_bas *bas;
	int error;
	uint8_t lcr;

	bas = &sc->sc_bas;
	error = 0;
	mtx_lock_spin(&sc->sc_hwmtx);
	switch (request) {
	case UART_IOCTL_BREAK:
		lcr = uart_getreg(bas, REG_LCR);
		if (data)
			lcr |= LCR_SBREAK;
		else
			lcr &= ~LCR_SBREAK;
		uart_setreg(bas, REG_LCR, lcr);
		uart_barrier(bas);
		break;
	default:
		error = EINVAL;
		break;
	}
	mtx_unlock_spin(&sc->sc_hwmtx);
	return (error);
}

static int
i8251_bus_ipend(struct uart_softc *sc)
{
	struct uart_bas *bas;
	int ipend;
	uint8_t iir, lsr;

	bas = &sc->sc_bas;
	mtx_lock_spin(&sc->sc_hwmtx);
	iir = uart_getreg(bas, REG_IIR);
	if (iir & IIR_NOPEND) {
		mtx_unlock_spin(&sc->sc_hwmtx);
		return (0);
	}
	ipend = 0;
	if (iir & IIR_RXRDY) {
		lsr = uart_getreg(bas, REG_LSR);
		mtx_unlock_spin(&sc->sc_hwmtx);
		if (lsr & LSR_OE)
			ipend |= UART_IPEND_OVERRUN;
		if (lsr & LSR_BI)
			ipend |= UART_IPEND_BREAK;
		if (lsr & LSR_RXRDY)
			ipend |= UART_IPEND_RXREADY;
	} else {
		mtx_unlock_spin(&sc->sc_hwmtx);
		if (iir & IIR_TXRDY)
			ipend |= UART_IPEND_TXIDLE;
		else
			ipend |= UART_IPEND_SIGCHG;
	}
	return ((sc->sc_leaving) ? 0 : ipend);
}

static int
i8251_bus_param(struct uart_softc *sc, int baudrate, int databits,
    int stopbits, int parity)
{
	struct uart_bas *bas;
	int error;

	bas = &sc->sc_bas;
	mtx_lock_spin(&sc->sc_hwmtx);
	error = i8251_param(bas, baudrate, databits, stopbits, parity);
	mtx_unlock_spin(&sc->sc_hwmtx);
	return (error);
}

static int
i8251_bus_probe(struct uart_softc *sc)
{
	struct uart_bas *bas;
	int count, delay, error, limit;
	uint8_t mcr;

	bas = &sc->sc_bas;

	error = i8251_probe(bas);
	if (error)
		return (error);

	mcr = MCR_IE;
	if (sc->sc_sysdev == NULL) {
		/* By using i8251_init() we also set DTR and RTS. */
		i8251_init(bas, 9600, 8, 1, UART_PARITY_NONE);
	} else
		mcr |= MCR_DTR | MCR_RTS;

	error = i8251_drain(bas, UART_DRAIN_TRANSMITTER);
	if (error)
		return (error);

	/*
	 * Set loopback mode. This avoids having garbage on the wire and
	 * also allows us send and receive data. We set DTR and RTS to
	 * avoid the possibility that automatic flow-control prevents
	 * any data from being sent. We clear IE to avoid raising interrupts.
	 */
	uart_setreg(bas, REG_MCR, MCR_LOOPBACK | MCR_DTR | MCR_RTS);
	uart_barrier(bas);

	/*
	 * Enable FIFOs. And check that the UART has them. If not, we're
	 * done. Otherwise we set DMA mode with the highest trigger level
	 * so that we can determine the FIFO size. Since this is the first
	 * time we enable the FIFOs, we reset them.
	 */
	uart_setreg(bas, REG_FCR, FCR_ENABLE);
	uart_barrier(bas);
	sc->sc_hasfifo = (uart_getreg(bas, REG_IIR) & IIR_FIFO_MASK) ? 1 : 0;
	if (!sc->sc_hasfifo) {
		/*
		 * NS16450 or II8251. We don't bother to differentiate
		 * between them. They're too old to be interesting.
		 */
		uart_setreg(bas, REG_MCR, mcr);
		uart_barrier(bas);
		device_set_desc(sc->sc_dev, "8250 or 16450 or compatible");
		return (0);
	}

	uart_setreg(bas, REG_FCR, FCR_ENABLE | FCR_DMA | FCR_RX_HIGH |
	    FCR_XMT_RST | FCR_RCV_RST);
	uart_barrier(bas);

	count = 0;
	delay = i8251_delay(bas);

	/* We have FIFOs. Drain the transmitter and receiver. */
	error = i8251_drain(bas, UART_DRAIN_RECEIVER|UART_DRAIN_TRANSMITTER);
	if (error) {
		uart_setreg(bas, REG_MCR, mcr);
		uart_setreg(bas, REG_FCR, 0);
		uart_barrier(bas);
		goto describe;
	}

	uart_setreg(bas, REG_IER, IER_ERXRDY);
	uart_barrier(bas);

	/*
	 * We should have a sufficiently clean "pipe" to determine the
	 * size of the FIFOs. We send as much characters as is reasonable
	 * and wait for the the RX interrupt to be asserted, counting the
	 * characters as we send them. Based on that count we know the
	 * FIFO size.
	 */
	while ((uart_getreg(bas, REG_IIR) & IIR_RXRDY) == 0 && count < 1030) {
		uart_setreg(bas, REG_DATA, 0);
		uart_barrier(bas);
		count++;

		limit = 30;
		while ((uart_getreg(bas, REG_LSR) & LSR_TEMT) == 0 && --limit)
			DELAY(delay);
		if (limit == 0) {
			uart_setreg(bas, REG_IER, 0);
			uart_setreg(bas, REG_MCR, mcr);
			uart_setreg(bas, REG_FCR, 0);
			uart_barrier(bas);
			count = 0;
			goto describe;
		}
	}

	uart_setreg(bas, REG_IER, 0);
	uart_setreg(bas, REG_MCR, mcr);

	/* Reset FIFOs. */
	i8251_flush(bas, UART_FLUSH_RECEIVER|UART_FLUSH_TRANSMITTER);

 describe:
	if (count >= 14 && count < 16) {
		sc->sc_rxfifosz = 16;
		device_set_desc(sc->sc_dev, "16550 or compatible");
	} else if (count >= 28 && count < 32) {
		sc->sc_rxfifosz = 32;
		device_set_desc(sc->sc_dev, "16650 or compatible");
	} else if (count >= 56 && count < 64) {
		sc->sc_rxfifosz = 64;
		device_set_desc(sc->sc_dev, "16750 or compatible");
	} else if (count >= 112 && count < 128) {
		sc->sc_rxfifosz = 128;
		device_set_desc(sc->sc_dev, "16950 or compatible");
	} else {
		sc->sc_rxfifosz = 1;
		device_set_desc(sc->sc_dev,
		    "Non-standard i8251 class UART with FIFOs");
	}

	/*
	 * Force the Tx FIFO size to 16 bytes for now. We don't program the
	 * Tx trigger. Also, we assume that all data has been sent when the
	 * interrupt happens.
	 */
	sc->sc_txfifosz = 16;

	return (0);
}

static int
i8251_bus_receive(struct uart_softc *sc)
{
	struct uart_bas *bas;
	int xc;
	uint8_t lsr;

	bas = &sc->sc_bas;
	mtx_lock_spin(&sc->sc_hwmtx);
	lsr = uart_getreg(bas, REG_LSR);
	while (lsr & LSR_RXRDY) {
		if (uart_rx_full(sc)) {
			sc->sc_rxbuf[sc->sc_rxput] = UART_STAT_OVERRUN;
			break;
		}
		xc = uart_getreg(bas, REG_DATA);
		if (lsr & LSR_FE)
			xc |= UART_STAT_FRAMERR;
		if (lsr & LSR_PE)
			xc |= UART_STAT_PARERR;
		uart_rx_put(sc, xc);
		lsr = uart_getreg(bas, REG_LSR);
	}
	/* Discard everything left in the Rx FIFO. */
	while (lsr & LSR_RXRDY) {
		(void)uart_getreg(bas, REG_DATA);
		uart_barrier(bas);
		lsr = uart_getreg(bas, REG_LSR);
	}
	mtx_unlock_spin(&sc->sc_hwmtx);
 	return (0);
}

static int
i8251_bus_setsig(struct uart_softc *sc, int sig)
{
	struct i8251_softc *i8251 = (struct i8251_softc*)sc;
	struct uart_bas *bas;
	uint32_t new, old;

	bas = &sc->sc_bas;
	do {
		old = sc->sc_hwsig;
		new = old;
		if (sig & SER_DDTR) {
			SIGCHG(sig & SER_DTR, new, SER_DTR,
			    SER_DDTR);
		}
		if (sig & SER_DRTS) {
			SIGCHG(sig & SER_RTS, new, SER_RTS,
			    SER_DRTS);
		}
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));
	mtx_lock_spin(&sc->sc_hwmtx);
	i8251->mcr &= ~(MCR_DTR|MCR_RTS);
	if (new & SER_DTR)
		i8251->mcr |= MCR_DTR;
	if (new & SER_RTS)
		i8251->mcr |= MCR_RTS;
	uart_setreg(bas, REG_MCR, i8251->mcr);
	uart_barrier(bas);
	mtx_unlock_spin(&sc->sc_hwmtx);
	return (0);
}

static int
i8251_bus_transmit(struct uart_softc *sc)
{
	struct i8251_softc *i8251 = (struct i8251_softc*)sc;
	struct uart_bas *bas;
	int i;

	bas = &sc->sc_bas;
	mtx_lock_spin(&sc->sc_hwmtx);
	while ((uart_getreg(bas, REG_LSR) & LSR_THRE) == 0)
		;
	uart_setreg(bas, REG_IER, i8251->ier | IER_ETXRDY);
	uart_barrier(bas);
	for (i = 0; i < sc->sc_txdatasz; i++) {
		uart_setreg(bas, REG_DATA, sc->sc_txbuf[i]);
		uart_barrier(bas);
	}
	sc->sc_txbusy = 1;
	mtx_unlock_spin(&sc->sc_hwmtx);
	return (0);
}
