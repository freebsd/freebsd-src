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
 * $FreeBSD$
 */

/*
 * Pseudo-nulmodem Driver
 */
#include "opt_compat.h"
#include <sys/param.h>
#include <sys/systm.h>
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

MALLOC_DEFINE(M_NLMDM, "nullmodem", "nullmodem data structures");

static void nmdmstart __P((struct tty *tp));
static void nmdmstop __P((struct tty *tp, int rw));
static void wakeup_other __P((struct tty *tp, int flag));
static void nmdminit __P((int n));

static	d_open_t	nmdmopen;
static	d_close_t	nmdmclose;
static	d_read_t	nmdmread;
static	d_write_t	nmdmwrite;
static	d_ioctl_t	nmdmioctl;

#define	CDEV_MAJOR	18
static struct cdevsw nmdm_cdevsw = {
	/* open */	nmdmopen,
	/* close */	nmdmclose,
	/* read */	nmdmread,
	/* write */	nmdmwrite,
	/* ioctl */	nmdmioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"pts",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY,
};

#define BUFSIZ 100		/* Chunk size iomoved to/from user */

struct softpart {
	struct tty nm_tty;
	dev_t	dev;
	int	modemsignals;	/* bits defined in sys/ttycom.h */
	int	gotbreak;
};

struct	nm_softc {
	int	pt_flags;
	struct softpart part1, part2;
	struct	prison *pt_prison;
};

#define	PF_STOPPED	0x10		/* user told stopped */

static void
nmdm_crossover(struct nm_softc *pti,
		struct softpart *ourpart,
		struct softpart *otherpart);

#define GETPARTS(tp, ourpart, otherpart) \
do {	\
	struct nm_softc *pti = tp->t_dev->si_drv1; \
	if (tp == &pti->part1.nm_tty) { \
		ourpart = &pti->part1; \
		otherpart = &pti->part2; \
	} else { \
		ourpart = &pti->part2; \
		otherpart = &pti->part1; \
	}  \
} while (0)

/*
 * This function creates and initializes a pair of ttys.
 */
static void
nmdminit(n)
	int n;
{
	dev_t dev1, dev2;
	struct nm_softc *pt;

	/* For now we only map the lower 8 bits of the minor */
	if (n & ~0xff)
		return;

	pt = malloc(sizeof(*pt), M_NLMDM, M_WAITOK);
	bzero(pt, sizeof(*pt));
	pt->part1.dev = dev1 = make_dev(&nmdm_cdevsw, n+n,
	    0, 0, 0666, "nmdm%dA", n);
	pt->part2.dev = dev2 = make_dev(&nmdm_cdevsw, n+n+1,
	    0, 0, 0666, "nmdm%dB", n);

	dev1->si_drv1 = dev2->si_drv1 = pt;
	dev1->si_tty = &pt->part1.nm_tty;
	dev2->si_tty = &pt->part2.nm_tty;
	ttyregister(&pt->part1.nm_tty);
	ttyregister(&pt->part2.nm_tty);
	pt->part1.nm_tty.t_oproc = nmdmstart;
	pt->part2.nm_tty.t_oproc = nmdmstart;
	pt->part1.nm_tty.t_stop = nmdmstop;
	pt->part2.nm_tty.t_dev = dev1;
	pt->part1.nm_tty.t_dev = dev2;
	pt->part2.nm_tty.t_stop = nmdmstop;
}

/*ARGSUSED*/
static	int
nmdmopen(dev, flag, devtype, p)
	dev_t dev;
	int flag, devtype;
	struct proc *p;
{
	register struct tty *tp, *tp2;
	int error;
	int minr;
	dev_t nextdev;
	struct nm_softc *pti;
	int is_b;
	int	pair;
	struct	softpart *ourpart, *otherpart;

	/*
	 * XXX: Gross hack for DEVFS:
	 * If we openned this device, ensure we have the
	 * next one too, so people can open it.
	 */
	minr = dev2unit(dev);
	pair = minr >> 1;
	is_b = minr & 1;
	
	if (pair < 127) {
		nextdev = makedev(major(dev), (pair+pair) + 1);
		if (!nextdev->si_drv1) {
			nmdminit(pair + 1);
		}
	}
	if (!dev->si_drv1)
		nmdminit(pair);

