/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: pps.c,v 1.14 1999/03/11 15:09:51 phk Exp $
 *
 * This driver implements a draft-mogul-pps-api-02.txt PPS source.
 *
 * The input pin is pin#10 
 * The echo output pin is pin#14
 *
 */

#include "opt_devfs.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/timepps.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <sys/malloc.h>

#include <dev/ppbus/ppbconf.h>
#include "pps.h"

#define PPS_NAME	"lppps"		/* our official name */

static struct pps_data {
	int	pps_unit;
	struct	ppb_device pps_dev;	
	struct	pps_state pps;
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
static	d_ioctl_t	ppsioctl;

#define CDEV_MAJOR 89
static struct cdevsw pps_cdevsw = 
	{ ppsopen,	ppsclose,	noread,		nowrite,
	  ppsioctl,	nullstop,	nullreset,	nodevtotty,
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
	sc->pps_dev.name = ppsdriver.name;
	sc->pps_dev.intr = ppsintr;

	sc->pps.ppscap = PPS_CAPTUREASSERT | PPS_ECHOASSERT;
	pps_init(&sc->pps);
	return (&sc->pps_dev);
}

static int
ppsattach(struct ppb_device *dev)
{
	dev_t devt;

	/*
	 * Report ourselves
	 */
	printf(PPS_NAME "%d: <Pulse per second Timing Interface> on ppbus %d\n",
	       dev->id_unit, dev->ppb->ppb_link->adapter_unit);

#ifdef DEVFS
	devfs_add_devswf(&pps_cdevsw,
		dev->id_unit, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, PPS_NAME "%d", dev->id_unit);
#endif
	devt = makedev(CDEV_MAJOR, 0);
	cdevsw_add(&devt, &pps_cdevsw, NULL);
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

	ppb_wctr(&sc->pps_dev, 0);
	ppb_wctr(&sc->pps_dev, IRQENABLE);

	return(0);
}

static	int
ppsclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct pps_data *sc = softc[minor(dev)];

	sc->pps.ppsparam.mode = 0;	/* PHK ??? */

	ppb_wdtr(&sc->pps_dev, 0);
	ppb_wctr(&sc->pps_dev, 0);

	ppb_release_bus(&sc->pps_dev);
	return(0);
}

static void
ppsintr(int unit)
{
	struct pps_data *sc = softc[unit];
	struct timecounter *tc;
	unsigned count;

	tc = timecounter;
	count = timecounter->tc_get_timecount(tc);
	if (!(ppb_rstr(&sc->pps_dev) & nACK))
		return;
	if (sc->pps.ppsparam.mode & PPS_ECHOASSERT) 
		ppb_wctr(&sc->pps_dev, IRQENABLE | AUTOFEED);
	pps_event(&sc->pps, tc, count, PPS_CAPTUREASSERT);
	if (sc->pps.ppsparam.mode & PPS_ECHOASSERT) 
		ppb_wctr(&sc->pps_dev, IRQENABLE);
}

static int
ppsioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct pps_data *sc = softc[minor(dev)];

	return (pps_ioctl(cmd, data, &sc->pps));
}

