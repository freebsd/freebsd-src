/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#ifndef BURN_BRIDGES
#if defined(COMPAT_43)
#include <sys/ioctl_compat.h>
#endif
#endif
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>

static MALLOC_DEFINE(M_PTY, "ptys", "pty data structures");

static void ptsstart(struct tty *tp);
static void ptsstop(struct tty *tp, int rw);
static void ptcwakeup(struct tty *tp, int flag);
static struct cdev *ptyinit(struct cdev *cdev);

static	d_open_t	ptsopen;
static	d_close_t	ptsclose;
static	d_read_t	ptsread;
static	d_write_t	ptswrite;
static	d_ioctl_t	ptsioctl;
static	d_open_t	ptcopen;
static	d_close_t	ptcclose;
static	d_read_t	ptcread;
static	d_ioctl_t	ptcioctl;
static	d_write_t	ptcwrite;
static	d_poll_t	ptcpoll;

static struct cdevsw pts_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	ptsopen,
	.d_close =	ptsclose,
	.d_read =	ptsread,
	.d_write =	ptswrite,
	.d_ioctl =	ptsioctl,
	.d_name =	"pts",
	.d_flags =	D_TTY | D_NEEDGIANT,
};

static struct cdevsw ptc_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	ptcopen,
	.d_close =	ptcclose,
	.d_read =	ptcread,
	.d_write =	ptcwrite,
	.d_ioctl =	ptcioctl,
	.d_poll =	ptcpoll,
	.d_name =	"ptc",
	.d_flags =	D_TTY | D_NEEDGIANT,
};

#define BUFSIZ 100		/* Chunk size iomoved to/from user */

struct	ptsc {
	int	pt_flags;
	struct	selinfo pt_selr, pt_selw;
	u_char	pt_send;
	u_char	pt_ucntl;
	struct tty *pt_tty;
	struct cdev *devs, *devc;
	struct	prison *pt_prison;
};

#define	PF_PKT		0x08		/* packet mode */
#define	PF_STOPPED	0x10		/* user told stopped */
#define	PF_NOSTOP	0x40
#define PF_UCNTL	0x80		/* user control mode */

#define	TSA_PTC_READ(tp)	((void *)&(tp)->t_outq.c_cf)
#define	TSA_PTC_WRITE(tp)	((void *)&(tp)->t_rawq.c_cl)
#define	TSA_PTS_READ(tp)	((void *)&(tp)->t_canq)

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
static struct cdev *
ptyinit(struct cdev *devc)
{
	struct cdev *devs;
	struct ptsc *pt;
	int n;

	n = minor(devc);
	/* For now we only map the lower 8 bits of the minor */
	if (n & ~0xff)
		return (NULL);

	devc->si_flags &= ~SI_CHEAPCLONE;

	pt = malloc(sizeof(*pt), M_PTY, M_WAITOK | M_ZERO);
	pt->devs = devs = make_dev(&pts_cdevsw, n,
	    UID_ROOT, GID_WHEEL, 0666, "tty%c%r", names[n / 32], n % 32);
	pt->devc = devc;

	pt->pt_tty = ttymalloc(pt->pt_tty);
	pt->pt_tty->t_sc = pt;
	devs->si_drv1 = devc->si_drv1 = pt;
	devs->si_tty = devc->si_tty = pt->pt_tty;
	pt->pt_tty->t_dev = devs;
	return (devc);
}

/*ARGSUSED*/
static	int
ptsopen(struct cdev *dev, int flag, int devtype, struct thread *td)
{
	struct tty *tp;
	int error;
	struct ptsc *pt;

	if (!dev->si_drv1)
		return(ENXIO);
	pt = dev->si_drv1;
	tp = dev->si_tty;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttyinitmode(tp, 1, 0);
	} else if (tp->t_state & TS_XCLUDE && suser(td))
		return (EBUSY);
	else if (pt->pt_prison != td->td_ucred->cr_prison)
		return (EBUSY);
	if (tp->t_oproc)			/* Ctrlr still around. */
		(void)ttyld_modem(tp, 1);
	while ((tp->t_state & TS_CARR_ON) == 0) {
		if (flag&FNONBLOCK)
			break;
		error = ttysleep(tp, TSA_CARR_ON(tp), TTIPRI | PCATCH,
				 "ptsopn", 0);
		if (error)
			return (error);
	}
	error = ttyld_open(tp, dev);
	if (error == 0)
		ptcwakeup(tp, FREAD|FWRITE);
	return (error);
}

static	int
ptsclose(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct tty *tp;
	int err;

	tp = dev->si_tty;
	err = ttyld_close(tp, flag);
	(void) tty_close(tp);
	return (err);
}

static	int
ptsread(struct cdev *dev, struct uio *uio, int flag)
{
	struct tty *tp = dev->si_tty;
	int error = 0;

	if (tp->t_oproc)
		error = ttyld_read(tp, uio, flag);
	ptcwakeup(tp, FWRITE);
	return (error);
}

