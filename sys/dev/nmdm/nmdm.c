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
__FBSDID("$FreeBSD: src/sys/dev/nmdm/nmdm.c,v 1.39.6.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * Pseudo-nulmodem driver
 * Mighty handy for use with serial console in Vmware
 */

#include "opt_compat.h"
#include "opt_tty.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/serial.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/taskqueue.h>

MALLOC_DEFINE(M_NLMDM, "nullmodem", "nullmodem data structures");

static d_close_t	nmdmclose;
static t_modem_t	nmdmmodem;
static d_open_t		nmdmopen;
static t_oproc_t	nmdmoproc;
static t_param_t	nmdmparam;
static t_stop_t		nmdmstop;

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
	struct callout		co;
	u_long			quota;
	u_long			accumulator;
	int			rate;
	int			credits;

#define QS 8	/* Quota shift */
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
nmdm_clone(void *arg, struct ucred *cred, char *name, int nameen,
    struct cdev **dev)
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
	dev_ref(*dev);
}

static void
nmdm_timeout(void *arg)
{
	struct softpart *sp;

	sp = arg;

	if (sp->rate == 0)
		return;

	/*
	 * Do a simple Floyd-Steinberg dither here to avoid FP math.
	 * Wipe out unused quota from last tick.
	 */
	sp->accumulator += sp->credits;
	sp->quota = sp->accumulator >> QS;
	sp->accumulator &= ((1 << QS) - 1);

	taskqueue_enqueue(taskqueue_swi_giant, &sp->pt_task);
	callout_reset(&sp->co, sp->rate, nmdm_timeout, arg);
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
		if (sp->rate && !sp->quota)
			return;
		if (otp->t_state & TS_TBLOCK)
			return;
		sp->quota--;
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

	pt->part1.nm_tty = ttyalloc();
	pt->part1.nm_tty->t_oproc = nmdmoproc;
	pt->part1.nm_tty->t_stop = nmdmstop;
	pt->part1.nm_tty->t_modem = nmdmmodem;
	pt->part1.nm_tty->t_param = nmdmparam;
	pt->part1.nm_tty->t_dev = dev1;
	pt->part1.nm_tty->t_sc = &pt->part1;
	TASK_INIT(&pt->part1.pt_task, 0, nmdm_task_tty, pt->part1.nm_tty);
	callout_init(&pt->part1.co, 0);

	pt->part2.nm_tty = ttyalloc();
	pt->part2.nm_tty->t_oproc = nmdmoproc;
	pt->part2.nm_tty->t_stop = nmdmstop;
	pt->part2.nm_tty->t_modem = nmdmmodem;
	pt->part2.nm_tty->t_param = nmdmparam;
	pt->part2.nm_tty->t_dev = dev2;
	pt->part2.nm_tty->t_sc = &pt->part2;
	TASK_INIT(&pt->part2.pt_task, 0, nmdm_task_tty, pt->part2.nm_tty);
	callout_init(&pt->part2.co, 0);

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
		ttyinitmode(tp, 0, 0);
		ttsetwater(tp); /* XXX ? */
	} else if (tp->t_state & TS_XCLUDE &&
	    priv_check(td, PRIV_TTY_EXCLUSIVE)) {
		return (EBUSY);
	}

	error = ttyld_open(tp, dev);
	return (error);
}

static int
bits_per_char(struct termios *t)
{
	int bits;

	bits = 1;		/* start bit */
	switch (t->c_cflag & CSIZE) {
	case CS5:	bits += 5;	break;
	case CS6:	bits += 6;	break;
	case CS7:	bits += 7;	break;
	case CS8:	bits += 8;	break;
	}
	bits++;			/* stop bit */
	if (t->c_cflag & PARENB)
		bits++;
	if (t->c_cflag & CSTOPB)
		bits++;
	return (bits);
}

static int
nmdmparam(struct tty *tp, struct termios *t)
{
	struct softpart *sp;
	struct tty *tp2;
	int bpc, rate, speed, i;

	sp = tp->t_sc;
	tp2 = sp->other->nm_tty;

	if (!((t->c_cflag | tp2->t_cflag) & CDSR_OFLOW)) {
		sp->rate = 0;
		sp->other->rate = 0;
		return (0);
	}

	/*
	 * DSRFLOW one either side enables rate-simulation for both
	 * directions.
	 * NB: the two directions may run at different rates.
	 */

	/* Find the larger of the number of bits transmitted */
	bpc = imax(bits_per_char(t), bits_per_char(&tp2->t_termios));

	for (i = 0; i < 2; i++) {
		/* Use the slower of our receive and their transmit rate */
		speed = imin(tp2->t_ospeed, t->c_ispeed);
		if (speed == 0) {
			sp->rate = 0;
			sp->other->rate = 0;
			return (0);
		}

		speed <<= QS;			/* [bit/sec, scaled] */
		speed /= bpc;			/* [char/sec, scaled] */
		rate = (hz << QS) / speed;	/* [hz per callout] */
		if (rate == 0)
			rate = 1;

		speed *= rate;
		speed /= hz;			/* [(char/sec)/tick, scaled */

		sp->credits = speed;
		sp->rate = rate;
		callout_reset(&sp->co, rate, nmdm_timeout, sp);

		/*
		 * swap pointers for second pass so the other end gets
		 * updated as well.
		 */
		sp = sp->other;
		t = &tp2->t_termios;
		tp2 = tp;
	}
	return (0);
}

static int
nmdmmodem(struct tty *tp, int sigon, int sigoff)
{
	struct softpart *sp;
	int i;

	sp = tp->t_sc;
	if (sigon || sigoff) {
		if (sigon & SER_DTR)
			sp->other->nm_dcd = 1;
		if (sigoff & SER_DTR)
			sp->other->nm_dcd = 0;
		ttyld_modem(sp->other->nm_tty, sp->other->nm_dcd);
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
	struct tty *tp = dev->si_tty;
	int error;

	error = ttyld_close(tp, flag);
	(void) tty_close(dev->si_tty);

	return (error);
}

static void
nmdmoproc(struct tty *tp)
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
