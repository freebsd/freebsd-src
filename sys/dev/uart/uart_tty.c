/*-
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
__FBSDID("$FreeBSD: src/sys/dev/uart/uart_tty.c,v 1.29 2006/07/27 00:07:10 marcel Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/reboot.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/termios.h>
#include <sys/tty.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include "uart_if.h"

static cn_probe_t uart_cnprobe;
static cn_init_t uart_cninit;
static cn_term_t uart_cnterm;
static cn_getc_t uart_cngetc;
static cn_putc_t uart_cnputc;

CONSOLE_DRIVER(uart);

static struct uart_devinfo uart_console;

static void
uart_cnprobe(struct consdev *cp)
{

	cp->cn_pri = CN_DEAD;

	KASSERT(uart_console.cookie == NULL, ("foo"));

	if (uart_cpu_getdev(UART_DEV_CONSOLE, &uart_console))
		return;

	if (uart_probe(&uart_console))
		return;

	strlcpy(cp->cn_name, uart_driver_name, sizeof(cp->cn_name));
	cp->cn_pri = (boothowto & RB_SERIAL) ? CN_REMOTE : CN_NORMAL;
	cp->cn_arg = &uart_console;
}

static void
uart_cninit(struct consdev *cp)
{
	struct uart_devinfo *di;

	/*
	 * Yedi trick: we need to be able to define cn_dev before we go
	 * single- or multi-user. The problem is that we don't know at
	 * this time what the device will be. Hence, we need to link from
	 * the uart_devinfo to the consdev that corresponds to it so that
	 * we can define cn_dev in uart_bus_attach() when we find the
	 * device during bus enumeration. That's when we'll know what the
	 * the unit number will be.
	 */
	di = cp->cn_arg;
	KASSERT(di->cookie == NULL, ("foo"));
	di->cookie = cp;
	di->type = UART_DEV_CONSOLE;
	uart_add_sysdev(di);
	uart_init(di);
}

static void
uart_cnterm(struct consdev *cp)
{

	uart_term(cp->cn_arg);
}

static void
uart_cnputc(struct consdev *cp, int c)
{

	uart_putc(cp->cn_arg, c);
}

static int
uart_cngetc(struct consdev *cp)
{

	return (uart_poll(cp->cn_arg));
}

static int
uart_tty_open(struct tty *tp, struct cdev *dev)
{
	struct uart_softc *sc;

	sc = tp->t_sc;

	if (sc == NULL || sc->sc_leaving)
		return (ENXIO);

	sc->sc_opened = 1;
	return (0);
}

static void
uart_tty_close(struct tty *tp)
{
	struct uart_softc *sc;

	sc = tp->t_sc;
	if (sc == NULL || sc->sc_leaving || !sc->sc_opened) 
		return;

	if (sc->sc_hwiflow)
		UART_IOCTL(sc, UART_IOCTL_IFLOW, 0);
	if (sc->sc_hwoflow)
		UART_IOCTL(sc, UART_IOCTL_OFLOW, 0);
	if (sc->sc_sysdev == NULL)
		UART_SETSIG(sc, SER_DDTR | SER_DRTS);

	wakeup(sc);
	sc->sc_opened = 0;
	return;
}

static void
uart_tty_oproc(struct tty *tp)
{
	struct uart_softc *sc;

	sc = tp->t_sc;
	if (sc == NULL || sc->sc_leaving)
		return;

	/*
	 * Handle input flow control. Note that if we have hardware support,
	 * we don't do anything here. We continue to receive until our buffer
	 * is full. At that time we cannot empty the UART itself and it will
	 * de-assert RTS for us. In that situation we're completely stuffed.
	 * Without hardware support, we need to toggle RTS ourselves.
	 */
	if ((tp->t_cflag & CRTS_IFLOW) && !sc->sc_hwiflow) {
		if ((tp->t_state & TS_TBLOCK) &&
		    (sc->sc_hwsig & SER_RTS))
			UART_SETSIG(sc, SER_DRTS);
		else if (!(tp->t_state & TS_TBLOCK) &&
		    !(sc->sc_hwsig & SER_RTS))
			UART_SETSIG(sc, SER_DRTS|SER_RTS);
	}

	if (tp->t_state & TS_TTSTOP)
		return;

	if ((tp->t_state & TS_BUSY) || sc->sc_txbusy)
		return;

	if (tp->t_outq.c_cc == 0) {
		ttwwakeup(tp);
		return;
	}

	sc->sc_txdatasz = q_to_b(&tp->t_outq, sc->sc_txbuf, sc->sc_txfifosz);
	tp->t_state |= TS_BUSY;
	UART_TRANSMIT(sc);
	ttwwakeup(tp);
}

