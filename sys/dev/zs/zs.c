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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 * $FreeBSD$
 */

/*
 * Zilog Z8530 Dual UART driver.
 */

#include "opt_ddb.h"
#include "opt_comconsole.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/syslog.h>
#include <sys/tty.h>

#include <ddb/ddb.h>

#include <ofw/openfirm.h>
#include <sparc64/sbus/sbusvar.h>

#include <dev/zs/z8530reg.h>

#define	CDEV_MAJOR	182

#define	ZS_READ(sc, r) \
	bus_space_read_1((sc)->sc_bt, (sc)->sc_bh, (r))
#define	ZS_WRITE(sc, r, v) \
	bus_space_write_1((sc)->sc_bt, (sc)->sc_bh, (r), (v))

#define	ZS_READ_REG(sc, r) ({ \
	ZS_WRITE((sc), ZS_CSR, (r)); \
	ZS_READ((sc), ZS_CSR); \
})

#define	ZS_WRITE_REG(sc, r, v) ({ \
	ZS_WRITE((sc), ZS_CSR, (r)); \
	ZS_WRITE((sc), ZS_CSR, (v)); \
})

#define	ZSTTY_LOCK(sz)		mtx_lock_spin(&(sc)->sc_mtx)
#define	ZSTTY_UNLOCK(sz)	mtx_unlock_spin(&(sc)->sc_mtx)

struct zstty_softc {
	device_t		sc_dev;
	struct zs_softc		*sc_parent;
	bus_space_tag_t		sc_bt;
	bus_space_handle_t	sc_bh;
	dev_t			sc_si;
	struct tty		*sc_tty;
	int			sc_channel;
	int			sc_icnt;
	uint8_t			*sc_iput;
	uint8_t			*sc_iget;
	uint8_t			*sc_oget;
	int			sc_ocnt;
	int			sc_brg_clk;
	int			sc_alt_break_state;
	struct mtx		sc_mtx;
	uint8_t			sc_console;
	uint8_t			sc_opening;
	uint8_t			sc_tx_busy;
	uint8_t			sc_tx_done;
	uint8_t			sc_preg_held;
	uint8_t			sc_creg[16];
	uint8_t			sc_preg[16];
	uint8_t			sc_obuf[CBLOCK];
	uint8_t			sc_ibuf[CBLOCK];
};

struct zs_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_bt;
	bus_space_handle_t	sc_bh;
	struct zstty_softc	*sc_child[ZS_NCHAN];
	void			*sc_ih;
	void			*sc_softih;
	struct resource		*sc_irqres;
	int			sc_irqrid;
	struct resource		*sc_memres;
	int			sc_memrid;
};

static uint8_t zs_init_reg[16] = {
	0,	/* 0: CMD (reset, etc.) */
	0,	/* 1: No interrupts yet. */
	0,	/* 2: IVECT */
	ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
	ZSWR4_CLK_X16 | ZSWR4_ONESB | ZSWR4_EVENP,
	ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
	0,	/* 6: TXSYNC/SYNCLO */
	0,	/* 7: RXSYNC/SYNCHI */
	0,	/* 8: alias for data port */
	ZSWR9_MASTER_IE | ZSWR9_NO_VECTOR,
	0,	/* 10: Misc. TX/RX control bits */
	ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
	((ZS_CLOCK/32)/9600)-2,
	0,
	ZSWR14_BAUD_ENA | ZSWR14_BAUD_FROM_PCLK,
	ZSWR15_BREAK_IE,
};

static int zs_probe(device_t dev);
static int zs_attach(device_t dev);
static int zs_detach(device_t dev);

static void zs_intr(void *v);
static void zs_softintr(void *v);
static void zs_shutdown(void *v);

static int zstty_probe(device_t dev);
static int zstty_attach(device_t dev);
static int zstty_detach(device_t dev);

static int zstty_intr(struct zstty_softc *sc, uint8_t rr3);
static void zstty_softintr(struct zstty_softc *sc) __unused;
static int zstty_mdmctrl(struct zstty_softc *sc, int bits, int how);
static int zstty_param(struct zstty_softc *sc, struct tty *tp,
    struct termios *t);
static void zstty_flush(struct zstty_softc *sc) __unused;
static int zstty_speed(struct zstty_softc *sc, int rate);
static void zstty_load_regs(struct zstty_softc *sc);
static int zstty_console(device_t dev, char *mode, int len);
static int zstty_keyboard(device_t dev);

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
static d_ioctl_t zsttyioctl;