/*
 * Write to pseudo-tty.
 * Wakeups of controlling tty will happen
 * indirectly, when tty driver calls ptsstart.
 */
static	int
ptswrite(struct cdev *dev, struct uio *uio, int flag)
{
	struct tty *tp;

	tp = dev->si_tty;
	if (tp->t_oproc == 0)
		return (EIO);
	return (ttyld_write(tp, uio, flag));
}

/*
 * Start output on pseudo-tty.
 * Wake up process selecting or sleeping for input from controlling tty.
 */
static void
ptsstart(struct tty *tp)
{
	struct ptsc *pt = tp->t_sc;

	if (tp->t_state & TS_TTSTOP)
		return;
	if (pt->pt_flags & PF_STOPPED) {
		pt->pt_flags &= ~PF_STOPPED;
		pt->pt_send = TIOCPKT_START;
	}
	ptcwakeup(tp, FREAD);
}

static void
ptcwakeup(struct tty *tp, int flag)
{
	struct ptsc *pt = tp->t_sc;

	if (flag & FREAD) {
		selwakeuppri(&pt->pt_selr, TTIPRI);
		wakeup(TSA_PTC_READ(tp));
	}
	if (flag & FWRITE) {
		selwakeuppri(&pt->pt_selw, TTOPRI);
		wakeup(TSA_PTC_WRITE(tp));
	}
}

static	int
ptcopen(struct cdev *dev, int flag, int devtype, struct thread *td)
{
	struct tty *tp;
	struct ptsc *pt;

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
	(void)ttyld_modem(tp, 1);
	tp->t_lflag &= ~EXTPROC;
	pt = dev->si_drv1;
	pt->pt_prison = td->td_ucred->cr_prison;
	pt->pt_flags = 0;
	pt->pt_send = 0;
	pt->pt_ucntl = 0;
	return (0);
}

static	int
ptcclose(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct tty *tp;

	tp = dev->si_tty;
	(void)ttyld_modem(tp, 0);

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
ptcread(struct cdev *dev, struct uio *uio, int flag)
{
	struct tty *tp = dev->si_tty;
	struct ptsc *pt = dev->si_drv1;
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
			if (pt->pt_flags&PF_PKT && pt->pt_send) {
				error = ureadc((int)pt->pt_send, uio);
				if (error)
					return (error);
				if (pt->pt_send & TIOCPKT_IOCTL) {
					cc = min(uio->uio_resid,
						sizeof(tp->t_termios));
					uiomove(&tp->t_termios, cc, uio);
				}
				pt->pt_send = 0;
				return (0);
			}
			if (pt->pt_flags&PF_UCNTL && pt->pt_ucntl) {
				error = ureadc((int)pt->pt_ucntl, uio);
				if (error)
					return (error);
				pt->pt_ucntl = 0;
				return (0);
			}
			if (tp->t_outq.c_cc && (tp->t_state&TS_TTSTOP) == 0)
				break;
		}
		if ((tp->t_state & TS_CONNECTED) == 0)
			return (0);	/* EOF */
		if (flag & O_NONBLOCK)
			return (EWOULDBLOCK);
		error = tsleep(TSA_PTC_READ(tp), TTIPRI | PCATCH, "ptcin", 0);
		if (error)
			return (error);
	}
	if (pt->pt_flags & (PF_PKT|PF_UCNTL))
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
ptsstop(struct tty *tp, int flush)
{
	struct ptsc *pt = tp->t_sc;
	int flag;

	/* note: FLUSHREAD and FLUSHWRITE already ok */
	if (flush == 0) {
		flush = TIOCPKT_STOP;
		pt->pt_flags |= PF_STOPPED;
	} else
		pt->pt_flags &= ~PF_STOPPED;
	pt->pt_send |= flush;
	/* change of perspective */
	flag = 0;
	if (flush & FREAD)
		flag |= FWRITE;
	if (flush & FWRITE)
		flag |= FREAD;
	ptcwakeup(tp, flag);
}

static	int
ptcpoll(struct cdev *dev, int events, struct thread *td)
{
	struct tty *tp = dev->si_tty;
	struct ptsc *pt = dev->si_drv1;
	int revents = 0;
	int s;

	if ((tp->t_state & TS_CONNECTED) == 0)
		return (events & 
		   (POLLHUP | POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM));

	/*
	 * Need to block timeouts (ttrstart).
	 */
	s = spltty();

	if (events & (POLLIN | POLLRDNORM))
		if ((tp->t_state & TS_ISOPEN) &&
		    ((tp->t_outq.c_cc && (tp->t_state & TS_TTSTOP) == 0) ||
		     ((pt->pt_flags & PF_PKT) && pt->pt_send) ||
		     ((pt->pt_flags & PF_UCNTL) && pt->pt_ucntl)))
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if (tp->t_state & TS_ISOPEN &&
		    (((tp->t_rawq.c_cc + tp->t_canq.c_cc < TTYHOG - 2) ||
		      (tp->t_canq.c_cc == 0 && (tp->t_lflag & ICANON)))))
			revents |= events & (POLLOUT | POLLWRNORM);

	if (events & POLLHUP)
		if ((tp->t_state & TS_CARR_ON) == 0)
			revents |= POLLHUP;

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(td, &pt->pt_selr);

		if (events & (POLLOUT | POLLWRNORM))
			selrecord(td, &pt->pt_selw);
	}
	splx(s);

	return (revents);
}

