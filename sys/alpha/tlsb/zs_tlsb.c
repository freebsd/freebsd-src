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
 *	$Id: zs_tlsb.c,v 1.3 1998/07/05 12:16:54 dfr Exp $
 */
/*
 * This driver is a hopeless hack to get the SimOS console working.  A real
 * driver would use the zs driver source from NetBSD.
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
#include <sys/ucred.h>
#include <machine/cons.h>
#include <machine/clock.h>

#include <alpha/tlsb/gbusvar.h>
#include <alpha/tlsb/tlsbreg.h>		/* XXX */
#include <alpha/tlsb/zsreg.h>
#include <alpha/tlsb/zsvar.h>

#define	KV(_addr)	((caddr_t)ALPHA_PHYS_TO_K0SEG((_addr)))

static int		zsc_get_channel(device_t dev);
static caddr_t		zsc_get_base(device_t dev);

struct zs_softc {
	struct tty	tty;
	int		channel;
	caddr_t		base;
};
#define ZS_SOFTC(unit)	\
	((struct zs_softc*)devclass_get_softc(zs_devclass, (unit)))

static	d_open_t	zsopen;
static	d_close_t	zsclose;
static	d_read_t	zsread;
static	d_write_t	zswrite;
static	d_ioctl_t	zsioctl;
static	d_stop_t	zsstop;
static	d_devtotty_t	zsdevtotty;

#define CDEV_MAJOR 98
static struct cdevsw zs_cdevsw = {
	zsopen,	zsclose,	zsread,	zswrite,
	zsioctl,	zsstop,	noreset,	zsdevtotty,
	ttpoll,		nommap,		NULL,		"zs",
	NULL,		-1,
};

static void	zsstart __P((struct tty *));
static int	zsparam __P((struct tty *, struct termios *));

/*
 * Helpers for console support.
 */

int		zs_cngetc __P((dev_t));
void		zs_cnputc __P((dev_t, int));
static void	zs_cnpollc __P((dev_t, int));

struct consdev zs_cons = {
	NULL, NULL, zs_cngetc, NULL, zs_cnputc,
	NULL, makedev(CDEV_MAJOR, 0), CN_NORMAL,
};

static caddr_t zs_console_addr;
static int zs_console;

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
	"zs",
	zs_methods,
	DRIVER_TYPE_MISC,
	sizeof(struct zs_softc),
};

static int
zs_probe(device_t dev)
{
	return 0;
}

static int
zs_attach(device_t dev)
{
	struct zs_softc *sc = device_get_softc(dev);

	sc->channel = zsc_get_channel(dev);
	sc->base = zsc_get_base(dev);

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

static void
zs_putc(caddr_t base, int chan, int c)
{
    while (!(zs_get_status(base, chan) & 4))
	DELAY(5);
    zs_put_data(base, chan, c);
}

extern struct consdev* cn_tab;

int
zs_cnattach(vm_offset_t base, vm_offset_t offset)
{
    zs_console_addr = (caddr_t) ALPHA_PHYS_TO_K0SEG(base + offset);
    zs_console = 1;

    cn_tab = &zs_cons;
    return 0;
}

int
zs_cngetc(dev_t dev)
{
    int s = spltty();
    int c = zs_getc(zs_console_addr, minor(dev));
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

static void
zs_cnpollc(dev_t dev, int onoff)
{
}


static int
zsopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct zs_softc* sc = ZS_SOFTC(minor(dev));
	struct tty *tp;
	int s;
	int error = 0;
 
	if (!sc)
		return ENXIO;

	s = spltty();

	tp = &sc->tty;

	tp->t_oproc = zsstart;
	tp->t_param = zsparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG|CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = 9600;
		ttsetwater(tp);
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(s);
		return EBUSY;
	}

	splx(s);

	error = (*linesw[tp->t_line].l_open)(dev, tp);

	return error;
}
 
static int
zsclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp = &ZS_SOFTC(minor(dev))->tty;

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	return 0;
}
 
static int
zsread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = &ZS_SOFTC(minor(dev))->tty;
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
static int
zswrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = &ZS_SOFTC(minor(dev))->tty;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}
 