static void zsttystart(struct tty *tp);
static void zsttystop(struct tty *tp, int rw);
static int zsttyparam(struct tty *tp, struct termios *t);

static struct cdevsw zstty_cdevsw = {
	/* open */	zsttyopen,
	/* close */	zsttyclose,
	/* read */	ttyread,
	/* write */	ttywrite,
	/* ioctl */	zsttyioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"zstty",
	/* major */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY | D_KQFILTER,
	/* kqfilter */	ttykqfilter,
};

static device_method_t zs_methods[] = {
	DEVMETHOD(device_probe,		zs_probe),
	DEVMETHOD(device_attach,	zs_attach),
	DEVMETHOD(device_detach,	zs_detach),

	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	{ 0, 0 }
};

static device_method_t zstty_methods[] = {
	DEVMETHOD(device_probe,		zstty_probe),
	DEVMETHOD(device_attach,	zstty_attach),
	DEVMETHOD(device_detach,	zstty_detach),

	{ 0, 0 }
};

static driver_t zs_driver = {
	"zs",
	zs_methods,
	sizeof(struct zs_softc),
};

static driver_t zstty_driver = {
	"zstty",
	zstty_methods,
	sizeof(struct zstty_softc),
};

static devclass_t zs_devclass;
static devclass_t zstty_devclass;

static struct zstty_softc *zstty_cons;

DRIVER_MODULE(zs, sbus, zs_driver, zs_devclass, 0, 0);
DRIVER_MODULE(zstty, zs, zstty_driver, zstty_devclass, 0, 0);

CONS_DRIVER(zs, zs_cnprobe, zs_cninit, zs_cnterm, zs_cngetc, zs_cncheckc,
    zs_cnputc, zs_cndbctl);

static int
zs_probe(device_t dev)
{

	if (strcmp(sbus_get_name(dev), "zs") != 0 ||
	    device_get_unit(dev) != 0)
		return (ENXIO);
	device_set_desc(dev, "Zilog Z8530");
	return (0);
}

static int
zs_attach(device_t dev)
{
	struct device *child[ZS_NCHAN];
	struct resource *irqres;
	struct resource *memres;
	struct zs_softc *sc;
	int irqrid;
	int memrid;
	int i;

	irqrid = 0;
	irqres = NULL;
	memres = NULL;
	memrid = 0;
	sc = device_get_softc(dev);
	memres = bus_alloc_resource(dev, SYS_RES_MEMORY, &memrid, 0, ~0, 1,
	    RF_ACTIVE);
	if (memres == NULL)
		goto error;
	irqres = bus_alloc_resource(dev, SYS_RES_IRQ, &irqrid, 0, ~0, 1,
	    RF_ACTIVE);
	if (irqres == NULL)
		goto error;
	if (bus_setup_intr(dev, irqres, INTR_TYPE_TTY | INTR_FAST, zs_intr,
	    sc, &sc->sc_ih) != 0)
		goto error;
	sc->sc_dev = dev;
	sc->sc_irqres = irqres;
	sc->sc_irqrid = irqrid;
	sc->sc_memres = memres;
	sc->sc_memrid = memrid;
	sc->sc_bt = rman_get_bustag(memres);
	sc->sc_bh = rman_get_bushandle(memres);

	for (i = 0; i < ZS_NCHAN; i++)
		child[i] = device_add_child(dev, "zstty", -1);
	bus_generic_attach(dev);
	for (i = 0; i < ZS_NCHAN; i++)
		sc->sc_child[i] = device_get_softc(child[i]);

	swi_add(&tty_ithd, "tty:zs", zs_softintr, sc, SWI_TTY,
	    INTR_TYPE_TTY, &sc->sc_softih);

	ZS_WRITE_REG(sc->sc_child[0], 2, zs_init_reg[2]);
	ZS_WRITE_REG(sc->sc_child[0], 9, zs_init_reg[9]);

	if (zstty_cons != NULL) {
		DELAY(50000);
		cninit();
	}

	EVENTHANDLER_REGISTER(shutdown_final, zs_shutdown, sc,
	    SHUTDOWN_PRI_DEFAULT);

	return (0);

error:
	if (irqres != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, irqrid, irqres);
	if (memres != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, memrid, memres);
	return (ENXIO);
}

static int
zs_detach(device_t dev)
{

	return (bus_generic_detach(dev));
}

