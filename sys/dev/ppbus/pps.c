/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: pps.c,v 1.2 1998/02/13 17:35:33 phk Exp $
 *
 */

#include "opt_devfs.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <sys/malloc.h>

#include <dev/ppbus/ppbconf.h>
#include "pps.h"

#define PPS_NAME	"pps"		/* our official name */

static struct pps_data {
	int	pps_unit;
	struct	ppb_device pps_dev;	
	struct  ppsclockev {
		struct	timespec timestamp;
		u_int	serial;
	} ev;
	int	sawtooth;
} *softc[NPPS];

static int npps;

/*
 * Make ourselves visible as a ppbus driver
 */

static struct ppb_device	*ppsprobe(struct ppb_data *ppb);
static int			ppsattach(struct ppb_device *dev);
static void			ppsintr(int unit);
static void			pps_drvinit(void *unused);

static struct ppb_driver ppsdriver = {
    ppsprobe, ppsattach, PPS_NAME
};

DATA_SET(ppbdriver_set, ppsdriver);

static	d_open_t	ppsopen;
static	d_close_t	ppsclose;
static	d_read_t	ppsread;
static	d_write_t	ppswrite;

#define CDEV_MAJOR 89
static struct cdevsw pps_cdevsw = 
	{ ppsopen,	ppsclose,	ppsread,	ppswrite,
	  noioctl,	nullstop,	nullreset,	nodevtotty,
	  seltrue,	nommap,		nostrat,	PPS_NAME,
	  NULL,		-1 };

static struct ppb_device *
ppsprobe(struct ppb_data *ppb)
{
	struct pps_data *sc;

	sc = (struct pps_data *) malloc(sizeof(struct pps_data),
							M_TEMP, M_NOWAIT);
	if (!sc) {
		printf(PPS_NAME ": cannot malloc!\n");
		return (0);
	}
	bzero(sc, sizeof(struct pps_data));

	softc[npps] = sc;

	sc->pps_unit = npps++;

	sc->pps_dev.id_unit = sc->pps_unit;
	sc->pps_dev.ppb = ppb;
	sc->pps_dev.intr = ppsintr;

	return (&sc->pps_dev);
}

static int
ppsattach(struct ppb_device *dev)
{
	/*
	 * Report ourselves
	 */
	printf(PPS_NAME "%d: <Pulse per second Timing Interface> on ppbus %d\n",
	       dev->id_unit, dev->ppb->ppb_link->adapter_unit);

#ifdef DEVFS
	devfs_add_devswf(&pps_cdevsw,
		dev->id_unit, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, PPS_NAME "%d", dev->id_unit);
	devfs_add_devswf(&pps_cdevsw,
		dev->id_unit | LP_BYPASS, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, PPS_NAME "%d.ctl", dev->id_unit);
#endif

	return (1);
}

static	int
ppsopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct pps_data *sc;
	u_int unit = minor(dev);

	if ((unit >= npps))
		return (ENXIO);

	sc = softc[unit];

	if (ppb_request_bus(&sc->pps_dev, PPB_WAIT|PPB_INTR))
		return (EINTR);

	ppb_wctr(&sc->pps_dev, 0x10);

	return(0);
}

static	int
ppsclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct pps_data *sc = softc[minor(dev)];

	ppb_release_bus(&sc->pps_dev);
	return(0);
}

static void
ppsintr(int unit)
{
/*
 * XXX: You want to thing carefully about what you actually want to do
 * here.
 */
#if 1
	struct pps_data *sc = softc[unit];
	struct timespec tc;
#if 1
	struct timeval tv;
#endif

	nanotime(&tc);
	if (!(ppb_rstr(&sc->pps_dev) & nACK))
		return;
	tc.tv_nsec -= sc->sawtooth;
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
ppsread(dev_t dev, struct uio *uio, int ioflag)
{
	struct pps_data *sc = softc[minor(dev)];
	int err, c;

	c = imin(uio->uio_resid, (int)sizeof sc->ev);
	err = uiomove((caddr_t)&sc->ev, c, uio);	
	return(err);
}

static	int
ppswrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct pps_data *sc = softc[minor(dev)];
	int err, c;

	c = imin(uio->uio_resid, (int)sizeof sc->sawtooth);
	err = uiomove((caddr_t)&sc->sawtooth, c, uio);	
	return(err);
}

static pps_devsw_installed = 0;

static void
pps_drvinit(void *unused)
{
	dev_t dev;

	if( ! pps_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev, &pps_cdevsw, NULL);
		pps_devsw_installed = 1;
    	}
}

SYSINIT(ppsdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,pps_drvinit,NULL)
