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

#define	UART_MINOR_CALLOUT	0x10000

static cn_probe_t uart_cnprobe;
static cn_init_t uart_cninit;
static cn_term_t uart_cnterm;
static cn_getc_t uart_cngetc;
static cn_checkc_t uart_cncheckc;
static cn_putc_t uart_cnputc;

CONS_DRIVER(uart, uart_cnprobe, uart_cninit, uart_cnterm, uart_cngetc,
    uart_cncheckc, uart_cnputc, NULL);

static d_open_t uart_tty_open;
static d_close_t uart_tty_close;
static d_ioctl_t uart_tty_ioctl;

static struct cdevsw uart_cdevsw = {
	.d_open = uart_tty_open,
	.d_close = uart_tty_close,
	.d_read = ttyread,
	.d_write = ttywrite,
	.d_ioctl = uart_tty_ioctl,
	.d_poll = ttypoll,
	.d_name = uart_driver_name,
	.d_maj = MAJOR_AUTO,
	.d_flags = D_TTY,
	.d_kqfilter = ttykqfilter,
};

static struct uart_devinfo uart_console;

static void
uart_cnprobe(struct consdev *cp)
{

	cp->cn_dev = NULL;
	cp->cn_pri = CN_DEAD;

	KASSERT(uart_console.cookie == NULL, ("foo"));

	if (uart_cpu_getdev(UART_DEV_CONSOLE, &uart_console))
		return;

	if (uart_probe(&uart_console))
		return;

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
uart_cncheckc(struct consdev *cp)
{

	return (uart_poll(cp->cn_arg));
}

static int
uart_cngetc(struct consdev *cp)
{

	return (uart_getc(cp->cn_arg));
}

static void
uart_tty_oproc(struct tty *tp)
{
	struct uart_softc *sc;

	KASSERT(tp->t_dev != NULL, ("foo"));
	sc = tp->t_dev->si_drv1;
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
		    (sc->sc_hwsig & UART_SIG_RTS))
			UART_SETSIG(sc, UART_SIG_DRTS);
		else if (!(tp->t_state & TS_TBLOCK) &&
		    !(sc->sc_hwsig & UART_SIG_RTS))
			UART_SETSIG(sc, UART_SIG_DRTS|UART_SIG_RTS);
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

	KASSERT(tp->t_dev != NULL, ("foo"));
	sc = tp->t_dev->si_drv1;
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
		UART_SETSIG(sc, UART_SIG_DDTR | UART_SIG_DRTS);
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
	UART_PARAM(sc, t->c_ospeed, databits, stopbits, parity);
	UART_SETSIG(sc, UART_SIG_DDTR | UART_SIG_DTR);
	/* Set input flow control state. */
	if (!sc->sc_hwiflow) {
		if ((t->c_cflag & CRTS_IFLOW) && (tp->t_state & TS_TBLOCK))
			UART_SETSIG(sc, UART_SIG_DRTS);
		else
			UART_SETSIG(sc, UART_SIG_DRTS | UART_SIG_RTS);
	} else
		UART_IOCTL(sc, UART_IOCTL_IFLOW, (t->c_cflag & CRTS_IFLOW));
	/* Set output flow control state. */
	if (sc->sc_hwoflow)
		UART_IOCTL(sc, UART_IOCTL_OFLOW, (t->c_cflag & CCTS_OFLOW));
	ttsetwater(tp);
	return (0);
}

static void
uart_tty_stop(struct tty *tp, int rw)
{
	struct uart_softc *sc;

	KASSERT(tp->t_dev != NULL, ("foo"));
	sc = tp->t_dev->si_drv1;
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
	if (!(pend & UART_IPEND_MASK))
		return;

	tp = sc->sc_u.u_tty.tp;

	if (pend & UART_IPEND_RXREADY) {
		while (!uart_rx_empty(sc) && !(tp->t_state & TS_TBLOCK)) {
			xc = uart_rx_get(sc);
			c = xc & 0xff;
			if (xc & UART_STAT_FRAMERR)
				c |= TTY_FE;
			if (xc & UART_STAT_PARERR)
				c |= TTY_PE;
			(*linesw[tp->t_line].l_rint)(c, tp);
		}
	}

	if (pend & UART_IPEND_BREAK) {
		if (tp != NULL && !(tp->t_iflag & IGNBRK))
			(*linesw[tp->t_line].l_rint)(0, tp);
	}

	if (pend & UART_IPEND_SIGCHG) {
		sig = pend & UART_IPEND_SIGMASK;
		if (sig & UART_SIG_DDCD)
			(*linesw[tp->t_line].l_modem)(tp, sig & UART_SIG_DCD);
		if ((sig & UART_SIG_DCTS) && (tp->t_cflag & CCTS_OFLOW) &&
		    !sc->sc_hwoflow) {
			if (sig & UART_SIG_CTS) {
				tp->t_state &= ~TS_TTSTOP;
				(*linesw[tp->t_line].l_start)(tp);
			} else
				tp->t_state |= TS_TTSTOP;
		}
	}

	if (pend & UART_IPEND_TXIDLE) {
		tp->t_state &= ~TS_BUSY;
		(*linesw[tp->t_line].l_start)(tp);
	}
}