static void
zs_intr(void *v)
{
	struct zs_softc *sc = v;
	int needsoft;
	uint8_t rr3;

	needsoft = 0;
	rr3 = ZS_READ_REG(sc->sc_child[0], 3);
	if ((rr3 & (ZSRR3_IP_A_RX | ZSRR3_IP_A_TX | ZSRR3_IP_A_STAT)) != 0) {
		ZS_WRITE(sc->sc_child[0], ZS_CSR, ZSWR0_CLR_INTR);
		needsoft |= zstty_intr(sc->sc_child[0], rr3);
	}
	if ((rr3 & (ZSRR3_IP_B_RX | ZSRR3_IP_B_TX | ZSRR3_IP_B_STAT)) != 0) {
		ZS_WRITE(sc->sc_child[1], ZS_CSR, ZSWR0_CLR_INTR);
		needsoft |= zstty_intr(sc->sc_child[1], rr3);
	}
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

static int
zstty_probe(device_t dev)
{

	if (zstty_keyboard(dev)) {
		if ((device_get_unit(dev) & 1) == 0)
			device_set_desc(dev, "keyboard");
		else
			device_set_desc(dev, "mouse");
	} else {
		if ((device_get_unit(dev) & 1) == 0)
			device_set_desc(dev, "ttya");
		else
			device_set_desc(dev, "ttyb");
	}
	return (0);
}

static int
zstty_attach(device_t dev)
{
	struct zstty_softc *sc;
	struct tty *tp;
	char mode[32];
	int baud;
	int clen;
	char parity;
	int stop;
	char c;

	sc = device_get_softc(dev);
	mtx_init(&sc->sc_mtx, "zstty", NULL, MTX_SPIN);
	sc->sc_dev = dev;
	sc->sc_parent = device_get_softc(device_get_parent(dev));
	sc->sc_bt = sc->sc_parent->sc_bt;
	sc->sc_channel = device_get_unit(dev) & 1;
	sc->sc_brg_clk = ZS_CLOCK / ZS_CLOCK_DIV;

	switch (sc->sc_channel) {
	case 0:
		sc->sc_bh = sc->sc_parent->sc_bh + ZS_CHAN_A;
		break;
	case 1:
		sc->sc_bh = sc->sc_parent->sc_bh + ZS_CHAN_B;
		break;
	}

	tp = ttymalloc(NULL);
	sc->sc_si = make_dev(&zstty_cdevsw, device_get_unit(dev),
	    UID_ROOT, GID_WHEEL, 0600, "%s", device_get_desc(dev));
	sc->sc_si->si_drv1 = sc;
	sc->sc_si->si_tty = tp;
	tp->t_dev = sc->sc_si;
	sc->sc_tty = tp;

	tp->t_oproc = zsttystart;
	tp->t_param = zsttyparam;
	tp->t_stop = zsttystop;
	tp->t_iflag = TTYDEF_IFLAG;
	tp->t_oflag = TTYDEF_OFLAG;
	tp->t_lflag = TTYDEF_LFLAG;
	tp->t_cflag = CREAD | CLOCAL | CS8;
	tp->t_ospeed = TTYDEF_SPEED;
	tp->t_ispeed = TTYDEF_SPEED;

	bcopy(zs_init_reg, sc->sc_creg, 16);
	bcopy(zs_init_reg, sc->sc_preg, 16);

	if (zstty_console(dev, mode, sizeof(mode))) {
		ttychars(tp);
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
	}

	return (0);
}

static int
zstty_detach(device_t dev)
{

	return (bus_generic_detach(dev));
}

static int
zstty_intr(struct zstty_softc *sc, uint8_t rr3)
{
	int needsoft;
	uint8_t rr0;
	uint8_t rr1;
	uint8_t c;
	int brk;

	ZSTTY_LOCK(sc);

	brk = 0;
	needsoft = 0;
	if ((rr3 & ZSRR3_IP_A_RX) != 0) {
		needsoft = 1;
		do {
			/*
			 * First read the status, because reading the received
			 * char destroys the status of this char.
			 */
			rr1 = ZS_READ_REG(sc, 1);
			c = ZS_READ(sc, ZS_DATA);

			if ((rr1 & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE)) != 0)
				ZS_WRITE(sc, ZS_CSR, ZSWR0_RESET_ERRORS);
#if defined(DDB) && defined(ALT_BREAK_TO_DEBUGGER)
			brk = db_alt_break(c, &sc->sc_alt_break_state);
#endif
			*sc->sc_iput++ = c;
			*sc->sc_iput++ = rr1;
			if (sc->sc_iput == sc->sc_ibuf + sizeof(sc->sc_ibuf))
				sc->sc_iput = sc->sc_ibuf;
		} while ((ZS_READ(sc, ZS_CSR) & ZSRR0_RX_READY) != 0);
	}

	if ((rr3 & ZSRR3_IP_A_STAT) != 0) {
		rr0 = ZS_READ(sc, ZS_CSR);
		ZS_WRITE(sc, ZS_CSR, ZSWR0_RESET_STATUS);
#if defined(DDB) && defined(BREAK_TO_DEBUGGER)
		if ((rr0 & ZSRR0_BREAK) != 0)
			brk = 1;
#endif
		/* XXX do something about flow control */
	}

	if ((rr3 & ZSRR3_IP_A_TX) != 0) {
		/*
		 * If we've delayed a paramter change, do it now.
		 */
		if (sc->sc_preg_held) {
			sc->sc_preg_held = 0;
			zstty_load_regs(sc);
		}
		if (sc->sc_ocnt > 0) {
			ZS_WRITE(sc, ZS_DATA, *sc->sc_oget++);
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

		(*linesw[tp->t_line].l_rint)(data, tp);
	}

	if (sc->sc_tx_done != 0) {
		sc->sc_tx_done = 0;
		tp->t_state &= ~TS_BUSY;
		(*linesw[tp->t_line].l_start)(tp);
	}
}

static int
zsttyopen(dev_t dev, int flags, int mode, struct thread *td)
{
	struct zstty_softc *sc;
	struct tty *tp;
	int error;

	sc = dev->si_drv1;
	tp = dev->si_tty;

	if ((tp->t_state & TS_ISOPEN) != 0 &&
	    (tp->t_state & TS_XCLUDE) != 0 &&
	    !suser(td))
		return (EBUSY);

	if ((tp->t_state & TS_ISOPEN) == 0) {
		struct termios t;

		/*
		 * Enable receive and status interrupts in zstty_param.
		 */
		sc->sc_preg[1] |= ZSWR1_RIE | ZSWR1_SIE;
		sc->sc_iput = sc->sc_iget = sc->sc_ibuf;

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		t.c_ispeed = 0;
		t.c_ospeed = TTYDEF_SPEED;
		t.c_cflag = TTYDEF_CFLAG;
		/* Make sure zstty_param() will do something. */
		tp->t_ospeed = 0;
		(void)zstty_param(sc, tp, &t);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		ttychars(tp);
		ttsetwater(tp);

		/* XXX turn on DTR */

		/* XXX handle initial DCD */
	}

	error = ttyopen(dev, tp);
	if (error != 0)
		return (error);

	error = (*linesw[tp->t_line].l_open)(dev, tp);
	if (error != 0)
		return (error);

	return (0);
}

static int
zsttyclose(dev_t dev, int flags, int mode, struct thread *td)
{
	struct tty *tp;

	tp = dev->si_tty;

	if ((tp->t_state & TS_ISOPEN) == 0)
		return (0);

	(*linesw[tp->t_line].l_close)(tp, flags);
	ttyclose(tp);

	return (0);
}

static int
zsttyioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	struct zstty_softc *sc;
	struct tty *tp;
	int error;

	sc = dev->si_drv1;
	tp = dev->si_tty;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flags, td);
	if (error != ENOIOCTL)
		return (error);

	error = ttioctl(tp, cmd, data, flags);
	if (error != ENOIOCTL)
		return (error);

	error = 0;
	switch (cmd) {
	case TIOCSBRK:
		ZS_WRITE_REG(sc, 5, ZS_READ_REG(sc, 5) | ZSWR5_BREAK);
		break;
	case TIOCCBRK:
		ZS_WRITE_REG(sc, 5, ZS_READ_REG(sc, 5) & ~ZSWR5_BREAK);
		break;
	case TIOCSDTR:
		zstty_mdmctrl(sc, TIOCM_DTR, DMBIS);
		break;
	case TIOCCDTR:
		zstty_mdmctrl(sc, TIOCM_DTR, DMBIC);
		break;
	case TIOCMBIS:
		zstty_mdmctrl(sc, *((int *)data), DMBIS);
		break;
	case TIOCMBIC:
		zstty_mdmctrl(sc, *((int *)data), DMBIC);
		break;
	case TIOCMGET:
		*((int *)data) = zstty_mdmctrl(sc, 0, DMGET);
		break;
	case TIOCMSET:
		zstty_mdmctrl(sc, *((int *)data), DMSET);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

static void
zsttystart(struct tty *tp)
{
	struct zstty_softc *sc;
	uint8_t c;

	sc = tp->t_dev->si_drv1;

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
		selwakeup(&tp->t_wsel);
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
	ZS_WRITE(sc, ZS_DATA, c);

	ttwwakeup(tp);
}

static void
zsttystop(struct tty *tp, int flag)
{
	struct zstty_softc *sc;

	sc = tp->t_dev->si_drv1;

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

	sc = tp->t_dev->si_drv1;
	return (zstty_param(sc, tp, t));
}

static int
zstty_mdmctrl(struct zstty_softc *sc, int bits, int how)
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

	zstty_mdmctrl(sc, TIOCM_DTR,
	    (t->c_ospeed == 0) ? DMBIC : DMBIS);

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

	ZSTTY_LOCK(sc);

	sc->sc_preg[3] = wr3;
	sc->sc_preg[4] = wr4;
	sc->sc_preg[5] = wr5;
	sc->sc_preg[12] = ospeed;
	sc->sc_preg[13] = ospeed >> 8;

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
		rr0 = ZS_READ(sc, ZS_CSR);
		if ((rr0 & ZSRR0_RX_READY) == 0)
			break;

		rr1 = ZS_READ_REG(sc, 1);
		c = ZS_READ(sc, ZS_DATA);

		if (rr1 & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE))
			ZS_WRITE(sc, ZS_CSR, ZSWR0_RESET_ERRORS);
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
	ZS_WRITE(sc, ZS_CSR, ZSM_RESET_ERR);

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
	ZS_WRITE(sc, ZS_CSR, ZSM_RESET_STINT);
	ZS_WRITE(sc, ZS_CSR, ZSM_RESET_STINT);

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
		cn->cn_dev = sc->sc_si;
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
zs_cngetc(dev_t dev)
{
	struct zstty_softc *sc = zstty_cons;

	if (sc == NULL)
		return (-1);
	return (zstty_cngetc(sc));
}

