/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)tty_pty.c	8.4 (Berkeley) 2/20/95
 * $FreeBSD$
 */

/*
 * Pseudo-teletype Driver
 * (Actually two drivers, requiring two entries in 'cdevsw')
 */
#include "opt_compat.h"
#include "opt_tty.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
#include <sys/ioctl_compat.h>
#endif
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>

static MALLOC_DEFINE(M_PTY, "ptys", "pty data structures");

static void ptsstart(struct tty *tp);
static void ptsstop(struct tty *tp, int rw);
static void ptcwakeup(struct tty *tp, int flag);
static dev_t ptyinit(dev_t cdev);

static	d_open_t	ptsopen;
static	d_close_t	ptsclose;
static	d_read_t	ptsread;
static	d_write_t	ptswrite;
static	d_ioctl_t	ptyioctl;
static	d_open_t	ptcopen;
static	d_close_t	ptcclose;
static	d_read_t	ptcread;
static	d_write_t	ptcwrite;
static	d_poll_t	ptcpoll;

#define	CDEV_MAJOR_S	5
static struct cdevsw pts_cdevsw = {
	.d_open =	ptsopen,
	.d_close =	ptsclose,
	.d_read =	ptsread,
	.d_write =	ptswrite,
	.d_ioctl =	ptyioctl,
	.d_poll =	ttypoll,
	.d_name =	"pts",
	.d_maj =	CDEV_MAJOR_S,
	.d_flags =	D_TTY,
	.d_kqfilter =	ttykqfilter,
};

#define	CDEV_MAJOR_C	6
static struct cdevsw ptc_cdevsw = {
	.d_open =	ptcopen,
	.d_close =	ptcclose,
	.d_read =	ptcread,
	.d_write =	ptcwrite,
	.d_ioctl =	ptyioctl,
	.d_poll =	ptcpoll,
	.d_name =	"ptc",
	.d_maj =	CDEV_MAJOR_C,
	.d_flags =	D_TTY,
	.d_kqfilter =	ttykqfilter,
};

#define BUFSIZ 100		/* Chunk size iomoved to/from user */

struct	pt_ioctl {
	int	pt_flags;
	struct	selinfo pt_selr, pt_selw;
	u_char	pt_send;
	u_char	pt_ucntl;
	struct tty pt_tty;
	dev_t	devs, devc;
	struct	prison *pt_prison;
};

#define	PF_PKT		0x08		/* packet mode */
#define	PF_STOPPED	0x10		/* user told stopped */
#define	PF_REMOTE	0x20		/* remote and flow controlled input */
#define	PF_NOSTOP	0x40
#define PF_UCNTL	0x80		/* user control mode */

static char *names = "pqrsPQRS";
/*
 * This function creates and initializes a pts/ptc pair
 *
 * pts == /dev/tty[pqrsPQRS][0123456789abcdefghijklmnopqrstuv]
 * ptc == /dev/pty[pqrsPQRS][0123456789abcdefghijklmnopqrstuv]
 *
 * XXX: define and add mapping of upper minor bits to allow more
 *      than 256 ptys.
 */
static dev_t
ptyinit(dev_t devc)
{
	dev_t devs;
	struct pt_ioctl *pt;
	int n;

	n = minor(devc);
	/* For now we only map the lower 8 bits of the minor */
	if (n & ~0xff)
		return (NODEV);

	devc->si_flags &= ~SI_CHEAPCLONE;

	pt = malloc(sizeof(*pt), M_PTY, M_WAITOK | M_ZERO);
	pt->devs = devs = make_dev(&pts_cdevsw, n,
	    UID_ROOT, GID_WHEEL, 0666, "tty%c%r", names[n / 32], n % 32);
	pt->devc = devc;

	devs->si_drv1 = devc->si_drv1 = pt;
	devs->si_tty = devc->si_tty = &pt->pt_tty;
	pt->pt_tty.t_dev = devs;
	ttyregister(&pt->pt_tty);
	return (devc);
}

/*ARGSUSED*/
static	int
ptsopen(dev, flag, devtype, td)
	dev_t dev;
	int flag, devtype;
	struct thread *td;
{
	struct tty *tp;
	int error;
	struct pt_ioctl *pti;

