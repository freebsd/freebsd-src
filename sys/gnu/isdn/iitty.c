static char     _ittyid[] = "@(#)$Id: iitty.c,v 1.1 1995/02/14 15:00:32 jkh Exp $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.1 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: iitty.c,v $
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

#include "param.h"
#include "systm.h"
#include "ioctl.h"
#include "select.h"
#include "tty.h"
#include "proc.h"
#include "user.h"
#include "conf.h"
#include "file.h"
#include "uio.h"
#include "kernel.h"
#include "syslog.h"
#include "types.h"

#include "gnu/isdn/isdn_ioctl.h"

int             ityattach(), ityparam();
void		itystart();

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
		tp->t_state |= TS_WOPEN;
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

	if(OUTBOUND(dev)) tp->t_cflag |= CLOCAL;

	while ((flag & O_NONBLOCK) == 0 && (tp->t_cflag & CLOCAL) == 0 &&
	       (tp->t_state & TS_CARR_ON) == 0)
	{
		tp->t_state |= TS_WOPEN;
		if (error = ttysleep(tp, (caddr_t) & tp->t_rawq, TTIPRI | PCATCH,
				     ttopen, 0))
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
ity_input(int no, int len, char *buf)
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
	if (tp->t_outq.c_cc <= tp->t_lowat)
	{
		if (tp->t_state & TS_ASLEEP)
		{
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t) & tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
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
	tp->t_state |= TS_CARR_ON;
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

int
ityselect(dev_t dev, int rw, struct proc *p)
{
	return (ttselect(dev, rw, p));
}

#endif
