/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#include <sys/malloc.h>

#include <machine/clock.h>
#include <machine/lpt.h>

#include <dev/ppbus/ppbconf.h>
#include "ppps.h"

#define PPPPS_NAME	"pps"		/* our official name */

static struct ppps_data {
	int	ppps_unit;
	struct	ppb_device ppps_dev;	
	struct  ppsclockev {
		struct	timespec timestamp;
		u_int	serial;
	} ev;
	int	sawtooth;
} *softc[NPPPPS];

static int nppps;
static int sawtooth;

/*
 * Make ourselves visible as a ppbus driver
 */

static struct ppb_device	*pppsprobe(struct ppb_data *ppb);
static int			pppsattach(struct ppb_device *dev);
static void			pppsintr(int unit);
static void			ppps_drvinit(void *unused);

static struct ppb_driver pppsdriver = {
    pppsprobe, pppsattach, PPPPS_NAME
};

DATA_SET(ppbdriver_set, pppsdriver);

static	d_open_t	pppsopen;
static	d_close_t	pppsclose;
static	d_read_t	pppsread;
static	d_write_t	pppswrite;

#define CDEV_MAJOR 89
static struct cdevsw ppps_cdevsw = 
	{ pppsopen,	pppsclose,	pppsread,	pppswrite,
	  noioctl,	nullstop,	nullreset,	nodevtotty,
	  seltrue,	nommap,		nostrat,	PPPPS_NAME,
	  NULL,		-1 };

static struct ppb_device *
pppsprobe(struct ppb_data *ppb)
{
	struct ppps_data *sc;

	sc = (struct ppps_data *) malloc(sizeof(struct ppps_data),
							M_TEMP, M_NOWAIT);
	if (!sc) {
		printf(PPPPS_NAME ": cannot malloc!\n");
		return (0);
	}
	bzero(sc, sizeof(struct ppps_data));

	softc[nppps] = sc;

	sc->ppps_unit = nppps++;

	sc->ppps_dev.id_unit = sc->ppps_unit;
	sc->ppps_dev.ppb = ppb;
	sc->ppps_dev.intr = pppsintr;

	return (&sc->ppps_dev);
}

static int
pppsattach(struct ppb_device *dev)
{
	/*
	 * Report ourselves
	 */
	printf(PPPPS_NAME "%d: <Pulse per second Timing Interface> on ppbus %d\n",
	       dev->id_unit, dev->ppb->ppb_link->adapter_unit);

#ifdef DEVFS
	sc->devfs_token = devfs_add_devswf(&ppps_cdevsw,
		dev->id_unit, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, PPPPS_NAME "%d", dev->id_unit);
	sc->devfs_token_ctl = devfs_add_devswf(&ppps_cdevsw,
		dev->id_unit | LP_BYPASS, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, PPPPS_NAME "%d.ctl", dev->id_unit);
#endif

	return (1);
}

static	int
pppsopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct ppps_data *sc;
	u_int unit = minor(dev);

	if ((unit >= nppps))
		return (ENXIO);

	sc = softc[unit];

	if (ppb_request_bus(&sc->ppps_dev, PPB_WAIT|PPB_INTR))
		return (EINTR);

	ppb_wctr(&sc->ppps_dev, 0x10);

	return(0);
}

static	int
pppsclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct ppps_data *sc = softc[minor(dev)];

	ppb_release_bus(&sc->ppps_dev);
	return(0);
}

static void
pppsintr(int unit)
{
/*
 * XXX: You want to thing carefully about what you actually want to do
 * here.
 */
#if 0
	struct ppps_data *sc = softc[unit];
	struct timespec tc;
#if 1
	struct timeval tv;
#endif

	nanotime(&tc);
	if (!(ppb_rstr(&sc->ppps_dev) & nACK))
		return;
	tc.tv_nsec -= sc->sawtooth;
	tc.tv_nsec += 10000;
	sc->sawtooth = 0;
	if (tc.tv_nsec > 1000000000) {
		tc.tv_sec++;
		tc.tv_nsec -= 1000000000;
	} else if (tc.tv_nsec < 0) {
		tc.tv_sec--;
		tc.tv_nsec += 1000000000;
	}
	sc->ev.timestamp = tc;
	sc->ev.serial++;
#if 1
	tv.tv_sec = tc.tv_sec;
	tv.tv_usec = tc.tv_nsec / 1000;
	hardpps(&tv, tv.tv_usec);
#endif
#endif
}

static	int
pppsread(dev_t dev, struct uio *uio, int ioflag)
{
	struct ppps_data *sc = softc[minor(dev)];
	int err, c;

	c = imin(uio->uio_resid, sizeof sc->ev);
	err = uiomove(&sc->ev, c, uio);	
	return(err);
}

static	int
pppswrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct ppps_data *sc = softc[minor(dev)];
	int err, c;

	c = imin(uio->uio_resid, sizeof sc->sawtooth);
	err = uiomove(&sc->sawtooth, c, uio);	
	return(err);
}



static ppps_devsw_installed = 0;

static void
ppps_drvinit(void *unused)
{
	dev_t dev;

	if( ! ppps_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&ppps_cdevsw, NULL);
		ppps_devsw_installed = 1;
    	}
}

SYSINIT(pppsdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ppps_drvinit,NULL)
