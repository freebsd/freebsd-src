/* tty_clk.c,v 3.1 1993/07/06 01:07:33 jbj Exp
 * tty_clk.c - Generic line driver for receiving radio clock timecodes
 */

#include "clk.h"
#if NCLK > 0

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
#include "../h/clist.h"

/*
 * This line discipline is intended to provide well performing
 * generic support for the reception and time stamping of radio clock
 * timecodes.  Most radio clock devices return a string where a
 * particular character in the code (usually a \r) is on-time
 * synchronized with the clock.  The idea here is to collect characters
 * until (one of) the synchronization character(s) (we allow two) is seen.
 * When the magic character arrives we take a timestamp by calling
 * microtime() and insert the eight bytes of struct timeval into the
 * buffer after the magic character.  We then wake up anyone waiting
 * for the buffer and return the whole mess on the next read.
 *
 * To use this the calling program is expected to first open the
 * port, and then to set the port into raw mode with the speed
 * set appropriately with a TIOCSETP ioctl(), with the erase and kill
 * characters set to those to be considered magic (yes, I know this
 * is gross, but they were so convenient).  If only one character is
 * magic you can set then both the same, or perhaps to the alternate
 * parity versions of said character.  After getting all this set,
 * change the line discipline to CLKLDISC and you are on your way.
 *
 * The only other bit of magic we do in here is to flush the receive
 * buffers on writes if the CRMOD flag is set (hack, hack).
 */

/*
 * We run this very much like a raw mode terminal, with the exception
 * that we store up characters locally until we hit one of the
 * magic ones and then dump it into the rawq all at once.  We keep
 * the buffered data in clists since we can then often move it to
 * the rawq without copying.  For sanity we limit the number of
 * characters between specials, and the total number of characters
 * before we flush the rawq, as follows.
 */
#define	CLKLINESIZE	(256)
#define	NCLKCHARS	(CLKLINESIZE*4)

struct clkdata {
	int inuse;
	struct clist clkbuf;
};
#define	clk_cc	clkbuf.c_cc
#define	clk_cf	clkbuf.c_cf
#define	clk_cl	clkbuf.c_cl

struct clkdata clk_data[NCLK];

/*
 * Routine for flushing the internal clist
 */
#define	clk_bflush(clk)		(ndflush(&((clk)->clkbuf), (clk)->clk_cc))

int clk_debug = 0;

/*ARGSUSED*/
clkopen(dev, tp)
	dev_t dev;
	register struct tty *tp;
{
	register struct clkdata *clk;

	/*
	 * Don't allow multiple opens.  This will also protect us
	 * from someone opening /dev/tty
	 */
	if (tp->t_line == CLKLDISC)
		return (EBUSY);
	ttywflush(tp);
	for (clk = clk_data; clk < &clk_data[NCLK]; clk++)
		if (!clk->inuse)
			break;
	if (clk >= &clk_data[NCLK])
		return (EBUSY);
	clk->inuse++;
	clk->clk_cc = 0;
	clk->clk_cf = clk->clk_cl = NULL;
	tp->T_LINEP = (caddr_t) clk;
	return (0);
}


/*
 * Break down... called when discipline changed or from device
 * close routine.
 */
clkclose(tp)
	register struct tty *tp;
{
	register struct clkdata *clk;
	register int s = spltty();

	clk = (struct clkdata *)tp->T_LINEP;
	if (clk->clk_cc > 0)
		clk_bflush(clk);
	clk->inuse = 0;
	tp->t_line = 0;			/* paranoid: avoid races */
	splx(s);
}


/*
 * Receive a write request.  We pass these requests on to the terminal
 * driver, except that if the CRMOD bit is set in the flags we
 * first flush the input queues.
 */
clkwrite(tp, uio)
	register struct tty *tp;
	struct uio *uio;
{
	if (tp->t_flags & CRMOD) {
		register struct clkdata *clk;
		int s;

		s = spltty();
		if (tp->t_rawq.c_cc > 0)
			ndflush(&tp->t_rawq, tp->t_rawq.c_cc);
		clk = (struct clkdata *) tp->T_LINEP;
		if (clk->clk_cc > 0)
			clk_bflush(clk);
		(void)splx(s);
	}
	ttwrite(tp, uio);
}