static int
zs_cncheckc(dev_t dev)
{
	struct zstty_softc *sc = zstty_cons;

	if (sc == NULL)
		return (-1);
	return (zstty_cncheckc(sc));
}

static void
zs_cnputc(dev_t dev, int c)
{
	struct zstty_softc *sc = zstty_cons;

	if (sc == NULL)
		return;
	zstty_cnputc(sc, c);
}

static void
zs_cndbctl(dev_t dev, int c)
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
	while ((ZS_READ(sc, ZS_CSR) & ZSRR0_RX_READY) == 0)
		;
	c = ZS_READ(sc, ZS_DATA);
	zstty_cnclose(sc);
	return (c);
}

static int
zstty_cncheckc(struct zstty_softc *sc)
{
	uint8_t c;

	c = -1;
	zstty_cnopen(sc);
	if ((ZS_READ(sc, ZS_CSR) & ZSRR0_RX_READY) != 0)
		c = ZS_READ(sc, ZS_DATA);
	zstty_cnclose(sc);
	return (c);
}

static void
zstty_cnputc(struct zstty_softc *sc, int c)
{

	zstty_cnopen(sc);
	while ((ZS_READ(sc, ZS_CSR) & ZSRR0_TX_READY) == 0)
		;
	ZS_WRITE(sc, ZS_DATA, c);
	zstty_cnclose(sc);
}