static int
uart_tty_param(struct tty *tp, struct termios *t)
{
	struct uart_softc *sc;
	int databits, parity, stopbits;

	sc = tp->t_sc;
	if (sc == NULL || sc->sc_leaving)
		return (ENODEV);
	if (t->c_ispeed != t->c_ospeed && t->c_ospeed != 0)
		return (EINVAL);
	/* Fixate certain parameters for system devices. */
	if (sc->sc_sysdev != NULL) {
		t->c_ispeed = t->c_ospeed = sc->sc_sysdev->baudrate;
		t->c_cflag |= CLOCAL;
		t->c_cflag &= ~HUPCL;
	}
	if (t->c_ospeed == 0) {
		UART_SETSIG(sc, SER_DDTR | SER_DRTS);
		return (0);
	}
	switch (t->c_cflag & CSIZE) {
	case CS5:	databits = 5; break;
	case CS6:	databits = 6; break;
	case CS7:	databits = 7; break;
	default:	databits = 8; break;
	}
	stopbits = (t->c_cflag & CSTOPB) ? 2 : 1;
	if (t->c_cflag & PARENB)
		parity = (t->c_cflag & PARODD) ? UART_PARITY_ODD
		    : UART_PARITY_EVEN;
	else
		parity = UART_PARITY_NONE;
	if (UART_PARAM(sc, t->c_ospeed, databits, stopbits, parity) != 0)
		return (EINVAL);
	UART_SETSIG(sc, SER_DDTR | SER_DTR);
	/* Set input flow control state. */
	if (!sc->sc_hwiflow) {
		if ((t->c_cflag & CRTS_IFLOW) && (tp->t_state & TS_TBLOCK))
			UART_SETSIG(sc, SER_DRTS);
		else
			UART_SETSIG(sc, SER_DRTS | SER_RTS);
	} else
		UART_IOCTL(sc, UART_IOCTL_IFLOW, (t->c_cflag & CRTS_IFLOW));
	/* Set output flow control state. */
	if (sc->sc_hwoflow)
		UART_IOCTL(sc, UART_IOCTL_OFLOW, (t->c_cflag & CCTS_OFLOW));
	ttsetwater(tp);
	return (0);
}

static int
uart_tty_modem(struct tty *tp, int biton, int bitoff)
{
	struct uart_softc *sc;

	sc = tp->t_sc;
	if (biton != 0 || bitoff != 0)
		UART_SETSIG(sc, SER_DELTA(bitoff|biton) | biton);
	return (sc->sc_hwsig);
}

static void
uart_tty_break(struct tty *tp, int state)
{
	struct uart_softc *sc;

	sc = tp->t_sc;
	UART_IOCTL(sc, UART_IOCTL_BREAK, state);
}

static void
uart_tty_stop(struct tty *tp, int rw)
{
	struct uart_softc *sc;

	sc = tp->t_sc;
	if (sc == NULL || sc->sc_leaving)
		return;
	if (rw & FWRITE) {
		if (sc->sc_txbusy) {
			sc->sc_txbusy = 0;
			UART_FLUSH(sc, UART_FLUSH_TRANSMITTER);
		}
		tp->t_state &= ~TS_BUSY;
	}
	if (rw & FREAD) {
		UART_FLUSH(sc, UART_FLUSH_RECEIVER);
		sc->sc_rxget = sc->sc_rxput = 0;
	}
}

void
uart_tty_intr(void *arg)
{
	struct uart_softc *sc = arg;
	struct tty *tp;
	int c, pend, sig, xc;

	if (sc->sc_leaving)
		return;

	pend = atomic_readandclear_32(&sc->sc_ttypend);
	if (!(pend & SER_INT_MASK))
		return;

	tp = sc->sc_u.u_tty.tp;

	if (pend & SER_INT_RXREADY) {
		while (!uart_rx_empty(sc) && !(tp->t_state & TS_TBLOCK)) {
			xc = uart_rx_get(sc);
			c = xc & 0xff;
			if (xc & UART_STAT_FRAMERR)
				c |= TTY_FE;
			if (xc & UART_STAT_OVERRUN)
				c |= TTY_OE;
			if (xc & UART_STAT_PARERR)
				c |= TTY_PE;
			ttyld_rint(tp, c);
		}
	}

	if (pend & SER_INT_BREAK) {
		if (tp != NULL && !(tp->t_iflag & IGNBRK))
			ttyld_rint(tp, 0);
	}

	if (pend & SER_INT_SIGCHG) {
		sig = pend & SER_INT_SIGMASK;
		if (sig & SER_DDCD)
			ttyld_modem(tp, sig & SER_DCD);
		if ((sig & SER_DCTS) && (tp->t_cflag & CCTS_OFLOW) &&
		    !sc->sc_hwoflow) {
			if (sig & SER_CTS) {
				tp->t_state &= ~TS_TTSTOP;
				ttyld_start(tp);
			} else
				tp->t_state |= TS_TTSTOP;
		}
	}

	if (pend & SER_INT_TXIDLE) {
		tp->t_state &= ~TS_BUSY;
		ttyld_start(tp);
	}
}

int
uart_tty_attach(struct uart_softc *sc)
{
	struct tty *tp;
	int unit;

	tp = ttyalloc();
	sc->sc_u.u_tty.tp = tp;
	tp->t_sc = sc;

	unit = device_get_unit(sc->sc_dev);

	tp->t_oproc = uart_tty_oproc;
	tp->t_param = uart_tty_param;
	tp->t_stop = uart_tty_stop;
	tp->t_modem = uart_tty_modem;
	tp->t_break = uart_tty_break;
	tp->t_open = uart_tty_open;
	tp->t_close = uart_tty_close;

	tp->t_pps = &sc->sc_pps;

	if (sc->sc_sysdev != NULL && sc->sc_sysdev->type == UART_DEV_CONSOLE) {
		sprintf(((struct consdev *)sc->sc_sysdev->cookie)->cn_name,
		    "ttyu%r", unit);
		ttyconsolemode(tp, 0);
	}

	swi_add(&tty_intr_event, uart_driver_name, uart_tty_intr, sc, SWI_TTY,
	    INTR_TYPE_TTY, &sc->sc_softih);

	ttycreate(tp, TS_CALLOUT, "u%r", unit);

	return (0);
}

int uart_tty_detach(struct uart_softc *sc)
{
	struct tty *tp;

	tp = sc->sc_u.u_tty.tp;
	tp->t_pps = NULL;
	ttygone(tp);
	swi_remove(sc->sc_softih);
	ttyfree(tp);

	return (0);
}
