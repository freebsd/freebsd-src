/*-
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)zs.c        8.1 (Berkeley) 7/19/93
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*-
 * Copyright (c) 2003 Jake Burkholder.
 * All rights reserved.
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
 *
 */

/*
 * Zilog Z8530 Dual UART driver.
 */

#include "opt_comconsole.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/serial.h>
#include <sys/syslog.h>
#include <sys/tty.h>

#include <dev/zs/z8530reg.h>
#include <dev/zs/z8530var.h>

#define	ZS_READ(sc, r) \
	bus_space_read_1((sc)->sc_bt, (r), 0)
#define	ZS_WRITE(sc, r, v) \
	bus_space_write_1((sc)->sc_bt, (r), 0, (v))

#define	ZS_READ_REG(sc, r) ({ \
	ZS_WRITE((sc), (sc)->sc_csr, (r)); \
	ZS_READ((sc), (sc)->sc_csr); \
})

#define	ZS_WRITE_REG(sc, r, v) ({ \
	ZS_WRITE((sc), (sc)->sc_csr, (r)); \
	ZS_WRITE((sc), (sc)->sc_csr, (v)); \
})

#define	ZSTTY_LOCK(sz)		mtx_lock_spin(&(sc)->sc_mtx)
#define	ZSTTY_UNLOCK(sz)	mtx_unlock_spin(&(sc)->sc_mtx)

static void zs_softintr(void *v);
static void zs_shutdown(void *v);

static int zstty_intr(struct zstty_softc *sc, uint8_t rr3);
static void zstty_softintr(struct zstty_softc *sc) __unused;
static int zstty_param(struct zstty_softc *sc, struct tty *tp,
    struct termios *t);
static void zstty_flush(struct zstty_softc *sc) __unused;
static int zstty_speed(struct zstty_softc *sc, int rate);
static void zstty_load_regs(struct zstty_softc *sc);

static cn_probe_t zs_cnprobe;
static cn_init_t zs_cninit;
static cn_term_t zs_cnterm;
static cn_getc_t zs_cngetc;
static cn_checkc_t zs_cncheckc;
static cn_putc_t zs_cnputc;
static cn_dbctl_t zs_cndbctl;

static int zstty_cngetc(struct zstty_softc *sc);
static int zstty_cncheckc(struct zstty_softc *sc);
static void zstty_cnputc(struct zstty_softc *sc, int c);

static d_open_t zsttyopen;
static d_close_t zsttyclose;

static void zsttystart(struct tty *tp);
static void zsttystop(struct tty *tp, int rw);
static int zsttyparam(struct tty *tp, struct termios *t);
static void zsttybreak(struct tty *tp, int brk);
static int zsttymodem(struct tty *tp, int biton, int bitoff);

static struct cdevsw zstty_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	zsttyopen,
	.d_close =	zsttyclose,
	.d_name =	"zstty",
	.d_flags =	D_TTY | D_NEEDGIANT,
};

static struct zstty_softc *zstty_cons;

CONS_DRIVER(zs, zs_cnprobe, zs_cninit, zs_cnterm, zs_cngetc, zs_cncheckc,
    zs_cnputc, zs_cndbctl);

int
zs_probe(device_t dev)
{

	device_set_desc(dev, "Zilog Z8530");
	return (0);
}

int
zs_attach(device_t dev)
{
	struct device *child[ZS_NCHAN];
	struct zs_softc *sc;
	int i;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	for (i = 0; i < ZS_NCHAN; i++)
		child[i] = device_add_child(dev, "zstty", -1);
	bus_generic_attach(dev);
	for (i = 0; i < ZS_NCHAN; i++)
		sc->sc_child[i] = device_get_softc(child[i]);

	swi_add(&tty_intr_event, "zs", zs_softintr, sc, SWI_TTY,
	    INTR_TYPE_TTY, &sc->sc_softih);

	ZS_WRITE_REG(sc->sc_child[0], 2, sc->sc_child[0]->sc_creg[2]);
	ZS_WRITE_REG(sc->sc_child[0], 9, sc->sc_child[0]->sc_creg[9]);

	if (zstty_cons != NULL) {
		DELAY(50000);
		cninit();
	}

	EVENTHANDLER_REGISTER(shutdown_final, zs_shutdown, sc,
	    SHUTDOWN_PRI_DEFAULT);

	return (0);
}

