static char     _isdnid[] = "@(#)$Id: isdn.c,v 1.9 1995/12/08 11:13:01 julian Exp $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.9 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: isdn.c,v $
 * Revision 1.9  1995/12/08  11:13:01  julian
 * Pass 3 of the great devsw changes
 * most devsw referenced functions are now static, as they are
 * in the same file as their devsw structure. I've also added DEVFS
 * support for nearly every device in the system, however
 * many of the devices have 'incorrect' names under DEVFS
 * because I couldn't quickly work out the correct naming conventions.
 * (but devfs won't be coming on line for a month or so anyhow so that doesn't
 * matter)
 *
 * If you "OWN" a device which would normally have an entry in /dev
 * then search for the devfs_add_devsw() entries and munge to make them right..
 * check out similar devices to see what I might have done in them in you
 * can't see what's going on..
 * for a laugh compare conf.c conf.h defore and after... :)
 * I have not doen DEVFS entries for any DISKSLICE devices yet as that will be
 * a much more complicated job.. (pass 5 :)
 *
 * pass 4 will be to make the devsw tables of type (cdevsw * )
 * rather than (cdevsw)
 * seems to work here..
 * complaints to the usual places.. :)
 *
 * Revision 1.8  1995/11/29  14:39:12  julian
 * If you're going to mechanically replicate something in 50 files
 * it's best to not have a (compiles cleanly) typo in it! (sigh)
 *
 * Revision 1.7  1995/11/29  10:47:10  julian
 * OK, that's it..
 * That's EVERY SINGLE driver that has an entry in conf.c..
 * my next trick will be to define cdevsw[] and bdevsw[]
 * as empty arrays and remove all those DAMNED defines as well..
 *
 * Revision 1.6  1995/11/16  10:47:21  bde
 * Fixed a call to the listen function.  A trailing arg was missing.
 *
 * Fixed the type of isdn_check().  A trailing arg was missing.
 *
 * Included "conf.h" to get some prototypes.
 *
 * Completed function declarations.
 *
 * Added prototypes.
 *
 * Removed some useless includes.
 *
 * Revision 1.5  1995/09/08  11:06:58  bde
 * Fix benign type mismatches in devsw functions.  82 out of 299 devsw
 * functions were wrong.
 *
 * Revision 1.4  1995/05/30  07:58:02  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.3  1995/03/28  07:54:44  bde
 * Add and move declarations to fix all of the warnings from `gcc -Wimplicit'
 * (except in netccitt, netiso and netns) that I didn't notice when I fixed
 * "all" such warnings before.
 *
 * Revision 1.2  1995/02/15  06:28:29  jkh
 * Fix up include paths, nuke some warnings.
 *
 * Revision 1.1  1995/02/14  15:00:33  jkh
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

/*
 * Copyright (c) 1994 Dietmar Friede (dietmar@friede.de) All rights reserved.
 * FSF/FSAG GNU Copyright applies
 *
 * An intermediate level for ISDN Drivers.
 *
 */

#include "isdn.h"
#include "ii.h"
#include "ity.h"
#include "itel.h"
#include "ispy.h"
#if NISDN > 0

#define TYPNR		4
#define N_ISDN_APPL	(NII + NITY + NITEL + NISPY)

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include "gnu/isdn/isdn_ioctl.h"


isdn_appl_t     isdn_appl[N_ISDN_APPL];
isdn_ctrl_t     isdn_ctrl[N_ISDN_CTRL];
int Isdn_Appl, Isdn_Ctrl, Isdn_Typ;

extern void	isdn_attach __P((void));
static timeout_t isdn_check;
extern char	*isdn_get_prot __P((int ap, int dir));
extern int	isdn_get_prot_size __P((int ap));
extern int	isdn_set_prot __P((int ap, int dir, char *p));
extern int	isdn_stat __P((int cn));
static void	passout __P((int unit, int l, char *buf));

static	d_open_t	isdnopen;
static	d_close_t	isdnclose;
static	d_rdwr_t	isdnrw;
static	d_read_t	isdnread;
static	d_write_t	isdnwrite;
static	d_ioctl_t	isdnioctl;

#define CDEV_MAJOR 55
static struct cdevsw isdn_cdevsw = 
	{ isdnopen,	isdnclose,	isdnread,	nowrite,	/*55*/
	  isdnioctl,	nostop,		nullreset,	nodevtotty,/* isdn */
	  seltrue,	nommap,		NULL,	"isdn",	NULL,	-1 };


static int      o_flags, r_flags, bufind[TYPNR];
static char     buffer[TYPNR][257];
static u_char appl_list[TYPNR];

typedef u_char  prot[2];
static u_char   prot_size[2] = {0, 2};
static prot     passiv[6]    = {{0}, {3, 3}};
static prot     activ[6]     = {{0}, {1, 3}};

u_short isdn_state= 0;
static isdn_timeout= 0;

int
isdn_get_prot_size(int ap)
{
	return (prot_size[isdn_appl[ap].prot]);
}

char *
isdn_get_prot(int ap, int dir)
{
	if(dir)
		return(activ[isdn_appl[ap].prot]);
	return(passiv[isdn_appl[ap].prot]);
}

int
isdn_set_prot(int ap, int dir, char *p)
{
	char           *pr;
	int             i, l;
	if ((l = isdn_get_prot_size(ap)) == 0)
		return (0);
	if (dir)
		pr = passiv[isdn_appl[ap].prot];
	else
		pr = activ[isdn_appl[ap].prot];
	for (i = 0; i < l; i++, pr++, p++)
		*p = *pr;
	return (l);
}

void
isdn_attach()
{
	isdn_appl_t    *appl;
	int i, an;

	appl_list[0]= Isdn_Typ= an= 0;

	for(i= 0 ; i<NII; i++,an++)
	{
		appl = &isdn_appl[an];
		appl->ctrl = -1;
		appl->state = 0;
		appl->appl = an;
		appl->typ = Isdn_Typ;
		appl->drivno = iiattach(an);
		appl->PassUp = ii_input;
		appl->PassDown = ii_out;
		appl->Connect = ii_connect;
		appl->DisConn = ii_disconnect;
	}

	appl_list[1]= an;
	Isdn_Typ= 1;
	for(i= 0 ; i<NITY; i++,an++)
	{
		appl = &isdn_appl[an];
		appl->ctrl = -1;
		appl->state = 0;
		appl->appl = an;
		appl->typ = Isdn_Typ;
		appl->drivno = ityattach(an);
		appl->PassUp = ity_input;
		appl->PassDown = ity_out;
		appl->Connect = ity_connect;
		appl->DisConn = ity_disconnect;
	}

	appl_list[2]= an;
	Isdn_Typ= 2;
	for(i= 0 ; i<NITEL; i++,an++)
	{
		appl = &isdn_appl[an];
		appl->ctrl = -1;
		appl->state = 0;
		appl->appl = an;
		appl->typ = Isdn_Typ;
		appl->drivno = itelattach(an);
		appl->PassUp = itel_input;
		appl->PassDown = itel_out;
		appl->Connect = itel_connect;
		appl->DisConn = itel_disconnect;
	}

	appl_list[3]= an;
	Isdn_Typ= 3;
	for(i= 0 ; i<NISPY; i++,an++)
	{
		appl = &isdn_appl[an];
		appl->ctrl = -1;
		appl->state = 0;
		appl->appl = an;
		appl->typ = Isdn_Typ;
		appl->drivno = ispyattach(an);
		appl->PassUp = ispy_input;
	}
	Isdn_Appl= an;
}

int
isdn_ctrl_attach(int n)
{
	int             c = Isdn_Ctrl;

	if(Isdn_Ctrl == 0) isdn_attach();
	if ((Isdn_Ctrl += n) <= N_ISDN_CTRL)
		return (c);
	Isdn_Ctrl = c;
#ifdef	DEVFS
/*SOMETHING GOES IN HERE I THINK*/
#endif
	return (-1);
}

/*
 * isdnopen() New open on device.
 *
 * I forbid all but one open per application. The only programs opening the
 *  isdn device are the ISDN-daemon
 */
static	int
isdnopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	int             err;

	if (minor(dev)>Isdn_Typ)
		return (ENXIO);

	/* Card busy ? */
	if (o_flags & (1 << minor(dev)))
		return (EBUSY);

	o_flags |= (1 << minor(dev));

	return (0);
}

static	int
isdnclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	o_flags &= ~(1 << minor(dev));
	return (0);
}

static	int
isdnread(dev_t dev, struct uio * uio, int ioflag)
{
	int             x;
	int             error = 0;
	int		unit= minor(dev);

	r_flags &= ~(1 << unit);

	x = splhigh();
	if(bufind[unit] == 0)
	{
		r_flags |= (1 << unit);
		error= tsleep((caddr_t) buffer[unit], PZERO + 1, "isdnin", hz);
	}
	if(bufind[unit])
	{
		buffer[unit][bufind[unit]++]= 0;
		error = uiomove(buffer[unit], bufind[unit], uio);
		bufind[unit] = 0;
	}
	splx(x);
	return error;
}

static	int
isdnioctl(dev_t dev, int cmd, caddr_t data, int flags, struct proc *p)
{
	int             err, x, i;
	isdn_appl_t    *appl;
	isdn_ctrl_t    *ctrl;
	short          *val = (short *) data;
	unsigned        ab, an, cn;

	err = 0;
	ab= appl_list[minor(dev)];

	switch (cmd)
	{
	case ISDN_LISTEN:
		{
			listen_t	*s= (listen_t *) data;

			an= ab;
			if (s->ctrl >= Isdn_Ctrl)
				return (ENODEV);
			cn= s->ctrl;
			ctrl = &isdn_ctrl[cn];

			x = splhigh();
			while(isdn_state)
			{
				err = tsleep((caddr_t) ctrl, PZERO | PCATCH, "slisten", 2);
				if (err != EWOULDBLOCK)
				{
					splx(x);
					return (err);
				}
			}

			isdn_state = 0xffff;
			while((err = (*ctrl->listen) (s->ctrl, minor(dev) | 0x30
				, s->inf_mask ,s->subadr_mask ,s->si_mask, /* XXX */ 0)) == EBUSY)
			{
				err = tsleep((caddr_t) ctrl, PZERO | PCATCH, "blisten", 2);
				if (err != EWOULDBLOCK)
				{
					splx(x);
					return (err);
				}
			}

			if (err)
			{
				splx(x);
				return (err);
			}
			while (isdn_state == 0xffff)
			{
				err = tsleep((caddr_t) ctrl, PZERO | PCATCH, "ilisten", 2);
				if (err != EWOULDBLOCK)
				{
					splx(x);
					return (err);
				}
			}
			splx(x);
			err= isdn_state;
			isdn_state= 0;
			return (err);	/* tricky but it works */
		}
		break;

	case ISDN_DIAL:
		{
			dial_t     *d= (dial_t*)data;
			telno_t        *t= &d->telno;

			an = d->appl + ab;
			cn = d->ctrl;

			if (an >= Isdn_Appl || cn >= Isdn_Ctrl)
				return (ENODEV);

			appl = &isdn_appl[an];

			if (ISBUSY(appl->ctrl) || appl->state)
				return (EBUSY);

			appl->state= 1;
			x = splhigh();

			while((err = (*isdn_ctrl[cn].connect) (cn, an
				     ,d->b_channel, d->inf_mask, d->out_serv
				     ,d->out_serv_add, d->src_subadr, t->length
				     ,t->no, d->spv)) == EBUSY)
			{
				err = tsleep((caddr_t) appl, PZERO | PCATCH, "idial", 2);
				if (err != EWOULDBLOCK)
				{
					splx(x);
					return (err);
				}
			}
			if(err) appl->state= 0;
			splx(x);
			return(err);
		}
		break;
	case ISDN_HANGUP:
		cn = data[0];
		if (cn >= Isdn_Ctrl)
			return (ENODEV);
		x = splhigh();

		while((err = (*isdn_ctrl[cn].disconnect) (cn, data[1])) == EBUSY)
		{
			err = tsleep((caddr_t) data, PZERO | PCATCH, "ihang", 2);
			if (err != EWOULDBLOCK)
			{
				splx(x);
				return (err);
			}
		}
		splx(x);
		break;
	case ISDN_ACCEPT:
		cn = data[0];
		an = data[1] + ab;
		if (cn >= Isdn_Ctrl)
			return (ENODEV);
		x = splhigh();
		while((err = (*isdn_ctrl[cn].accept) (cn, an, data[2])) == EBUSY)
		{
			err = tsleep((caddr_t) data, PZERO | PCATCH, "iaccept", 2);
			if (err != EWOULDBLOCK)
			{
				splx(x);
				return (err);
			}
		}
		splx(x);
		break;
	case ISDN_SET_PARAM:
		{
			isdn_param     *p = (isdn_param *) data;

			an = p->appl + ab;
			if (an >= Isdn_Appl)
				return (ENODEV);
			appl = &isdn_appl[an];
			bcopy(p, appl, sizeof(isdn_param));
			appl->appl+= ab;
		}
		break;
	case ISDN_GET_PARAM:
		{
			isdn_param     *p = (isdn_param *) data;
			an = p->appl + ab;
			if (an >= Isdn_Appl)
				return (ENODEV);
			appl = &isdn_appl[an];
			bcopy(appl, p, sizeof(isdn_param));
		}
		break;
	default:
		err = ENODEV;
	}

	return (err);
}

void
isdn_start_out(int cn)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	isdn_appl_t    *appl = &isdn_appl[ctrl->appl];
	int x;

	x= splhigh();
	if (ctrl->o_len == 0)
	{
		int l;
		l = isdn_set_prot(ctrl->appl, ctrl->islisten, ctrl->o_buf);
		ctrl->o_len = (*appl->PassDown) (appl->drivno, ctrl->o_buf+l,2048-l);

		if (ctrl->o_len == 0)
		{
			splx(x);
			return;
		}
		ctrl->o_len+= l;
		(*ctrl->output) (cn);
	}

	splx(x);
}

int
isdn_stat(int cn)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	return((*ctrl->state) (cn));
}

int
isdn_output(int an)
{
	isdn_appl_t    *appl = &isdn_appl[an];

	if (ISFREE(appl->ctrl))
	{
		int             l;
		char	buf[10];

		if(appl->state)
			return(0);

		l = sprintf(buf,"d %d", an-appl_list[appl->typ]);
		passout(appl->typ,l,buf);
		return(0);
	}
	isdn_start_out(appl->ctrl);
	return (0);
}

int
isdn_msg(int an)
{
	isdn_appl_t    *appl = &isdn_appl[an];

	if (ISFREE(appl->ctrl))
	{
		int             l;
		char	buf[256];

		l = sprintf(buf,"M %d", an-appl_list[appl->typ]);
		l += (*appl->PassDown) (appl->drivno, buf+l,256-l);
		passout(appl->typ,l,buf);
		return(0);
	}
	return (1);
}

int
isdn_input(int an, int len, char *buf, int dir)
{
	int             l;
	char           *p;
	isdn_appl_t    *appl = &isdn_appl[an];

	if (l = isdn_get_prot_size(an))
	{
		p= isdn_get_prot(an,dir);
		if((p[0] != buf[0]) || (p[1] != buf[1]))
			return(0);
		len -= l;
		buf += l;
	}
	return ((*appl->PassUp) (appl->drivno, len, buf, dir));
}

void
isdn_accept_con_ind(int an, int cn, char serv, char serv_add, char subadr, char nl, char *num)
{
	int             l;
	char	buf[32];

	an&= 0xf;
	l = sprintf(buf, "a %d %d %d %d %c %d %d %s", an, cn ,serv, serv_add
			, subadr,(u_char) num[0], nl, num + 1);
	passout(an,l,buf);
}

void
isdn_info(int an, int typ, int len, char *data)
{
	int             l;
	char	buf[64];
	u_short		no;

	if(an < Isdn_Appl)
		no= isdn_appl[an].typ;
	else	no= an&0xf;

	if(no > Isdn_Typ) no= 3;

	if(len>48) len= 48;
	data[len]= 0;
	l = sprintf(buf,"i %d %d %d %s", an, typ, len, data);
	passout(no,l,buf);
}

static void
isdn_check(void *chan)
{
	int i;

	isdn_timeout= 0;
	for(i= 0; i < Isdn_Ctrl; i++)
	{
		int an;
		isdn_ctrl_t    *ctrl = &isdn_ctrl[i];

		if((an= ctrl->appl) < Isdn_Appl)
		{
			isdn_appl_t    *appl = &isdn_appl[an];

			if(appl->timeout)
			{
				isdn_timeout= 1;
				if(time.tv_sec > (ctrl->lastact + (appl->timeout)))
				{
					isdn_disconnect(an,0);
					break;
				}
			}
		}
	}

	if(isdn_timeout)
	{
		timeout(isdn_check,0,hz/2);
	}
}

void
isdn_conn_ind(int an, int cn, int dial)
{
	isdn_appl_t    *appl = &isdn_appl[an];
	int             l;
	char	buf[10];

	if (appl->Connect)
		(*appl->Connect) (appl->drivno);

	l = sprintf(buf,"C %d %d %d", an-appl_list[appl->typ], cn, dial);
	passout(appl->typ,l,buf);
	if((isdn_timeout == 0) && appl->timeout)
	{
		isdn_timeout= 1;
		timeout(isdn_check,0,hz/2);
	}
}

void
isdn_disconn_ind(int an)
{
	isdn_appl_t    *appl = &isdn_appl[an];
	int             l;
	char	buf[10];

	if(( an < 0) || (an >= Isdn_Appl))
		return;

	appl->state= 0;
	if (appl->DisConn)
		(*appl->DisConn) (appl->drivno);
	l = sprintf(buf,"D %d", an-appl_list[appl->typ]);
	passout(appl->typ,l,buf);
}

void
isdn_disconnect(int an, int rea)
{
	isdn_appl_t    *appl = &isdn_appl[an];

	if (ISBUSY(appl->ctrl))
	{
		int x;
		x = splhigh();
		(*isdn_ctrl[appl->ctrl].disconnect)(appl->ctrl,rea);
		splx(x);
	}
}

static void
passout(int unit, int l, char *buf)
{
	int             x;

	x = splhigh();
	if ((bufind[unit] + l) >= 256)
	{
		splx(x);
		return;
	}
	bcopy(buf,&buffer[unit][bufind[unit]],l);
	bufind[unit] += l;
	buffer[unit][bufind[unit]++]= 0;
	if (r_flags & (1<<unit))
	{
		r_flags &= ~(1 << unit);
		wakeup((caddr_t) buffer[unit]);
	}
	splx(x);
}

static isdn_devsw_installed = 0;

static void
isdn_drvinit(void *unused)
{
	dev_t dev;

	if( ! isdn_devsw_installed ) {
		dev = makedev(CDEV_MAJOR,0);
		cdevsw_add(&dev,&isdn_cdevsw,NULL);
		isdn_devsw_installed = 1;
    	}
}

SYSINIT(isdndev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,isdn_drvinit,NULL)

#endif				/* NISDN > 0 */
