/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 * This driver implements a draft-mogul-pps-api-02.txt PPS source.
 *
 * The input pin is pin#10 
 * The echo output pin is pin#14
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/timepps.h>
#include <sys/malloc.h>

#include <dev/ppbus/ppbconf.h>
#include "pps.h"

#define PPS_NAME	"pps"		/* our official name */

struct pps_data {
	int	pps_open;
	struct	ppb_device pps_dev;	
	struct	pps_state pps;
};

static int npps;

/*
 * Make ourselves visible as a ppbus driver
 */

static struct ppb_device	*ppsprobe(struct ppb_data *ppb);
static int			ppsattach(struct ppb_device *dev);
static void			ppsintr(struct ppb_device *ppd);

static struct ppb_driver ppsdriver = {
    ppsprobe, ppsattach, PPS_NAME
};

DATA_SET(ppbdriver_set, ppsdriver);

static	d_open_t	ppsopen;
static	d_close_t	ppsclose;
static	d_ioctl_t	ppsioctl;

#define CDEV_MAJOR 89
static struct cdevsw pps_cdevsw = {
	/* open */	ppsopen,
	/* close */	ppsclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	ppsioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	PPS_NAME,
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};


static struct ppb_device *
ppsprobe(struct ppb_data *ppb)
{
	struct pps_data *sc;
	static int once;
	dev_t dev;

	if (!once++)
		cdevsw_add(&pps_cdevsw);

	sc = (struct pps_data *) malloc(sizeof(struct pps_data),
							M_TEMP, M_NOWAIT);
	if (!sc) {
		printf(PPS_NAME ": cannot malloc!\n");
		return (0);
	}
	bzero(sc, sizeof(struct pps_data));

	dev = make_dev(&pps_cdevsw, npps,
	    UID_ROOT, GID_WHEEL, 0644, PPS_NAME "%d", npps);

	dev->si_drv1 = sc;

	sc->pps_dev.id_unit = npps++;
	sc->pps_dev.ppb = ppb;
	sc->pps_dev.name = ppsdriver.name;
	sc->pps_dev.bintr = ppsintr;
	sc->pps_dev.drv1 = sc;

	sc->pps.ppscap = PPS_CAPTUREASSERT | PPS_ECHOASSERT;
	pps_init(&sc->pps);
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

	return (1);
}

static	int
ppsopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct pps_data *sc;
	u_int unit = minor(dev);

	if ((unit >= npps))
		return (ENXIO);

	sc = dev->si_drv1;

	if (!sc->pps_open) {
		if (ppb_request_bus(&sc->pps_dev, PPB_WAIT|PPB_INTR))
			return (EINTR);

		ppb_wctr(&sc->pps_dev, 0);
		ppb_wctr(&sc->pps_dev, IRQENABLE);
		sc->pps_open = 1;
	}

	return(0);
}

static	int
ppsclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct pps_data *sc = dev->si_drv1;

	sc->pps.ppsparam.mode = 0;	/* PHK ??? */

	ppb_wdtr(&sc->pps_dev, 0);
	ppb_wctr(&sc->pps_dev, 0);

	ppb_release_bus(&sc->pps_dev);
	sc->pps_open = 0;
	return(0);
}

static void
ppsintr(struct ppb_device *ppd)
{
	struct pps_data *sc = ppd->drv1;
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
	struct pps_data *sc = dev->si_drv1;

	return (pps_ioctl(cmd, data, &sc->pps));
}