void
zs_intr(void *v)
{
	struct zs_softc *sc = v;
	int needsoft;
	uint8_t rr3;

	/*
	 * There is only one status register, which is on channel a.  In order
	 * to avoid needing to know which channel we're on in the tty interrupt
	 * handler we shift the channel a status bits into the channel b
	 * bit positions and always test the channel b bits.
	 */
	needsoft = 0;
	rr3 = ZS_READ_REG(sc->sc_child[0], 3);
	if ((rr3 & (ZSRR3_IP_A_RX | ZSRR3_IP_A_TX | ZSRR3_IP_A_STAT)) != 0)
		needsoft |= zstty_intr(sc->sc_child[0], rr3 >> 3);
	if ((rr3 & (ZSRR3_IP_B_RX | ZSRR3_IP_B_TX | ZSRR3_IP_B_STAT)) != 0)
		needsoft |= zstty_intr(sc->sc_child[1], rr3);
	if (needsoft)
		swi_sched(sc->sc_softih, 0);
}

static void
zs_softintr(void *v)
{
	struct zs_softc *sc = v;

	zstty_softintr(sc->sc_child[0]);
	zstty_softintr(sc->sc_child[1]);
}

static void
zs_shutdown(void *v)
{
}

int
zstty_probe(device_t dev)
{
	return (0);
}

int
zstty_attach(device_t dev)
{
	struct zstty_softc *sc;
	struct tty *tp;
	char mode[32];
	int reset;
	int baud;
	int clen;
	char parity;
	int stop;
	char c;

	sc = device_get_softc(dev);
	mtx_init(&sc->sc_mtx, "zstty", NULL, MTX_SPIN);
	sc->sc_dev = dev;
	sc->sc_iput = sc->sc_iget = sc->sc_ibuf;
	sc->sc_oget = sc->sc_obuf;

	tp = ttyalloc();
	tp->t_sc = sc;
	sc->sc_si = make_dev(&zstty_cdevsw, device_get_unit(dev),
	    UID_ROOT, GID_WHEEL, 0600, "%s", device_get_desc(dev));
	sc->sc_si->si_drv1 = sc;
	sc->sc_si->si_tty = tp;
	tp->t_dev = sc->sc_si;
	sc->sc_tty = tp;

	tp->t_oproc = zsttystart;
	tp->t_param = zsttyparam;
	tp->t_modem = zsttymodem;
	tp->t_break = zsttybreak;
	tp->t_stop = zsttystop;
	ttyinitmode(tp, 0, 0);
	tp->t_cflag = CREAD | CLOCAL | CS8;

	if (zstty_console(dev, mode, sizeof(mode))) {
		ttychars(tp);
		/* format: 9600,8,n,1,- */
		if (sscanf(mode, "%d,%d,%c,%d,%c", &baud, &clen, &parity,
		    &stop, &c) == 5) {
			tp->t_ospeed = baud;
			tp->t_ispeed = baud;
			tp->t_cflag = CREAD | CLOCAL;
			switch (clen) {
			case 5:
				tp->t_cflag |= CS5;
				break;
			case 6:
				tp->t_cflag |= CS6;
				break;
			case 7:
				tp->t_cflag |= CS7;
				break;
			case 8:
			default:
				tp->t_cflag |= CS8;
				break;
			}

			if (parity == 'e')
				tp->t_cflag |= PARENB;
			else if (parity == 'o')
				tp->t_cflag |= PARENB | PARODD;

			if (stop == 2)
				tp->t_cflag |= CSTOPB;
		}
		device_printf(dev, "console %s\n", mode);
		sc->sc_console = 1;
		zstty_cons = sc;
	} else {
		if ((device_get_unit(dev) & 1) == 0)
			reset = ZSWR9_A_RESET;
		else
			reset = ZSWR9_B_RESET;
		ZS_WRITE_REG(sc, 9, reset);
	}

	return (0);
}

/*
 * Note that the rr3 value is shifted so the channel a status bits are in the
 * channel b bit positions, which makes the bit positions uniform for both
 * channels.
 */
