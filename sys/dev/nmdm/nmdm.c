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
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/serial.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/taskqueue.h>

MALLOC_DEFINE(M_NLMDM, "nullmodem", "nullmodem data structures");

static void 	nmdmstart(struct tty *tp);
static void 	nmdmstop(struct tty *tp, int rw);
static void 	nmdminit(struct cdev *dev);
static t_modem_t	nmdmmodem;

static d_open_t		nmdmopen;
static d_close_t	nmdmclose;

static struct cdevsw nmdm_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	nmdmopen,
	.d_close =	nmdmclose,
	.d_name =	"nmdn",
	.d_flags =	D_TTY | D_PSEUDO | D_NEEDGIANT,
};

#define BUFSIZ 		100		/* Chunk size iomoved to/from user */
#define NMDM_MAX_NUM	128		/* Artificially limit # devices. */
#define	PF_STOPPED	0x10		/* user told stopped */
#define BFLAG		CLONE_FLAG0

struct softpart {
	struct tty		*nm_tty;
	struct cdev 		*dev;
	int			nm_dcd;
	struct task		pt_task;
	struct softpart		*other;
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
nmdm_clone(void *arg, char *name, int nameen, struct cdev **dev)
{
	int i, unit;
	char *p;
	struct cdev *d1, *d2;

	if (*dev != NULL)
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
nmdm_task_tty(void *arg, int pending __unused)
{
	struct tty *tp, *otp;
	struct softpart *sp;
	int c;

	tp = arg;
	sp = tp->t_sc;
	otp = sp->other->nm_tty;
	KASSERT(otp != NULL, ("NULL otp in nmdmstart"));
	KASSERT(otp != tp, ("NULL otp == tp nmdmstart"));
	if (sp->other->nm_dcd) {
		if (!(tp->t_state & TS_ISOPEN)) {
			sp->other->nm_dcd = 0;
			(void)ttyld_modem(otp, 0);
		}
	} else {
		if (tp->t_state & TS_ISOPEN) {
			sp->other->nm_dcd = 1;
			(void)ttyld_modem(otp, 1);
		}
	}
	if (tp->t_state & TS_TTSTOP)
		return;
	while (tp->t_outq.c_cc != 0) {
		if (otp->t_state & TS_TBLOCK)
			return;
		c = getc(&tp->t_outq);
		if (otp->t_state & TS_ISOPEN)
			ttyld_rint(otp, c);
	}
	if (tp->t_outq.c_cc == 0)
		ttwwakeup(tp);
}

/*
 * This function creates and initializes a pair of ttys.
 */
static void
nmdminit(struct cdev *dev1)
{
	struct cdev *dev2;
	struct nm_softc *pt;

	dev2 = dev1->si_drv2;

	dev1->si_flags &= ~SI_CHEAPCLONE;
	dev2->si_flags &= ~SI_CHEAPCLONE;

	pt = malloc(sizeof(*pt), M_NLMDM, M_WAITOK | M_ZERO);
	TAILQ_INSERT_TAIL(&nmdmhead, pt, pt_list);

	dev1->si_drv1 = dev2->si_drv1 = pt;

	pt->part1.dev = dev1;
	pt->part2.dev = dev2;

	pt->part1.nm_tty = ttymalloc(pt->part1.nm_tty);
	pt->part1.nm_tty->t_oproc = nmdmstart;
	pt->part1.nm_tty->t_stop = nmdmstop;
	pt->part1.nm_tty->t_modem = nmdmmodem;
	pt->part1.nm_tty->t_dev = dev1;
	pt->part1.nm_tty->t_sc = &pt->part1;
	TASK_INIT(&pt->part1.pt_task, 0, nmdm_task_tty, pt->part1.nm_tty);

	pt->part2.nm_tty = ttymalloc(pt->part2.nm_tty);
	pt->part2.nm_tty->t_oproc = nmdmstart;
	pt->part2.nm_tty->t_stop = nmdmstop;
	pt->part2.nm_tty->t_modem = nmdmmodem;
	pt->part2.nm_tty->t_dev = dev2;
	pt->part2.nm_tty->t_sc = &pt->part2;
	TASK_INIT(&pt->part2.pt_task, 0, nmdm_task_tty, pt->part2.nm_tty);

	pt->part1.other = &pt->part2;
	pt->part2.other = &pt->part1;

	dev1->si_tty = pt->part1.nm_tty;
	dev1->si_drv1 = pt;

	dev2->si_tty = pt->part2.nm_tty;
	dev2->si_drv1 = pt;
}

/*
 * Device opened from userland
 */
static	int
nmdmopen(struct cdev *dev, int flag, int devtype, struct thread *td)
{
	struct tty *tp, *tp2;
	int error;
	struct nm_softc *pti;
	struct softpart *sp;

	if (dev->si_drv1 == NULL)
		nmdminit(dev);
	pti = dev->si_drv1;
	if (pti->pt_prison != td->td_ucred->cr_prison)
		return (EBUSY);

	tp = dev->si_tty;
	sp = tp->t_sc;
	tp2 = sp->other->nm_tty;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);		/* Set up default chars */
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_lflag = 0;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ttsetwater(tp); /* XXX ? */
	} else if (tp->t_state & TS_XCLUDE && suser(td)) {
		return (EBUSY);
	}

	error = ttyld_open(tp, dev);
	return (error);
}

static int
nmdmmodem(struct tty *tp, int sigon, int sigoff)
{
	struct softpart *sp;
	int i;

	sp = tp->t_sc;
	if (sigon || sigoff) {
		if (sigon & SER_DTR) {
			sp->other->nm_dcd = 1;
			ttyld_modem(sp->other->nm_tty, sp->other->nm_dcd);
		}
		if (sigoff & SER_DTR) {
			sp->other->nm_dcd = 0;
			ttyld_modem(sp->other->nm_tty, sp->other->nm_dcd);
		}
		return (0);
	} else {
		i = 0;
		if (sp->nm_dcd)
			i |= SER_DCD;
		if (sp->other->nm_dcd)
			i |= SER_DTR;
		return (i);
	}
}

static int
nmdmclose(struct cdev *dev, int flag, int mode, struct thread *td)
{

	return (tty_close(dev->si_tty));
}

static void
nmdmstart(struct tty *tp)
{
	struct softpart *pt;

	pt = tp->t_sc;
	taskqueue_enqueue(taskqueue_swi_giant, &pt->pt_task);
}

static void
nmdmstop(struct tty *tp, int flush)
{
	struct softpart *pt;

	pt = tp->t_sc;
	taskqueue_enqueue(taskqueue_swi_giant, &pt->pt_task);
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
