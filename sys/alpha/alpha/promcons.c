/* $Id: promcons.c,v 1.2 1998/07/05 11:59:44 dfr Exp $ */
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
#include <sys/ucred.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <sys/lock.h>
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
static	d_read_t	promread;
static	d_write_t	promwrite;
static	d_ioctl_t	promioctl;
static	d_stop_t	promstop;
static	d_devtotty_t	promdevtotty;

#define CDEV_MAJOR 97
static struct cdevsw prom_cdevsw = {
	promopen,	promclose,	promread,	promwrite,
	promioctl,	promstop,	noreset,	promdevtotty,
	ttpoll,		nommap,		NULL,		"prom",
	NULL,		-1,
};

static struct  tty prom_tty[1];
static int polltime;
static struct callout_handle promtimeouthandle
	= CALLOUT_HANDLE_INITIALIZER(&promtimeouthandle);

void	promstart __P((struct tty *));
void	promtimeout __P((void *));
int	promparam __P((struct tty *, struct termios *));

int
promopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct tty *tp;
	int s;
	int error = 0, setuptimeout = 0;
 
	if (!pmap_uses_prom_console() || unit >= 1)
		return ENXIO;

	s = spltty();

	tp = &prom_tty[unit];

	tp->t_oproc = promstart;
	tp->t_param = promparam;
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

		setuptimeout = 1;
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0) {
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
promclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct tty *tp = &prom_tty[unit];

	untimeout(promtimeout, tp, promtimeouthandle);
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	return 0;
}
 
int
promread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp = &prom_tty[minor(dev)];

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
promwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp = &prom_tty[minor(dev)];
 
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}
 
int
promioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = minor(dev);
	struct tty *tp = &prom_tty[unit];
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
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

struct tty *
promdevtotty(dev)
	dev_t dev;
{

	if (minor(dev) != 0)
		panic("promtty: bogus");

	return &prom_tty[0];
}

CDEV_MODULE(prom, CDEV_MAJOR, prom_cdevsw, 0, 0);

#endif /* _PMAP_MAY_USE_PROM_CONSOLE */