static int
zstty_intr(struct zstty_softc *sc, uint8_t rr3)
{
	int needsoft;
	uint8_t rr0;
	uint8_t rr1;
	uint8_t c;
	int brk;

	ZSTTY_LOCK(sc);

	ZS_WRITE(sc, sc->sc_csr, ZSWR0_CLR_INTR);

	brk = 0;
	needsoft = 0;
	if ((rr3 & ZSRR3_IP_B_RX) != 0) {
		needsoft = 1;
		do {
			/*
			 * First read the status, because reading the received
			 * char destroys the status of this char.
			 */
			rr1 = ZS_READ_REG(sc, 1);
			c = ZS_READ(sc, sc->sc_data);

			if ((rr1 & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE)) != 0)
				ZS_WRITE(sc, sc->sc_csr, ZSWR0_RESET_ERRORS);
#if defined(KDB) && defined(ALT_BREAK_TO_DEBUGGER)
			if (sc->sc_console != 0)
				brk = kdb_alt_break(c,
				    &sc->sc_alt_break_state);
#endif
			*sc->sc_iput++ = c;
			*sc->sc_iput++ = rr1;
			if (sc->sc_iput == sc->sc_ibuf + sizeof(sc->sc_ibuf))
				sc->sc_iput = sc->sc_ibuf;
		} while ((ZS_READ(sc, sc->sc_csr) & ZSRR0_RX_READY) != 0);
	}

	if ((rr3 & ZSRR3_IP_B_STAT) != 0) {
		rr0 = ZS_READ(sc, sc->sc_csr);
		ZS_WRITE(sc, sc->sc_csr, ZSWR0_RESET_STATUS);
#if defined(KDB) && defined(BREAK_TO_DEBUGGER)
		if (sc->sc_console != 0 && (rr0 & ZSRR0_BREAK) != 0)
			brk = 1;
#endif
		/* XXX do something about flow control */
	}

	if ((rr3 & ZSRR3_IP_B_TX) != 0) {
		/*
		 * If we've delayed a paramter change, do it now.
		 */
		if (sc->sc_preg_held) {
			sc->sc_preg_held = 0;
			zstty_load_regs(sc);
		}
		if (sc->sc_ocnt > 0) {
			ZS_WRITE(sc, sc->sc_data, *sc->sc_oget++);
			sc->sc_ocnt--;
		} else {
			/*
			 * Disable transmit completion interrupts if
			 * necessary.
			 */
			if ((sc->sc_preg[1] & ZSWR1_TIE) != 0) {
				sc->sc_preg[1] &= ~ZSWR1_TIE;
				sc->sc_creg[1] = sc->sc_preg[1];
				ZS_WRITE_REG(sc, 1, sc->sc_creg[1]);
			}
			sc->sc_tx_done = 1;
			sc->sc_tx_busy = 0;
			needsoft = 1;
		}
	}

	ZSTTY_UNLOCK(sc);

	if (brk != 0)
		breakpoint();

	return (needsoft);
}

static void
zstty_softintr(struct zstty_softc *sc)
{
	struct tty *tp = sc->sc_tty;
	int data;
	int stat;

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	while (sc->sc_iget != sc->sc_iput) {
		data = *sc->sc_iget++;
		stat = *sc->sc_iget++;
		if ((stat & ZSRR1_PE) != 0)
			data |= TTY_PE;
		if ((stat & ZSRR1_FE) != 0)
			data |= TTY_FE;
		if (sc->sc_iget == sc->sc_ibuf + sizeof(sc->sc_ibuf))
			sc->sc_iget = sc->sc_ibuf;

		ttyld_rint(tp, data);
	}

	if (sc->sc_tx_done != 0) {
		sc->sc_tx_done = 0;
		tp->t_state &= ~TS_BUSY;
		ttyld_start(tp);
	}
}

static int
zsttyopen(struct cdev *dev, int flags, int mode, struct thread *td)
{
	struct zstty_softc *sc;
	struct tty *tp;
	int error;

	sc = dev->si_drv1;
	tp = dev->si_tty;

	if ((tp->t_state & TS_ISOPEN) != 0 &&
	    (tp->t_state & TS_XCLUDE) != 0 &&
	    suser(td) != 0)
		return (EBUSY);

	if ((tp->t_state & TS_ISOPEN) == 0) {
		struct termios t;

		/*
		 * Enable receive and status interrupts in zstty_param.
		 */
		sc->sc_preg[1] |= ZSWR1_RIE | ZSWR1_SIE;
		sc->sc_iput = sc->sc_iget = sc->sc_ibuf;

		ttyconsolemode(tp, 0);
		/* Make sure zstty_param() will do something. */
		tp->t_ospeed = 0;
		(void)zstty_param(sc, tp, &t);
		ttychars(tp);

		/* XXX turn on DTR */

		/* XXX handle initial DCD */
	}

	error = tty_open(dev, tp);
	if (error != 0)
		return (error);

	error = ttyld_open(tp, dev);
	if (error != 0)
		return (error);

	return (0);
}

