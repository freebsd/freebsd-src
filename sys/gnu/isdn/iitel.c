static char     _itelid[] = "@(#)$Id: iitel.c,v 1.10 1995/12/08 23:19:42 phk Exp $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.10 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 */

#include "itel.h"
#if NITEL > 0

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include "gnu/isdn/isdn_ioctl.h"


static int	applnr[NITEL];
static int	next_if =0;
#define ITEL_SIZE	1024
#define OPEN		1
#define CONNECT		2
#define READ_WAIT	4
#define WRITE_WAIT	8
#define min(a,b)        ((a)<(b)?(a):(b))

static	d_open_t	itelopen;
static	d_close_t	itelclose;
static	d_read_t	itelread;
static	d_write_t	itelwrite;
static	d_ioctl_t	itelioctl;

#define CDEV_MAJOR 57
static struct cdevsw itel_cdevsw = 
	{ itelopen,	itelclose,	itelread,	itelwrite,	/*57*/
	  itelioctl,	nostop,		nullreset,	nodevtotty,/* itel */
	  seltrue,	nommap,		NULL,	"itel",	NULL,	-1 };

static
struct itel_data
{
	char ibuf[ITEL_SIZE];
	char obuf[ITEL_SIZE];
	int state;
	int ilen, olen;
#ifdef	DEVFS
	void	*devfs_token;
#endif
} itel_data[NITEL];

int
itelattach(int ap)
{
	struct itel_data *itel;
	char	name[32];

	if(next_if >= NITEL)
		return(-1);
	itel= &itel_data[next_if];
	itel->ilen= itel->olen= 0;
	itel->state= 0;
	applnr[next_if]= ap;
#ifdef	DEVFS
	sprintf(name,"itel%d",next_if);
	itel->devfs_token = devfs_add_devsw("/isdn",name,&itel_cdevsw,next_if,
				DV_CHR, 0, 0, 0600);
#endif
	return(next_if++);
}

int
itel_input(int no, int len, char *buf, int dir)
{
	struct itel_data *itel= &itel_data[no];

	if(itel->ilen || ( len > ITEL_SIZE))
		return(0);
	if(len)
		bcopy(buf, itel->ibuf, len);
	itel->ilen= len;
	if(itel->state & READ_WAIT)
	{
		itel->state &= ~READ_WAIT;
		wakeup((caddr_t) itel->ibuf);
	}
	return(len);
}

int
itel_out(int no, char *buf, int len)
{
	struct itel_data *itel= &itel_data[no];
	int l;

	if((itel->state & CONNECT) == 0)
		return(0);
        if((l= itel->olen) && (itel->olen <= len))
		bcopy(itel->obuf, buf, l);

	itel->olen= 0;
	if(itel->state & WRITE_WAIT)
	{
		itel->state &= ~WRITE_WAIT;
		wakeup((caddr_t) itel->obuf);
	}
	return(l);
}

void
itel_connect(int no)
{
	itel_data[no].state |= CONNECT;
}

void
itel_disconnect(int no)
{
	struct itel_data *itel= &itel_data[no];
	int s;

	s= itel->state;
	if(itel->state &= OPEN)
	{
		itel->ilen= itel->olen= 0;
		if(s & READ_WAIT)
			wakeup((caddr_t) itel->ibuf);
		if(s & WRITE_WAIT)
			wakeup((caddr_t) itel->obuf);
	}
}

int
itelopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	int             err;
	struct itel_data *itel;

	if (minor(dev)>NITEL)
		return (ENXIO);

	itel= &itel_data[minor(dev)];
	if((itel->state & CONNECT) == 0)
		return(EIO);

	if(itel->state&OPEN) return(0);
	itel->ilen= itel->olen= 0;
	itel->state |= OPEN;

	return (0);
}

int
itelclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct itel_data *itel= &itel_data[minor(dev)];

	if(itel->state & READ_WAIT)
		wakeup((caddr_t) itel->ibuf);
	if(itel->state & WRITE_WAIT)
		wakeup((caddr_t) itel->obuf);
	itel_data[minor(dev)].state &= CONNECT;
	return (0);
}

int
itelioctl (dev_t dev, int cmd, caddr_t data, int flags, struct proc *p)
{
        int     unit = minor(dev);

        switch (cmd) {
            default:
                return (ENOTTY);
        }
        return (0);
}

int
itelread(dev_t dev, struct uio * uio, int ioflag)
{
	int             x;
	int             error = 0;
	struct itel_data *itel= &itel_data[minor(dev)];

	if((itel->state & CONNECT) == 0)
		return(EIO);

	while((itel->ilen == 0) && (itel->state & CONNECT))
	{
		itel->state |= READ_WAIT;
                sleep((caddr_t) itel->ibuf, PZERO | PCATCH);
	}

	x = splhigh();
	if(itel->ilen)
	{
		error = uiomove(itel->ibuf, itel->ilen, uio);
		itel->ilen= 0;
	} else error= EIO;
	splx(x);
	return error;
}

int
itelwrite(dev_t dev, struct uio * uio, int ioflag)
{
	int             x;
	int             error = 0;
	struct itel_data *itel= &itel_data[minor(dev)];

	if((itel->state & CONNECT) == 0)
		return(EIO);

	while(itel->olen  && (itel->state & CONNECT))
	{
		itel->state |= WRITE_WAIT;
                sleep((caddr_t) itel->obuf, PZERO | PCATCH);
	}

	x = splhigh();
	if((itel->state & CONNECT) == 0)
	{
		splx(x);
		return(0);
	}

	if(itel->olen == 0)
	{
		itel->olen= min(ITEL_SIZE, uio->uio_resid);
		error = uiomove(itel->obuf, itel->olen, uio);
		isdn_output(applnr[minor(dev)]);
	}
	splx(x);
	return error;
}

static itel_devsw_installed = 0;

static void 	itel_drvinit(void *unused)
{
	dev_t dev;

	if( ! itel_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&itel_cdevsw, NULL);
		itel_devsw_installed = 1;
    	}
}

SYSINIT(iteldev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,itel_drvinit,NULL)

#endif
