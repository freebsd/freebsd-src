/*
 * Copyright (c) 1995 Ugen J.S.Antsilevich
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * Snoop stuff.
 */

#include "snp.h"

#if NSNP > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/ioctl_compat.h>	/* Oooh..We need O/NTTYDISC	 */
#include <sys/proc.h>
#define TTYDEFCHARS
#include <sys/tty.h>
#undef  TTYDEFCHARS
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <sys/snoop.h>



#ifdef ST_PTY
/*
 * This should be same as in "kern/tty_pty.c"
 */
#include "pty.h"

#if NPTY == 1
#undef NPTY
#define NPTY 	32
#endif

extern struct tty pt_tty[];
#endif				/* ST_PTY */


#ifdef ST_SIO
/*
 * This should be same as "i386/isa/sio.c"
 */
#include "sio.h"

extern struct tty sio_tty[];
#endif				/* ST_SIO */


#ifdef ST_VTY
/*
 * This should match "i386/isa/sc.c"
 */

#if !defined(MAXCONS)
#define MAXCONS 16
#endif

extern struct tty sccons[];
#endif				/* ST_VTY */


/*
 * This is local structure to hold data for all tty arrays we serve.
 */
typedef struct tty tty_arr[];
struct tty_tab {
	int             lt_max;
	tty_arr        *lt_tab;
};

static struct tty_tab tty_tabs[] = {
#ifdef ST_PTY
	{NPTY, &pt_tty},
#else
	{-1, NULL},
#endif
#ifdef ST_VTY
	{MAXCONS, &sccons},
#else
	{-1, NULL},
#endif
#ifdef	ST_SIO
	{NSIO, &sio_tty}
#else
	{-1, NULL}
#endif
};


#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif


static struct snoop snoopsw[NSNP];

int
snpread(dev, uio, flag)
	dev_t           dev;
	struct uio     *uio;
	int             flag;
{
	int             unit = minor(dev), s;
	struct snoop   *snp = &snoopsw[unit];
	int             len, n, nblen, error = 0;
	caddr_t         from;
	char           *nbuf;

#ifdef DIAGNOSTIC
	if ((snp->snp_len + snp->snp_base) > snp->snp_blen)
		panic("snoop buffer error");
#endif
	if (snp->snp_unit == -1)
		return (EIO);

	snp->snp_flags &= ~SNOOP_RWAIT;

	do {
		if (snp->snp_len == 0) {
			if (snp->snp_flags & SNOOP_NBIO) {
				return EWOULDBLOCK;
			}
			snp->snp_flags |= SNOOP_RWAIT;
			tsleep((caddr_t) snp, (PZERO + 1) | PCATCH, "snoopread", 0);
		}
	} while (snp->snp_len == 0);

	n = snp->snp_len;

	while (snp->snp_len > 0 && uio->uio_resid > 0 && error == 0) {
		len = MIN(uio->uio_resid, snp->snp_len);
		from = (caddr_t) (snp->snp_buf + snp->snp_base);
		if (len == 0)
			break;

		error = uiomove(from, len, uio);
		snp->snp_base += len;
		snp->snp_len -= len;
	}
	if ((snp->snp_flags & SNOOP_OFLOW) && (n < snp->snp_len)) {
		snp->snp_flags &= ~SNOOP_OFLOW;
	}
	s = spltty();
	nblen = snp->snp_blen;
	if (((nblen / 2) >= SNOOP_MINLEN) && (nblen / 2) >= snp->snp_len) {
		while (((nblen / 2) >= snp->snp_len) && ((nblen / 2) >= SNOOP_MINLEN))
			nblen = nblen / 2;
		if (nbuf = malloc(nblen, M_TTYS, M_NOWAIT)) {
			bcopy(snp->snp_buf + snp->snp_base, nbuf, snp->snp_len);
			free(snp->snp_buf, M_TTYS);
			snp->snp_buf = nbuf;
			snp->snp_blen = nblen;
			snp->snp_base = 0;
		}
	}
	splx(s);

	return error;
}