static int
zsttyclose(struct cdev *dev, int flags, int mode, struct thread *td)
{
	struct tty *tp;

	tp = dev->si_tty;

	if ((tp->t_state & TS_ISOPEN) == 0)
		return (0);

	ttyld_close(tp, flags);
	tty_close(tp);

	return (0);
}

static void
zsttystart(struct tty *tp)
{
	struct zstty_softc *sc;
	uint8_t c;

	sc = tp->t_sc;

	if ((tp->t_state & TS_TBLOCK) != 0)
		/* XXX clear RTS */;
	else
		/* XXX set RTS */;

	if ((tp->t_state & (TS_BUSY | TS_TIMEOUT | TS_TTSTOP)) != 0) {
		ttwwakeup(tp);
		return;
	}

	if (tp->t_outq.c_cc <= tp->t_olowat) {
		if ((tp->t_state & TS_SO_OLOWAT) != 0) {
			tp->t_state &= ~TS_SO_OLOWAT;
			wakeup(TSA_OLOWAT(tp));
		}
		selwakeuppri(&tp->t_wsel, TTOPRI);
		if (tp->t_outq.c_cc == 0) {
			if ((tp->t_state & (TS_BUSY | TS_SO_OCOMPLETE)) ==
			    TS_SO_OCOMPLETE && tp->t_outq.c_cc == 0) {
				tp->t_state &= ~TS_SO_OCOMPLETE;
				wakeup(TSA_OCOMPLETE(tp));
			}
			return;
		}
	}

	sc->sc_ocnt = q_to_b(&tp->t_outq, sc->sc_obuf, sizeof(sc->sc_obuf));
	if (sc->sc_ocnt == 0)
		return;
	c = sc->sc_obuf[0];
	sc->sc_oget = sc->sc_obuf + 1;
	sc->sc_ocnt--;

	tp->t_state |= TS_BUSY;
	sc->sc_tx_busy = 1;

	/*
	 * Enable transmit interrupts if necessary and send the first
	 * character to start up the transmitter.
	 */
	if ((sc->sc_preg[1] & ZSWR1_TIE) == 0) {
		sc->sc_preg[1] |= ZSWR1_TIE;
		sc->sc_creg[1] = sc->sc_preg[1];
		ZS_WRITE_REG(sc, 1, sc->sc_creg[1]);
	}
	ZS_WRITE(sc, sc->sc_data, c);

	ttwwakeup(tp);
}

static void
zsttystop(struct tty *tp, int flag)
{
	struct zstty_softc *sc;

	sc = tp->t_sc;

	if ((flag & FREAD) != 0) {
		/* XXX stop reading, anything to do? */;
	}

	if ((flag & FWRITE) != 0) {
		if ((tp->t_state & TS_BUSY) != 0) {
			/* XXX do what? */
			if ((tp->t_state & TS_TTSTOP) == 0)
				tp->t_state |= TS_FLUSH;
		}
	}
}

static int
zsttyparam(struct tty *tp, struct termios *t)
{
	struct zstty_softc *sc;

	sc = tp->t_sc;
	return (zstty_param(sc, tp, t));
}


static void
zsttybreak(struct tty *tp, int brk)
{
	struct zstty_softc *sc;

	sc = tp->t_sc;

	if (brk)
		ZS_WRITE_REG(sc, 5, ZS_READ_REG(sc, 5) | ZSWR5_BREAK);
	else
		ZS_WRITE_REG(sc, 5, ZS_READ_REG(sc, 5) & ~ZSWR5_BREAK);
}

static int
zsttymodem(struct tty *tp, int biton, int bitoff)
{
	/* XXX implement! */
	return (0);
}

