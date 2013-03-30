/*-
 * Copyright (c) 2013 Adrian Chadd <adrian@FreeBSD.org>
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

#include <mips/atheros/ar933x_uart.h>

#include "uart_if.h"

/*
 * Default system clock is 25MHz; see ar933x_chip.c for how
 * the startup process determines whether it's 25MHz or 40MHz.
 */
#define	DEFAULT_RCLK	(25 * 1000 * 1000)

#define	ar933x_getreg(bas, reg)           \
	bus_space_read_4((bas)->bst, (bas)->bsh, reg)
#define	ar933x_setreg(bas, reg, value)    \
	bus_space_write_4((bas)->bst, (bas)->bsh, reg, value)


#if 0
/*
 * Clear pending interrupts. THRE is cleared by reading IIR. Data
 * that may have been received gets lost here.
 */
static void
ar933x_clrint(struct uart_bas *bas)
{
	uint8_t iir, lsr;

	iir = uart_getreg(bas, REG_IIR);
	while ((iir & IIR_NOPEND) == 0) {
		iir &= IIR_IMASK;
		if (iir == IIR_RLS) {
			lsr = uart_getreg(bas, REG_LSR);
			if (lsr & (LSR_BI|LSR_FE|LSR_PE))
				(void)uart_getreg(bas, REG_DATA);
		} else if (iir == IIR_RXRDY || iir == IIR_RXTOUT)
			(void)uart_getreg(bas, REG_DATA);
		else if (iir == IIR_MLSC)
			(void)uart_getreg(bas, REG_MSR);
		uart_barrier(bas);
		iir = uart_getreg(bas, REG_IIR);
	}
}
#endif

#if 0
static int
ar933x_drain(struct uart_bas *bas, int what)
{
	int delay, limit;

	delay = ar933x_delay(bas);

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
			/* printf("ns8250: transmitter appears stuck... "); */
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
			/* printf("ns8250: receiver appears broken... "); */
			return (EIO);
		}
	}
	return (0);
}
#endif

#if 0
/*
 * We can only flush UARTs with FIFOs. UARTs without FIFOs should be
 * drained. WARNING: this function clobbers the FIFO setting!
 */
static void
ar933x_flush(struct uart_bas *bas, int what)
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
#endif

#if 0
static int
ar933x_param(struct uart_bas *bas, int baudrate, int databits, int stopbits,
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
		divisor = ar933x_divisor(bas->rclk, baudrate);
		if (divisor == 0)
			return (EINVAL);
		uart_setreg(bas, REG_LCR, lcr | LCR_DLAB);
		uart_barrier(bas);
		uart_setreg(bas, REG_DLL, divisor & 0xff);
		uart_setreg(bas, REG_DLH, (divisor >> 8) & 0xff);
		uart_barrier(bas);
	}

	/* Set LCR and clear DLAB. */
	uart_setreg(bas, REG_LCR, lcr);
	uart_barrier(bas);
	return (0);
}
#endif

/*
 * Low-level UART interface.
 */
static int ar933x_probe(struct uart_bas *bas);
static void ar933x_init(struct uart_bas *bas, int, int, int, int);
static void ar933x_term(struct uart_bas *bas);
static void ar933x_putc(struct uart_bas *bas, int);
static int ar933x_rxready(struct uart_bas *bas);
static int ar933x_getc(struct uart_bas *bas, struct mtx *);

static struct uart_ops uart_ar933x_ops = {
	.probe = ar933x_probe,
	.init = ar933x_init,
	.term = ar933x_term,
	.putc = ar933x_putc,
	.rxready = ar933x_rxready,
	.getc = ar933x_getc,
};

static int
ar933x_probe(struct uart_bas *bas)
{
#if 0
	u_char val;

	/* Check known 0 bits that don't depend on DLAB. */
	val = uart_getreg(bas, REG_IIR);
	if (val & 0x30)
		return (ENXIO);
	/*
	 * Bit 6 of the MCR (= 0x40) appears to be 1 for the Sun1699
	 * chip, but otherwise doesn't seem to have a function. In
	 * other words, uart(4) works regardless. Ignore that bit so
	 * the probe succeeds.
	 */
	val = uart_getreg(bas, REG_MCR);
	if (val & 0xa0)
		return (ENXIO);
#endif
	return (0);
}

