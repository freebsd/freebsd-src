/*-
 * Copyright (c) 2005 M. Warner Losh
 * Copyright (c) 2005 cognet
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
#include <machine/bus.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_bus.h>
#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91_usartreg.h>

#include "uart_if.h"

#define      DEFAULT_RCLK    AT91C_MASTER_CLOCK

#define	SIGCHG(c, i, s, d)				\
	do {						\
		if (c) {				\
			i |= (i & s) ? s : s | d;	\
		} else {				\
			i = (i & s) ? (i & ~s) | d : i;	\
		}					\
	} while (0);

/*
 * Low-level UART interface.
 */
static int at91_usart_probe(struct uart_bas *bas);
static void at91_usart_init(struct uart_bas *bas, int, int, int, int);
static void at91_usart_term(struct uart_bas *bas);
static void at91_usart_putc(struct uart_bas *bas, int);
static int at91_usart_poll(struct uart_bas *bas);
static int at91_usart_getc(struct uart_bas *bas);

extern SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs;

static int
at91_usart_param(struct uart_bas *bas, int baudrate, int databits,
    int stopbits, int parity)
{
	uint32_t mr;

	/*
	 * Assume 3-write RS-232 configuration.
	 * XXX Not sure how uart will present the other modes to us, so
	 * XXX they are unimplemented.  maybe ioctl?
	 */
	mr = USART_MR_MODE_NORMAL;
	mr |= USART_MR_USCLKS_MCK;	/* Assume MCK */

	/*
	 * Or in the databits requested
	 */
	if (databits < 9)
		mr &= ~USART_MR_MODE9;
	switch (databits) {
	case 5:
		mr |= USART_MR_CHRL_5BITS;
		break;
	case 6:
		mr |= USART_MR_CHRL_6BITS;
		break;
	case 7:
		mr |= USART_MR_CHRL_7BITS;
		break;
	case 8:
		mr |= USART_MR_CHRL_8BITS;
		break;
	case 9:
		mr |= USART_MR_CHRL_8BITS | USART_MR_MODE9;
		break;
	default:
		return (EINVAL);
	}

	/*
	 * Or in the parity
	 */
	switch (parity) {
	case UART_PARITY_NONE:
		mr |= USART_MR_PAR_NONE;
		break;
	case UART_PARITY_ODD:
		mr |= USART_MR_PAR_ODD;
		break;
	case UART_PARITY_EVEN:
		mr |= USART_MR_PAR_EVEN;
		break;
	case UART_PARITY_MARK:
		mr |= USART_MR_PAR_MARK;
		break;
	case UART_PARITY_SPACE:
		mr |= USART_MR_PAR_SPACE;
		break;
	default:
		return (EINVAL);
	}

	/*
	 * Or in the stop bits.  Note: The hardware supports
	 * 1.5 stop bits in async mode, but there's no way to
	 * specify that AFAICT.
	 */
	if (stopbits > 1)
		mr |= USART_MR_NBSTOP_2;
	else
		mr |= USART_MR_NBSTOP_2;
	/* else if (stopbits == 1.5)
		mr |= USART_MR_NBSTOP_1_5; */

	/*
	 * We want normal plumbing mode too, none of this fancy
	 * loopback or echo mode.
	 */
	mr |= USART_MR_CHMODE_NORMAL;

	mr &= ~USART_MR_MSBF;	/* lsb first */
	mr &= ~USART_MR_CKLO_SCK;	/* Don't drive SCK */

	/* XXX Need to take possible synchronous mode into account */
	return (0);
}

struct uart_ops at91_usart_ops = {
	.probe = at91_usart_probe,
	.init = at91_usart_init,
	.term = at91_usart_term,
	.putc = at91_usart_putc,
	.poll = at91_usart_poll,
	.getc = at91_usart_getc,
};

static int
at91_usart_probe(struct uart_bas *bas)
{
	/* We know that this is always here */
	return (0);
}

/*
 * Initialize this device (I think as the console)
 */
