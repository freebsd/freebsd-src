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
 *	$FreeBSD$
 */
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
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

static	d_open_t	sscopen;
static	d_close_t	sscclose;

static struct cdevsw ssc_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	sscopen,
	.d_close =	sscclose,
	.d_name =	"ssc",
	.d_flags =	D_TTY | D_NEEDGIANT,
};

static struct tty *ssc_tp = NULL;
static int polltime;
static struct callout_handle ssctimeouthandle
	= CALLOUT_HANDLE_INITIALIZER(&ssctimeouthandle);

static void	sscstart(struct tty *);
static void	ssctimeout(void *);
static int	sscparam(struct tty *, struct termios *);
static void	sscstop(struct tty *, int);

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
ssccnprobe(struct consdev *cp)
{
	sprintf(cp->cn_name, "ssccons");
	cp->cn_pri = CN_INTERNAL;
}

static void
ssccninit(struct consdev *cp)
{
}

static void
ssccnattach(void *arg)
{
	make_dev(&ssc_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "ssccons");
}
SYSINIT(ssccnattach, SI_SUB_DRIVERS, SI_ORDER_ANY, ssccnattach, 0);

static void
ssccnputc(struct consdev *cp, int c)
{
	ssc(c, 0, 0, 0, SSC_PUTCHAR);
}

static int
ssccngetc(struct consdev *cp)
{
	int c;
	do {
		c = ssc(0, 0, 0, 0, SSC_GETCHAR);
	} while (c == 0);

	return c;
}

static int
ssccncheckc(struct consdev *cp)
{
    int c;
    c = ssc(0, 0, 0, 0, SSC_GETCHAR);
    if (!c)
	    return -1;
    return c;
}

static int
sscopen(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct tty *tp;
	int s;
	int error = 0, setuptimeout = 0;
 
	tp = ssc_tp = dev->si_tty = ttymalloc(ssc_tp);

	s = spltty();
	tp->t_oproc = sscstart;
	tp->t_param = sscparam;
	tp->t_stop = sscstop;
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
		splx(s);
		return EBUSY;
	}

	splx(s);

	error = ttyld_open(tp, dev);

	if (error == 0 && setuptimeout) {
		polltime = hz / SSC_POLL_HZ;
		if (polltime < 1)
			polltime = 1;
		ssctimeouthandle = timeout(ssctimeout, tp, polltime);
	}
	return error;
}
 
static int
sscclose(struct cdev *dev, int flag, int mode, struct thread *td)
{
	int unit = minor(dev);
	struct tty *tp = ssc_tp;

	if (unit != 0)
		return ENXIO;

	untimeout(ssctimeout, tp, ssctimeouthandle);
	ttyld_close(tp, flag);
	tty_close(tp);
	return 0;
}
 
static int
sscparam(struct tty *tp, struct termios *t)
{

	return 0;
}

static void
sscstart(struct tty *tp)
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
		ssccnputc(NULL, getc(&tp->t_outq));
	tp->t_state &= ~TS_BUSY;

	ttwwakeup(tp);
	splx(s);
}

/*
 * Stop output on a line.
 */
static void
sscstop(struct tty *tp, int flag)
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
}

static void
ssctimeout(void *v)
{
	struct tty *tp = v;
	int c;

	while ((c = ssccncheckc(NULL)) != -1) {
		if (tp->t_state & TS_ISOPEN)
			ttyld_rint(tp, c);
	}
	ssctimeouthandle = timeout(ssctimeout, tp, polltime);
}

CONS_DRIVER(ssc, ssccnprobe, ssccninit, NULL, ssccngetc, ssccncheckc, ssccnputc, NULL);
