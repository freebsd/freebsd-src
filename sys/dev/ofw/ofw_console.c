/*
 * Copyright (C) 2001 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/tty.h>

#include <dev/ofw/openfirm.h>

#define	OFW_POLL_HZ	4

static d_open_t		ofw_dev_open;
static d_close_t	ofw_dev_close;
static d_ioctl_t	ofw_dev_ioctl;

#define	CDEV_MAJOR	97

static struct cdevsw ofw_cdevsw = {
	/* open */	ofw_dev_open,
	/* close */	ofw_dev_close,
	/* read */	ttyread,
	/* write */	ttywrite,
	/* ioctl */	ofw_dev_ioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"ofw",
	/* major */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static struct tty		*ofw_tp = NULL;
static int			polltime;
static struct callout_handle	ofw_timeouthandle
    = CALLOUT_HANDLE_INITIALIZER(&ofw_timeouthandle);

static void	ofw_tty_start(struct tty *);
static int	ofw_tty_param(struct tty *, struct termios *);
static void	ofw_tty_stop(struct tty *, int);
static void	ofw_timeout(void *);

static cn_probe_t	ofw_cons_probe;
static cn_init_t	ofw_cons_init;
static cn_getc_t	ofw_cons_getc;
static cn_checkc_t 	ofw_cons_checkc;
static cn_putc_t	ofw_cons_putc;

CONS_DRIVER(ofw, ofw_cons_probe, ofw_cons_init, NULL, ofw_cons_getc,
    ofw_cons_checkc, ofw_cons_putc, NULL);

static void
cn_drvinit(void *unused)
{

	make_dev(&ofw_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "ofwcons");
}

SYSINIT(cndev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,cn_drvinit,NULL)

static int	stdin;
static int	stdout;

static int
ofw_dev_open(dev_t dev, int flag, int mode, struct thread *td)
{
	struct	tty *tp;
	int	unit;
	int	error, setuptimeout;

	error = 0;
	setuptimeout = 0;
	unit = minor(dev);

	tp = ofw_tp = dev->si_tty = ttymalloc(ofw_tp);

	tp->t_oproc = ofw_tty_start;
	tp->t_param = ofw_tty_param;
	tp->t_stop = ofw_tty_stop;
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
	} else if ((tp->t_state & TS_XCLUDE) && suser(td)) {
		return (EBUSY);
	}

	error = (*linesw[tp->t_line].l_open)(dev, tp);

	if (error == 0 && setuptimeout) {
		polltime = hz / OFW_POLL_HZ;
		if (polltime < 1) {
			polltime = 1;
		}

		ofw_timeouthandle = timeout(ofw_timeout, tp, polltime);
	}

	return (error);
}

static int
ofw_dev_close(dev_t dev, int flag, int mode, struct thread *td)
{
	int	unit;
	struct	tty *tp;

	unit = minor(dev);
	tp = ofw_tp;

	if (unit != 0) {
		return (ENXIO);
	}

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);

	return (0);
}

static int
ofw_dev_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	int	unit;
	struct	tty *tp;
	int	error;

	unit = minor(dev);
	tp = ofw_tp;

	if (unit != 0) {
		return (ENXIO);
	}

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, td);
	if (error != ENOIOCTL) {
		return (error);
	}

	error = ttioctl(tp, cmd, data, flag);
	if (error != ENOIOCTL) {
		return (error);
	}

	return (ENOTTY);
}

static int
ofw_tty_param(struct tty *tp, struct termios *t)
{

	return (0);
}

static void
ofw_tty_start(struct tty *tp)
{

	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
		return;
	}

	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0) {
		ofw_cons_putc(tp->t_dev, getc(&tp->t_outq));
	}
	tp->t_state &= ~TS_BUSY;

	ttwwakeup(tp);
}

static void
ofw_tty_stop(struct tty *tp, int flag)
{

	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0) {
			tp->t_state |= TS_FLUSH;
		}
	}
}

static void
ofw_timeout(void *v)
{
	struct	tty *tp;
	int 	c;

	tp = (struct tty *)v;

	while ((c = ofw_cons_checkc(tp->t_dev)) != -1) {
		if (tp->t_state & TS_ISOPEN) {
			(*linesw[tp->t_line].l_rint)(c, tp);
		}
	}

	ofw_timeouthandle = timeout(ofw_timeout, tp, polltime);
}

static void
ofw_cons_probe(struct consdev *cp)
{
	int chosen;

	if ((chosen = OF_finddevice("/chosen")) == -1) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	if (OF_getprop(chosen, "stdin", &stdin, sizeof(stdin)) == -1) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	if (OF_getprop(chosen, "stdout", &stdout, sizeof(stdout)) == -1) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	cp->cn_dev = makedev(CDEV_MAJOR, 0);
	cp->cn_pri = CN_INTERNAL;
	cp->cn_tp = ofw_tp;
}

static void
ofw_cons_init(struct consdev *cp)
{

	return;
}

static int
ofw_cons_getc(dev_t dev)
{
	unsigned char ch;
	int l;

	ch = '\0';

	while ((l = OF_read(stdin, &ch, 1)) != 1) {
		if (l != -2 && l != 0) {
			return (-1);
		}
	}

	return (ch);
}

static int
ofw_cons_checkc(dev_t dev)
{
	unsigned char ch;

	if (OF_read(stdin, &ch, 1) > 0) {
		return (ch);
	}

	return (-1);
}

static void
ofw_cons_putc(dev_t dev, int c)
{
	char cbuf;

	if (c == '\n') {
		cbuf = '\r';
		OF_write(stdout, &cbuf, 1);
	}

	cbuf = c;
	OF_write(stdout, &cbuf, 1);
}
