static char     _ispyid[] = "@(#)$Id: iispy.c,v 1.8 1995/12/08 11:12:52 julian Exp $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.8 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: iispy.c,v $
 * Revision 1.8  1995/12/08  11:12:52  julian
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
 * Revision 1.7  1995/12/06  23:43:37  bde
 * Removed unnecessary #includes of <sys/user.h>.  Some of these were just
 * to get the definitions of TRUE and FALSE which happen to be defined in
 * a deeply nested include.
 *
 * Added nearby #includes of <sys/conf.h> where appropriate.
 *
 * Revision 1.6  1995/11/29  14:39:10  julian
 * If you're going to mechanically replicate something in 50 files
 * it's best to not have a (compiles cleanly) typo in it! (sigh)
 *
 * Revision 1.5  1995/11/29  10:47:07  julian
 * OK, that's it..
 * That's EVERY SINGLE driver that has an entry in conf.c..
 * my next trick will be to define cdevsw[] and bdevsw[]
 * as empty arrays and remove all those DAMNED defines as well..
 *
 * Revision 1.4  1995/09/08  11:06:56  bde
 * Fix benign type mismatches in devsw functions.  82 out of 299 devsw
 * functions were wrong.
 *
 * Revision 1.3  1995/03/28  07:54:40  bde
 * Add and move declarations to fix all of the warnings from `gcc -Wimplicit'
 * (except in netccitt, netiso and netns) that I didn't notice when I fixed
 * "all" such warnings before.
 *
 * Revision 1.2  1995/02/15  06:28:27  jkh
 * Fix up include paths, nuke some warnings.
 *
 * Revision 1.1  1995/02/14  15:00:29  jkh
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

#include "ispy.h"
#if NISPY > 0

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
/*#include "malloc.h"*/

#include <gnu/isdn/isdn_ioctl.h>


int     nispy = NISPY;
int	ispy_applnr;
static int	next_if =0;
static unsigned long ispy_cnt, ispy_out;
static char	dir;
#define ISPY_SIZE	260
#define OPEN		1
#define READ_WAIT	2
#define ISPYBUF		16
#define ISPYMASK	(ISPYBUF-1)
/* ISPYBUF has to be a power of 2 */

static
struct ispy_data
{
	struct ispy_buf
	{
		unsigned long cnt;
		struct timeval stamp;
		char ibuf[ISPY_SIZE];
		unsigned char dir;
		int ilen;
	} b[ISPYBUF];
	int state;
#ifdef	DEVFS
	void	*devfs_token;
#endif
} ispy_data[NISPY];

static	d_open_t	ispyopen;
static	d_close_t	ispyclose;
static	d_read_t	ispyread;
static	d_ioctl_t	ispyioctl;

#define CDEV_MAJOR 59
static struct cdevsw ispy_cdevsw = 
	{ ispyopen,	ispyclose,	ispyread,	nowrite,	/*59*/
	  ispyioctl,	nostop,		nullreset,	nodevtotty,/* ispy */
	  seltrue,	nommap,         NULL,	"ispy",	NULL,	-1 };


int
ispyattach(int ap)
{
	char	name[32];
	struct ispy_data *ispy;

	if(next_if >= NISPY)
		return(-1);
	ispy= &ispy_data[next_if];
	ispy->state= 0;
	ispy_applnr= ap;
#ifdef DEVFS
	sprintf(name,"ispy%d",next_if);
	ispy->devfs_token  =devfs_add_devsw("/isdn",name,&ispy_cdevsw,next_if,
						DV_CHR,	0,  0, 0600);
#endif
	return(next_if++);
}

int
ispy_input(int no, int len, char *buf, int out)
{
	struct ispy_data *ispy= &ispy_data[no];
	struct ispy_buf *b= &ispy->b[ispy_cnt&ISPYMASK];

	if(len > ISPY_SIZE)
		return(0);
	if(len)
	{
		b->cnt= ispy_cnt++;
		b->stamp= time;
		b->dir= out;
		bcopy(buf, b->ibuf, len);
	}
	b->ilen= len;
	if(ispy->state & READ_WAIT)
	{
		ispy->state &= ~READ_WAIT;
		wakeup((caddr_t) &ispy->state);
	}
	return(len);
}

static	int
ispyopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	int             err;
	struct ispy_data *ispy;

	if (minor(dev)>NISPY)
		return (ENXIO);

	ispy= &ispy_data[minor(dev)];

	if(ispy->state&OPEN) return(EBUSY);
	ispy->state |= OPEN;

	return (0);
}

static	int
ispyclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct ispy_data *ispy= &ispy_data[minor(dev)];

	if(ispy->state & READ_WAIT)
		wakeup((caddr_t) &ispy->state);
	ispy->state = 0;
	return (0);
}

static	int
ispyioctl (dev_t dev, int cmd, caddr_t data, int flags, struct proc *p)
{
        int     unit = minor(dev);

        switch (cmd) {
            default:
                return (ENOTTY);
        }
        return (0);
}

static	int
ispyread(dev_t dev, struct uio * uio, int ioflag)
{
	int             x;
	int             error = 0;
	struct ispy_data *ispy= &ispy_data[minor(dev)];
	struct ispy_buf *b;

	if((ispy_cnt-ispy_out) > ISPYBUF)
		ispy_out= ispy_cnt - ISPYBUF;
	b= &ispy->b[ispy_out&ISPYMASK];
	ispy_out++;
	while(b->ilen == 0)
	{
		ispy->state |= READ_WAIT;
		if(error= tsleep((caddr_t) &ispy->state, TTIPRI | PCATCH, "ispy", 0 ))
			return(error);
	}

	x = splhigh();
	if(b->ilen)
	{
		error = uiomove((char *) &b->dir, 1, uio);
		if(error == 0)
			error = uiomove((char *) &b->cnt
			,sizeof(unsigned long)+sizeof(struct timeval)+b->ilen, uio);
		b->ilen= 0;
	}
	splx(x);
	return error;
}

static ispy_devsw_installed = 0;

static void 
ispy_drvinit(void *unused)
{
	dev_t dev;

	if( ! ispy_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&ispy_cdevsw, NULL);
		ispy_devsw_installed = 1;
    	}
}

SYSINIT(ispydev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ispy_drvinit,NULL)

#endif