int
snpin(snp, buf, n)
	struct snoop   *snp;
	char           *buf;
	int             n;
{
	int             s_free, s_tail;
	int             s, len, nblen;
	caddr_t         from, to;
	char           *nbuf;
	struct tty_tab *l_tty;
	struct tty     *tp;


	if (n == 0)
		return 0;

#ifdef DIAGNOSTIC
	if (n < 0)
		panic("bad snoop char count");

	if (!(snp->snp_flags & SNOOP_OPEN)) {
		printf("Snoop: data coming to closed device.\n");
		return 0;
	}
#endif
	if (snp->snp_flags & SNOOP_DOWN) {
		printf("Snoop: more data to down interface.\n");
		return 0;
	}
	if (snp->snp_flags & SNOOP_OFLOW) {
		printf("Snoop: buffer overflow.\n");
		/*
		 * On overflow we just repeat the standart close
		 * procedure...yes , this is waste of space but.. Then next
		 * read from device will fail if one would recall he is
		 * snooping and retry...
		 */

		snp->snp_blen = SNOOP_MINLEN;
		free(snp->snp_buf, M_TTYS);
		snp->snp_buf = malloc(SNOOP_MINLEN, M_TTYS, M_WAITOK);
		snp->snp_flags |= SNOOP_DOWN;
		snp->snp_flags &= ~SNOOP_OFLOW;

		return (snp_detach(snp));
	}
	s_tail = snp->snp_blen - (snp->snp_len + snp->snp_base);
	s_free = snp->snp_blen - snp->snp_len;


	if (n > s_free) {
		s = spltty();
		nblen = snp->snp_blen;
		while ((n > s_free) && ((nblen * 2) <= SNOOP_MAXLEN)) {
			nblen = snp->snp_blen * 2;
			s_free = nblen - (snp->snp_len + snp->snp_base);
		}
		if ((n <= s_free) && (nbuf = malloc(nblen, M_TTYS, M_NOWAIT))) {
			bcopy(snp->snp_buf + snp->snp_base, nbuf, snp->snp_len);
			free(snp->snp_buf, M_TTYS);
			snp->snp_buf = nbuf;
			snp->snp_blen = nblen;
			snp->snp_base = 0;
		} else {
			snp->snp_flags |= SNOOP_OFLOW;
			if (snp->snp_flags & SNOOP_RWAIT) {
				snp->snp_flags &= ~SNOOP_RWAIT;
				wakeup((caddr_t) snp);
			}
			splx(s);
			return 0;
		}
		splx(s);
	}
	if (n > s_tail) {
		from = (caddr_t) (snp->snp_buf + snp->snp_base);
		to = (caddr_t) (snp->snp_buf);
		len = snp->snp_len;
		bcopy(from, to, len);
		snp->snp_base = 0;
	}
	to = (caddr_t) (snp->snp_buf + snp->snp_base + snp->snp_len);
	bcopy(buf, to, n);
	snp->snp_len += n;

	if (snp->snp_flags & SNOOP_RWAIT) {
		snp->snp_flags &= ~SNOOP_RWAIT;
		wakeup((caddr_t) snp);
	}
	selwakeup(&snp->snp_sel);
	snp->snp_sel.si_pid = 0;

	return n;
}

int
snpopen(dev, flag, mode, p)
	dev_t           dev;
	int             flag, mode;
	struct proc    *p;
{
	struct snoop   *snp;
	register int    unit, error;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);

	if ((unit = minor(dev)) >= NSNP)
		return (ENXIO);
	snp = &snoopsw[unit];
	if (snp->snp_flags & SNOOP_OPEN)
		return (ENXIO);
	/*
	 * We intentionally do not OR flags with SNOOP_OPEN,but set them so
	 * all previous settings (especially SNOOP_OFLOW) will be cleared.
	 */
	snp->snp_flags = SNOOP_OPEN;

	snp->snp_buf = malloc(SNOOP_MINLEN, M_TTYS, M_WAITOK);
	snp->snp_blen = SNOOP_MINLEN;
	snp->snp_base = 0;
	snp->snp_len = 0;

	/*
	 * unit == -1  is for inactive snoop devices.
	 */
	snp->snp_unit = -1;

	return (0);
}


int
snp_detach(snp)
	struct snoop   *snp;
{
	struct tty     *tp;
	struct tty_tab *l_tty;


	snp->snp_base = 0;
	snp->snp_len = 0;

	/*
	 * If line disc. changed we do not touch this pointer,SLIP/PPP will
	 * change it anyway.
	 */

