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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Pseudo-nulmodem driver
 * Mighty handy for use with serial console in Vmware
 */

#include "opt_compat.h"
#include "opt_tty.h"

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

static void 	nmdmstart(struct tty *tp);
static void 	nmdmstop(struct tty *tp, int rw);
static void 	wakeup_other(struct tty *tp, int flag);
static void 	nmdminit(dev_t dev);

static d_open_t		nmdmopen;
static d_close_t	nmdmclose;
static d_read_t		nmdmread;
static d_write_t	nmdmwrite;
static d_ioctl_t	nmdmioctl;

static struct cdevsw nmdm_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	nmdmopen,
	.d_close =	nmdmclose,
	.d_read =	nmdmread,
	.d_write =	nmdmwrite,
	.d_ioctl =	nmdmioctl,
	.d_name =	"nmdn",
	.d_flags =	D_TTY | D_PSEUDO | D_NEEDGIANT,
};

#define BUFSIZ 		100		/* Chunk size iomoved to/from user */
#define NMDM_MAX_NUM	128		/* Artificially limit # devices. */
#define	PF_STOPPED	0x10		/* user told stopped */
#define BFLAG		CLONE_FLAG0

struct softpart {
	struct tty	nm_tty;
	dev_t	dev;
	int	modemsignals;	/* bits defined in sys/ttycom.h */
	int	gotbreak;
};

struct	nm_softc {
	TAILQ_ENTRY(nm_softc)	pt_list;
	int			pt_flags;
	struct softpart 	part1, part2;
	struct	prison 		*pt_prison;
};

static struct clonedevs *nmdmclones;
static TAILQ_HEAD(,nm_softc) nmdmhead = TAILQ_HEAD_INITIALIZER(nmdmhead);

static void
nmdm_clone(void *arg, char *name, int nameen, dev_t *dev)
{
	int i, unit;
	char *p;
	dev_t d1, d2;

	if (*dev != NODEV)
		return;
	if (strcmp(name, "nmdm") == 0) {
		p = NULL;
		unit = -1;
	} else {
		i = dev_stdclone(name, &p, "nmdm", &unit);
		if (i == 0)
			return;
		if (p[0] != '\0' && p[0] != 'A' && p[0] != 'B')
			return;
		else if (p[0] != '\0' && p[1] != '\0')
			return;
	}
	i = clone_create(&nmdmclones, &nmdm_cdevsw, &unit, &d1, 0);
	if (i) {
		d1 = make_dev(&nmdm_cdevsw, unit2minor(unit),
		     0, 0, 0666, "nmdm%dA", unit);
		if (d1 == NULL)
			return;
		d2 = make_dev(&nmdm_cdevsw, unit2minor(unit) | BFLAG,
		     0, 0, 0666, "nmdm%dB", unit);
		if (d2 == NULL) {
			destroy_dev(d1);
			return;
		}
		d2->si_drv2 = d1;
		d1->si_drv2 = d2;
		dev_depends(d1, d2);
		dev_depends(d2, d1);
		d1->si_flags |= SI_CHEAPCLONE;
		d2->si_flags |= SI_CHEAPCLONE;
	}
	if (p != NULL && p[0] == 'B')
		*dev = d1->si_drv2;
	else
		*dev = d1;
}

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
nmdminit(dev_t dev1)
{
	dev_t dev2;
	struct nm_softc *pt;

	dev2 = dev1->si_drv2;

	dev1->si_flags &= ~SI_CHEAPCLONE;
	dev2->si_flags &= ~SI_CHEAPCLONE;

	pt = malloc(sizeof(*pt), M_NLMDM, M_WAITOK | M_ZERO);
	TAILQ_INSERT_TAIL(&nmdmhead, pt, pt_list);
	dev1->si_drv1 = dev2->si_drv1 = pt;

	pt->part1.dev = dev1;
	pt->part2.dev = dev2;
	dev1->si_tty = &pt->part1.nm_tty;
	dev2->si_tty = &pt->part2.nm_tty;
	ttyregister(&pt->part1.nm_tty);
	ttyregister(&pt->part2.nm_tty);
	pt->part1.nm_tty.t_oproc = nmdmstart;
	pt->part2.nm_tty.t_oproc = nmdmstart;
	pt->part1.nm_tty.t_stop = nmdmstop;
	pt->part2.nm_tty.t_stop = nmdmstop;
	pt->part2.nm_tty.t_dev = dev1;
	pt->part1.nm_tty.t_dev = dev2;
}