/*
 * Low level character input routine.
 * If the character looks okay, grab a time stamp.  If the stuff in
 * the buffer is too old, dump it and start fresh.  If the character is
 * non-BCDish, everything in the buffer too.
 */
clkinput(c, tp)
	register int c;
	register struct tty *tp;
{
	register struct clkdata *clk;
	register int i;
	register long s;
	struct timeval tv;

	/*
	 * Check to see whether this isn't the magic character.  If not,
	 * save the character and return.
	 */
#ifdef ultrix
	if (c != tp->t_cc[VERASE] && c != tp->t_cc[VKILL]) {
#else
	if (c != tp->t_erase && c != tp->t_kill) {
#endif
		clk = (struct clkdata *) tp->T_LINEP;
		if (clk->clk_cc >= CLKLINESIZE)
			clk_bflush(clk);
		if (putc(c, &clk->clkbuf) == -1) {
			/*
			 * Hopeless, no clists.  Flush what we have
			 * and hope things improve.
			 */
			clk_bflush(clk);
		}
		return;
	}

	/*
	 * Here we have a magic character.  Get a timestamp and store
	 * everything.
	 */
	microtime(&tv);
	clk = (struct clkdata *) tp->T_LINEP;

	if (putc(c, &clk->clkbuf) == -1)
		goto flushout;
	
	s = tv.tv_sec;
	for (i = 0; i < sizeof(long); i++) {
		if (putc((s >> 24) & 0xff, &clk->clkbuf) == -1)
			goto flushout;
		s <<= 8;
	}

	s = tv.tv_usec;
	for (i = 0; i < sizeof(long); i++) {
		if (putc((s >> 24) & 0xff, &clk->clkbuf) == -1)
			goto flushout;
		s <<= 8;
	}

	/*
	 * If the length of the rawq exceeds our sanity limit, dump
	 * all the old crap in there before copying this in.
	 */
	if (tp->t_rawq.c_cc > NCLKCHARS)
		ndflush(&tp->t_rawq, tp->t_rawq.c_cc);
	
	/*
	 * Now copy the buffer in.  There is a special case optimization
	 * here.  If there is nothing on the rawq at present we can
	 * just copy the clists we own over.  Otherwise we must concatenate
	 * the present data on the end.
	 */
	s = (long)spltty();
	if (tp->t_rawq.c_cc <= 0) {
		tp->t_rawq = clk->clkbuf;
		clk->clk_cc = 0;
		clk->clk_cl = clk->clk_cf = NULL;
		(void) splx((int)s);
	} else {
		(void) splx((int)s);
		catq(&clk->clkbuf, &tp->t_rawq);
		clk_bflush(clk);
	}

	/*
	 * Tell the world
	 */
	ttwakeup(tp);
	return;

flushout:
	/*
	 * It would be nice if this never happened.  Flush the
	 * internal clists and hope someone else frees some of them
	 */
	clk_bflush(clk);
	return;
}


/*
 * Handle ioctls.  We reject most tty-style except those that
 * change the line discipline and a couple of others..
 */
clkioctl(tp, cmd, data, flag)
	struct tty *tp;
	int cmd;
	caddr_t data;
	int flag;
{
	int flags;
	struct sgttyb *sg;

	if ((cmd>>8) != 't')
		return (-1);
	switch (cmd) {
	case TIOCSETD:
	case TIOCGETD:
	case TIOCGETP:
	case TIOCGETC:
	case TIOCOUTQ:
		return (-1);

	case TIOCSETP:
		/*
		 * He likely wants to set new magic characters in.
		 * Do this part.
		 */
		sg = (struct sgttyb *)data;
#ifdef ultrix
		tp->t_cc[VERASE] = sg->sg_erase;
		tp->t_cc[VKILL] = sg->sg_kill;
#else
		tp->t_erase = sg->sg_erase;
		tp->t_kill = sg->sg_kill;
#endif
		return (0);

	case TIOCFLUSH:
		flags = *(int *)data;
		if (flags == 0 || (flags & FREAD)) {
			register struct clkdata *clk;

			clk = (struct clkdata *) tp->T_LINEP;
			if (clk->clk_cc > 0)
				clk_bflush(clk);
		}
		return (-1);
	
	default:
		break;
	}
	return (ENOTTY);	/* not quite appropriate */
}
#endif NCLK