static void
ar933x_init(struct uart_bas *bas, int baudrate, int databits, int stopbits,
    int parity)
{
#if 0
	u_char	ier;

	if (bas->rclk == 0)
		bas->rclk = DEFAULT_RCLK;
	ar933x_param(bas, baudrate, databits, stopbits, parity);

	/* Disable all interrupt sources. */
	/*
	 * We use 0xe0 instead of 0xf0 as the mask because the XScale PXA
	 * UARTs split the receive time-out interrupt bit out separately as
	 * 0x10.  This gets handled by ier_mask and ier_rxbits below.
	 */
	ier = uart_getreg(bas, REG_IER) & 0xe0;
	uart_setreg(bas, REG_IER, ier);
	uart_barrier(bas);

	/* Disable the FIFO (if present). */
	uart_setreg(bas, REG_FCR, 0);
	uart_barrier(bas);

	/* Set RTS & DTR. */
	uart_setreg(bas, REG_MCR, MCR_IE | MCR_RTS | MCR_DTR);
	uart_barrier(bas);

	ar933x_clrint(bas);
#endif
}

static void
ar933x_term(struct uart_bas *bas)
{
#if 0
	/* Clear RTS & DTR. */
	uart_setreg(bas, REG_MCR, MCR_IE);
	uart_barrier(bas);
#endif
}

static void
ar933x_putc(struct uart_bas *bas, int c)
{
	int limit;

	limit = 250000;

	/* Wait for space in the TX FIFO */
	while ( ((ar933x_getreg(bas, AR933X_UART_DATA_REG) &
	    AR933X_UART_DATA_TX_CSR) == 0) && --limit)
		DELAY(4);

	/* Write the actual byte */
	ar933x_setreg(bas, AR933X_UART_DATA_REG,
	    (c & 0xff) | AR933X_UART_DATA_TX_CSR);
}

static int
ar933x_rxready(struct uart_bas *bas)
{

	/* Wait for a character to come ready */
	return (!!(ar933x_getreg(bas, AR933X_UART_DATA_REG)
	    & AR933X_UART_DATA_RX_CSR));
}

static int
ar933x_getc(struct uart_bas *bas, struct mtx *hwmtx)
{
	int c;

	uart_lock(hwmtx);

	/* Wait for a character to come ready */
	while ((ar933x_getreg(bas, AR933X_UART_DATA_REG) &
	    AR933X_UART_DATA_RX_CSR) == 0) {
		uart_unlock(hwmtx);
		DELAY(4);
		uart_lock(hwmtx);
	}

	/* Read the top of the RX FIFO */
	c = ar933x_getreg(bas, AR933X_UART_DATA_REG) & 0xff;

	/* Remove that entry from said RX FIFO */
	ar933x_setreg(bas, AR933X_UART_DATA_REG, AR933X_UART_DATA_RX_CSR);

	uart_unlock(hwmtx);

	return (c);
}

/*
 * High-level UART interface.
 */
struct ar933x_softc {
	struct uart_softc base;

	uint32_t	u_ier;
};

static int ar933x_bus_attach(struct uart_softc *);
static int ar933x_bus_detach(struct uart_softc *);
static int ar933x_bus_flush(struct uart_softc *, int);
static int ar933x_bus_getsig(struct uart_softc *);
static int ar933x_bus_ioctl(struct uart_softc *, int, intptr_t);
static int ar933x_bus_ipend(struct uart_softc *);
static int ar933x_bus_param(struct uart_softc *, int, int, int, int);
static int ar933x_bus_probe(struct uart_softc *);
static int ar933x_bus_receive(struct uart_softc *);
static int ar933x_bus_setsig(struct uart_softc *, int);
static int ar933x_bus_transmit(struct uart_softc *);

