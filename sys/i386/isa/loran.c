/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: loran.c,v 1.1 1998/02/24 22:05:12 phk Exp $
 *
 * This device-driver helps the userland controlprogram for a LORAN-C
 * receiver avoid monopolizing the CPU.
 *
 * This is clearly a candidate for the "most weird hardware support in
 * FreeBSD" prize.  At this time only two copies of the receiver are
 * known to exist in the entire world.
 *
 * Details can be found at:
 *     ftp://ftp.eecis.udel.edu/pub/ntp/loran.tar.Z
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/uio.h>

#include <i386/isa/isa_device.h>

static int	loranprobe (struct isa_device *dvp);
static int	loranattach (struct isa_device *isdp);

struct	isa_driver lorandriver = {
	loranprobe, loranattach, "loran"
};

struct timespec loran_token;

static	d_open_t	loranopen;
static	d_close_t	loranclose;
static	d_read_t	loranread;

#define CDEV_MAJOR 94
static struct cdevsw loran_cdevsw = 
	{ loranopen,	loranclose,	loranread,	nowrite,
	  noioctl,	nullstop,	nullreset,	nodevtotty,
	  seltrue,	nommap,		nostrat,	"loran",
	  NULL,		-1 };


int
loranprobe(struct isa_device *dvp)
{
	dvp->id_iobase = 0x300;
	return (8);
}

int
loranattach(struct isa_device *isdp)
{
	printf("loran0: LORAN-C Receiver\n");
	return (1);
}

static	int
loranopen (dev_t dev, int flags, int fmt, struct proc *p)
{

	return(0);
}

static	int
loranclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	
	return(0);
}

static	int
loranread(dev_t dev, struct uio * uio, int ioflag)
{
        int err, c;

	tsleep ((caddr_t)&loran_token, PZERO + 8 |PCATCH, "loranrd", hz*10);
        c = imin(uio->uio_resid, (int)sizeof loran_token);
        err = uiomove((caddr_t)&loran_token, c, uio);        
	return(err);
}

void
loranintr(int unit)
{
	nanotime(&loran_token);
	wakeup((caddr_t)&loran_token);
}

static loran_devsw_installed = 0;

static void 	loran_drvinit(void *unused)
{
	dev_t dev;

	if( ! loran_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&loran_cdevsw, NULL);
		loran_devsw_installed = 1;
    	}
}

SYSINIT(lorandev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,loran_drvinit,NULL)