	if (!dev->si_drv1)
		return(ENXIO);
	pti = dev->si_drv1;
	tp = dev->si_tty;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);		/* Set up default chars */
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
	} else if (tp->t_state & TS_XCLUDE && suser(td)) {
		return (EBUSY);
	} else if (pti->pt_prison != td->td_ucred->cr_prison) {
		return (EBUSY);
	}
	if (tp->t_oproc)			/* Ctrlr still around. */
		(void)(*linesw[tp->t_line].l_modem)(tp, 1);
	while ((tp->t_state & TS_CARR_ON) == 0) {
		if (flag&FNONBLOCK)
			break;
		error = ttysleep(tp, TSA_CARR_ON(tp), TTIPRI | PCATCH,
				 "ptsopn", 0);
		if (error)
			return (error);
	}
	error = (*linesw[tp->t_line].l_open)(dev, tp);
	if (error == 0)
		ptcwakeup(tp, FREAD|FWRITE);
	return (error);
}

static	int
ptsclose(dev, flag, mode, td)
	dev_t dev;
	int flag, mode;
	struct thread *td;
{
	struct tty *tp;
	int err;

	tp = dev->si_tty;
	err = (*linesw[tp->t_line].l_close)(tp, flag);
	ptsstop(tp, FREAD|FWRITE);
	(void) ttyclose(tp);
	return (err);
}

static	int
ptsread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct tty *tp = dev->si_tty;
	struct pt_ioctl *pti = dev->si_drv1;
	struct pgrp *pg;
	int error = 0;

again:
	if (pti->pt_flags & PF_REMOTE) {
		while (isbackground(p, tp)) {
			sx_slock(&proctree_lock);
			PROC_LOCK(p);
			if (SIGISMEMBER(p->p_sigacts->ps_sigignore, SIGTTIN) ||
			    SIGISMEMBER(td->td_sigmask, SIGTTIN) ||
			    p->p_pgrp->pg_jobc == 0 || p->p_flag & P_PPWAIT) {
				PROC_UNLOCK(p);
				sx_sunlock(&proctree_lock);
				return (EIO);
			}
			pg = p->p_pgrp;
			PROC_UNLOCK(p);
			PGRP_LOCK(pg);
			sx_sunlock(&proctree_lock);
			pgsignal(pg, SIGTTIN, 1);
			PGRP_UNLOCK(pg);
			error = ttysleep(tp, &lbolt, TTIPRI | PCATCH, "ptsbg",
					 0);
			if (error)
				return (error);
		}
		if (tp->t_canq.c_cc == 0) {
			if (flag & IO_NDELAY)
				return (EWOULDBLOCK);
			error = ttysleep(tp, TSA_PTS_READ(tp), TTIPRI | PCATCH,
					 "ptsin", 0);
			if (error)
				return (error);
			goto again;
		}
		while (tp->t_canq.c_cc > 1 && uio->uio_resid > 0)
			if (ureadc(getc(&tp->t_canq), uio) < 0) {
				error = EFAULT;
				break;
			}
		if (tp->t_canq.c_cc == 1)
			(void) getc(&tp->t_canq);
		if (tp->t_canq.c_cc)
			return (error);
	} else
		if (tp->t_oproc)
			error = (*linesw[tp->t_line].l_read)(tp, uio, flag);
	ptcwakeup(tp, FWRITE);
	return (error);
}

/*
 * Write to pseudo-tty.
 * Wakeups of controlling tty will happen
 * indirectly, when tty driver calls ptsstart.
 */
static	int
ptswrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp;

	tp = dev->si_tty;
	if (tp->t_oproc == 0)
		return (EIO);
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

/*
 * Start output on pseudo-tty.
 * Wake up process selecting or sleeping for input from controlling tty.
 */
static void
ptsstart(tp)
	struct tty *tp;
{
	struct pt_ioctl *pti = tp->t_dev->si_drv1;