static int
zstty_param(struct zstty_softc *sc, struct tty *tp, struct termios *t)
{
	tcflag_t cflag;
	uint8_t wr3;
	uint8_t wr4;
	uint8_t wr5;
	int ospeed;

	ospeed = zstty_speed(sc, t->c_ospeed);
	if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return (EINVAL);

	/*
	 * If there were no changes, don't do anything.  This avoids dropping
	 * input and improves performance when all we did was frob things like
	 * VMIN and VTIME.
	 */
	if (tp->t_ospeed == t->c_ospeed &&
	    tp->t_cflag == t->c_cflag)
		return (0);

	if (t->c_ospeed != 0)
		zsttymodem(tp, SER_DTR, 0);
	else
		zsttymodem(tp, 0, SER_DTR);

	cflag = t->c_cflag;

	if (sc->sc_console != 0) {
		cflag |= CLOCAL;
		cflag &= ~HUPCL;
	}

	wr3 = ZSWR3_RX_ENABLE;
	wr5 = ZSWR5_TX_ENABLE | ZSWR5_DTR | ZSWR5_RTS;

	switch (cflag & CSIZE) {
	case CS5:
		wr3 |= ZSWR3_RX_5;
		wr5 |= ZSWR5_TX_5;
		break;
	case CS6:
		wr3 |= ZSWR3_RX_6;
		wr5 |= ZSWR5_TX_6;
		break;
	case CS7:
		wr3 |= ZSWR3_RX_7;
		wr5 |= ZSWR5_TX_7;
		break;
	case CS8:
	default:
		wr3 |= ZSWR3_RX_8;
		wr5 |= ZSWR5_TX_8;
		break;
	}

	wr4 = ZSWR4_CLK_X16 | (cflag & CSTOPB ? ZSWR4_TWOSB : ZSWR4_ONESB);
	if ((cflag & PARODD) == 0)
		wr4 |= ZSWR4_EVENP;
	if (cflag & PARENB)
		wr4 |= ZSWR4_PARENB;

	tp->t_ispeed = 0;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = cflag;

	ttsetwater(tp);

	ZSTTY_LOCK(sc);

	sc->sc_preg[3] = wr3;
	sc->sc_preg[4] = wr4;
	sc->sc_preg[5] = wr5;

	zstty_set_speed(sc, ospeed);

	if (cflag & CRTSCTS)
		sc->sc_preg[15] |= ZSWR15_CTS_IE;
	else
		sc->sc_preg[15] &= ~ZSWR15_CTS_IE;

	zstty_load_regs(sc);

	ZSTTY_UNLOCK(sc);
	
	return (0);
}

static void
zstty_flush(struct zstty_softc *sc)
{
	uint8_t rr0;
	uint8_t rr1;
	uint8_t c;

	for (;;) {
		rr0 = ZS_READ(sc, sc->sc_csr);
		if ((rr0 & ZSRR0_RX_READY) == 0)
			break;

		rr1 = ZS_READ_REG(sc, 1);
		c = ZS_READ(sc, sc->sc_data);

		if (rr1 & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE))
			ZS_WRITE(sc, sc->sc_data, ZSWR0_RESET_ERRORS);
	}
}