static int
zsioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct tty *tp = &ZS_SOFTC(minor(dev))->tty;
	int error;
	
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error != ENOIOCTL)
		return error;
	error = ttioctl(tp, cmd, data, flag);
	if (error != ENOIOCTL)
		return error;

	return ENOTTY;
}

static int
zsparam(struct tty *tp, struct termios *t)
{

	return 0;
}

static void
zsstart(struct tty *tp)
{
	struct zs_softc* sc = (struct zs_softc*) tp;
	int s;

	s = spltty();

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
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
}

static struct tty *
zsdevtotty(dev_t dev)
{
	struct zs_softc* sc = ZS_SOFTC(minor(dev));
	if (!sc)
		return (NULL);
	return (&sc->tty);
}

CDEV_DRIVER_MODULE(zs, zsc, zs_driver, zs_devclass,
		   CDEV_MAJOR, zs_cdevsw, 0, 0);

/*
 * The zsc bus holds two zs devices, one for channel A, one for channel B.
 */

struct zsc_softc {
	caddr_t base;
	struct zs_softc* sc_a;
	struct zs_softc* sc_b;
};

static int zsc_tlsb_probe(device_t dev);
static int zsc_tlsb_attach(device_t dev);
static void zsc_tlsb_print_child(device_t dev, device_t child);
static driver_intr_t zsc_tlsb_intr;


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
	DEVMETHOD(bus_create_intr,	bus_generic_create_intr),
	DEVMETHOD(bus_connect_intr,	bus_generic_connect_intr),

	{ 0, 0 }
};

static driver_t zsc_tlsb_driver = {
	"zsc",
	zsc_tlsb_methods,
	DRIVER_TYPE_MISC,
	sizeof(struct zsc_softc),
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
	struct zsc_softc* sc = device_get_softc(bus);
	return sc->base;
}

static int
zsc_tlsb_probe(device_t dev)
{
	struct zsc_softc* sc = device_get_softc(dev);

	device_set_desc(dev, "Z8530 uart");

	sc->base = (caddr_t) ALPHA_PHYS_TO_K0SEG(TLSB_GBUS_BASE
						 + gbus_get_offset(dev));

	/*
	 * Add channel A and channel B
	 */
	device_add_child(dev, "zs", 0, (void*) 0);
	device_add_child(dev, "zs", 1, (void*) 0);

	return 0;
}

static int
zsc_tlsb_attach(device_t dev)
{
	struct zsc_softc* sc = device_get_softc(dev);
	device_t parent = device_get_parent(dev);

	bus_generic_attach(dev);
	
	/* XXX */
	sc->sc_a = ZS_SOFTC(0);
	sc->sc_b = ZS_SOFTC(1);

	BUS_CONNECT_INTR(parent,
			 BUS_CREATE_INTR(parent, dev,
					 1, zsc_tlsb_intr, sc));

	return 0;
}

static void
zsc_tlsb_print_child(device_t bus, device_t dev)
{
	printf(" at %s%d channel %c",
	       device_get_name(bus), device_get_unit(bus),
	       'A' + (device_get_unit(dev) & 1));
}

static void
zsc_tlsb_intr(void* arg)
{
	struct zsc_softc* sc = arg;
	caddr_t base = sc->base;

	int rr3 = zs_get_rr3(base, 0);
	if (rr3 & 0x20) {
		struct tty* tp = &sc->sc_a->tty;
		int c;

		while (zs_get_status(base, 0) & 1) {
			c = zs_get_data(base, 0);
#ifdef DDB
			if (c == CTRL('\\'))
				Debugger("manual escape to debugger");
#endif
			if (tp->t_state & TS_ISOPEN)
				(*linesw[tp->t_line].l_rint)(c, tp);
			DELAY(5);
		}
	}
	if (rr3 & 0x04) {
		struct tty* tp = &sc->sc_b->tty;
		int c;

		while (zs_get_status(base, 1) & 1) {
			c = zs_get_data(base, 1);
#ifdef DDB
			if (c == CTRL('\\'))
				Debugger("manual escape to debugger");
#endif
			if (tp->t_state & TS_ISOPEN)
				(*linesw[tp->t_line].l_rint)(c, tp);
			DELAY(5);
		}
	}
}

DRIVER_MODULE(zsc_tlsb, gbus, zsc_tlsb_driver, zsc_devclass, 0, 0);
