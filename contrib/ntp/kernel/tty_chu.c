/* tty_chu.c,v 3.1 1993/07/06 01:07:30 jbj Exp
 * tty_chu.c - CHU line driver
 */

#include "chu.h"
#if NCHU > 0

#include "../h/param.h"
#include "../h/types.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/ioctl.h"
#include "../h/tty.h"
#include "../h/proc.h"
#include "../h/file.h"
#include "../h/conf.h"
#include "../h/buf.h"
#include "../h/uio.h"

#include "../h/chudefs.h"

/*
 * Line discipline for receiving CHU time codes.
 * Does elementary noise elimination, takes time stamps after
 * the arrival of each character, returns a buffer full of the
 * received 10 character code and the associated time stamps.
 */
#define	NUMCHUBUFS	3

struct chudata {
	u_char used;		/* Set to 1 when structure in use */
	u_char lastindex;	/* least recently used buffer */
	u_char curindex;	/* buffer to use */
	u_char sleeping;	/* set to 1 when we're sleeping on a buffer */
	struct chucode chubuf[NUMCHUBUFS];
} chu_data[NCHU];

/*
 * Number of microseconds we allow between
 * character arrivals.  The speed is 300 baud
 * so this should be somewhat more than 30 msec
 */
#define	CHUMAXUSEC	(50*1000)	/* 50 msec */

int chu_debug = 0;

/*
 * Open as CHU time discipline.  Called when discipline changed
 * with ioctl, and changes the interpretation of the information
 * in the tty structure.
 */
/*ARGSUSED*/
chuopen(dev, tp)
	dev_t dev;
	register struct tty *tp;
{
	register struct chudata *chu;

	/*
	 * Don't allow multiple opens.  This will also protect us
	 * from someone opening /dev/tty
	 */
	if (tp->t_line == CHULDISC)
		return (EBUSY);
	ttywflush(tp);
	for (chu = chu_data; chu < &chu_data[NCHU]; chu++)
		if (!chu->used)
			break;
	if (chu >= &chu[NCHU])
		return (EBUSY);
	chu->used++;
	chu->lastindex = chu->curindex = 0;
	chu->sleeping = 0;
	chu->chubuf[0].ncodechars = 0;
	tp->T_LINEP = (caddr_t) chu;
	return (0);
}

/*
 * Break down... called when discipline changed or from device
 * close routine.
 */
chuclose(tp)
	register struct tty *tp;
{
	register int s = spl5();

	((struct chudata *) tp->T_LINEP)->used = 0;
	tp->t_cp = 0;
	tp->t_inbuf = 0;
	tp->t_rawq.c_cc = 0;		/* clear queues -- paranoid */
	tp->t_canq.c_cc = 0;
	tp->t_line = 0;			/* paranoid: avoid races */
	splx(s);
}

/*
 * Read a CHU buffer.  Sleep on the current buffer
 */
churead(tp, uio)
	register struct tty *tp;
	struct uio *uio;
{
	register struct chudata *chu;
	register struct chucode *chucode;
	register int s;

	if ((tp->t_state&TS_CARR_ON)==0)
		return (EIO);

	chu = (struct chudata *) (tp->T_LINEP);

	s = spl5();
	chucode = &(chu->chubuf[chu->lastindex]);
	while (chu->curindex == chu->lastindex) {
		chu->sleeping = 1;
		sleep((caddr_t)chucode, TTIPRI);
	}
	chu->sleeping = 0;
	if (++(chu->lastindex) >= NUMCHUBUFS)
		chu->lastindex = 0;
	splx(s);

	return (uiomove((caddr_t)chucode, sizeof(*chucode), UIO_READ, uio));
}

/*
 * Low level character input routine.
 * If the character looks okay, grab a time stamp.  If the stuff in
 * the buffer is too old, dump it and start fresh.  If the character is
 * non-BCDish, everything in the buffer too.
 */