	if (tp->t_state & TS_TTSTOP)
		return;
	if (pti->pt_flags & PF_STOPPED) {
		pti->pt_flags &= ~PF_STOPPED;
		pti->pt_send = TIOCPKT_START;
	}
	ptcwakeup(tp, FREAD);
}

static void
ptcwakeup(tp, flag)
	struct tty *tp;
	int flag;
{
	struct pt_ioctl *pti = tp->t_dev->si_drv1;

	if (flag & FREAD) {
		selwakeup(&pti->pt_selr);
		wakeup(TSA_PTC_READ(tp));
	}
	if (flag & FWRITE) {
		selwakeup(&pti->pt_selw);
		wakeup(TSA_PTC_WRITE(tp));
	}
}

static	int
ptcopen(dev, flag, devtype, td)
	dev_t dev;
	int flag, devtype;
	struct thread *td;
{
	struct tty *tp;
	struct pt_ioctl *pti;

	if (!dev->si_drv1)
		ptyinit(dev);
	if (!dev->si_drv1)
		return(ENXIO);
	tp = dev->si_tty;
	if (tp->t_oproc)
		return (EIO);
	tp->t_timeout = -1;
	tp->t_oproc = ptsstart;
	tp->t_stop = ptsstop;
	(void)(*linesw[tp->t_line].l_modem)(tp, 1);
	tp->t_lflag &= ~EXTPROC;
	pti = dev->si_drv1;
	pti->pt_prison = td->td_ucred->cr_prison;
	pti->pt_flags = 0;
	pti->pt_send = 0;
	pti->pt_ucntl = 0;
	return (0);
}

static	int
ptcclose(dev, flags, fmt, td)
	dev_t dev;
	int flags;
	int fmt;
	struct thread *td;
{
	struct tty *tp;

	tp = dev->si_tty;
	(void)(*linesw[tp->t_line].l_modem)(tp, 0);

	/*
	 * XXX MDMBUF makes no sense for ptys but would inhibit the above
	 * l_modem().  CLOCAL makes sense but isn't supported.   Special
	 * l_modem()s that ignore carrier drop make no sense for ptys but
	 * may be in use because other parts of the line discipline make
	 * sense for ptys.  Recover by doing everything that a normal
	 * ttymodem() would have done except for sending a SIGHUP.
	 */
	if (tp->t_state & TS_ISOPEN) {
		tp->t_state &= ~(TS_CARR_ON | TS_CONNECTED);
		tp->t_state |= TS_ZOMBIE;
		ttyflush(tp, FREAD | FWRITE);
	}

	tp->t_oproc = 0;		/* mark closed */
	return (0);
}

static	int
ptcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp = dev->si_tty;
	struct pt_ioctl *pti = dev->si_drv1;
	char buf[BUFSIZ];
	int error = 0, cc;

	/*
	 * We want to block until the slave
	 * is open, and there's something to read;
	 * but if we lost the slave or we're NBIO,
	 * then return the appropriate error instead.
	 */
	for (;;) {
		if (tp->t_state&TS_ISOPEN) {
			if (pti->pt_flags&PF_PKT && pti->pt_send) {
				error = ureadc((int)pti->pt_send, uio);
				if (error)
					return (error);
				if (pti->pt_send & TIOCPKT_IOCTL) {
					cc = min(uio->uio_resid,
						sizeof(tp->t_termios));
					uiomove(&tp->t_termios, cc, uio);
				}
				pti->pt_send = 0;
				return (0);
			}
			if (pti->pt_flags&PF_UCNTL && pti->pt_ucntl) {
				error = ureadc((int)pti->pt_ucntl, uio);
				if (error)
					return (error);
				pti->pt_ucntl = 0;
				return (0);
			}
			if (tp->t_outq.c_cc && (tp->t_state&TS_TTSTOP) == 0)
				break;
		}
		if ((tp->t_state & TS_CONNECTED) == 0)
			return (0);	/* EOF */
		if (flag & IO_NDELAY)
			return (EWOULDBLOCK);
		error = tsleep(TSA_PTC_READ(tp), TTIPRI | PCATCH, "ptcin", 0);
		if (error)
			return (error);
	}
	if (pti->pt_flags & (PF_PKT|PF_UCNTL))
		error = ureadc(0, uio);
	while (uio->uio_resid > 0 && error == 0) {
		cc = q_to_b(&tp->t_outq, buf, min(uio->uio_resid, BUFSIZ));
		if (cc <= 0)
			break;
		error = uiomove(buf, cc, uio);
	}
	ttwwakeup(tp);
	return (error);
}

