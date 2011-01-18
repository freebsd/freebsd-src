/*-
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)subr_log.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Error log buffer for kernel printf's.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/filio.h>
#include <sys/ttycom.h>
#include <sys/msgbuf.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/poll.h>
#include <sys/filedesc.h>
#include <sys/sysctl.h>

#define LOG_RDPRI	(PZERO + 1)

#define LOG_ASYNC	0x04
#define LOG_RDWAIT	0x08

static	d_open_t	logopen;
static	d_close_t	logclose;
static	d_read_t	logread;
static	d_ioctl_t	logioctl;
static	d_poll_t	logpoll;

static	void logtimeout(void *arg);

static struct cdevsw log_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	logopen,
	.d_close =	logclose,
	.d_read =	logread,
	.d_ioctl =	logioctl,
	.d_poll =	logpoll,
	.d_name =	"log",
};

static struct logsoftc {
	int	sc_state;		/* see above for possibilities */
	struct	selinfo sc_selp;	/* process waiting on select call */
	struct  sigio *sc_sigio;	/* information for async I/O */
	struct	callout sc_callout;	/* callout to wakeup syslog  */
} logsoftc;

int	log_open;			/* also used in log() */

/* Times per second to check for a pending syslog wakeup. */
static int	log_wakeups_per_second = 5;
SYSCTL_INT(_kern, OID_AUTO, log_wakeups_per_second, CTLFLAG_RW,
    &log_wakeups_per_second, 0, "");

/*ARGSUSED*/
static	int
logopen(struct cdev *dev, int flags, int mode, struct thread *td)
{
	if (log_open)
		return (EBUSY);
	log_open = 1;
	callout_init(&logsoftc.sc_callout, 0);
	fsetown(td->td_proc->p_pid, &logsoftc.sc_sigio);	/* signal process only */
	if (log_wakeups_per_second < 1) {
		printf("syslog wakeup is less than one.  Adjusting to 1.\n");
		log_wakeups_per_second = 1;
	}
	callout_reset(&logsoftc.sc_callout, hz / log_wakeups_per_second,
	    logtimeout, NULL);
	return (0);
}

/*ARGSUSED*/
static	int
logclose(struct cdev *dev, int flag, int mode, struct thread *td)
{

	log_open = 0;
	callout_stop(&logsoftc.sc_callout);
	logsoftc.sc_state = 0;
	funsetown(&logsoftc.sc_sigio);
	return (0);
}

/*ARGSUSED*/
static	int
logread(struct cdev *dev, struct uio *uio, int flag)
{
	char buf[128];
	struct msgbuf *mbp = msgbufp;
	int error = 0, l, s;

	s = splhigh();
	while (msgbuf_getcount(mbp) == 0) {
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		logsoftc.sc_state |= LOG_RDWAIT;
		if ((error = tsleep(mbp, LOG_RDPRI | PCATCH, "klog", 0))) {
			splx(s);
			return (error);
		}
	}
	splx(s);
	logsoftc.sc_state &= ~LOG_RDWAIT;

	while (uio->uio_resid > 0) {
		l = imin(sizeof(buf), uio->uio_resid);
		l = msgbuf_getbytes(mbp, buf, l);
		if (l == 0)
			break;
		error = uiomove(buf, l, uio);
		if (error)
			break;
	}
	return (error);
}

/*ARGSUSED*/
static	int
logpoll(struct cdev *dev, int events, struct thread *td)
{
	int s;
	int revents = 0;

	s = splhigh();

	if (events & (POLLIN | POLLRDNORM)) {
		if (msgbuf_getcount(msgbufp) > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &logsoftc.sc_selp);
	}
	splx(s);
	return (revents);
}

static void
logtimeout(void *arg)
{

	if (!log_open)
		return;
	if (log_wakeups_per_second < 1) {
		printf("syslog wakeup is less than one.  Adjusting to 1.\n");
		log_wakeups_per_second = 1;
	}
	if (msgbuftrigger == 0) {
		callout_reset(&logsoftc.sc_callout,
		    hz / log_wakeups_per_second, logtimeout, NULL);
		return;
	}
	msgbuftrigger = 0;
	selwakeuppri(&logsoftc.sc_selp, LOG_RDPRI);
	if ((logsoftc.sc_state & LOG_ASYNC) && logsoftc.sc_sigio != NULL)
		pgsigio(&logsoftc.sc_sigio, SIGIO, 0);
	if (logsoftc.sc_state & LOG_RDWAIT) {
		wakeup(msgbufp);
		logsoftc.sc_state &= ~LOG_RDWAIT;
	}
	callout_reset(&logsoftc.sc_callout, hz / log_wakeups_per_second,
	    logtimeout, NULL);
}

/*ARGSUSED*/
static	int
logioctl(struct cdev *dev, u_long com, caddr_t data, int flag, struct thread *td)
{

	switch (com) {

	/* return number of characters immediately available */
	case FIONREAD:
		*(int *)data = msgbuf_getcount(msgbufp);
		break;

	case FIONBIO:
		break;

	case FIOASYNC:
		if (*(int *)data)
			logsoftc.sc_state |= LOG_ASYNC;
		else
			logsoftc.sc_state &= ~LOG_ASYNC;
		break;

	case FIOSETOWN:
		return (fsetown(*(int *)data, &logsoftc.sc_sigio));

	case FIOGETOWN:
		*(int *)data = fgetown(&logsoftc.sc_sigio);
		break;

	/* This is deprecated, FIOSETOWN should be used instead. */
	case TIOCSPGRP:
		return (fsetown(-(*(int *)data), &logsoftc.sc_sigio));

	/* This is deprecated, FIOGETOWN should be used instead */
	case TIOCGPGRP:
		*(int *)data = -fgetown(&logsoftc.sc_sigio);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

static void
log_drvinit(void *unused)
{

	make_dev_credf(MAKEDEV_ETERNAL, &log_cdevsw, 0, NULL, UID_ROOT,
	    GID_WHEEL, 0600, "klog");
}

SYSINIT(logdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE,log_drvinit,NULL);
