/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Axiado Corporation
 * All rights reserved.
 *
 * This software was developed in part by Kristof Provost under contract for
 * Axiado Corporation.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/clk/clk.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_cpu_fdt.h>

#include "uart_if.h"

#define	SFUART_TXDATA			0x00
#define		SFUART_TXDATA_FULL	(1 << 31)
#define	SFUART_RXDATA			0x04
#define		SFUART_RXDATA_EMPTY	(1 << 31)
#define	SFUART_TXCTRL			0x08
#define		SFUART_TXCTRL_ENABLE	0x01
#define		SFUART_TXCTRL_NSTOP	0x02
#define		SFUART_TXCTRL_TXCNT	0x70000
#define		SFUART_TXCTRL_TXCNT_SHIFT	16
#define	SFUART_RXCTRL			0x0c
#define		SFUART_RXCTRL_ENABLE	0x01
#define		SFUART_RXCTRL_RXCNT	0x70000
#define		SFUART_RXCTRL_RXCNT_SHIFT	16
#define	SFUART_IRQ_ENABLE		0x10
#define		SFUART_IRQ_ENABLE_TXWM	0x01
#define		SFUART_IRQ_ENABLE_RXWM	0x02
#define	SFUART_IRQ_PENDING		0x14
#define		SFUART_IRQ_PENDING_TXWM	0x01
#define		SFUART_IRQ_PENDING_RXQM	0x02
#define	SFUART_DIV			0x18
#define	SFUART_REGS_SIZE		0x1c

#define	SFUART_RX_FIFO_DEPTH		8
#define	SFUART_TX_FIFO_DEPTH		8

struct sfuart_softc {
	struct uart_softc	uart_softc;
	clk_t			clk;
};

static int
sfuart_probe(struct uart_bas *bas)
{

	bas->regiowidth = 4;

	return (0);
}

static void
sfuart_init(struct uart_bas *bas, int baudrate, int databits, int stopbits,
    int parity)
{
	uint32_t reg;

	uart_setreg(bas, SFUART_IRQ_ENABLE, 0);

	/* Enable RX and configure the watermark so that we get an interrupt
	 * when a single character arrives (if interrupts are enabled). */
	reg = SFUART_RXCTRL_ENABLE;
	reg |= (0 << SFUART_RXCTRL_RXCNT_SHIFT);
	uart_setreg(bas, SFUART_RXCTRL, reg);

	/* Enable TX and configure the watermark so that we get an interrupt
	 * when there's room for one more character in the TX fifo (if
	 * interrupts are enabled). */
	reg = SFUART_TXCTRL_ENABLE;
	reg |= (1 << SFUART_TXCTRL_TXCNT_SHIFT);
	if (stopbits == 2)
		reg |= SFUART_TXCTRL_NSTOP;
	uart_setreg(bas, SFUART_TXCTRL, reg);

	/* Don't touch DIV. Assume that's set correctly until we can
	 * reconfigure. */
}

static void
sfuart_putc(struct uart_bas *bas, int c)
{

	while ((uart_getreg(bas, SFUART_TXDATA) & SFUART_TXDATA_FULL) 
	    != 0)
		cpu_spinwait();

	uart_setreg(bas, SFUART_TXDATA, c);
}

static int
sfuart_rxready(struct uart_bas *bas)
{
	/*
	 * Unfortunately the FIFO empty flag is in the FIFO data register so
	 * reading it would dequeue the character. Instead, rely on the fact
	 * we've configured the watermark to be 0 and that interrupts are off
	 * when using the low-level console function, and read the interrupt
	 * pending state instead.
	 */
	return ((uart_getreg(bas, SFUART_IRQ_PENDING) &
	    SFUART_IRQ_PENDING_RXQM) != 0);
}