static	void
ptsstop(tp, flush)
	struct tty *tp;
	int flush;
{
	struct pt_ioctl *pti = tp->t_dev->si_drv1;
	int flag;

	/* note: FLUSHREAD and FLUSHWRITE already ok */
	if (flush == 0) {
		flush = TIOCPKT_STOP;
		pti->pt_flags |= PF_STOPPED;
	} else
		pti->pt_flags &= ~PF_STOPPED;
	pti->pt_send |= flush;
	/* change of perspective */
	flag = 0;
	if (flush & FREAD)
		flag |= FWRITE;
	if (flush & FWRITE)
		flag |= FREAD;
	ptcwakeup(tp, flag);
}

static	int
ptcpoll(dev, events, td)
	dev_t dev;
	int events;
	struct thread *td;
{
	struct tty *tp = dev->si_tty;
	struct pt_ioctl *pti = dev->si_drv1;
	int revents = 0;
	int s;

	if ((tp->t_state & TS_CONNECTED) == 0)
		return (seltrue(dev, events, td) | POLLHUP);

	/*
	 * Need to block timeouts (ttrstart).
	 */
	s = spltty();

	if (events & (POLLIN | POLLRDNORM))
		if ((tp->t_state & TS_ISOPEN) &&
		    ((tp->t_outq.c_cc && (tp->t_state & TS_TTSTOP) == 0) ||
		     ((pti->pt_flags & PF_PKT) && pti->pt_send) ||
		     ((pti->pt_flags & PF_UCNTL) && pti->pt_ucntl)))
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if (tp->t_state & TS_ISOPEN &&
		    ((pti->pt_flags & PF_REMOTE) ?
		     (tp->t_canq.c_cc == 0) :
		     ((tp->t_rawq.c_cc + tp->t_canq.c_cc < TTYHOG - 2) ||
		      (tp->t_canq.c_cc == 0 && (tp->t_lflag & ICANON)))))
			revents |= events & (POLLOUT | POLLWRNORM);

	if (events & POLLHUP)
		if ((tp->t_state & TS_CARR_ON) == 0)
			revents |= POLLHUP;

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(td, &pti->pt_selr);

		if (events & (POLLOUT | POLLWRNORM))
			selrecord(td, &pti->pt_selw);
	}
	splx(s);

	return (revents);
}

static	int
ptcwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp = dev->si_tty;
	u_char *cp = 0;
	int cc = 0;
	u_char locbuf[BUFSIZ];
	int cnt = 0;
	struct pt_ioctl *pti = dev->si_drv1;
	int error = 0;

