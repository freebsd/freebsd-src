static char     _ittyid[] = "@(#)$Id: iitty.c,v 1.18 1995/12/10 15:54:13 bde Exp $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.18 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 */

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
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include "gnu/isdn/isdn_ioctl.h"

static	d_open_t	ityopen;
static	d_close_t	ityclose;
static	d_read_t	ityread;
static	d_write_t	itywrite;
static	d_ioctl_t	ityioctl;
static	d_stop_t	itystop;
static	d_devtotty_t	itydevtotty;

#define CDEV_MAJOR 56
static struct cdevsw ity_cdevsw = 
	{ ityopen,	ityclose,	ityread,	itywrite,	/*56*/
	  ityioctl,	itystop,	noreset,	itydevtotty,/* ity */
	  ttselect,	nommap,		NULL,	"ity",	NULL,	-1 };


static int	ityparam __P((struct tty *tp, struct termios *t));
static void	itystart __P((struct tty *tp));

static int      itydefaultrate = 64000;
static short    ity_addr[NITY];
static struct tty ity_tty[NITY];
static int	applnr[NITY];
static int	next_if= 0;
#ifdef	DEVFS
void		*devfs_token[NITY];
static void	*devfs_token_out[NITY];
#endif

#define	UNIT(x)		(minor(x)&0x3f)
#define	OUTBOUND(x)	((minor(x)&0x80)==0x80)

int
ityattach(int ap)
{
	char	name[32];
	if(next_if >= NITY)
		return(-1);

	applnr[next_if]= ap;
#ifdef	DEVFS
	sprintf(name,"ity%d",next_if);
	devfs_token[next_if] = devfs_add_devsw("/isdn",name,
		&ity_cdevsw,next_if, DV_CHR, 0, 0, 0600);
	sprintf(name,"Oity%d",next_if); /* XXX find out real name */
	devfs_token[next_if] = devfs_add_devsw("/isdn",name,
		&ity_cdevsw,(next_if | 0x80), DV_CHR, 0, 0, 0600);
#endif
	return(next_if++);
}

/* ARGSUSED */
static	int
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
static	int
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

static	int
ityread(dev, uio, flag)
	dev_t           dev;
	struct uio     *uio;
	int flag;
{
	register struct tty *tp = &ity_tty[UNIT(dev)];

	return ((*linesw[tp->t_line].l_read) (tp, uio, flag));
}

static	int
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

static void
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

static	int
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

static int
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
static	void
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

static	struct tty *
itydevtotty(dev_t dev)
{
	register int unit = UNIT(dev);
	if (unit >= next_if)
		return (NULL);

	return (&ity_tty[unit]);
}

static ity_devsw_installed = 0;

static void
ity_drvinit(void *unused)
{
	dev_t dev;

	if( ! ity_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&ity_cdevsw, NULL);
		ity_devsw_installed = 1;
    	}
}

SYSINIT(itydev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ity_drvinit,NULL)

#endif