static int
sfuart_getc(struct uart_bas *bas, struct mtx *hwmtx)
{
	int c;

	uart_lock(hwmtx);

	while (((c = uart_getreg(bas, SFUART_RXDATA)) &
	    SFUART_RXDATA_EMPTY) != 0) {
		uart_unlock(hwmtx);
		DELAY(4);
		uart_lock(hwmtx);
	}

	uart_unlock(hwmtx);

	return (c & 0xff);
}

static int
sfuart_bus_probe(struct uart_softc *sc)
{
	int error;

	error = sfuart_probe(&sc->sc_bas);
	if (error)
		return (error);

	sc->sc_rxfifosz = SFUART_RX_FIFO_DEPTH;
	sc->sc_txfifosz = SFUART_TX_FIFO_DEPTH;
	sc->sc_hwiflow = 0;
	sc->sc_hwoflow = 0;

	device_set_desc(sc->sc_dev, "SiFive UART");

	return (0);
}

static int
sfuart_bus_attach(struct uart_softc *sc)
{
	struct uart_bas *bas;
	struct sfuart_softc *sfsc;
	uint64_t freq;
	uint32_t reg;
	int error;

	sfsc = (struct sfuart_softc *)sc;
	bas = &sc->sc_bas;

	error = clk_get_by_ofw_index(sc->sc_dev, 0, 0, &sfsc->clk);
	if (error) {
		device_printf(sc->sc_dev, "couldn't allocate clock\n");
		return (ENXIO);
	}

	error = clk_enable(sfsc->clk);
	if (error) {
		device_printf(sc->sc_dev, "couldn't enable clock\n");
		return (ENXIO);
	}

	error = clk_get_freq(sfsc->clk, &freq);
	if (error || freq == 0) {
		clk_disable(sfsc->clk);
		device_printf(sc->sc_dev, "couldn't get clock frequency\n");
		return (ENXIO);
	}

	bas->rclk = freq;

	/* Enable RX/RX */
	reg = SFUART_RXCTRL_ENABLE;
	reg |= (0 << SFUART_RXCTRL_RXCNT_SHIFT);
	uart_setreg(bas, SFUART_RXCTRL, reg);

	reg = SFUART_TXCTRL_ENABLE;
	reg |= (1 << SFUART_TXCTRL_TXCNT_SHIFT);
	uart_setreg(bas, SFUART_TXCTRL, reg);

	/* Enable RX interrupt */
	uart_setreg(bas, SFUART_IRQ_ENABLE, SFUART_IRQ_ENABLE_RXWM);

	return (0);
}

static int
sfuart_bus_detach(struct uart_softc *sc)
{
	struct sfuart_softc *sfsc;
	struct uart_bas *bas;

	sfsc = (struct sfuart_softc *)sc;
	bas = &sc->sc_bas;

	/* Disable RX/TX */
	uart_setreg(bas, SFUART_RXCTRL, 0);
	uart_setreg(bas, SFUART_TXCTRL, 0);

	/* Disable interrupts */
	uart_setreg(bas, SFUART_IRQ_ENABLE, 0);

	clk_disable(sfsc->clk);

	return (0);
}

static int
sfuart_bus_flush(struct uart_softc *sc, int what)
{
	struct uart_bas *bas;
	uint32_t reg;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);

	if (what & UART_FLUSH_TRANSMITTER) {
		do {
			reg = uart_getreg(bas, SFUART_TXDATA);
		} while ((reg & SFUART_TXDATA_FULL) != 0);
	}

	if (what & UART_FLUSH_RECEIVER) {
		do {
			reg = uart_getreg(bas, SFUART_RXDATA);
		} while ((reg & SFUART_RXDATA_EMPTY) == 0);
	}
	uart_unlock(sc->sc_hwmtx);

	return (0);
}

#define	SIGCHG(c, i, s, d)						\
	do {								\
		if (c)							\
			i |= ((i) & (s)) ? (s) : (s) | (d);		\
		else		 					\
			i = ((i) & (s)) ? ((i) & ~(s)) | (d) : (i);	\
	} while (0)

