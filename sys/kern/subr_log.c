/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)subr_log.c	7.11 (Berkeley) 3/17/91
 *	$Id: subr_log.c,v 1.5 1993/11/25 01:33:16 wollman Exp $
 */

/*
 * Error log buffer for kernel printf's.
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "vnode.h"
#include "ioctl.h"
#include "msgbuf.h"
#include "file.h"

#define LOG_RDPRI	(PZERO + 1)

#define LOG_ASYNC	0x04
#define LOG_RDWAIT	0x08

struct logsoftc {
	int	sc_state;		/* see above for possibilities */
	pid_t	sc_sel;		/* pid of process waiting on select call 16 Jun 93 */
	int	sc_pgid;		/* process/group for async I/O */
} logsoftc;

int	log_open;			/* also used in log() */

/*ARGSUSED*/
int
logopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	register struct msgbuf *mbp = msgbufp;

	if (log_open)
		return (EBUSY);
	log_open = 1;
	logsoftc.sc_pgid = p->p_pid;		/* signal process only */
	/*
	 * Potential race here with putchar() but since putchar should be
	 * called by autoconf, msg_magic should be initialized by the time
	 * we get here.
	 */
	if (mbp->msg_magic != MSG_MAGIC) {
		register int i;

		mbp->msg_magic = MSG_MAGIC;
		mbp->msg_bufx = mbp->msg_bufr = 0;
		for (i=0; i < MSG_BSIZE; i++)
			mbp->msg_bufc[i] = 0;
	}
	return (0);
}

/*ARGSUSED*/
int
logclose(dev, flag)
	dev_t dev;
	int flag;
{
	log_open = 0;
	logsoftc.sc_state = 0;
	logsoftc.sc_sel = 0; /* 16 Jun 93 */
	return 0;
}

/*ARGSUSED*/
int
logread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct msgbuf *mbp = msgbufp;
	register long l;
	register int s;
	int error = 0;

	s = splhigh();
	while (mbp->msg_bufr == mbp->msg_bufx) {
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		logsoftc.sc_state |= LOG_RDWAIT;
		if (error = tsleep((caddr_t)mbp, LOG_RDPRI | PCATCH,
		    "klog", 0)) {
			splx(s);
			return (error);
		}
	}
	splx(s);
	logsoftc.sc_state &= ~LOG_RDWAIT;

	while (uio->uio_resid > 0) {
		l = mbp->msg_bufx - mbp->msg_bufr;
		if (l < 0)
			l = MSG_BSIZE - mbp->msg_bufr;
		l = MIN(l, uio->uio_resid);
		if (l == 0)
			break;
		error = uiomove((caddr_t)&mbp->msg_bufc[mbp->msg_bufr],
			(int)l, uio);
		if (error)
			break;
		mbp->msg_bufr += l;
		if (mbp->msg_bufr < 0 || mbp->msg_bufr >= MSG_BSIZE)
			mbp->msg_bufr = 0;
	}
	return (error);
}

/*ARGSUSED*/
int
logselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	int s = splhigh();

	switch (rw) {

	case FREAD:
		if (msgbufp->msg_bufr != msgbufp->msg_bufx) {
			splx(s);
			return (1);
		}
		logsoftc.sc_sel = p->p_pid; /* 16 Jun 93 */
		break;
	}
	splx(s);
	return (0);
}

void
logwakeup()
{
	struct proc *p;

	if (!log_open)
		return;
	if (logsoftc.sc_sel) {			/* 16 Jun 93 */
		selwakeup(logsoftc.sc_sel, 0);
		logsoftc.sc_sel = 0;
	}
	if (logsoftc.sc_state & LOG_ASYNC) {
		if (logsoftc.sc_pgid < 0)
			gsignal(-logsoftc.sc_pgid, SIGIO); 
		else if (p = pfind(logsoftc.sc_pgid))
			psignal(p, SIGIO);
	}
	if (logsoftc.sc_state & LOG_RDWAIT) {
		wakeup((caddr_t)msgbufp);
		logsoftc.sc_state &= ~LOG_RDWAIT;
	}
}

/*ARGSUSED*/
int
logioctl(dev, com, data, flag)
	dev_t dev;
	int com;
	caddr_t data;
	int flag;
{
	long l;
	int s;

	switch (com) {

	/* return number of characters immediately available */
	case FIONREAD:
		s = splhigh();
		l = msgbufp->msg_bufx - msgbufp->msg_bufr;
		splx(s);
		if (l < 0)
			l += MSG_BSIZE;
		*(off_t *)data = l;
		break;

	case FIONBIO:
		break;

	case FIOASYNC:
		if (*(int *)data)
			logsoftc.sc_state |= LOG_ASYNC;
		else
			logsoftc.sc_state &= ~LOG_ASYNC;
		break;

	case TIOCSPGRP:
		logsoftc.sc_pgid = *(int *)data;
		break;

	case TIOCGPGRP:
		*(int *)data = logsoftc.sc_pgid;
		break;

	default:
		return (-1);
	}
	return (0);
}