again:
	if ((tp->t_state&TS_ISOPEN) == 0)
		goto block;
	if (pti->pt_flags & PF_REMOTE) {
		if (tp->t_canq.c_cc)
			goto block;
		while ((uio->uio_resid > 0 || cc > 0) &&
		       tp->t_canq.c_cc < TTYHOG - 1) {
			if (cc == 0) {
				cc = min(uio->uio_resid, BUFSIZ);
				cc = min(cc, TTYHOG - 1 - tp->t_canq.c_cc);
				cp = locbuf;
				error = uiomove(cp, cc, uio);
				if (error)
					return (error);
				/* check again for safety */
				if ((tp->t_state & TS_ISOPEN) == 0) {
					/* adjust as usual */
					uio->uio_resid += cc;
					return (EIO);
				}
			}
			if (cc > 0) {
				cc = b_to_q((char *)cp, cc, &tp->t_canq);
				/*
				 * XXX we don't guarantee that the canq size
				 * is >= TTYHOG, so the above b_to_q() may
				 * leave some bytes uncopied.  However, space
				 * is guaranteed for the null terminator if
				 * we don't fail here since (TTYHOG - 1) is
				 * not a multiple of CBSIZE.
				 */
				if (cc > 0)
					break;
			}
		}
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		(void) putc(0, &tp->t_canq);
		ttwakeup(tp);
		wakeup(TSA_PTS_READ(tp));
		return (0);
	}
	while (uio->uio_resid > 0 || cc > 0) {
		if (cc == 0) {
			cc = min(uio->uio_resid, BUFSIZ);
			cp = locbuf;
			error = uiomove(cp, cc, uio);
			if (error)
				return (error);
			/* check again for safety */
			if ((tp->t_state & TS_ISOPEN) == 0) {
				/* adjust for data copied in but not written */
				uio->uio_resid += cc;
				return (EIO);
			}
		}
		while (cc > 0) {
			if ((tp->t_rawq.c_cc + tp->t_canq.c_cc) >= TTYHOG - 2 &&
			   (tp->t_canq.c_cc > 0 || !(tp->t_lflag&ICANON))) {
				wakeup(TSA_HUP_OR_INPUT(tp));
				goto block;
			}
			(*linesw[tp->t_line].l_rint)(*cp++, tp);
			cnt++;
			cc--;
		}
		cc = 0;
	}
	return (0);
block:
	/*
	 * Come here to wait for slave to open, for space
	 * in outq, or space in rawq, or an empty canq.
	 */
	if ((tp->t_state & TS_CONNECTED) == 0) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		return (EIO);
	}
	if (flag & IO_NDELAY) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		if (cnt == 0)
			return (EWOULDBLOCK);
		return (0);
	}
	error = tsleep(TSA_PTC_WRITE(tp), TTOPRI | PCATCH, "ptcout", 0);
	if (error) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		return (error);
	}
	goto again;
}