static kobj_method_t ar933x_methods[] = {
	KOBJMETHOD(uart_attach,		ar933x_bus_attach),
	KOBJMETHOD(uart_detach,		ar933x_bus_detach),
	KOBJMETHOD(uart_flush,		ar933x_bus_flush),
	KOBJMETHOD(uart_getsig,		ar933x_bus_getsig),
	KOBJMETHOD(uart_ioctl,		ar933x_bus_ioctl),
	KOBJMETHOD(uart_ipend,		ar933x_bus_ipend),
	KOBJMETHOD(uart_param,		ar933x_bus_param),
	KOBJMETHOD(uart_probe,		ar933x_bus_probe),
	KOBJMETHOD(uart_receive,	ar933x_bus_receive),
	KOBJMETHOD(uart_setsig,		ar933x_bus_setsig),
	KOBJMETHOD(uart_transmit,	ar933x_bus_transmit),
	{ 0, 0 }
};

struct uart_class uart_ar933x_class = {
	"ar933x",
	ar933x_methods,
	sizeof(struct ar933x_softc),
	.uc_ops = &uart_ar933x_ops,
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
ar933x_bus_attach(struct uart_softc *sc)
{
#if 0
	struct ar933x_softc *ns8250 = (struct ar933x_softc*)sc;
	struct uart_bas *bas;
	unsigned int ivar;

	bas = &sc->sc_bas;

	ns8250->mcr = uart_getreg(bas, REG_MCR);
	ns8250->fcr = FCR_ENABLE;
	if (!resource_int_value("uart", device_get_unit(sc->sc_dev), "flags",
	    &ivar)) {
		if (UART_FLAGS_FCR_RX_LOW(ivar)) 
			ns8250->fcr |= FCR_RX_LOW;
		else if (UART_FLAGS_FCR_RX_MEDL(ivar)) 
			ns8250->fcr |= FCR_RX_MEDL;
		else if (UART_FLAGS_FCR_RX_HIGH(ivar)) 
			ns8250->fcr |= FCR_RX_HIGH;
		else
			ns8250->fcr |= FCR_RX_MEDH;
	} else 
		ns8250->fcr |= FCR_RX_MEDH;
	
	/* Get IER mask */
	ivar = 0xf0;
	resource_int_value("uart", device_get_unit(sc->sc_dev), "ier_mask",
	    &ivar);
	ns8250->ier_mask = (uint8_t)(ivar & 0xff);
	
	/* Get IER RX interrupt bits */
	ivar = IER_EMSC | IER_ERLS | IER_ERXRDY;
	resource_int_value("uart", device_get_unit(sc->sc_dev), "ier_rxbits",
	    &ivar);
	ns8250->ier_rxbits = (uint8_t)(ivar & 0xff);
	
	uart_setreg(bas, REG_FCR, ns8250->fcr);
	uart_barrier(bas);
	ar933x_bus_flush(sc, UART_FLUSH_RECEIVER|UART_FLUSH_TRANSMITTER);

	if (ns8250->mcr & MCR_DTR)
		sc->sc_hwsig |= SER_DTR;
	if (ns8250->mcr & MCR_RTS)
		sc->sc_hwsig |= SER_RTS;
	ar933x_bus_getsig(sc);

	ar933x_clrint(bas);
	ns8250->ier = uart_getreg(bas, REG_IER) & ns8250->ier_mask;
	ns8250->ier |= ns8250->ier_rxbits;
	uart_setreg(bas, REG_IER, ns8250->ier);
	uart_barrier(bas);
#endif
	return (0);
}

static int
ar933x_bus_detach(struct uart_softc *sc)
{
#if 0
	struct ar933x_softc *ns8250;
	struct uart_bas *bas;
	u_char ier;

	ns8250 = (struct ar933x_softc *)sc;
	bas = &sc->sc_bas;
	ier = uart_getreg(bas, REG_IER) & ns8250->ier_mask;
	uart_setreg(bas, REG_IER, ier);
	uart_barrier(bas);
	ar933x_clrint(bas);
#endif
	return (0);
}

static int
ar933x_bus_flush(struct uart_softc *sc, int what)
{
#if 0
	struct ar933x_softc *ns8250 = (struct ar933x_softc*)sc;
	struct uart_bas *bas;
	int error;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);
	if (sc->sc_rxfifosz > 1) {
		ar933x_flush(bas, what);
		uart_setreg(bas, REG_FCR, ns8250->fcr);
		uart_barrier(bas);
		error = 0;
	} else
		error = ar933x_drain(bas, what);
	uart_unlock(sc->sc_hwmtx);
	return (error);
#endif
	return (ENXIO);
}