static	int
ptcwrite(struct cdev *dev, struct uio *uio, int flag)
{
	struct tty *tp = dev->si_tty;
	u_char *cp = 0;
	int cc = 0;
	u_char locbuf[BUFSIZ];
	int cnt = 0;
	int error = 0;

again:
	if ((tp->t_state&TS_ISOPEN) == 0)
		goto block;
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
			ttyld_rint(tp, *cp++);
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
	if (flag & O_NONBLOCK) {
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
ptcioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct tty *tp = dev->si_tty;
	struct ptsc *pt = dev->si_drv1;

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
			if (pt->pt_flags & PF_UCNTL)
				return (EINVAL);
			pt->pt_flags |= PF_PKT;
		} else
			pt->pt_flags &= ~PF_PKT;
		return (0);

	case TIOCUCNTL:
		if (*(int *)data) {
			if (pt->pt_flags & PF_PKT)
				return (EINVAL);
			pt->pt_flags |= PF_UCNTL;
		} else
			pt->pt_flags &= ~PF_UCNTL;
		return (0);
	}

	/*
	 * The rest of the ioctls shouldn't be called until
	 * the slave is open.
	 */
	if ((tp->t_state & TS_ISOPEN) == 0)
		return (EAGAIN);

	switch (cmd) {
#ifndef BURN_BRIDGES
#ifdef COMPAT_43
	case TIOCSETP:
	case TIOCSETN:
#endif
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

	return (ptsioctl(dev, cmd, data, flag, td));
}

/*ARGSUSED*/
static	int
ptsioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct tty *tp = dev->si_tty;
	struct ptsc *pt = dev->si_drv1;
	u_char *cc = tp->t_cc;
	int stop, error;

	if (cmd == TIOCEXT) {
		/*
		 * When the EXTPROC bit is being toggled, we need
		 * to send an TIOCPKT_IOCTL if the packet driver
		 * is turned on.
		 */
		if (*(int *)data) {
			if (pt->pt_flags & PF_PKT) {
				pt->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag |= EXTPROC;
		} else {
			if ((tp->t_lflag & EXTPROC) &&
			    (pt->pt_flags & PF_PKT)) {
				pt->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag &= ~EXTPROC;
		}
		return(0);
	}
	error = ttyioctl(dev, cmd, data, flag, td);
	if (error == ENOTTY) {
		if (pt->pt_flags & PF_UCNTL &&
		    (cmd & ~0xff) == UIOCCMD(0)) {
			if (cmd & 0xff) {
				pt->pt_ucntl = (u_char)cmd;
				ptcwakeup(tp, FREAD);
			}
			return (0);
		}
		error = ENOTTY;
	}
	/*
	 * If external processing and packet mode send ioctl packet.
	 */
	if ((tp->t_lflag&EXTPROC) && (pt->pt_flags & PF_PKT)) {
		switch(cmd) {
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
#ifndef BURN_BRIDGES
#ifdef COMPAT_43
		case TIOCSETP:
		case TIOCSETN:
		case TIOCSETC:
		case TIOCSLTC:
		case TIOCLBIS:
		case TIOCLBIC:
		case TIOCLSET:
#endif
#endif
			pt->pt_send |= TIOCPKT_IOCTL;
			ptcwakeup(tp, FREAD);
			break;
		default:
			break;
		}
	}
	stop = (tp->t_iflag & IXON) && CCEQ(cc[VSTOP], CTRL('s'))
		&& CCEQ(cc[VSTART], CTRL('q'));
	if (pt->pt_flags & PF_NOSTOP) {
		if (stop) {
			pt->pt_send &= ~TIOCPKT_NOSTOP;
			pt->pt_send |= TIOCPKT_DOSTOP;
			pt->pt_flags &= ~PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	} else {
		if (!stop) {
			pt->pt_send &= ~TIOCPKT_DOSTOP;
			pt->pt_send |= TIOCPKT_NOSTOP;
			pt->pt_flags |= PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	}
	return (error);
}

static void
pty_clone(void *arg, char *name, int namelen, struct cdev **dev)
{
	int u;

	if (*dev != NULL)
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
	dev_ref(*dev);
	(*dev)->si_flags |= SI_CHEAPCLONE;
	return;
}

static void
ptc_drvinit(void *unused)
{

	EVENTHANDLER_REGISTER(dev_clone, pty_clone, 0, 1000);
}

SYSINIT(ptcdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE,ptc_drvinit,NULL)