int
uart_tty_attach(struct uart_softc *sc)
{
	struct tty *tp;

	tp = ttymalloc(NULL);
	sc->sc_u.u_tty.tp = tp;

	sc->sc_u.u_tty.si[0] = make_dev(&uart_cdevsw,
	    device_get_unit(sc->sc_dev), UID_ROOT, GID_WHEEL, 0600, "ttyu%r",
	    device_get_unit(sc->sc_dev));
	sc->sc_u.u_tty.si[0]->si_drv1 = sc;
	sc->sc_u.u_tty.si[0]->si_tty = tp;
	sc->sc_u.u_tty.si[1] = make_dev(&uart_cdevsw,
	    device_get_unit(sc->sc_dev) | UART_MINOR_CALLOUT, UID_UUCP,
	    GID_DIALER, 0660, "uart%r", device_get_unit(sc->sc_dev));
	sc->sc_u.u_tty.si[1]->si_drv1 = sc;
	sc->sc_u.u_tty.si[1]->si_tty = tp;

	tp->t_oproc = uart_tty_oproc;
	tp->t_param = uart_tty_param;
	tp->t_stop = uart_tty_stop;

	if (sc->sc_sysdev != NULL && sc->sc_sysdev->type == UART_DEV_CONSOLE) {
		((struct consdev *)sc->sc_sysdev->cookie)->cn_dev =
		    makedev(uart_cdevsw.d_maj, device_get_unit(sc->sc_dev));
	}

	swi_add(&tty_ithd, uart_driver_name, uart_tty_intr, sc, SWI_TTY,
	    INTR_TYPE_TTY, &sc->sc_softih);

	return (0);
}

int uart_tty_detach(struct uart_softc *sc)
{

	ithread_remove_handler(sc->sc_softih);
	destroy_dev(sc->sc_u.u_tty.si[0]);
	destroy_dev(sc->sc_u.u_tty.si[1]);
	/* ttyfree(sc->sc_u.u_tty.tp); */

	return (0);
}

static int
uart_tty_open(dev_t dev, int flags, int mode, struct thread *td)
{
	struct uart_softc *sc;
	struct tty *tp;
	int error;

	sc = dev->si_drv1;
	if (sc == NULL || sc->sc_leaving)
		return (ENODEV);

	tp = dev->si_tty;

 loop:
	if (sc->sc_opened) {
		KASSERT(tp->t_state & TS_ISOPEN, ("foo"));
		/*
		 * The device is open, so everything has been initialized.
		 * Handle conflicts.
		 */
		if (minor(dev) & UART_MINOR_CALLOUT) {
			if (!sc->sc_callout)
				return (EBUSY);
		} else {
			if (sc->sc_callout) {
				if (flags & O_NONBLOCK)
					return (EBUSY);
				error =	tsleep(sc, TTIPRI|PCATCH, "uartbi", 0);
				if (error)
					return (error);
				sc = dev->si_drv1;
				if (sc == NULL || sc->sc_leaving)
					return (ENODEV);
				goto loop;
			}
		}
		if (tp->t_state & TS_XCLUDE && suser(td) != 0)
			return (EBUSY);
	} else {
		KASSERT(!(tp->t_state & TS_ISOPEN), ("foo"));
		/*
		 * The device isn't open, so there are no conflicts.
		 * Initialize it.  Initialization is done twice in many
		 * cases: to preempt sleeping callin opens if we are
		 * callout, and to complete a callin open after DCD rises.
		 */
		sc->sc_callout = (minor(dev) & UART_MINOR_CALLOUT) ? 1 : 0;
		tp->t_dev = dev;

		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ttychars(tp);
		error = uart_tty_param(tp, &tp->t_termios);
		if (error)
			return (error);
		/*
		 * Handle initial DCD.
		 */
		if ((sc->sc_hwsig & UART_SIG_DCD) || sc->sc_callout)
			(*linesw[tp->t_line].l_modem)(tp, 1);
	}
	/*
	 * Wait for DCD if necessary.
	 */
	if (!(tp->t_state & TS_CARR_ON) && !sc->sc_callout &&
	    !(tp->t_cflag & CLOCAL) && !(flags & O_NONBLOCK)) {
		error = tsleep(TSA_CARR_ON(tp), TTIPRI|PCATCH, "uartdcd", 0);
		if (error)
			return (error);
		sc = dev->si_drv1;
		if (sc == NULL || sc->sc_leaving)
			return (ENODEV);
		goto loop;
	}
	error = ttyopen(dev, tp);
	if (error)
		return (error);
	error = (*linesw[tp->t_line].l_open)(dev, tp);
	if (error)
		return (error);

	KASSERT(tp->t_state & TS_ISOPEN, ("foo"));
	sc->sc_opened = 1;
	return (0);
}