	if (!dev->si_drv1)
		return(ENXIO);	

	pti = dev->si_drv1;
	if (is_b) 
		tp = &pti->part2.nm_tty;
	else 
		tp = &pti->part1.nm_tty;
	GETPARTS(tp, ourpart, otherpart);
	tp2 = &otherpart->nm_tty;
	ourpart->modemsignals |= TIOCM_LE;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);		/* Set up default chars */
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
	} else if (tp->t_state & TS_XCLUDE && suser(p)) {
		return (EBUSY);
	} else if (pti->pt_prison != p->p_ucred->cr_prison) {
		return (EBUSY);
	}

	/*
	 * If the other side is open we have carrier
	 */
	if (tp2->t_state & TS_ISOPEN) {
		(void)(*linesw[tp->t_line].l_modem)(tp, 1);
	}

	/*
	 * And the other side gets carrier as we are now open.
	 */
	(void)(*linesw[tp2->t_line].l_modem)(tp2, 1);

	/* External processing makes no sense here */
	tp->t_lflag &= ~EXTPROC;

	/* 
	 * Wait here if we don't have carrier.
	 */
#if 0
	while ((tp->t_state & TS_CARR_ON) == 0) {
		if (flag & FNONBLOCK)
			break;
		error = ttysleep(tp, TSA_CARR_ON(tp), TTIPRI | PCATCH,
				 "nmdopn", 0);
		if (error)
			return (error);
	}
#endif

	/*
	 * Give the line disciplin a chance to set this end up.
	 */
	error = (*linesw[tp->t_line].l_open)(dev, tp);

	/*
	 * Wake up the other side.
	 * Theoretically not needed.
	 */
	ourpart->modemsignals |= TIOCM_DTR;
	nmdm_crossover(pti, ourpart, otherpart);
	if (error == 0)
		wakeup_other(tp, FREAD|FWRITE); /* XXX */
	return (error);
}

static	int
nmdmclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct tty *tp, *tp2;
	int err;
	struct softpart *ourpart, *otherpart;

	/*
	 * let the other end know that the game is up
	 */
	tp = dev->si_tty;
	GETPARTS(tp, ourpart, otherpart);
	tp2 = &otherpart->nm_tty;
	(void)(*linesw[tp2->t_line].l_modem)(tp2, 0);

	/*
	 * XXX MDMBUF makes no sense for nmdms but would inhibit the above
	 * l_modem().  CLOCAL makes sense but isn't supported.   Special
	 * l_modem()s that ignore carrier drop make no sense for nmdms but
	 * may be in use because other parts of the line discipline make
	 * sense for nmdms.  Recover by doing everything that a normal
	 * ttymodem() would have done except for sending a SIGHUP.
	 */
	if (tp2->t_state & TS_ISOPEN) {
		tp2->t_state &= ~(TS_CARR_ON | TS_CONNECTED);
		tp2->t_state |= TS_ZOMBIE;
		ttyflush(tp2, FREAD | FWRITE);
	}

	err = (*linesw[tp->t_line].l_close)(tp, flag);
	ourpart->modemsignals &= ~TIOCM_DTR;
	nmdm_crossover(dev->si_drv1, ourpart, otherpart);
	nmdmstop(tp, FREAD|FWRITE);
	(void) ttyclose(tp);
	return (err);
}

static	int
nmdmread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int error = 0;
	struct tty *tp, *tp2;
	struct softpart *ourpart, *otherpart;

	tp = dev->si_tty;
	GETPARTS(tp, ourpart, otherpart);
	tp2 = &otherpart->nm_tty;

#if 0
	if (tp2->t_state & TS_ISOPEN) {
		error = (*linesw[tp->t_line].l_read)(tp, uio, flag);
		wakeup_other(tp, FWRITE);
	} else {
		if (flag & IO_NDELAY) {
			return (EWOULDBLOCK);
		}
		error = tsleep(TSA_PTC_READ(tp),
				TTIPRI | PCATCH, "nmdout", 0);
		}
	}
#else
	if ((error = (*linesw[tp->t_line].l_read)(tp, uio, flag)) == 0)
		wakeup_other(tp, FWRITE);
#endif
	return (error);
}