static int
zstty_console(device_t dev, char *mode, int len)
{
	device_t parent;
	phandle_t chosen;
	phandle_t options;
	ihandle_t stdin;
	ihandle_t stdout;
	char output[32];
	char input[32];
	char name[32];

	parent = device_get_parent(dev);
	chosen = OF_finddevice("/chosen");
	options = OF_finddevice("/options");
	if (OF_getprop(chosen, "stdin", &stdin, sizeof(stdin)) == -1 ||
	    OF_getprop(chosen, "stdout", &stdout, sizeof(stdout)) == -1 ||
	    OF_getprop(options, "input-device", input, sizeof(input)) == -1 ||
	    OF_getprop(options, "output-device", output, sizeof(output)) == -1)
		return (0);
	if (sbus_get_node(parent) == OF_instance_to_package(stdin) &&
	    sbus_get_node(parent) == OF_instance_to_package(stdout) &&
	    strcmp(input, device_get_desc(dev)) == 0 &&
	    strcmp(output, device_get_desc(dev)) == 0) {
		if (mode != NULL) {
			sprintf(name, "%s-mode", input);
			return (OF_getprop(options, name, mode, len) != -1);
		} else
			return (1);
	}
	return (0);
}

static int
zstty_keyboard(device_t dev)
{
	device_t parent;

	parent = device_get_parent(dev);
	return (OF_getproplen(sbus_get_node(parent), "keyboard") == 0);
}