static int
ar933x_bus_getsig(struct uart_softc *sc)
{
#if 0
	uint32_t new, old, sig;
	uint8_t msr;

	do {
		old = sc->sc_hwsig;
		sig = old;
		uart_lock(sc->sc_hwmtx);
		msr = uart_getreg(&sc->sc_bas, REG_MSR);
		uart_unlock(sc->sc_hwmtx);
		SIGCHG(msr & MSR_DSR, sig, SER_DSR, SER_DDSR);
		SIGCHG(msr & MSR_CTS, sig, SER_CTS, SER_DCTS);
		SIGCHG(msr & MSR_DCD, sig, SER_DCD, SER_DDCD);
		SIGCHG(msr & MSR_RI,  sig, SER_RI,  SER_DRI);
		new = sig & ~SER_MASK_DELTA;
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));
	return (sig);
#endif
	return (0);
}

static int
ar933x_bus_ioctl(struct uart_softc *sc, int request, intptr_t data)
{
#if 0
	struct uart_bas *bas;
	int baudrate, divisor, error;
	uint8_t efr, lcr;

	bas = &sc->sc_bas;
	error = 0;
	uart_lock(sc->sc_hwmtx);
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
	case UART_IOCTL_IFLOW:
		lcr = uart_getreg(bas, REG_LCR);
		uart_barrier(bas);
		uart_setreg(bas, REG_LCR, 0xbf);
		uart_barrier(bas);
		efr = uart_getreg(bas, REG_EFR);
		if (data)
			efr |= EFR_RTS;
		else
			efr &= ~EFR_RTS;
		uart_setreg(bas, REG_EFR, efr);
		uart_barrier(bas);
		uart_setreg(bas, REG_LCR, lcr);
		uart_barrier(bas);
		break;
	case UART_IOCTL_OFLOW:
		lcr = uart_getreg(bas, REG_LCR);
		uart_barrier(bas);
		uart_setreg(bas, REG_LCR, 0xbf);
		uart_barrier(bas);
		efr = uart_getreg(bas, REG_EFR);
		if (data)
			efr |= EFR_CTS;
		else
			efr &= ~EFR_CTS;
		uart_setreg(bas, REG_EFR, efr);
		uart_barrier(bas);
		uart_setreg(bas, REG_LCR, lcr);
		uart_barrier(bas);
		break;
	case UART_IOCTL_BAUD:
		lcr = uart_getreg(bas, REG_LCR);
		uart_setreg(bas, REG_LCR, lcr | LCR_DLAB);
		uart_barrier(bas);
		divisor = uart_getreg(bas, REG_DLL) |
		    (uart_getreg(bas, REG_DLH) << 8);
		uart_barrier(bas);
		uart_setreg(bas, REG_LCR, lcr);
		uart_barrier(bas);
		baudrate = (divisor > 0) ? bas->rclk / divisor / 16 : 0;
		if (baudrate > 0)
			*(int*)data = baudrate;
		else
			error = ENXIO;
		break;
	default:
		error = EINVAL;
		break;
	}
	uart_unlock(sc->sc_hwmtx);
	return (error);
#endif
	return (ENXIO);
}