/*
 * Device opened from userland
 */
static	int
nmdmopen(dev_t dev, int flag, int devtype, struct thread *td)
{
	register struct tty *tp, *tp2;
	int error;
	struct nm_softc *pti;
	struct	softpart *ourpart, *otherpart;

	if (dev->si_drv1 == NULL)
		nmdminit(dev);
	pti = dev->si_drv1;

	if (minor(dev) & BFLAG)
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
	} else if (tp->t_state & TS_XCLUDE && suser(td)) {
		return (EBUSY);
	} else if (pti->pt_prison != td->td_ucred->cr_prison) {
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

/*
 * Device closed again
 */
static	int
nmdmclose(dev_t dev, int flag, int mode, struct thread *td)
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

/*
 * handle read(2) request from userland
 */
static	int
nmdmread(dev_t dev, struct uio *uio, int flag)
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
nmdmwrite(dev_t dev, struct uio *uio, int flag)
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
nmdmstart(struct tty *tp)
{
	register struct nm_softc *pti = tp->t_dev->si_drv1;

	if (tp->t_state & TS_TTSTOP)
		return;
	pti->pt_flags &= ~PF_STOPPED;
	wakeup_other(tp, FREAD);
}

/* Wakes up the OTHER tty;*/
static void
wakeup_other(struct tty *tp, int flag)
{
	struct softpart *ourpart, *otherpart;

	GETPARTS(tp, ourpart, otherpart);
	if (flag & FREAD) {
		selwakeuppri(&otherpart->nm_tty.t_rsel, TTIPRI);
		wakeup(TSA_PTC_READ((&otherpart->nm_tty)));
	}
	if (flag & FWRITE) {
		selwakeuppri(&otherpart->nm_tty.t_wsel, TTOPRI);
		wakeup(TSA_PTC_WRITE((&otherpart->nm_tty)));
	}
}

/*
 * stopped output on tty, called when device is closed
 */
static	void
nmdmstop(register struct tty *tp, int flush)
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

/*
 * handle ioctl(2) request from userland
 */
static	int
nmdmioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	register struct tty *tp = dev->si_tty;
	struct nm_softc *pti = dev->si_drv1;
	int error, s;
	register struct tty *tp2;
	struct softpart *ourpart, *otherpart;

	s = spltty();
	GETPARTS(tp, ourpart, otherpart);
	tp2 = &otherpart->nm_tty;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, td);
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
			/* FALLTHROUGH */
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
nmdm_crossover(struct nm_softc *pti, struct softpart *ourpart,
    struct softpart *otherpart)
{
	otherpart->modemsignals &= ~(TIOCM_CTS|TIOCM_CAR);
	if (ourpart->modemsignals & TIOCM_RTS)
		otherpart->modemsignals |= TIOCM_CTS;
	if (ourpart->modemsignals & TIOCM_DTR)
		otherpart->modemsignals |= TIOCM_CAR;
}

/*
 * Module handling
 */
static int
nmdm_modevent(module_t mod, int type, void *data)
{
	static eventhandler_tag tag;
	struct nm_softc *pt, *tpt;
        int error = 0;

        switch(type) {
        case MOD_LOAD: 
		clone_setup(&nmdmclones);
		tag = EVENTHANDLER_REGISTER(dev_clone, nmdm_clone, 0, 1000);
		if (tag == NULL)
			return (ENOMEM);
		break;

	case MOD_SHUTDOWN:
		/* FALLTHROUGH */
	case MOD_UNLOAD:
		EVENTHANDLER_DEREGISTER(dev_clone, tag);
		TAILQ_FOREACH_SAFE(pt, &nmdmhead, pt_list, tpt) {
			destroy_dev(pt->part1.dev);
			TAILQ_REMOVE(&nmdmhead, pt, pt_list);
			free(pt, M_NLMDM);
		}
		clone_cleanup(&nmdmclones);
		break;
	default:
		error = EOPNOTSUPP;
	}
	return (error);
}

DEV_MODULE(nmdm, nmdm_modevent, NULL);