static int
uart_tty_close(dev_t dev, int flags, int mode, struct thread *td)
{
	struct uart_softc *sc;
	struct tty *tp;

	sc = dev->si_drv1;
	if (sc == NULL || sc->sc_leaving)
		return (ENODEV);
	tp = dev->si_tty;
	if (!sc->sc_opened) {
		KASSERT(!(tp->t_state & TS_ISOPEN), ("foo"));
		return (0);
	}
	KASSERT(tp->t_state & TS_ISOPEN, ("foo"));

	if (sc->sc_hwiflow)
		UART_IOCTL(sc, UART_IOCTL_IFLOW, 0);
	if (sc->sc_hwoflow)
		UART_IOCTL(sc, UART_IOCTL_OFLOW, 0);
	if (sc->sc_sysdev == NULL)
		UART_SETSIG(sc, UART_SIG_DDTR | UART_SIG_DRTS);

	(*linesw[tp->t_line].l_close)(tp, flags);
	ttyclose(tp);
	wakeup(sc);
	wakeup(TSA_CARR_ON(tp));
	KASSERT(!(tp->t_state & TS_ISOPEN), ("foo"));
	sc->sc_opened = 0;
	return (0);
}

static int
uart_tty_ioctl(dev_t dev, u_long cmd, caddr_t data, int flags,
    struct thread *td)
{
	struct uart_softc *sc;
	struct tty *tp;
	int bits, error, sig;

	sc = dev->si_drv1;
	if (sc == NULL || sc->sc_leaving)
		return (ENODEV);

	tp = dev->si_tty;
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flags, td);
	if (error != ENOIOCTL)
		return (error);
	error = ttioctl(tp, cmd, data, flags);
	if (error != ENOIOCTL)
		return (error);

	switch (cmd) {
	case TIOCSBRK:
		UART_IOCTL(sc, UART_IOCTL_BREAK, 1);
		break;
	case TIOCCBRK:
		UART_IOCTL(sc, UART_IOCTL_BREAK, 0);
		break;
	case TIOCSDTR:
		UART_SETSIG(sc, UART_SIG_DDTR | UART_SIG_DTR);
		break;
	case TIOCCDTR:
		UART_SETSIG(sc, UART_SIG_DDTR);
		break;
	case TIOCMSET:
		bits = *(int*)data;
		sig = UART_SIG_DDTR | UART_SIG_DRTS;
		if (bits & TIOCM_DTR)
			sig |= UART_SIG_DTR;
		if (bits & TIOCM_RTS)
			sig |= UART_SIG_RTS;
		UART_SETSIG(sc, sig);
		break;
        case TIOCMBIS:
		bits = *(int*)data;
		sig = 0;
		if (bits & TIOCM_DTR)
			sig |= UART_SIG_DDTR | UART_SIG_DTR;
		if (bits & TIOCM_RTS)
			sig |= UART_SIG_DRTS | UART_SIG_RTS;
		UART_SETSIG(sc, sig);
		break;
        case TIOCMBIC:
		bits = *(int*)data;
		sig = 0;
		if (bits & TIOCM_DTR)
			sig |= UART_SIG_DDTR;
		if (bits & TIOCM_RTS)
			sig |= UART_SIG_DRTS;
		UART_SETSIG(sc, sig);
		break;
        case TIOCMGET:
		sig = sc->sc_hwsig;
		bits = TIOCM_LE;
		if (sig & UART_SIG_DTR)
			bits |= TIOCM_DTR;
		if (sig & UART_SIG_RTS)
			bits |= TIOCM_RTS;
		if (sig & UART_SIG_DSR)
			bits |= TIOCM_DSR;
		if (sig & UART_SIG_CTS)
			bits |= TIOCM_CTS;
		if (sig & UART_SIG_DCD)
			bits |= TIOCM_CD;
		if (sig & (UART_SIG_DRI | UART_SIG_RI))
			bits |= TIOCM_RI;
		*(int*)data = bits;
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}
