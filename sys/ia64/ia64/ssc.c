/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD: src/sys/ia64/ia64/ssc.c,v 1.30 2006/11/06 17:43:10 rwatson Exp $
 */
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>

#define SSC_GETCHAR			21
#define SSC_PUTCHAR			31

#define	SSC_POLL_HZ	50

static	d_open_t	ssc_open;
static	d_close_t	ssc_close;

static struct cdevsw ssc_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	ssc_open,
	.d_close =	ssc_close,
	.d_name =	"ssc",
	.d_flags =	D_TTY | D_NEEDGIANT,
};

static struct tty *ssc_tp = NULL;
static int polltime;
static struct callout_handle ssc_timeouthandle
	= CALLOUT_HANDLE_INITIALIZER(&ssc_timeouthandle);

static void	ssc_start(struct tty *);
static void	ssc_timeout(void *);
static int	ssc_param(struct tty *, struct termios *);
static void	ssc_stop(struct tty *, int);

static u_int64_t
ssc(u_int64_t in0, u_int64_t in1, u_int64_t in2, u_int64_t in3, int which)
{
	register u_int64_t ret0 __asm("r8");

	__asm __volatile("mov r15=%1\n\t"
			 "break 0x80001"
			 : "=r"(ret0)
			 : "r"(which), "r"(in0), "r"(in1), "r"(in2), "r"(in3));
	return ret0;
}

static void
ssc_cnprobe(struct consdev *cp)
{
	sprintf(cp->cn_name, "ssccons");
	cp->cn_pri = CN_INTERNAL;
}

static void
ssc_cninit(struct consdev *cp)
{
}

static void
ssc_cnterm(struct consdev *cp)
{
}

static void
ssc_cnattach(void *arg)
{
	make_dev(&ssc_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "ssccons");
	ssc_tp = ttyalloc();
}

SYSINIT(ssc_cnattach, SI_SUB_DRIVERS, SI_ORDER_ANY, ssc_cnattach, 0);

static void
ssc_cnputc(struct consdev *cp, int c)
{
	ssc(c, 0, 0, 0, SSC_PUTCHAR);
}

static int
ssc_cngetc(struct consdev *cp)
{
    int c;
    c = ssc(0, 0, 0, 0, SSC_GETCHAR);
    if (!c)
	    return -1;
    return c;
}

static int
ssc_open(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct tty *tp;
	int s;
	int error = 0, setuptimeout = 0;
 
	tp = dev->si_tty = ssc_tp;

	s = spltty();
	tp->t_oproc = ssc_start;
	tp->t_param = ssc_param;
	tp->t_stop = ssc_stop;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttyconsolemode(tp, 0);

		setuptimeout = 1;
	} else if ((tp->t_state & TS_XCLUDE) &&
	    priv_check(td, PRIV_TTY_EXCLUSIVE)) {
		splx(s);
		return EBUSY;
	}

	splx(s);

	error = ttyld_open(tp, dev);

	if (error == 0 && setuptimeout) {
		polltime = hz / SSC_POLL_HZ;
		if (polltime < 1)
			polltime = 1;
		ssc_timeouthandle = timeout(ssc_timeout, tp, polltime);
	}
	return error;
}
 
static int
ssc_close(struct cdev *dev, int flag, int mode, struct thread *td)
{
	int unit = minor(dev);
	struct tty *tp = ssc_tp;

	if (unit != 0)
		return ENXIO;

	untimeout(ssc_timeout, tp, ssc_timeouthandle);
	ttyld_close(tp, flag);
	tty_close(tp);
	return 0;
}
 
static int
ssc_param(struct tty *tp, struct termios *t)
{

	return 0;
}

static void
ssc_start(struct tty *tp)
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
		ssc_cnputc(NULL, getc(&tp->t_outq));
	tp->t_state &= ~TS_BUSY;

	ttwwakeup(tp);
	splx(s);
}

/*
 * Stop output on a line.
 */
static void
ssc_stop(struct tty *tp, int flag)
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
}

static void
ssc_timeout(void *v)
{
	struct tty *tp = v;
	int c;

	while ((c = ssc_cngetc(NULL)) != -1) {
		if (tp->t_state & TS_ISOPEN)
			ttyld_rint(tp, c);
	}
	ssc_timeouthandle = timeout(ssc_timeout, tp, polltime);
}

CONSOLE_DRIVER(ssc);
