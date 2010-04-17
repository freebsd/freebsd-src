/*-
 * Copyright (c) 2010 Marcel Moolenaar
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
#include <sys/endian.h>
#include <machine/bus.h>
#include <machine/sal.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_bus.h>

#include "uart_if.h"

/*
 * Low-level UART interface.
 */
static int sgisn_probe(struct uart_bas *bas);
static void sgisn_init(struct uart_bas *bas, int, int, int, int);
static void sgisn_term(struct uart_bas *bas);
static void sgisn_putc(struct uart_bas *bas, int);
static int sgisn_rxready(struct uart_bas *bas);
static int sgisn_getc(struct uart_bas *bas, struct mtx *);

static struct uart_ops uart_sgisn_ops = {
	.probe = sgisn_probe,
	.init = sgisn_init,
	.term = sgisn_term,
	.putc = sgisn_putc,
	.rxready = sgisn_rxready,
	.getc = sgisn_getc,
};

static int
sgisn_probe(struct uart_bas *bas)
{
	struct ia64_sal_result result;

	result = ia64_sal_entry(SAL_SGISN_SN_INFO, 0, 0, 0, 0, 0, 0, 0);
	return ((result.sal_status != 0) ? ENXIO : 0);
}

static void
sgisn_init(struct uart_bas *bas, int baudrate, int databits, int stopbits,
    int parity)
{
}

static void
sgisn_term(struct uart_bas *bas)
{
}

static void
sgisn_putc(struct uart_bas *bas, int c)
{
	struct ia64_sal_result result;

	result = ia64_sal_entry(SAL_SGISN_PUTC, c, 0, 0, 0, 0, 0, 0);
}

static int
sgisn_rxready(struct uart_bas *bas)
{
	struct ia64_sal_result result;

	result = ia64_sal_entry(SAL_SGISN_POLL, 0, 0, 0, 0, 0, 0, 0);
	return (!result.sal_status && result.sal_result[0]);
}

static int
sgisn_getc(struct uart_bas *bas, struct mtx *hwmtx)
{
	struct ia64_sal_result result;

	result = ia64_sal_entry(SAL_SGISN_GETC, 0, 0, 0, 0, 0, 0, 0);
	return ((!result.sal_status) ? result.sal_result[0] : -1);
}

/*
 * High-level UART interface.
 */
struct sgisn_softc {
	struct uart_softc base;
};

static int sgisn_bus_attach(struct uart_softc *);
static int sgisn_bus_detach(struct uart_softc *);
static int sgisn_bus_flush(struct uart_softc *, int);
static int sgisn_bus_getsig(struct uart_softc *);
static int sgisn_bus_ioctl(struct uart_softc *, int, intptr_t);
static int sgisn_bus_ipend(struct uart_softc *);
static int sgisn_bus_param(struct uart_softc *, int, int, int, int);
static int sgisn_bus_probe(struct uart_softc *);
static int sgisn_bus_receive(struct uart_softc *);
static int sgisn_bus_setsig(struct uart_softc *, int);
static int sgisn_bus_transmit(struct uart_softc *);

static kobj_method_t sgisn_methods[] = {
	KOBJMETHOD(uart_attach,		sgisn_bus_attach),
	KOBJMETHOD(uart_detach,		sgisn_bus_detach),
	KOBJMETHOD(uart_flush,		sgisn_bus_flush),
	KOBJMETHOD(uart_getsig,		sgisn_bus_getsig),
	KOBJMETHOD(uart_ioctl,		sgisn_bus_ioctl),
	KOBJMETHOD(uart_ipend,		sgisn_bus_ipend),
	KOBJMETHOD(uart_param,		sgisn_bus_param),
	KOBJMETHOD(uart_probe,		sgisn_bus_probe),
	KOBJMETHOD(uart_receive,	sgisn_bus_receive),
	KOBJMETHOD(uart_setsig,		sgisn_bus_setsig),
	KOBJMETHOD(uart_transmit,	sgisn_bus_transmit),
	{ 0, 0 }
};

struct uart_class uart_sgisn_class = {
	"sgisn",
	sgisn_methods,
	sizeof(struct sgisn_softc),
	.uc_ops = &uart_sgisn_ops,
	.uc_range = 0,
	.uc_rclk = 0
};

#define	SIGCHG(c, i, s, d)				\
	if (c) {					\
		i |= (i & s) ? s : s | d;		\
	} else {					\
		i = (i & s) ? (i & ~s) | d : i;		\
	}

static int
sgisn_bus_attach(struct uart_softc *sc)
{

	sc->sc_rxfifosz = 1;
	sc->sc_txfifosz = 1;

	(void)sgisn_bus_getsig(sc);

	return (0);
}

static int
sgisn_bus_detach(struct uart_softc *sc)
{

	return (0);
}

static int
sgisn_bus_flush(struct uart_softc *sc, int what)
{

	return (0);
}

static int
sgisn_bus_getsig(struct uart_softc *sc)
{
	uint32_t new, old, sig;
	uint32_t dummy;

	do {
		old = sc->sc_hwsig;
		sig = old;
		/* XXX SIGNALS */
		dummy = 0;
		SIGCHG(dummy, sig, SER_CTS, SER_DCTS);
		SIGCHG(dummy, sig, SER_DCD, SER_DDCD);
		SIGCHG(dummy, sig, SER_DSR, SER_DDSR);
		new = sig & ~SER_MASK_DELTA;
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));
	return (sig);
}

static int
sgisn_bus_ioctl(struct uart_softc *sc, int request, intptr_t data)
{
	int error;

	error = 0;
	switch (request) {
	case UART_IOCTL_BREAK:
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static int
sgisn_bus_ipend(struct uart_softc *sc)
{
	int ipend;

	ipend = 0;
	return (ipend);
}

static int
sgisn_bus_param(struct uart_softc *sc, int baudrate, int databits,
    int stopbits, int parity)
{

	return (0);
}

static int
sgisn_bus_probe(struct uart_softc *sc)
{
	char buf[80];
	int error;

	error = sgisn_probe(&sc->sc_bas);
	if (error)
		return (error);

	snprintf(buf, sizeof(buf), "SGI L1");
	device_set_desc_copy(sc->sc_dev, buf);
	return (0);
}

static int
sgisn_bus_receive(struct uart_softc *sc)
{

	return (0);
}

static int
sgisn_bus_setsig(struct uart_softc *sc, int sig)
{
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

	/* XXX SIGNALS */
	return (0);
}

static int
sgisn_bus_transmit(struct uart_softc *sc)
{

	return (0);
}
