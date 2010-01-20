/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_comconsole.h"
#include "opt_ofw.h"

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/tty.h>

#include <dev/ofw/openfirm.h>

#include <ddb/ddb.h>

#ifndef	OFWCONS_POLL_HZ
#define	OFWCONS_POLL_HZ	4	/* 50-100 works best on Ultra2 */
#endif
#define OFBURSTLEN	128	/* max number of bytes to write in one chunk */

static d_open_t		ofw_dev_open;
static d_close_t	ofw_dev_close;

static struct cdevsw ofw_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	ofw_dev_open,
	.d_close =	ofw_dev_close,
	.d_name =	"ofw",
	.d_flags =	D_TTY | D_NEEDGIANT,
};

static struct tty		*ofw_tp = NULL;
static int			polltime;
static struct callout_handle	ofw_timeouthandle
    = CALLOUT_HANDLE_INITIALIZER(&ofw_timeouthandle);

#if defined(KDB) && defined(ALT_BREAK_TO_DEBUGGER)
static int			alt_break_state;
#endif

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
	phandle_t options;
	char output[32];
	struct cdev *dev;

	if (ofw_consdev.cn_pri != CN_DEAD &&
	    ofw_consdev.cn_name[0] != '\0') {
		if ((options = OF_finddevice("/options")) == -1 ||
		    OF_getprop(options, "output-device", output,
		    sizeof(output)) == -1)
			return;
		/*
		 * XXX: This is a hack and it may result in two /dev/ttya
		 * XXX: devices on platforms where the sab driver works.
		 */
		dev = make_dev(&ofw_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "%s",
		    output);
		make_dev_alias(dev, "ofwcons");
	}
}

SYSINIT(cndev, SI_SUB_CONFIGURE, SI_ORDER_MIDDLE, cn_drvinit, NULL)

static int	stdin;
static int	stdout;

static int
ofw_dev_open(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct	tty *tp;
	int	unit;
	int	error, setuptimeout;

	error = 0;
	setuptimeout = 0;
	unit = minor(dev);

	/*
	 * XXX: BAD, should happen at attach time
	 */
	if (dev->si_tty == NULL) {
		ofw_tp = ttyalloc();
		dev->si_tty = ofw_tp;
		ofw_tp->t_dev = dev;
	}
	tp = dev->si_tty;

	tp->t_oproc = ofw_tty_start;
	tp->t_param = ofw_tty_param;
	tp->t_stop = ofw_tty_stop;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttyconsolemode(tp, 0);
		ttsetwater(tp);

		setuptimeout = 1;
	} else if ((tp->t_state & TS_XCLUDE) && suser(td)) {
		return (EBUSY);
	}

	error = ttyld_open(tp, dev);

	if (error == 0 && setuptimeout) {
		polltime = hz / OFWCONS_POLL_HZ;
		if (polltime < 1) {
			polltime = 1;
		}

		ofw_timeouthandle = timeout(ofw_timeout, tp, polltime);
	}

	return (error);
}

static int
ofw_dev_close(struct cdev *dev, int flag, int mode, struct thread *td)
{
	int	unit;
	struct	tty *tp;

	unit = minor(dev);
	tp = dev->si_tty;

	if (unit != 0) {
		return (ENXIO);
	}

	/* XXX Should be replaced with callout_stop(9) */
	untimeout(ofw_timeout, tp, ofw_timeouthandle);
	ttyld_close(tp, flag);
	tty_close(tp);

	return (0);
}


static int
ofw_tty_param(struct tty *tp, struct termios *t)
{

	return (0);
}

static void
ofw_tty_start(struct tty *tp)
{
	struct clist *cl;
	int len;
	u_char buf[OFBURSTLEN];


	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		return;

	tp->t_state |= TS_BUSY;
	cl = &tp->t_outq;
	len = q_to_b(cl, buf, OFBURSTLEN);
	OF_write(stdout, buf, len);
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

	while ((c = ofw_cons_checkc(NULL)) != -1) {
		if (tp->t_state & TS_ISOPEN) {
			ttyld_rint(tp, c);
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

	cp->cn_pri = CN_LOW;
}

static void
ofw_cons_init(struct consdev *cp)
{

	/* XXX: This is the alias, but that should be good enough */
	sprintf(cp->cn_name, "ofwcons");
	cp->cn_tp = ofw_tp;
}

static int
ofw_cons_getc(struct consdev *cp)
{
	unsigned char ch;
	int l;

	ch = '\0';

	while ((l = OF_read(stdin, &ch, 1)) != 1) {
		if (l != -2 && l != 0) {
			return (-1);
		}
	}

#if defined(KDB) && defined(ALT_BREAK_TO_DEBUGGER)
	if (kdb_alt_break(ch, &alt_break_state))
		kdb_enter("Break sequence on console");
#endif

	return (ch);
}

static int
ofw_cons_checkc(struct consdev *cp)
{
	unsigned char ch;

	if (OF_read(stdin, &ch, 1) > 0) {
#if defined(KDB) && defined(ALT_BREAK_TO_DEBUGGER)
		if (kdb_alt_break(ch, &alt_break_state))
			kdb_enter("Break sequence on console");
#endif
		return (ch);
	}

	return (-1);
}

static void
ofw_cons_putc(struct consdev *cp, int c)
{
	char cbuf;

	if (c == '\n') {
		cbuf = '\r';
		OF_write(stdout, &cbuf, 1);
	}

	cbuf = c;
	OF_write(stdout, &cbuf, 1);
}