static int
ar933x_bus_ipend(struct uart_softc *sc)
{
#if 0
	struct uart_bas *bas;
	struct ar933x_softc *ns8250;
	int ipend;
	uint8_t iir, lsr;

	ns8250 = (struct ar933x_softc *)sc;
	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);
	iir = uart_getreg(bas, REG_IIR);
	if (iir & IIR_NOPEND) {
		uart_unlock(sc->sc_hwmtx);
		return (0);
	}
	ipend = 0;
	if (iir & IIR_RXRDY) {
		lsr = uart_getreg(bas, REG_LSR);
		if (lsr & LSR_OE)
			ipend |= SER_INT_OVERRUN;
		if (lsr & LSR_BI)
			ipend |= SER_INT_BREAK;
		if (lsr & LSR_RXRDY)
			ipend |= SER_INT_RXREADY;
	} else {
		if (iir & IIR_TXRDY) {
			ipend |= SER_INT_TXIDLE;
			uart_setreg(bas, REG_IER, ns8250->ier);
		} else
			ipend |= SER_INT_SIGCHG;
	}
	if (ipend == 0)
		ar933x_clrint(bas);
	uart_unlock(sc->sc_hwmtx);
	return (ipend);
#endif
	return (0);
}

static int
ar933x_bus_param(struct uart_softc *sc, int baudrate, int databits,
    int stopbits, int parity)
{
#if 0
	struct uart_bas *bas;
	int error;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);
	error = ar933x_param(bas, baudrate, databits, stopbits, parity);
	uart_unlock(sc->sc_hwmtx);
	return (error);
#endif
	return (ENXIO);
}

static int
ar933x_bus_probe(struct uart_softc *sc)
{
#if 0
	struct ar933x_softc *ns8250;
	struct uart_bas *bas;
	int count, delay, error, limit;
	uint8_t lsr, mcr, ier;

	ns8250 = (struct ar933x_softc *)sc;
	bas = &sc->sc_bas;

	error = ar933x_probe(bas);
	if (error)
		return (error);

	mcr = MCR_IE;
	if (sc->sc_sysdev == NULL) {
		/* By using ar933x_init() we also set DTR and RTS. */
		ar933x_init(bas, 115200, 8, 1, UART_PARITY_NONE);
	} else
		mcr |= MCR_DTR | MCR_RTS;

	error = ar933x_drain(bas, UART_DRAIN_TRANSMITTER);
	if (error)
		return (error);

	/*
	 * Set loopback mode. This avoids having garbage on the wire and
	 * also allows us send and receive data. We set DTR and RTS to
	 * avoid the possibility that automatic flow-control prevents
	 * any data from being sent.
	 */
	uart_setreg(bas, REG_MCR, MCR_LOOPBACK | MCR_IE | MCR_DTR | MCR_RTS);
	uart_barrier(bas);

	/*
	 * Enable FIFOs. And check that the UART has them. If not, we're
	 * done. Since this is the first time we enable the FIFOs, we reset
	 * them.
	 */
	uart_setreg(bas, REG_FCR, FCR_ENABLE);
	uart_barrier(bas);
	if (!(uart_getreg(bas, REG_IIR) & IIR_FIFO_MASK)) {
		/*
		 * NS16450 or INS8250. We don't bother to differentiate
		 * between them. They're too old to be interesting.
		 */
		uart_setreg(bas, REG_MCR, mcr);
		uart_barrier(bas);
		sc->sc_rxfifosz = sc->sc_txfifosz = 1;
		device_set_desc(sc->sc_dev, "8250 or 16450 or compatible");
		return (0);
	}

	uart_setreg(bas, REG_FCR, FCR_ENABLE | FCR_XMT_RST | FCR_RCV_RST);
	uart_barrier(bas);

	count = 0;
	delay = ar933x_delay(bas);

	/* We have FIFOs. Drain the transmitter and receiver. */
	error = ar933x_drain(bas, UART_DRAIN_RECEIVER|UART_DRAIN_TRANSMITTER);
	if (error) {
		uart_setreg(bas, REG_MCR, mcr);
		uart_setreg(bas, REG_FCR, 0);
		uart_barrier(bas);
		goto describe;
	}

	/*
	 * We should have a sufficiently clean "pipe" to determine the
	 * size of the FIFOs. We send as much characters as is reasonable
	 * and wait for the overflow bit in the LSR register to be
	 * asserted, counting the characters as we send them. Based on
	 * that count we know the FIFO size.
	 */
	do {
		uart_setreg(bas, REG_DATA, 0);
		uart_barrier(bas);
		count++;

		limit = 30;
		lsr = 0;
		/*
		 * LSR bits are cleared upon read, so we must accumulate
		 * them to be able to test LSR_OE below.
		 */
		while (((lsr |= uart_getreg(bas, REG_LSR)) & LSR_TEMT) == 0 &&
		    --limit)
			DELAY(delay);
		if (limit == 0) {
			ier = uart_getreg(bas, REG_IER) & ns8250->ier_mask;
			uart_setreg(bas, REG_IER, ier);
			uart_setreg(bas, REG_MCR, mcr);
			uart_setreg(bas, REG_FCR, 0);
			uart_barrier(bas);
			count = 0;
			goto describe;
		}
	} while ((lsr & LSR_OE) == 0 && count < 130);
	count--;

	uart_setreg(bas, REG_MCR, mcr);

	/* Reset FIFOs. */
	ar933x_flush(bas, UART_FLUSH_RECEIVER|UART_FLUSH_TRANSMITTER);

 describe:
	if (count >= 14 && count <= 16) {
		sc->sc_rxfifosz = 16;
		device_set_desc(sc->sc_dev, "16550 or compatible");
	} else if (count >= 28 && count <= 32) {
		sc->sc_rxfifosz = 32;
		device_set_desc(sc->sc_dev, "16650 or compatible");
	} else if (count >= 56 && count <= 64) {
		sc->sc_rxfifosz = 64;
		device_set_desc(sc->sc_dev, "16750 or compatible");
	} else if (count >= 112 && count <= 128) {
		sc->sc_rxfifosz = 128;
		device_set_desc(sc->sc_dev, "16950 or compatible");
	} else {
		sc->sc_rxfifosz = 16;
		device_set_desc(sc->sc_dev,
		    "Non-standard ns8250 class UART with FIFOs");
	}

	/*
	 * Force the Tx FIFO size to 16 bytes for now. We don't program the
	 * Tx trigger. Also, we assume that all data has been sent when the
	 * interrupt happens.
	 */
	sc->sc_txfifosz = 16;

#if 0
	/*
	 * XXX there are some issues related to hardware flow control and
	 * it's likely that uart(4) is the cause. This basicly needs more
	 * investigation, but we avoid using for hardware flow control
	 * until then.
	 */
	/* 16650s or higher have automatic flow control. */
	if (sc->sc_rxfifosz > 16) {
		sc->sc_hwiflow = 1;
		sc->sc_hwoflow = 1;
	}
#endif
#endif
	return (0);
}