static int
sfuart_bus_getsig(struct uart_softc *sc)
{
	uint32_t new, old, sig;

	do {
		old = sc->sc_hwsig;
		sig = old;
		SIGCHG(1, sig, SER_DSR, SER_DDSR);
		SIGCHG(1, sig, SER_DCD, SER_DDCD);
		SIGCHG(1, sig, SER_CTS, SER_DCTS);
		new = sig & ~SER_MASK_DELTA;
	} while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));

	return (sig);
}

static int
sfuart_bus_setsig(struct uart_softc *sc, int sig)
{
	uint32_t new, old;

	do {
		old = sc->sc_hwsig;
		new = old;
		if (sig & SER_DDTR) {
			SIGCHG(sig & SER_DTR, new, SER_DTR, SER_DDTR);
		}
		if (sig & SER_DRTS) {
			SIGCHG(sig & SER_RTS, new, SER_RTS, SER_DRTS);
		}
	 } while (!atomic_cmpset_32(&sc->sc_hwsig, old, new));

	return (0);
}

static int
sfuart_bus_ioctl(struct uart_softc *sc, int request, intptr_t data)
{
	struct uart_bas *bas;
	uint32_t reg;
	int error;

	bas = &sc->sc_bas;

	uart_lock(sc->sc_hwmtx);

	switch (request) {
	case UART_IOCTL_BAUD:
		reg = uart_getreg(bas, SFUART_DIV);
		if (reg == 0) {
			/* Possible if the divisor hasn't been set up yet. */
			error = ENXIO;
			break;
		}
		*(int*)data = bas->rclk / (reg + 1);
		error = 0;
		break;
	default:
		error = EINVAL;
		break;
	}

	uart_unlock(sc->sc_hwmtx);

	return (error);
}

static int
sfuart_bus_ipend(struct uart_softc *sc)
{
	struct uart_bas *bas;
	int ipend;
	uint32_t reg, ie;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);

	ipend = 0;
	reg = uart_getreg(bas, SFUART_IRQ_PENDING);
	ie = uart_getreg(bas, SFUART_IRQ_ENABLE);

	if ((reg & SFUART_IRQ_PENDING_TXWM) != 0 &&
	    (ie & SFUART_IRQ_ENABLE_TXWM) != 0) {
		ipend |= SER_INT_TXIDLE;

		/* Disable TX interrupt */
		ie &= ~(SFUART_IRQ_ENABLE_TXWM);
		uart_setreg(bas, SFUART_IRQ_ENABLE, ie);
	}

	if ((reg & SFUART_IRQ_PENDING_RXQM) != 0)
		ipend |= SER_INT_RXREADY;

	uart_unlock(sc->sc_hwmtx);

	return (ipend);
}

static int
sfuart_bus_param(struct uart_softc *sc, int baudrate, int databits,
    int stopbits, int parity)
{
	struct uart_bas *bas;
	uint32_t reg;

	bas = &sc->sc_bas;

	if (databits != 8)
		return (EINVAL);

	if (parity != UART_PARITY_NONE)
		return (EINVAL);

	uart_lock(sc->sc_hwmtx);

	reg = uart_getreg(bas, SFUART_TXCTRL);
	if (stopbits == 2) {
		reg |= SFUART_TXCTRL_NSTOP;
	} else if (stopbits == 1) {
		reg &= ~SFUART_TXCTRL_NSTOP;
	} else {
		uart_unlock(sc->sc_hwmtx);
		return (EINVAL);
	}

	if (baudrate > 0 && bas->rclk != 0) {
		reg = (bas->rclk / baudrate) - 1;
		uart_setreg(bas, SFUART_DIV, reg);
	}

	uart_unlock(sc->sc_hwmtx);
	return (0);
}