chuinput(c, tp)
	register int c;
	register struct tty *tp;
{
	register struct chudata *chu = (struct chudata *) tp->T_LINEP;
	register struct chucode *chuc;
	register int i;
	long sec, usec;
	struct timeval tv;

	/*
	 * Do a check on the BSDness of the character.  This delays
	 * the time stamp a bit but saves a fair amount of overhead
	 * when the static is bad.
	 */
	if (((c) & 0xf) > 9 || (((c)>>4) & 0xf) > 9) {
		chuc = &(chu->chubuf[chu->curindex]);
		chuc->ncodechars = 0;	/* blow all previous away */
		return;
	}

	/*
	 * Call microtime() to get the current time of day
	 */
	microtime(&tv);

	/*
	 * Compute the difference in this character's time stamp
	 * and the last.  If it exceeds the margin, blow away all
	 * the characters currently in the buffer.
	 */
	chuc = &(chu->chubuf[chu->curindex]);
	i = (int)chuc->ncodechars;
	if (i > 0) {
		sec = tv.tv_sec - chuc->codetimes[i-1].tv_sec;
		usec = tv.tv_usec - chuc->codetimes[i-1].tv_usec;
		if (usec < 0) {
			sec -= 1;
			usec += 1000000;
		}
		if (sec != 0 || usec > CHUMAXUSEC) {
			i = 0;
			chuc->ncodechars = 0;
		}
	}

	/*
	 * Store the character.  If we're done, have to tell someone
	 */
	chuc->codechars[i] = (u_char)c;
	chuc->codetimes[i] = tv;

	if (++i < NCHUCHARS) {
		/*
		 * Not much to do here.  Save the count and wait
		 * for another character.
		 */
		chuc->ncodechars = (u_char)i;
	} else {
		/*
		 * Mark this buffer full and point at next.  If the
		 * next buffer is full we overwrite it by bumping the
		 * next pointer.
		 */
		chuc->ncodechars = NCHUCHARS;
		if (++(chu->curindex) >= NUMCHUBUFS)
			chu->curindex = 0;
		if (chu->curindex == chu->lastindex)
			if (++(chu->lastindex) >= NUMCHUBUFS)
				chu->lastindex = 0;
		chu->chubuf[chu->curindex].ncodechars = 0;

		/*
		 * Wake up anyone sleeping on this.  Also wake up
		 * selectors and/or deliver a SIGIO as required.
		 */
		if (tp->t_rsel) {
			selwakeup(tp->t_rsel, tp->t_state&TS_RCOLL);
			tp->t_state &= ~TS_RCOLL;
			tp->t_rsel = 0;
		}
		if (tp->t_state & TS_ASYNC)
			gsignal(tp->t_pgrp, SIGIO);
		if (chu->sleeping)
			(void) wakeup((caddr_t)chuc);
	}
}

/*
 * Handle ioctls.  We reject all tty-style except those that
 * change the line discipline.
 */
chuioctl(tp, cmd, data, flag)
	struct tty *tp;
	int cmd;
	caddr_t data;
	int flag;
{

	if ((cmd>>8) != 't')
		return (-1);
	switch (cmd) {
	case TIOCSETD:
	case TIOCGETD:
	case TIOCGETP:
	case TIOCGETC:
		return (-1);
	}
	return (ENOTTY);	/* not quite appropriate */
}


chuselect(dev, rw)
	dev_t dev;
	int rw;
{
	register struct tty *tp = &cdevsw[major(dev)].d_ttys[minor(dev)];
	struct chudata *chu;
	int s = spl5();

	chu = (struct chudata *) (tp->T_LINEP);

	switch (rw) {

	case FREAD:
		if (chu->curindex != chu->lastindex)
			goto win;
		if (tp->t_rsel && tp->t_rsel->p_wchan == (caddr_t)&selwait)
			tp->t_state |= TS_RCOLL;
		else
			tp->t_rsel = u.u_procp;
		break;

	case FWRITE:
		goto win;
	}
	splx(s);
	return (0);
win:
	splx(s);
	return (1);
}
#endif NCHU
