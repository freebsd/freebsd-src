/* $FreeBSD$ */
/* $NetBSD: promcons.c,v 1.13 1998/03/21 22:52:59 mycroft Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/cons.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_zone.h>

#include <machine/prom.h>

#define _PMAP_MAY_USE_PROM_CONSOLE /* XXX for now */

#ifdef _PMAP_MAY_USE_PROM_CONSOLE

#define	PROM_POLL_HZ	50

static	d_open_t	promopen;
static	d_close_t	promclose;
static	d_ioctl_t	promioctl;

#define CDEV_MAJOR 97
static struct cdevsw prom_cdevsw = {
	/* open */	promopen,
	/* close */	promclose,
	/* read */	ttyread,
	/* write */	ttywrite,
	/* ioctl */	promioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"prom",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};


static struct tty *prom_tp = NULL;
static int polltime;
static struct callout_handle promtimeouthandle
	= CALLOUT_HANDLE_INITIALIZER(&promtimeouthandle);

void	promstart __P((struct tty *));
void	promtimeout __P((void *));
int	promparam __P((struct tty *, struct termios *));
void	promstop __P((struct tty *, int));

int
promopen(dev, flag, mode, td)
	dev_t dev;
	int flag, mode;
	struct thread *td;
{
	struct tty *tp;
	int unit = minor(dev);
	int s;
	int error = 0, setuptimeout = 0;
 
	if (pmap_uses_prom_console() == 0 || unit != 0)
		return ENXIO;


	tp = prom_tp = dev->si_tty = ttymalloc(prom_tp);

	s = spltty();
	tp->t_oproc = promstart;
	tp->t_param = promparam;
	tp->t_stop = promstop;
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
		polltime = hz / PROM_POLL_HZ;
		if (polltime < 1)
			polltime = 1;
		promtimeouthandle = timeout(promtimeout, tp, polltime);
	}
	return error;
}
 
int
promclose(dev, flag, mode, td)
	dev_t dev;
	int flag, mode;
	struct thread *td;
{
	int unit = minor(dev);
	struct tty *tp = prom_tp;

	if (unit != 0)
		return ENXIO;

	untimeout(promtimeout, tp, promtimeouthandle);
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	return 0;
}
 
int
promioctl(dev, cmd, data, flag, td)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct thread *td;
{
	int unit = minor(dev);
	struct tty *tp = prom_tp;
	int error;

	if (unit != 0)
		return ENXIO;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, td);
	if (error != ENOIOCTL)
		return error;
	error = ttioctl(tp, cmd, data, flag);
	if (error != ENOIOCTL)
		return error;

	return ENOTTY;
}

int
promparam(tp, t)
	struct tty *tp;
	struct termios *t;
{

	return 0;
}

void
promstart(tp)
	struct tty *tp;
{
	int s;

	s = spltty();

	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
		splx(s);
		return;
	}

	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0)
		promcnputc(tp->t_dev, getc(&tp->t_outq));
	tp->t_state &= ~TS_BUSY;

	ttwwakeup(tp);
	splx(s);
}

/*
 * Stop output on a line.
 */
void
promstop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
}

void
promtimeout(v)
	void *v;
{
	struct tty *tp = v;
	int c;

	while ((c = promcncheckc(tp->t_dev)) != -1) {
		if (tp->t_state & TS_ISOPEN)
			(*linesw[tp->t_line].l_rint)(c, tp);
	}
	promtimeouthandle = timeout(promtimeout, tp, polltime);
}

CONS_DRIVER(prom, NULL, NULL, NULL, promcngetc, promcncheckc, promcnputc, NULL);

static int promcn_attached = 0;
void
promcnattach(int alpha_console)
{
	prom_consdev.cn_pri = CN_NORMAL;
	prom_consdev.cn_dev = makedev(CDEV_MAJOR, 0);
	make_dev(&prom_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "promcons");
	cnadd(&prom_consdev);
	promcn_attached = 1;
}

void
promcndetach(void)
{
	if (promcn_attached) {
		cnremove(&prom_consdev);
		promcn_attached = 0;
	}
}
/*
 * promcnputc, promcngetc and promchcheckc in prom.c for layering reasons
 */
#endif /* _PMAP_MAY_USE_PROM_CONSOLE */