/*
 * Write to pseudo-tty.
 * Wakeups of controlling tty will happen
 * indirectly, when tty driver calls nmdmstart.
 */
static	int
nmdmwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register u_char *cp = 0;
	register int cc = 0;
	u_char locbuf[BUFSIZ];
	int cnt = 0;
	int error = 0;
	struct tty *tp1, *tp;
	struct softpart *ourpart, *otherpart;

	tp1 = dev->si_tty;
	/*
	 * Get the other tty struct.
	 * basically we are writing into the INPUT side of the other device.
	 */
	GETPARTS(tp1, ourpart, otherpart);
	tp = &otherpart->nm_tty;

again:
	if ((tp->t_state & TS_ISOPEN) == 0) 
		return (EIO);
	while (uio->uio_resid > 0 || cc > 0) {
		/*
		 * Fill up the buffer if it's empty
		 */
		if (cc == 0) {
			cc = min(uio->uio_resid, BUFSIZ);
			cp = locbuf;
			error = uiomove((caddr_t)cp, cc, uio);
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
			if (((tp->t_rawq.c_cc + tp->t_canq.c_cc) >= (TTYHOG-2))
			&& ((tp->t_canq.c_cc > 0) || !(tp->t_iflag&ICANON))) {
				/*
	 			 * Come here to wait for space in outq,
				 * or space in rawq, or an empty canq.
	 			 */
				wakeup(TSA_HUP_OR_INPUT(tp));
				if ((tp->t_state & TS_CONNECTED) == 0) {
					/*
					 * Data piled up because not connected.
					 * Adjust for data copied in but
					 * not written.
					 */
					uio->uio_resid += cc;
					return (EIO);
				}
				if (flag & IO_NDELAY) {
					/*
				         * Don't wait if asked not to.
					 * Adjust for data copied in but
					 * not written.
					 */
					uio->uio_resid += cc;
					if (cnt == 0)
						return (EWOULDBLOCK);
					return (0);
				}
				error = tsleep(TSA_PTC_WRITE(tp),
						TTOPRI | PCATCH, "nmdout", 0);
				if (error) {
					/*
					 * Tsleep returned (signal?).
					 * Go find out what the user wants.
					 * adjust for data copied in but
					 * not written
					 */
					uio->uio_resid += cc;
					return (error);
				}
				goto again;
			}
			(*linesw[tp->t_line].l_rint)(*cp++, tp);
			cnt++;
			cc--;
		}
		cc = 0;
	}
	return (0);
}

/*
 * Start output on pseudo-tty.
 * Wake up process selecting or sleeping for input from controlling tty.
 */
static void
nmdmstart(tp)
	struct tty *tp;
{
	register struct nm_softc *pti = tp->t_dev->si_drv1;

	if (tp->t_state & TS_TTSTOP)
		return;
	pti->pt_flags &= ~PF_STOPPED;
	wakeup_other(tp, FREAD);
}

/* Wakes up the OTHER tty;*/
static void
wakeup_other(tp, flag)
	struct tty *tp;
	int flag;
{
	struct softpart *ourpart, *otherpart;

	GETPARTS(tp, ourpart, otherpart);
	if (flag & FREAD) {
		selwakeup(&otherpart->nm_tty.t_rsel);
		wakeup(TSA_PTC_READ((&otherpart->nm_tty)));
	}
	if (flag & FWRITE) {
		selwakeup(&otherpart->nm_tty.t_wsel);
		wakeup(TSA_PTC_WRITE((&otherpart->nm_tty)));
	}
}

static	void
nmdmstop(tp, flush)
	register struct tty *tp;
	int flush;
{
	struct nm_softc *pti = tp->t_dev->si_drv1;
	int flag;

	/* note: FLUSHREAD and FLUSHWRITE already ok */
	if (flush == 0) {
		flush = TIOCPKT_STOP;
		pti->pt_flags |= PF_STOPPED;
	} else
		pti->pt_flags &= ~PF_STOPPED;
	/* change of perspective */
	flag = 0;
	if (flush & FREAD)
		flag |= FWRITE;
	if (flush & FWRITE)
		flag |= FREAD;
	wakeup_other(tp, flag);
}

