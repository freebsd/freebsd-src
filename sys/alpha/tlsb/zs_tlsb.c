/*-
 * Copyright (c) 1998 Doug Rabson
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
 * This driver is a somewhat hack. A real driver might use the zs driver
 * source from NetBSD, except that it's no real winner either.
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/cons.h>
#include <machine/clock.h>

#include <alpha/tlsb/gbusvar.h>
#include <alpha/tlsb/tlsbreg.h>
#include <alpha/tlsb/zsreg.h>
#include <alpha/tlsb/zsvar.h>

#define	KV(_addr)	((caddr_t)ALPHA_PHYS_TO_K0SEG((_addr)))

static int		zsc_get_channel(device_t dev);
static caddr_t		zsc_get_base(device_t dev);

struct zs_softc {
	struct tty *	tp;	
	device_t	dev;
	int		channel;
	caddr_t		base;
	struct callout_handle zst;
};
#define ZS_SOFTC(unit)	\
	((struct zs_softc *) devclass_get_softc(zs_devclass, (unit)))

static	d_open_t	zsopen;
static	d_close_t	zsclose;
static	d_ioctl_t	zsioctl;

#define CDEV_MAJOR 135
static struct cdevsw zs_cdevsw = {
	/* open */	zsopen,
	/* close */	zsclose,
	/* read */	ttyread,
	/* write */	ttywrite,
	/* ioctl */	zsioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"zs",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static void	zsstart __P((struct tty *));
static int	zsparam __P((struct tty *, struct termios *));
static void	zsstop __P((struct tty *tp, int flag));

/*
 * Helpers for console support.
 */

static int zs_probe(device_t);
static int zs_attach(device_t);

static devclass_t zs_devclass;
static devclass_t zsc_devclass;

static device_method_t zs_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		zs_probe),
	DEVMETHOD(device_attach,	zs_attach),
	{ 0, 0 }
};

static driver_t zs_driver = {
	"zs", zs_methods, sizeof (struct zs_softc),
};

static void zs_poll_intr __P((void *));
static int zspolltime;


static int
zs_probe(device_t dev)
{
	return 0;
}

static int
zs_attach(device_t dev)
{
	struct zs_softc *sc = device_get_softc(dev);
	sc->dev = dev;
	sc->channel = zsc_get_channel(dev);
	sc->base = zsc_get_base(dev);
	make_dev(&zs_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "zs%d", device_get_unit(dev));
	return 0;
}

static caddr_t
zs_statusreg(caddr_t base, int chan)
{
	if (chan == 0)
		return base + ZSC_CHANNELA + ZSC_STATUS;
	if (chan == 1)
		return base + ZSC_CHANNELB + ZSC_STATUS;
	panic("zs_statusreg: bogus channel");
}

static caddr_t
zs_datareg(caddr_t base, int chan)
{
	if (chan == 0)
		return base + ZSC_CHANNELA + ZSC_DATA;
	if (chan == 1)
		return base + ZSC_CHANNELB + ZSC_DATA;
	panic("zs_statusreg: bogus channel");
}

static int
zs_get_status(caddr_t base, int chan)
{
	return *(u_int32_t*) zs_statusreg(base, chan) & 0xff;
}

#ifdef	IF_RR3_WORKED
static void
zs_put_status(caddr_t base, int chan, int v)
{
	*(u_int32_t*) zs_statusreg(base, chan) = v;
	alpha_mb();
}

static int
zs_get_rr3(caddr_t base, int chan)
{
	if (chan != 0)
		panic("zs_get_rr3: bad channel");
	zs_put_status(base, chan, 3);
	return zs_get_status(base, chan);
}
#endif

static int
zs_get_data(caddr_t base, int chan)
{
	return *(u_int32_t*) zs_datareg(base, chan) & 0xff;
}

static void
zs_put_data(caddr_t base, int chan, int v)
{
	*(u_int32_t*) zs_datareg(base, chan) = v;
	alpha_mb();
}

static int
zs_getc(caddr_t base, int chan)
{
	while (!(zs_get_status(base, chan) & 1))
		DELAY(5);
	return zs_get_data(base, chan);
}

static int
zs_maygetc(caddr_t base, int chan)
{
	if (zs_get_status(base, chan) & 1)
		return zs_get_data(base, chan);
	else
		return (-1);
}