	if (snp->snp_unit == -1)
		goto destroy_notty;


	l_tty = &tty_tabs[snp->snp_type];
	tp = &((*l_tty->lt_tab)[snp->snp_unit]);

	if ((tp->t_sc == snp) && (tp->t_state & TS_SNOOP) &&
	    (tp->t_line == OTTYDISC || tp->t_line == NTTYDISC)) {
		tp->t_sc = NULL;
		tp->t_state &= ~TS_SNOOP;
	} else
		printf("Snoop: bad attached tty data.\n");

	snp->snp_unit = -1;

destroy_notty:
	selwakeup(&snp->snp_sel);
	snp->snp_sel.si_pid = 0;

	return (0);
}

int
snpclose(dev, flag)
	dev_t           dev;
	int             flag;
{
	register int    unit = minor(dev);
	struct snoop   *snp = &snoopsw[unit];

	snp->snp_blen = 0;
	free(snp->snp_buf, M_TTYS);
	snp->snp_flags &= ~SNOOP_OPEN;

	return (snp_detach(snp));
}




int
snpioctl(dev, cmd, data, flag)
	dev_t           dev;
	int             cmd;
	caddr_t         data;
	int             flag;
{
	int             unit = minor(dev), s;
	int             tunit, ttype;
	struct snoop   *snp = &snoopsw[unit];
	struct tty     *tp, *tpo;
	struct tty_tab *l_tty, *l_otty;

	switch (cmd) {
	case SNPSTTY:
		tunit = ((struct snptty *) data)->st_unit;
		ttype = ((struct snptty *) data)->st_type;

		if (ttype < 0 || ttype > ST_MAXTYPE)
			return (EINVAL);

		l_tty = &tty_tabs[ttype];
		if (l_tty->lt_tab == NULL)
			return (EINVAL);

		if (tunit < 0 || tunit >= l_tty->lt_max)
			return (EINVAL);

		tp = &((*l_tty->lt_tab)[tunit]);

		if (tp->t_sc != (caddr_t) snp && (tp->t_state & TS_SNOOP))
			return (EBUSY);

		if (tp->t_line != OTTYDISC && tp->t_line != NTTYDISC)
			return (EBUSY);

		s = spltty();
		if (snp->snp_unit != -1) {
			l_otty = &tty_tabs[snp->snp_type];
			tpo = &((*l_otty->lt_tab)[snp->snp_unit]);
			tpo->t_state &= ~TS_SNOOP;
		}
		tp->t_sc = (caddr_t) snp;
		tp->t_state |= TS_SNOOP;
		snp->snp_unit = tunit;
		snp->snp_type = ttype;
		snp->snp_flags &= ~SNOOP_DOWN;
		splx(s);

		break;
	case SNPGTTY:
		((struct snptty *) data)->st_unit = snp->snp_unit;
		((struct snptty *) data)->st_type = snp->snp_type;
		break;

	case FIONBIO:
		if (*(int *) data)
			snp->snp_flags |= SNOOP_NBIO;
		else
			snp->snp_flags &= ~SNOOP_NBIO;
		break;
	case FIOASYNC:
		if (*(int *) data)
			snp->snp_flags |= SNOOP_ASYNC;
		else
			snp->snp_flags &= ~SNOOP_ASYNC;
		break;
	case FIONREAD:
		s = spltty();
		if (snp->snp_unit != -1)
			*(int *) data = snp->snp_len;
		else if (snp->snp_flags & SNOOP_DOWN)
			*(int *) data = -1;
		else
			*(int *) data = 0;
		splx(s);
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}


int
snpselect(dev, rw, p)
	dev_t           dev;
	int             rw;
	struct proc    *p;
{
	int             unit = minor(dev), s;
	struct snoop   *snp = &snoopsw[unit];

	if (rw != FREAD) {
		return 0;
	}
	if (snp->snp_len > 0) {
		return 1;
	}
	/*
	 * If snoop is down,we don't want to select() forever so we return 1.
	 * Caller should see if we down via FIONREAD ioctl().The last should
	 * return -1 to indicate down state.
	 */
	if (snp->snp_flags & SNOOP_DOWN) {
		return 1;
	}
	selrecord(p, &snp->snp_sel);
	return 0;
}

#endif
