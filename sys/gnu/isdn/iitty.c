static char     _ittyid[] = "@(#)$Id: iitty.c,v 1.12 1995/11/16 10:35:29 bde Exp $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.12 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: iitty.c,v $
 * Revision 1.12  1995/11/16  10:35:29  bde
 * Fixed the type of ity_input().  A trailing arg was missing.
 *
 * Completed function declarations.
 *
 * Added prototypes.
 *
 * Removed some useless includes.
 *
 * Revision 1.11  1995/07/31  21:28:42  bde
 * Use tsleep() instead of ttysleep() to wait for carrier since a generation
 * change isn't an error.
 *
 * Revision 1.10  1995/07/31  21:01:03  bde
 * Obtained from:	partly from ancient patches of mine via 1.1.5
 *
 * Introduce TS_CONNECTED and TS_ZOMBIE states.  TS_CONNECTED is set
 * while a connection is established.  It is set while (TS_CARR_ON or
 * CLOCAL is set) and TS_ZOMBIE is clear.  TS_ZOMBIE is set for on to
 * off transitions of TS_CARR_ON that occur when CLOCAL is clear and
 * is cleared for off to on transitions of CLOCAL.  I/o can only occur
 * while TS_CONNECTED is set.  TS_ZOMBIE prevents further i/o.
 *
 * Split the input-event sleep address TSA_CARR_ON(tp) into TSA_CARR_ON(tp)
 * and TSA_HUP_OR_INPUT(tp).  The former address is now used only for
 * off to on carrier transitions and equivalent CLOCAL transitions.
 * The latter is used for all input events, all carrier transitions
 * and certain CLOCAL transitions.  There are some harmless extra
 * wakeups for rare connection- related events.  Previously there were
 * too many extra wakeups for non-rare input events.
 *
 * Drivers now call l_modem() instead of setting TS_CARR_ON directly
 * to handle even the initial off to on transition of carrier.  They
 * should always have done this.  l_modem() now handles TS_CONNECTED
 * and TS_ZOMBIE as well as TS_CARR_ON.
 *
 * gnu/isdn/iitty.c:
 * Set TS_CONNECTED for first open ourself to go with bogusly setting
 * CLOCAL.
 *
 * i386/isa/syscons.c, i386/isa/pcvt/pcvt_drv.c:
 * We fake carrier, so don't also fake CLOCAL.
 *
 * kern/tty.c:
 * Testing TS_CONNECTED instead of TS_CARR_ON fixes TIOCCONS forgetting to
 * test CLOCAL.  TS_ISOPEN was tested instead, but that broke when we disabled
 * the clearing of TS_ISOPEN for certain transitions of CLOCAL.
 *
 * Testing TS_CONNECTED fixes ttyselect() returning false success for output
 * to devices in state !TS_CARR_ON && !CLOCAL.
 *
 * Optimize the other selwakeup() call (this is not related to the other
 * changes).
 *
 * kern/tty_pty.c:
 * ptcopen() can be declared in traditional C now that dev_t isn't short.
 *
 * Revision 1.9  1995/07/22  16:44:26  bde
 * Obtained from:	partly from ancient patches of mine via 1.1.5
 *
 * Give names to the magic tty i/o sleep addresses and use them.  This makes
 * it easier to remember what the addresses are for and to keep them unique.
 *
 * Revision 1.8  1995/07/22  01:29:28  bde
 * Move the inline code for waking up writers to a new function
 * ttwwakeup().  The conditions for doing the wakeup will soon become
 * more complicated and I don't want them duplicated in all drivers.
 *
 * It's probably not worth making ttwwakeup() a macro or an inline
 * function.  The cost of the function call is relatively small when
 * there is a process to wake up.  There is usually a process to wake
 * up for large writes and the system call overhead dwarfs the function
 * call overhead for small writes.
 *
 * Revision 1.7  1995/07/21  20:52:21  bde
 * Obtained from:	partly from ancient patches by ache and me via 1.1.5
 *
 * Nuke `symbolic sleep message strings'.  Use unique literal messages so that
 * `ps l' shows unambiguously where processes are sleeping.
 *
 * Revision 1.6  1995/07/21  16:30:37  bde
 * Obtained from:	partly from an ancient patch of mine via 1.1.5
 *
 * Temporarily nuke TS_WOPEN.  It was only used for the obscure MDMBUF
 * flow control option in the kernel and for informational purposes
 * in `pstat -t'.  The latter worked properly only for ptys.  In
 * general there may be multiple processes sleeping in open() and
 * multiple processes that successfully opened the tty by opening it
 * in O_NONBLOCK mode or during a window when CLOCAL was set.  tty.c
 * doesn't have enough information to maintain the flag but always
 * cleared it in ttyopen().
 *
 * TS_WOPEN should be restored someday just so that `pstat -t' can
 * display it (MDMBUF is already fixed).  Fixing it requires counting
 * of processes sleeping in open() in too many serial drivers.
 *
 * Revision 1.5  1995/03/28  07:54:43  bde
 * Add and move declarations to fix all of the warnings from `gcc -Wimplicit'
 * (except in netccitt, netiso and netns) that I didn't notice when I fixed
 * "all" such warnings before.
 *
 * Revision 1.4  1995/02/28  00:20:30  pst
 * Incorporate bde's code-review comments.
 *
 * (a) bring back ttselect, now that we have xxxdevtotty() it isn't dangerous.
 * (b) remove all of the wrappers that have been replaced by ttselect
 * (c) fix formatting in syscons.c and definition in syscons.h
 * (d) add cxdevtotty
 *
 * NOT DONE:
 * (e) make pcvt work... it was already broken...when someone fixes pcvt to
 * 	link properly, just rename get_pccons to xxxdevtotty and we're done
 *
 * Revision 1.3  1995/02/25  20:08:52  pst
 * (a) remove the pointer to each driver's tty structure array from cdevsw
 * (b) add a function callback vector to tty drivers that will return a pointer
 *     to a valid tty structure based upon a dev_t
 * (c) make syscons structures the same size whether or not APM is enabled so
 *     utilities don't crash if NAPM changes (and make the damn kernel compile!)
 * (d) rewrite /dev/snp ioctl interface so that it is device driver and i386
 *     independant
 *
 * Revision 1.2  1995/02/15  06:28:28  jkh
 * Fix up include paths, nuke some warnings.
 *
 * Revision 1.1  1995/02/14  15:00:32  jkh
 * An ISDN driver that supports the EDSS1 and the 1TR6 ISDN interfaces.
 * EDSS1 is the "Euro-ISDN", 1TR6 is the soon obsolete german ISDN Interface.
 * Obtained from: Dietmar Friede <dfriede@drnhh.neuhaus.de> and
 * 	Juergen Krause <jkr@saarlink.de>
 *
 * This is only one part - the rest to follow in a couple of hours.
 * This part is a benign import, since it doesn't affect anything else.
 *
 *
 ******************************************************************************/

#include "ity.h"
#if NITY > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>

#include "gnu/isdn/isdn_ioctl.h"

#ifdef JREMOD
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#define CDEV_MAJOR 56
#endif /*JREMOD*/

extern int	ityparam __P((struct tty *tp, struct termios *t));
extern void	itystart __P((struct tty *tp));

int             nity = NITY;
int             itydefaultrate = 64000;
short           ity_addr[NITY];
struct tty     ity_tty[NITY];
static int	applnr[NITY];
static int	next_if= 0;

#define	UNIT(x)		(minor(x)&0x3f)
#define	OUTBOUND(x)	((minor(x)&0x80)==0x80)

int
ityattach(int ap)
{
	if(next_if >= NITY)
		return(-1);

	applnr[next_if]= ap;
	return(next_if++);
}

/* ARGSUSED */
int
ityopen(dev_t dev, int flag, int mode, struct proc * p)
{
	register struct tty *tp;
	register int    unit;
	int             error = 0;

	unit = UNIT(dev);
	if (unit >= next_if)
		return (ENXIO);

	tp = &ity_tty[unit];
	tp->t_oproc = itystart;
	tp->t_param = ityparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0)
	{
		ttychars(tp);
		if (tp->t_ispeed == 0)
		{
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = itydefaultrate;
		}
		ityparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);
	(void) spltty();

	if (OUTBOUND(dev)) {
		/*
		 * XXX should call l_modem() here and not meddle with CLOCAL,
		 * but itystart() wants TS_CARR_ON to give the true carrier.
		 */
		tp->t_cflag |= CLOCAL;
		tp->t_state |= TS_CONNECTED;
	}

	while ((flag & O_NONBLOCK) == 0 && (tp->t_cflag & CLOCAL) == 0 &&
	       (tp->t_state & TS_CARR_ON) == 0)
	{
		error = tsleep(TSA_CARR_ON(tp), TTIPRI | PCATCH, "iidcd", 0);
		if (error)
			break;
	}
	(void) spl0();
	if (error == 0)
		error = (*linesw[tp->t_line].l_open) (dev, tp);
	return (error);
}

/* ARGSUSED */
int
ityclose(dev, flag, mode, p)
	dev_t           dev;
	int             flag, mode;
	struct proc    *p;
{
	register struct tty *tp;
	register        ity;
	register int    unit;

	unit = UNIT(dev);
	ity = ity_addr[unit];
	if(tp = &ity_tty[unit])
	(*linesw[tp->t_line].l_close) (tp, flag);
	ttyclose(tp);
	isdn_disconnect(applnr[unit],0);
	return (0);
}

int
ityread(dev, uio, flag)
	dev_t           dev;
	struct uio     *uio;
	int flag;
{
	register struct tty *tp = &ity_tty[UNIT(dev)];

	return ((*linesw[tp->t_line].l_read) (tp, uio, flag));
}

int
itywrite(dev, uio, flag)
	dev_t           dev;
	struct uio     *uio;
	int flag;
{
	int             unit = UNIT(dev);
	register struct tty *tp = &ity_tty[unit];

	return ((*linesw[tp->t_line].l_write) (tp, uio, flag));
}

int
ity_input(int no, int len, char *buf, int dir)
{
	register struct tty *tp = &ity_tty[no];
	int i;

	if (tp->t_state & TS_ISOPEN)
		for(i= 0; i<len; i++)
			(*linesw[tp->t_line].l_rint)(buf[i], tp);
	else len= 0;
	return(len);
}

void
itystart(struct tty *tp)
{
	int             s, unit;

	unit = UNIT(tp->t_dev);

	s = splhigh();
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP))
	{
		splx(s);
		return;
	}
	ttwwakeup(tp);
	if (tp->t_outq.c_cc)
	{
		if(OUTBOUND(tp->t_dev) && (tp->t_cflag & CLOCAL) &&
			((tp->t_state & TS_CARR_ON) == 0))
			isdn_msg(applnr[unit]);
		else isdn_output(applnr[unit]);
		tp->t_state |= TS_BUSY;
	}
	splx(s);
}

int
ity_out(int no, char *buf, int len)
{
	struct tty *tp = &ity_tty[no];
	int i;

	if(tp == NULL)
		return(0);
	if(tp->t_outq.c_cc)
	{
		for (i = 0; i < len && tp->t_outq.c_cc; ++i)
			buf[i]= getc(&tp->t_outq);
		return(i);
	}
	tp->t_state &=~ (TS_BUSY|TS_FLUSH);
	if (tp->t_line)
		(*linesw[tp->t_line].l_start)(tp);
	else
		itystart(tp);
	return(0);
}

void
ity_connect(int no)
{
	struct tty *tp = &ity_tty[no];

	if(tp == NULL)
		return;
	if(OUTBOUND(tp->t_dev)) tp->t_cflag &= ~CLOCAL;
	(*linesw[tp->t_line].l_modem) (tp, 1);
	tp->t_state &=~ (TS_BUSY|TS_FLUSH);
	if (tp->t_line)
		(*linesw[tp->t_line].l_start)(tp);
	else
		itystart(tp);
}

void
ity_disconnect(int no)
{
	struct tty *tp = &ity_tty[no];
	if(tp) (*linesw[tp->t_line].l_modem) (tp, 0);
}

int
ityioctl(dev, cmd, data, flag,p)
	dev_t           dev;
        int             cmd;
	caddr_t         data;
        int             flag;
        struct proc     *p;
{
	register struct tty *tp;
	register int    unit = UNIT(dev);
	register int    error;

	tp = &ity_tty[unit];
	error = (*linesw[tp->t_line].l_ioctl) (tp, cmd, data, flag,p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag);
	if (error >= 0)
		return (error);

	switch (cmd)
	{
	default:
		return (ENOTTY);
	}
	return (0);
}

int
ityparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register        ity;
	register int    cfcr, cflag = t->c_cflag;
	int             unit = UNIT(tp->t_dev);
	int             ospeed = t->c_ospeed;

	/* check requested parameters */
	if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return (EINVAL);
	/* and copy to tty */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = cflag;

	if (ospeed == 0)
	{
		isdn_disconnect(applnr[unit],0);
		return (0);
	}
	return (0);
}

/*
 * Stop output on a line.
 */
/* ARGSUSED */
void
itystop(struct tty *tp, int flag)
{
	register int    s;

	s = splhigh();
	if (tp->t_state & TS_BUSY)
	{
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}

struct tty *
itydevtotty(dev_t dev)
{
	register int unit = UNIT(dev);
	if (unit >= next_if)
		return (NULL);

	return (&ity_tty[unit]);
}

#ifdef JREMOD
struct cdevsw ity_cdevsw = 
	{ ityopen,	ityclose,	ityread,	itywrite,	/*56*/
	  ityioctl,	nostop,		nxreset,	itydevtotty,/* ity */
	  ttselect,	nommap,		NULL };

static ity_devsw_installed = 0;

static void 	ity_drvinit(void *unused)
{
	dev_t dev;

	if( ! ity_devsw_installed ) {
		dev = makedev(CDEV_MAJOR,0);
		cdevsw_add(&dev,&ity_cdevsw,NULL);
		ity_devsw_installed = 1;
#ifdef DEVFS
		{
			int x;
/* default for a simple device with no probe routine (usually delete this) */
			x=devfs_add_devsw(
/*	path	name	devsw		minor	type   uid gid perm*/
	"/",	"ity",	major(dev),	0,	DV_CHR,	0,  0, 0600);
		}
    	}
#endif
}

SYSINIT(itydev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ity_drvinit,NULL)

#endif /* JREMOD */

#endif