static void
zs_putc(caddr_t base, int chan, int c)
{
	while (!(zs_get_status(base, chan) & 4))
		DELAY(5);
	zs_put_data(base, chan, c);
}

/*
 * Console support
 */
int		zs_cngetc __P((dev_t));
int		zs_cncheckc __P((dev_t));
void		zs_cnputc __P((dev_t, int));

static caddr_t zs_console_addr;
CONS_DRIVER(zs, NULL, NULL, NULL, zs_cngetc, zs_cncheckc, zs_cnputc, NULL);

int
zs_cnattach(vm_offset_t base, vm_offset_t offset)
{
	/* should really bet part of ivars */
	zs_console_addr = (caddr_t) ALPHA_PHYS_TO_K0SEG(base + offset);

	zs_consdev.cn_dev = makedev(CDEV_MAJOR, 0);
	zs_consdev.cn_pri = CN_NORMAL;
	make_dev(&zs_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "zs0");
	cnadd(&zs_consdev);
	return (0);
}

int
zs_cngetc(dev_t dev)
{
	int s = spltty();
	int c = zs_getc(zs_console_addr, minor(dev));
	splx(s);
	return c;
}

int
zs_cncheckc(dev_t dev)
{
	int s = spltty();
	int c = zs_maygetc(zs_console_addr, minor(dev));
	splx(s);
	return c;
}

void
zs_cnputc(dev_t dev, int c)
{
	int s = spltty();
	zs_putc(zs_console_addr, minor(dev), c);
	splx(s);
}


static int
zsopen(dev_t dev, int flag, int mode, struct thread *td)
{
	struct zs_softc *sc = ZS_SOFTC(minor(dev));
	struct tty *tp;
	int error = 0, setuptimeout = 0;
	int s;
 
	if (!sc)
		return ENXIO;

	s = spltty();
	tp = sc->tp = dev->si_tty = ttymalloc(sc->tp);
	tp->t_oproc = zsstart;
	tp->t_param = zsparam;
	tp->t_stop = zsstop;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG|CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ttsetwater(tp);
		setuptimeout = 1;
	} else if ((tp->t_state & TS_XCLUDE) && suser(td->td_proc)) {
		splx(s);
		return EBUSY;
	}

	splx(s);

	error = (*linesw[tp->t_line].l_open)(dev, tp);

	if (error == 0 && setuptimeout) {
		zspolltime = hz / 50;
		if (zspolltime < 1)
			zspolltime = 1;
		/* XXX we're not set up to do interrupts yet */
		sc->zst = timeout(zs_poll_intr, sc, zspolltime);
	}

	return (error);
}
 
static int
zsclose(dev_t dev, int flag, int mode, struct thread *td)
{
	struct zs_softc *sc = ZS_SOFTC(minor(dev));
	struct tty *tp;
	int s;

	if (sc == NULL)
		return (ENXIO);

	tp = sc->tp;

	s = spltty();
	untimeout(zs_poll_intr, sc, sc->zst);
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	splx(s);

	return (0);
}
 
static int
zsioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct zs_softc *sc = ZS_SOFTC(minor(dev));
	struct tty *tp;
	int error;
	
	if (sc == NULL)
		return (ENXIO);

	tp = ZS_SOFTC(minor(dev))->tp;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, td);

	if (error != ENOIOCTL)
		return (error);

	error = ttioctl(tp, cmd, data, flag);

	if (error != ENOIOCTL)
		return (error);
	else
		return (ENOTTY);
}

static int
zsparam(struct tty *tp, struct termios *t)
{
	return (0);
}

static void
zsstart(struct tty *tp)
{
	struct zs_softc *sc = ZS_SOFTC(minor(tp->t_dev));
	int s = spltty();

	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
		splx(s);
		return;
	}

	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0)
		zs_putc(sc->base, minor(tp->t_dev), getc(&tp->t_outq));
	tp->t_state &= ~TS_BUSY;

	ttwwakeup(tp);
	splx(s);
}

/*
 * Stop output on a line.
 */
static void
zsstop(struct tty *tp, int flag)
{
	int s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
}

DRIVER_MODULE(zs, zsc, zs_driver, zs_devclass, 0, 0);

/*
 * The zsc bus holds two zs devices, one for channel A, one for channel B.
 */