static void
at91_usart_init(struct uart_bas *bas, int baudrate, int databits, int stopbits,
    int parity)
{
	at91_usart_param(bas, baudrate, databits, stopbits, parity);

	/* Turn on rx and tx */
	uart_setreg(bas, USART_CR, USART_CR_RSTRX | USART_CR_RSTTX);
	uart_setreg(bas, USART_CR, USART_CR_RXEN | USART_CR_TXEN);
	uart_setreg(bas, USART_IER, USART_CSR_TXRDY | USART_CSR_RXRDY);
}

/*
 * Free resources now that we're no longer the console.  This appears to
 * be never called, and I'm unsure quite what to do if I am called.
 */
static void
at91_usart_term(struct uart_bas *bas)
{
	/* XXX */
}

/*
 * Put a character of console output (so we do it here polling rather than
 * interrutp driven).
 */
static void
at91_usart_putc(struct uart_bas *bas, int c)
{

	while (!(uart_getreg(bas, USART_CSR) & 
	    USART_CSR_TXRDY));
	uart_setreg(bas, USART_THR, c);
}

/*
 * Poll for a character available
 */
static int
at91_usart_poll(struct uart_bas *bas)
{

	if (!(uart_getreg(bas, USART_CSR) & USART_CSR_RXRDY))
		return (-1);
	return (uart_getreg(bas, USART_RHR) & 0xff);
}

/*
 * Block waiting for a character.
 */
static int
at91_usart_getc(struct uart_bas *bas)
{
	int c;

	while (!(uart_getreg(bas, USART_CSR) & USART_CSR_RXRDY)) 
		;
	c = uart_getreg(bas, USART_RHR);
	c &= 0xff;
	return (c);
}

static int at91_usart_bus_probe(struct uart_softc *sc);
static int at91_usart_bus_attach(struct uart_softc *sc);
static int at91_usart_bus_flush(struct uart_softc *, int);
static int at91_usart_bus_getsig(struct uart_softc *);
static int at91_usart_bus_ioctl(struct uart_softc *, int, intptr_t);
static int at91_usart_bus_ipend(struct uart_softc *);
static int at91_usart_bus_param(struct uart_softc *, int, int, int, int);
static int at91_usart_bus_receive(struct uart_softc *);
static int at91_usart_bus_setsig(struct uart_softc *, int);
static int at91_usart_bus_transmit(struct uart_softc *);

static kobj_method_t at91_usart_methods[] = {
	KOBJMETHOD(uart_probe,		at91_usart_bus_probe),
	KOBJMETHOD(uart_attach, 	at91_usart_bus_attach),
	KOBJMETHOD(uart_flush,		at91_usart_bus_flush),
	KOBJMETHOD(uart_getsig,		at91_usart_bus_getsig),
	KOBJMETHOD(uart_ioctl,		at91_usart_bus_ioctl),
	KOBJMETHOD(uart_ipend,		at91_usart_bus_ipend),
	KOBJMETHOD(uart_param,		at91_usart_bus_param),
	KOBJMETHOD(uart_receive,	at91_usart_bus_receive),
	KOBJMETHOD(uart_setsig,		at91_usart_bus_setsig),
	KOBJMETHOD(uart_transmit,	at91_usart_bus_transmit),
	
	{ 0, 0 }
};

int
at91_usart_bus_probe(struct uart_softc *sc)
{
	return (0);
}