/*ARGSUSED*/
static	int
ptyioctl(dev, cmd, data, flag, td)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct thread *td;
{
	struct tty *tp = dev->si_tty;
	struct pt_ioctl *pti = dev->si_drv1;
	u_char *cc = tp->t_cc;
	int stop, error;

	if (devsw(dev)->d_open == ptcopen) {
		switch (cmd) {

		case TIOCGPGRP:
			/*
			 * We avoid calling ttioctl on the controller since,
			 * in that case, tp must be the controlling terminal.
			 */
			*(int *)data = tp->t_pgrp ? tp->t_pgrp->pg_id : 0;
			return (0);

		case TIOCPKT:
			if (*(int *)data) {
				if (pti->pt_flags & PF_UCNTL)
					return (EINVAL);
				pti->pt_flags |= PF_PKT;
			} else
				pti->pt_flags &= ~PF_PKT;
			return (0);

		case TIOCUCNTL:
			if (*(int *)data) {
				if (pti->pt_flags & PF_PKT)
					return (EINVAL);
				pti->pt_flags |= PF_UCNTL;
			} else
				pti->pt_flags &= ~PF_UCNTL;
			return (0);

		case TIOCREMOTE:
			if (*(int *)data)
				pti->pt_flags |= PF_REMOTE;
			else
				pti->pt_flags &= ~PF_REMOTE;
			ttyflush(tp, FREAD|FWRITE);
			return (0);
		}

		/*
		 * The rest of the ioctls shouldn't be called until
		 * the slave is open.
		 */
		if ((tp->t_state & TS_ISOPEN) == 0)
			return (EAGAIN);

		switch (cmd) {
#ifdef COMPAT_43
		case TIOCSETP:
		case TIOCSETN:
#endif
		case TIOCSETD:
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
			/*
			 * IF CONTROLLER STTY THEN MUST FLUSH TO PREVENT A HANG.
			 * ttywflush(tp) will hang if there are characters in
			 * the outq.
			 */
			ndflush(&tp->t_outq, tp->t_outq.c_cc);
			break;

		case TIOCSIG:
			if (*(unsigned int *)data >= NSIG ||
			    *(unsigned int *)data == 0)
				return(EINVAL);
			if ((tp->t_lflag&NOFLSH) == 0)
				ttyflush(tp, FREAD|FWRITE);
			if (tp->t_pgrp != NULL) {
				PGRP_LOCK(tp->t_pgrp);
				pgsignal(tp->t_pgrp, *(unsigned int *)data, 1);
				PGRP_UNLOCK(tp->t_pgrp);
			}
			if ((*(unsigned int *)data == SIGINFO) &&
			    ((tp->t_lflag&NOKERNINFO) == 0))
				ttyinfo(tp);
			return(0);
		}
	}
	if (cmd == TIOCEXT) {
		/*
		 * When the EXTPROC bit is being toggled, we need
		 * to send an TIOCPKT_IOCTL if the packet driver
		 * is turned on.
		 */
		if (*(int *)data) {
			if (pti->pt_flags & PF_PKT) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag |= EXTPROC;
		} else {
			if ((tp->t_lflag & EXTPROC) &&
			    (pti->pt_flags & PF_PKT)) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag &= ~EXTPROC;
		}
		return(0);
	}
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, td);
	if (error == ENOIOCTL)
		 error = ttioctl(tp, cmd, data, flag);
	if (error == ENOIOCTL) {
		if (pti->pt_flags & PF_UCNTL &&
		    (cmd & ~0xff) == UIOCCMD(0)) {
			if (cmd & 0xff) {
				pti->pt_ucntl = (u_char)cmd;
				ptcwakeup(tp, FREAD);
			}
			return (0);
		}
		error = ENOTTY;
	}
	/*
	 * If external processing and packet mode send ioctl packet.
	 */
	if ((tp->t_lflag&EXTPROC) && (pti->pt_flags & PF_PKT)) {
		switch(cmd) {
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
#ifdef COMPAT_43
		case TIOCSETP:
		case TIOCSETN:
#endif
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
		case TIOCSETC:
		case TIOCSLTC:
		case TIOCLBIS:
		case TIOCLBIC:
		case TIOCLSET:
#endif
			pti->pt_send |= TIOCPKT_IOCTL;
			ptcwakeup(tp, FREAD);
		default:
			break;
		}
	}
	stop = (tp->t_iflag & IXON) && CCEQ(cc[VSTOP], CTRL('s'))
		&& CCEQ(cc[VSTART], CTRL('q'));
	if (pti->pt_flags & PF_NOSTOP) {
		if (stop) {
			pti->pt_send &= ~TIOCPKT_NOSTOP;
			pti->pt_send |= TIOCPKT_DOSTOP;
			pti->pt_flags &= ~PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	} else {
		if (!stop) {
			pti->pt_send &= ~TIOCPKT_DOSTOP;
			pti->pt_send |= TIOCPKT_NOSTOP;
			pti->pt_flags |= PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	}
	return (error);
}


static void ptc_drvinit(void *unused);

static void pty_clone(void *arg, char *name, int namelen, dev_t *dev);

static void
pty_clone(arg, name, namelen, dev)
	void *arg;
	char *name;
	int namelen;
	dev_t *dev;
{
	int u;

	if (*dev != NODEV)
		return;
	if (bcmp(name, "pty", 3) != 0)
		return;
	if (name[5] != '\0')
		return;
	switch (name[3]) {
	case 'p': u =   0; break;
	case 'q': u =  32; break;
	case 'r': u =  64; break;
	case 's': u =  96; break;
	case 'P': u = 128; break;
	case 'Q': u = 160; break;
	case 'R': u = 192; break;
	case 'S': u = 224; break;
	default: return;
	}
	if (name[4] >= '0' && name[4] <= '9')
		u += name[4] - '0';
	else if (name[4] >= 'a' && name[4] <= 'v')
		u += name[4] - 'a' + 10;
	else
		return;
	*dev = make_dev(&ptc_cdevsw, u,
	    UID_ROOT, GID_WHEEL, 0666, "pty%c%r", names[u / 32], u % 32);
	(*dev)->si_flags |= SI_CHEAPCLONE;
	return;
}

static void
ptc_drvinit(unused)
	void *unused;
{

	EVENTHANDLER_REGISTER(dev_clone, pty_clone, 0, 1000);
}

SYSINIT(ptcdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR_C,ptc_drvinit,NULL)