struct zsc_softc {
	caddr_t base;
	struct zs_softc *sc_a;
	struct zs_softc *sc_b;
	void *intr;
};
static driver_intr_t zsc_tlsb_intr;

static int zsc_tlsb_probe(device_t dev);
static int zsc_tlsb_attach(device_t dev);
static int zsc_tlsb_print_child(device_t dev, device_t child);

static device_method_t zsc_tlsb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		zsc_tlsb_probe),
	DEVMETHOD(device_attach,	zsc_tlsb_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	zsc_tlsb_print_child),
	DEVMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static driver_t zsc_tlsb_driver = {
	"zsc", zsc_tlsb_methods, sizeof(struct zsc_softc),
};

static int
zsc_get_channel(device_t dev)
{
	return (long) device_get_ivars(dev);
}

static caddr_t
zsc_get_base(device_t dev)
{
	device_t bus = device_get_parent(dev);
	struct zsc_softc *sc = device_get_softc(bus);
	return sc->base;
}

static int
zsc_tlsb_probe(device_t dev)
{
	static int zs_unit = 0;
	struct zsc_softc *sc = device_get_softc(dev);

	device_set_desc(dev, "Z8530 uart");
	sc->base = (caddr_t)
	    ALPHA_PHYS_TO_K0SEG(TLSB_GBUS_BASE + gbus_get_offset(dev));
	/*
	 * Add channel A and channel B
	 */
	device_add_child(dev, "zs", zs_unit++);
	device_add_child(dev, "zs", zs_unit++);
	return (0);
}

static int
zsc_tlsb_attach(device_t dev)
{
	static int once = 1;
	struct zsc_softc *sc = device_get_softc(dev);
	device_t parent = device_get_parent(dev);

	if (once) {
		once = 0;
		cdevsw_add(&zs_cdevsw);
	}
	bus_generic_attach(dev);

	/* XXX */
	sc->sc_a = ZS_SOFTC(device_get_unit(dev));
	sc->sc_b = ZS_SOFTC(device_get_unit(dev)+1);

	/* XXX should use resource argument to communicate vector */
	return BUS_SETUP_INTR(parent, dev, NULL, INTR_TYPE_TTY,
	    zsc_tlsb_intr, sc, &sc->intr);
}

static int
zsc_tlsb_print_child(device_t bus, device_t dev)
{
	int retval = 0;

	retval += bus_print_child_header(bus, dev);
	retval += printf(" on %s channel %c\n", device_get_nameunit(bus),
			 'A' + (device_get_unit(dev) & 1));

	return (retval);
}

static void
zs_poll_intr(void *arg)
{
	struct zs_softc *sc = arg;
	int s = spltty();
	zsc_tlsb_intr(device_get_softc(device_get_parent(sc->dev)));
	sc->zst = timeout(zs_poll_intr, sc, zspolltime);
	splx(s);
}

static void
zsc_tlsb_intr(void *arg)
{
	struct zsc_softc *sc = arg;
	caddr_t base = sc->base;
	int rr3;

	if (base == NULL)
		panic("null base in zsc_tlsb_intr");


#ifdef	IF_RR3_WORKED
	rr3 = zs_get_rr3(base, 0);
#else
	rr3 = 0x20;
#endif

	if (rr3 & 0x20) {
		struct tty *tp = sc->sc_a->tp;
		int c;

		while (zs_get_status(base, 0) & 1) {
			c = zs_get_data(base, 0);
#ifdef DDB
			if (c == CTRL('\\'))
				Debugger("manual escape to debugger");
#endif
			if (tp && (tp->t_state & TS_ISOPEN))
				(*linesw[tp->t_line].l_rint)(c, tp);
			DELAY(5);
		}
	}
	if (rr3 & 0x04) {
		struct tty *tp = sc->sc_b->tp;
		int c;

		while (zs_get_status(base, 1) & 1) {
			c = zs_get_data(base, 1);
#ifdef DDB
			if (c == CTRL('\\'))
				Debugger("manual escape to debugger");
#endif
			if (tp && (tp->t_state & TS_ISOPEN))
				(*linesw[tp->t_line].l_rint)(c, tp);
			DELAY(5);
		}
	}
}
DRIVER_MODULE(zsc, gbus, zsc_tlsb_driver, zsc_devclass, 0, 0);