static int
ar933x_bus_receive(struct uart_softc *sc)
{
#if 0
	struct uart_bas *bas;
	int xc;
	uint8_t lsr;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);
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
	uart_unlock(sc->sc_hwmtx);
#endif
	return (0);
}

static int
ar933x_bus_setsig(struct uart_softc *sc, int sig)
{
#if 0
	struct ar933x_softc *ns8250 = (struct ar933x_softc*)sc;
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
	uart_lock(sc->sc_hwmtx);
	ns8250->mcr &= ~(MCR_DTR|MCR_RTS);
	if (new & SER_DTR)
		ns8250->mcr |= MCR_DTR;
	if (new & SER_RTS)
		ns8250->mcr |= MCR_RTS;
	uart_setreg(bas, REG_MCR, ns8250->mcr);
	uart_barrier(bas);
	uart_unlock(sc->sc_hwmtx);
#endif
	return (0);
}

static int
ar933x_bus_transmit(struct uart_softc *sc)
{
#if 0
	struct ar933x_softc *ns8250 = (struct ar933x_softc*)sc;
	struct uart_bas *bas;
	int i;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);
	while ((uart_getreg(bas, REG_LSR) & LSR_THRE) == 0)
		;
	uart_setreg(bas, REG_IER, ns8250->ier | IER_ETXRDY);
	uart_barrier(bas);
	for (i = 0; i < sc->sc_txdatasz; i++) {
		uart_setreg(bas, REG_DATA, sc->sc_txbuf[i]);
		uart_barrier(bas);
	}
	sc->sc_txbusy = 1;
	uart_unlock(sc->sc_hwmtx);
#endif
	return (0);
}