static int
at91_usart_bus_attach(struct uart_softc *sc)
{
	sc->sc_txfifosz = 1;
	sc->sc_rxfifosz = 1;
	sc->sc_hwiflow = 0;
	return (0);
}
static int
at91_usart_bus_transmit(struct uart_softc *sc)
{
	int i;

	/* XXX VERY sub-optimial */
	mtx_lock_spin(&sc->sc_hwmtx);
	sc->sc_txbusy = 1;
	for (i = 0; i < sc->sc_txdatasz; i++)
		at91_usart_putc(&sc->sc_bas, sc->sc_txbuf[i]);
	mtx_unlock_spin(&sc->sc_hwmtx);
#ifdef USART0_CONSOLE
	/*
	 * XXX: Gross hack : Skyeye doesn't raise an interrupt once the
	 * transfer is done, so simulate it.
	 */
	uart_setreg(&sc->sc_bas, USART_IER, USART_CSR_TXRDY);
#endif
	return (0);
}
static int
at91_usart_bus_setsig(struct uart_softc *sc, int sig)
{
	uint32_t new, old, cr;
	struct uart_bas *bas;

	do {
		old = sc->sc_hwsig;
		new = old;
		if (sig & SER_DDTR)
			SIGCHG(sig & SER_DTR, new, SER_DTR, SER_DDTR);
		if (sig & SER_DRTS)
			SIGCHG(sig & SER_RTS, new, SER_RTS, SER_DRTS);
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));
	bas = &sc->sc_bas;
	mtx_lock_spin(&sc->sc_hwmtx);
	cr = uart_getreg(bas, USART_CR);
	cr &= ~(USART_CR_DTREN | USART_CR_DTRDIS | USART_CR_RTSEN |
	    USART_CR_RTSDIS);
	if (new & SER_DTR)
		cr |= USART_CR_DTREN;
	else
		cr |= USART_CR_DTRDIS;
	if (new & SER_RTS)
		cr |= USART_CR_RTSEN;
	else
		cr |= USART_CR_RTSDIS;
	uart_setreg(bas, USART_CR, cr);
	mtx_unlock_spin(&sc->sc_hwmtx);
	return (0);
}
static int
at91_usart_bus_receive(struct uart_softc *sc)
{
	
	mtx_lock_spin(&sc->sc_hwmtx);
	uart_rx_put(sc, at91_usart_getc(&sc->sc_bas));
	mtx_unlock_spin(&sc->sc_hwmtx);
	return (0);
}
static int
at91_usart_bus_param(struct uart_softc *sc, int baudrate, int databits,
    int stopbits, int parity)
{
	return (at91_usart_param(&sc->sc_bas, baudrate, databits, stopbits,
	    parity));
}
static int
at91_usart_bus_ipend(struct uart_softc *sc)
{
	int csr = uart_getreg(&sc->sc_bas, USART_CSR);
	int ipend = 0;
	
#ifdef USART0_CONSOLE
	/* 
	 * XXX: We have to cheat for skyeye, as it will return 0xff for all
	 * the devices it doesn't emulate.
	 */
	if (sc->sc_bas.chan != 1)
		return (0);
#endif
	   
	mtx_lock_spin(&sc->sc_hwmtx);
	if (csr & USART_CSR_TXRDY && sc->sc_txbusy)
		ipend |= SER_INT_TXIDLE;
	if (csr & USART_CSR_RXRDY)
		ipend |= SER_INT_RXREADY;
	mtx_unlock_spin(&sc->sc_hwmtx);
	return (ipend);
}
static int
at91_usart_bus_flush(struct uart_softc *sc, int what)
{
	return (0);
}

static int
at91_usart_bus_getsig(struct uart_softc *sc)
{
	uint32_t new, sig;
	uint8_t csr;

	mtx_lock_spin(&sc->sc_hwmtx);
	csr = uart_getreg(&sc->sc_bas, USART_CSR);
	sig = 0;
	if (csr & USART_CSR_CTS)
		sig |= SER_CTS;
	if (csr & USART_CSR_DCD)
		sig |= SER_DCD;
	if (csr & USART_CSR_DSR)
		sig |= SER_DSR;
	if (csr & USART_CSR_RI)
		sig |= SER_RI;
	new = sig & ~SER_MASK_DELTA;
	sc->sc_hwsig = new;
	mtx_unlock_spin(&sc->sc_hwmtx);
	return (sig);
}

static int
at91_usart_bus_ioctl(struct uart_softc *sc, int request, intptr_t data)
{
	return (EINVAL);
}
struct uart_class at91_usart_class = {
	"at91_usart class",
	at91_usart_methods,
	1,
	.uc_range = 8,
	.uc_rclk = DEFAULT_RCLK
};