static void
zstty_load_regs(struct zstty_softc *sc)
{

	/*
	 * If the transmitter may be active, just hold the change and do it
	 * in the tx interrupt handler.  Changing the registers while tx is
	 * active may hang the chip.
	 */
	if (sc->sc_tx_busy != 0) {
		sc->sc_preg_held = 1;
		return;
	}

	/* If the regs are the same do nothing. */
	if (bcmp(sc->sc_preg, sc->sc_creg, 16) == 0)
		return;

	bcopy(sc->sc_preg, sc->sc_creg, 16);

	/* XXX: reset error condition */
	ZS_WRITE(sc, sc->sc_csr, ZSM_RESET_ERR);

	/* disable interrupts */
	ZS_WRITE_REG(sc, 1, sc->sc_creg[1] & ~ZSWR1_IMASK);

	/* baud clock divisor, stop bits, parity */
	ZS_WRITE_REG(sc, 4, sc->sc_creg[4]);

	/* misc. TX/RX control bits */
	ZS_WRITE_REG(sc, 10, sc->sc_creg[10]);

	/* char size, enable (RX/TX) */
	ZS_WRITE_REG(sc, 3, sc->sc_creg[3] & ~ZSWR3_RX_ENABLE);
	ZS_WRITE_REG(sc, 5, sc->sc_creg[5] & ~ZSWR5_TX_ENABLE);

	/* Shut down the BRG */
	ZS_WRITE_REG(sc, 14, sc->sc_creg[14] & ~ZSWR14_BAUD_ENA);

	/* clock mode control */
	ZS_WRITE_REG(sc, 11, sc->sc_creg[11]);

	/* baud rate (lo/hi) */
	ZS_WRITE_REG(sc, 12, sc->sc_creg[12]);
	ZS_WRITE_REG(sc, 13, sc->sc_creg[13]);

	/* Misc. control bits */
	ZS_WRITE_REG(sc, 14, sc->sc_creg[14]);

	/* which lines cause status interrupts */
	ZS_WRITE_REG(sc, 15, sc->sc_creg[15]);

	/*
	 * Zilog docs recommend resetting external status twice at this
	 * point. Mainly as the status bits are latched, and the first
	 * interrupt clear might unlatch them to new values, generating
	 * a second interrupt request.
	 */
	ZS_WRITE(sc, sc->sc_csr, ZSM_RESET_STINT);
	ZS_WRITE(sc, sc->sc_csr, ZSM_RESET_STINT);

	/* char size, enable (RX/TX)*/
	ZS_WRITE_REG(sc, 3, sc->sc_creg[3]);
	ZS_WRITE_REG(sc, 5, sc->sc_creg[5]);

	/* interrupt enables: RX, TX, STATUS */
	ZS_WRITE_REG(sc, 1, sc->sc_creg[1]);
}

static int
zstty_speed(struct zstty_softc *sc, int rate)
{
	int tconst;

	if (rate == 0)
		return (0);
	tconst = BPS_TO_TCONST(sc->sc_brg_clk, rate);
	if (tconst < 0 || TCONST_TO_BPS(sc->sc_brg_clk, tconst) != rate)
		return (-1);
	return (tconst);
}

static void
zs_cnprobe(struct consdev *cn)
{
	struct zstty_softc *sc = zstty_cons;

	if (sc == NULL)
		cn->cn_pri = CN_DEAD;
	else {
		cn->cn_pri = CN_REMOTE;
		strcpy(cn->cn_name, devtoname(sc->sc_si));
		cn->cn_tp = sc->sc_tty;
	}
}

static void
zs_cninit(struct consdev *cn)
{
}

static void
zs_cnterm(struct consdev *cn)
{
}

static int
zs_cngetc(struct consdev *cn)
{
	struct zstty_softc *sc = zstty_cons;

	if (sc == NULL)
		return (-1);
	return (zstty_cngetc(sc));
}

static int
zs_cncheckc(struct consdev *cn)
{
	struct zstty_softc *sc = zstty_cons;

	if (sc == NULL)
		return (-1);
	return (zstty_cncheckc(sc));
}

static void
zs_cnputc(struct consdev *cn, int c)
{
	struct zstty_softc *sc = zstty_cons;

	if (sc == NULL)
		return;
	zstty_cnputc(sc, c);
}

static void
zs_cndbctl(struct consdev *cn, int c)
{
}

static void
zstty_cnopen(struct zstty_softc *sc)
{
}

static void
zstty_cnclose(struct zstty_softc *sc)
{
}

static int
zstty_cngetc(struct zstty_softc *sc)
{
	uint8_t c;

	zstty_cnopen(sc);
	while ((ZS_READ(sc, sc->sc_csr) & ZSRR0_RX_READY) == 0)
		;
	c = ZS_READ(sc, sc->sc_data);
	zstty_cnclose(sc);
	return (c);
}

static int
zstty_cncheckc(struct zstty_softc *sc)
{
	int c;

	c = -1;
	zstty_cnopen(sc);
	if ((ZS_READ(sc, sc->sc_csr) & ZSRR0_RX_READY) != 0)
		c = ZS_READ(sc, sc->sc_data);
	zstty_cnclose(sc);
	return (c);
}

static void
zstty_cnputc(struct zstty_softc *sc, int c)
{

	zstty_cnopen(sc);
	while ((ZS_READ(sc, sc->sc_csr) & ZSRR0_TX_READY) == 0)
		;
	ZS_WRITE(sc, sc->sc_data, c);
	zstty_cnclose(sc);
}
