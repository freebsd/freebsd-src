static char     _ispyid[] = "@(#)$Id: iispy.c,v 1.9 1995/12/08 23:19:40 phk Exp $";
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
 */

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
