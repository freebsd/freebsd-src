/*-
 * Copyright (c) 2007 Bruce M. Simpson.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_comconsole.h"

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/tty.h>

#include <dev/cfe/cfe_api.h>
#include <dev/cfe/cfe_error.h>

#include <ddb/ddb.h>

#ifndef	CFECONS_POLL_HZ
#define	CFECONS_POLL_HZ	4
#endif
#define CFEBURSTLEN	128	/* max number of bytes to write in one chunk */

static d_open_t		cfe_dev_open;
static d_close_t	cfe_dev_close;

static struct cdevsw cfe_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	cfe_dev_open,
	.d_close =	cfe_dev_close,
	.d_name =	"cfe",
	.d_flags =	D_TTY | D_NEEDGIANT,
};

static int			conhandle = -1;
static struct tty		*cfe_tp = NULL;
/* XXX does cfe have to poll? */
static int			polltime;
static struct callout_handle	cfe_timeouthandle
    = CALLOUT_HANDLE_INITIALIZER(&cfe_timeouthandle);

#if defined(KDB) && defined(ALT_BREAK_TO_DEBUGGER)
static int			alt_break_state;
#endif

static void	cfe_tty_start(struct tty *);
static int	cfe_tty_param(struct tty *, struct termios *);
static void	cfe_tty_stop(struct tty *, int);
static void	cfe_timeout(void *);

static cn_probe_t	cfe_cnprobe;
static cn_init_t	cfe_cninit;
static cn_term_t	cfe_cnterm;
static cn_getc_t	cfe_cngetc;
static cn_putc_t	cfe_cnputc;

CONSOLE_DRIVER(cfe);

static void
cn_drvinit(void *unused)
{
	char output[32];
	struct cdev *dev;

	if (cfe_consdev.cn_pri != CN_DEAD &&
	    cfe_consdev.cn_name[0] != '\0') {
		dev = make_dev(&cfe_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "%s",
		    output);
		make_dev_alias(dev, "cfecons");
	}
}

static int
cfe_dev_open(struct cdev *dev, int flag, int mode, struct thread *td)
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
		cfe_tp = ttyalloc();
		dev->si_tty = cfe_tp;
		cfe_tp->t_dev = dev;
	}
	tp = dev->si_tty;

	tp->t_oproc = cfe_tty_start;
	tp->t_param = cfe_tty_param;
	tp->t_stop = cfe_tty_stop;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttyconsolemode(tp, 0);

		setuptimeout = 1;
	} else if ((tp->t_state & TS_XCLUDE) &&
	    priv_check(td, PRIV_TTY_EXCLUSIVE)) {
		return (EBUSY);
	}

	error = ttyld_open(tp, dev);

	if (error == 0 && setuptimeout) {
		polltime = hz / CFECONS_POLL_HZ;
		if (polltime < 1) {
			polltime = 1;
		}

		cfe_timeouthandle = timeout(cfe_timeout, tp, polltime);
	}

	return (error);
}

static int
cfe_dev_close(struct cdev *dev, int flag, int mode, struct thread *td)
{
	int	unit;
	struct	tty *tp;

	unit = minor(dev);
	tp = dev->si_tty;

	if (unit != 0) {
		return (ENXIO);
	}

	/* XXX Should be replaced with callout_stop(9) */
	untimeout(cfe_timeout, tp, cfe_timeouthandle);
	ttyld_close(tp, flag);
	tty_close(tp);

	return (0);
}


static int
cfe_tty_param(struct tty *tp, struct termios *t)
{

	return (0);
}

static void
cfe_tty_start(struct tty *tp)
{
	struct clist *cl;
	int len;
	u_char buf[CFEBURSTLEN];

	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		return;

	tp->t_state |= TS_BUSY;
	cl = &tp->t_outq;
	len = q_to_b(cl, buf, CFEBURSTLEN);
	while (cfe_write(conhandle, buf, len) == 0)
		;
	tp->t_state &= ~TS_BUSY;

	ttwwakeup(tp);
}

static void
cfe_tty_stop(struct tty *tp, int flag)
{

	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0) {
			tp->t_state |= TS_FLUSH;
		}
	}
}

static void
cfe_timeout(void *v)
{
	struct	tty *tp;
	int 	c;

	tp = (struct tty *)v;

	while ((c = cfe_cngetc(NULL)) != -1) {
		if (tp->t_state & TS_ISOPEN) {
			ttyld_rint(tp, c);
		}
	}

	cfe_timeouthandle = timeout(cfe_timeout, tp, polltime);
}

static void
cfe_cnprobe(struct consdev *cp)
{

	conhandle = cfe_getstdhandle(CFE_STDHANDLE_CONSOLE);
	if (conhandle < 0) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	/* XXX */
	if (bootverbose) {
		char *bootmsg = "Using CFE firmware console.\n";
		int i;

		for (i = 0; i < strlen(bootmsg); i++)
			cfe_cnputc(cp, bootmsg[i]);
	}

	cp->cn_pri = CN_LOW;
}

static void
cfe_cninit(struct consdev *cp)
{

	sprintf(cp->cn_name, "cfecons");
	cp->cn_tp = cfe_tp;
}

static void
cfe_cnterm(struct consdev *cp)
{

}

static int
cfe_cngetc(struct consdev *cp)
{
	int result;
	unsigned char ch;

	while ((result = cfe_read(conhandle, &ch, 1)) == 0)
		;

	if (result > 0) {
#if defined(KDB) && defined(ALT_BREAK_TO_DEBUGGER)
		if (kdb_alt_break(ch, &alt_break_state))
			kdb_enter(KDB_WHY_BREAK, "Break sequence on console");
#endif
		return (ch);
	}

	return (-1);
}

static void
cfe_cnputc(struct consdev *cp, int c)
{
	char cbuf;

	if (c == '\n')
		cfe_cnputc(cp, '\r');

	cbuf = c;
	while (cfe_write(conhandle, &cbuf, 1) == 0)
		;
}

SYSINIT(cndev, SI_SUB_CONFIGURE, SI_ORDER_MIDDLE, cn_drvinit, NULL)