#if 0
static int
nmdmpoll(dev_t dev, int events, struct proc *p)
{
	register struct tty *tp = dev->si_tty;
	register struct tty *tp2;
	int revents = 0;
	int s;
	struct softpart *ourpart, *otherpart;

	GETPARTS(tp, ourpart, otherpart);
	tp2 = &otherpart->nm_tty;

	if ((tp->t_state & TS_CONNECTED) == 0)
		return (seltrue(dev, events, p) | POLLHUP);

	/*
	 * Need to block timeouts (ttrstart).
	 */
	s = spltty();

	/*
	 * First check if there is something to report immediatly.
	 */
	if ((events & (POLLIN | POLLRDNORM))) {
		if (tp->t_iflag & ICANON)  {
			if (tp->t_canq.c_cc)
				revents |= events & (POLLIN | POLLRDNORM);
		} else {
			if (tp->t_rawq.c_cc + tp->t_canq.c_cc)
				revents |= events & (POLLIN | POLLRDNORM);
		}
	}

	/*
	 * check if there is room in the other tty's input buffers.
	 */
	if ((events & (POLLOUT | POLLWRNORM)) 
	&& ((tp2->t_rawq.c_cc + tp2->t_canq.c_cc < TTYHOG - 2)
	   || (tp2->t_canq.c_cc == 0 && (tp2->t_iflag & ICANON)))) {
			revents |= events & (POLLOUT | POLLWRNORM);
	}

	if (events & POLLHUP)
		if ((tp->t_state & TS_CARR_ON) == 0)
			revents |= POLLHUP;

	/*
	 * If nothing immediate, set us to return when something IS found.
	 */
	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(p, &tp->t_rsel);

		if (events & (POLLOUT | POLLWRNORM)) 
			selrecord(p, &tp->t_wsel);
	}
	splx(s);

	return (revents);
}
#endif	/* 0 */

/*ARGSUSED*/
static	int
nmdmioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct tty *tp = dev->si_tty;
	struct nm_softc *pti = dev->si_drv1;
	int error, s;
	register struct tty *tp2;
	struct softpart *ourpart, *otherpart;

	s = spltty();
	GETPARTS(tp, ourpart, otherpart);
	tp2 = &otherpart->nm_tty;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error == ENOIOCTL)
		 error = ttioctl(tp, cmd, data, flag);
	if (error == ENOIOCTL) {
		switch (cmd) {
		case TIOCSBRK:
			otherpart->gotbreak = 1;
			break;
		case TIOCCBRK:
			break;
		case TIOCSDTR:
			ourpart->modemsignals |= TIOCM_DTR;
			break;
		case TIOCCDTR:
			ourpart->modemsignals &= TIOCM_DTR;
			break;
		case TIOCMSET:
			ourpart->modemsignals = *(int *)data;
			otherpart->modemsignals = *(int *)data;
			break;
		case TIOCMBIS:
			ourpart->modemsignals |= *(int *)data;
			break;
		case TIOCMBIC:
			ourpart->modemsignals &= ~(*(int *)data);
			otherpart->modemsignals &= ~(*(int *)data);
			break;
		case TIOCMGET:
			*(int *)data = ourpart->modemsignals;
			break;
		case TIOCMSDTRWAIT:
			break;
		case TIOCMGDTRWAIT:
			*(int *)data = 0;
			break;
		case TIOCTIMESTAMP:
		case TIOCDCDTIMESTAMP:
		default:
			splx(s);
			error = ENOTTY;
			return (error);
		}
		error = 0;
		nmdm_crossover(pti, ourpart, otherpart);
	}
	splx(s);
	return (error);
}

static void
nmdm_crossover(struct nm_softc *pti,
		struct softpart *ourpart,
		struct softpart *otherpart)
{
	otherpart->modemsignals &= ~(TIOCM_CTS|TIOCM_CAR);
	if (ourpart->modemsignals & TIOCM_RTS)
		otherpart->modemsignals |= TIOCM_CTS;
	if (ourpart->modemsignals & TIOCM_DTR)
		otherpart->modemsignals |= TIOCM_CAR;
}



static void nmdm_drvinit __P((void *unused));

static void
nmdm_drvinit(unused)
	void *unused;
{
	cdevsw_add(&nmdm_cdevsw);
	/* XXX: Gross hack for DEVFS */
	nmdminit(0);
}

SYSINIT(nmdmdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,nmdm_drvinit,NULL)