static int
sfuart_bus_receive(struct uart_softc *sc)
{
	struct uart_bas *bas;
	uint32_t reg;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);

	reg = uart_getreg(bas, SFUART_RXDATA);
	while ((reg & SFUART_RXDATA_EMPTY) == 0) {
		if (uart_rx_full(sc)) {
			sc->sc_rxbuf[sc->sc_rxput] = UART_STAT_OVERRUN;
			break;
		}

		uart_rx_put(sc, reg & 0xff);

		reg = uart_getreg(bas, SFUART_RXDATA);
	}

	uart_unlock(sc->sc_hwmtx);

	return (0);
}

static int
sfuart_bus_transmit(struct uart_softc *sc)
{
	struct uart_bas *bas;
	int i;
	uint32_t reg;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);

	reg = uart_getreg(bas, SFUART_IRQ_ENABLE);
	reg |= SFUART_IRQ_ENABLE_TXWM;
	uart_setreg(bas, SFUART_IRQ_ENABLE, reg);

	for (i = 0; i < sc->sc_txdatasz; i++)
		sfuart_putc(bas, sc->sc_txbuf[i]);

	sc->sc_txbusy = 1;

	uart_unlock(sc->sc_hwmtx);

	return (0);
}

static void
sfuart_bus_grab(struct uart_softc *sc)
{
	struct uart_bas *bas;
	uint32_t reg;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);

	reg = uart_getreg(bas, SFUART_IRQ_ENABLE);
	reg &= ~(SFUART_IRQ_ENABLE_TXWM | SFUART_IRQ_PENDING_RXQM);
	uart_setreg(bas, SFUART_IRQ_ENABLE, reg);

	uart_unlock(sc->sc_hwmtx);
}

static void
sfuart_bus_ungrab(struct uart_softc *sc)
{
	struct uart_bas *bas;
	uint32_t reg;

	bas = &sc->sc_bas;
	uart_lock(sc->sc_hwmtx);

	reg = uart_getreg(bas, SFUART_IRQ_ENABLE);
	reg |= SFUART_IRQ_ENABLE_TXWM | SFUART_IRQ_PENDING_RXQM;
	uart_setreg(bas, SFUART_IRQ_ENABLE, reg);

	uart_unlock(sc->sc_hwmtx);
}

static kobj_method_t sfuart_methods[] = {
	KOBJMETHOD(uart_probe,		sfuart_bus_probe),
	KOBJMETHOD(uart_attach,		sfuart_bus_attach),
	KOBJMETHOD(uart_detach,		sfuart_bus_detach),
	KOBJMETHOD(uart_flush,		sfuart_bus_flush),
	KOBJMETHOD(uart_getsig,		sfuart_bus_getsig),
	KOBJMETHOD(uart_setsig,		sfuart_bus_setsig),
	KOBJMETHOD(uart_ioctl,		sfuart_bus_ioctl),
	KOBJMETHOD(uart_ipend,		sfuart_bus_ipend),
	KOBJMETHOD(uart_param,		sfuart_bus_param),
	KOBJMETHOD(uart_receive,	sfuart_bus_receive),
	KOBJMETHOD(uart_transmit,	sfuart_bus_transmit),
	KOBJMETHOD(uart_grab,		sfuart_bus_grab),
	KOBJMETHOD(uart_ungrab,		sfuart_bus_ungrab),
	KOBJMETHOD_END
};

static struct uart_ops sfuart_ops = {
	.probe = sfuart_probe,
	.init = sfuart_init,
	.term = NULL,
	.putc = sfuart_putc,
	.rxready = sfuart_rxready,
	.getc = sfuart_getc,
};

struct uart_class sfuart_class = {
	"sifiveuart",
	sfuart_methods,
	sizeof(struct sfuart_softc),
	.uc_ops = &sfuart_ops,
	.uc_range = SFUART_REGS_SIZE,
	.uc_rclk = 0,
	.uc_rshift = 0
};

static struct ofw_compat_data compat_data[] = {
	{ "sifive,uart0",	(uintptr_t)&sfuart_class },
	{ NULL,			(uintptr_t)NULL }
};

UART_FDT_CLASS_AND_DEVICE(compat_data);
