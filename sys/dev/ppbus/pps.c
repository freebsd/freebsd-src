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
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/timetc.h>
#include <sys/timepps.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/ppbus/ppbconf.h>
#include "ppbus_if.h"
#include <dev/ppbus/ppbio.h>

#define PPS_NAME	"pps"		/* our official name */

#define PRVERBOSE(arg...)	if (bootverbose) printf(##arg);

struct pps_data {
	struct	ppb_device pps_dev;	
	struct	pps_state pps[9];
	dev_t	devs[9];
	device_t ppsdev;
	device_t ppbus;
	int	busy;
	struct callout_handle timeout;
	int	lastdata;

	struct resource *intr_resource;	/* interrupt resource */
	void *intr_cookie;		/* interrupt registration cookie */
};

static void	ppsintr(void *arg);
static void 	ppshcpoll(void *arg);

#define DEVTOSOFTC(dev) \
	((struct pps_data *)device_get_softc(dev))

static devclass_t pps_devclass;

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
};

static void
ppsidentify(driver_t *driver, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, PPS_NAME, -1);
}

static int
ppstry(device_t ppbus, int send, int expect)
{
	int i;

	ppb_wdtr(ppbus, send);
	i = ppb_rdtr(ppbus);
	PRVERBOSE("S: %02x E: %02x G: %02x\n", send, expect, i);
	return (i != expect);
}

static int
ppsprobe(device_t ppsdev)
{
	device_set_desc(ppsdev, "Pulse per second Timing Interface");

	return (0);
}

static int
ppsattach(device_t dev)
{
	struct pps_data *sc = DEVTOSOFTC(dev);
	device_t ppbus = device_get_parent(dev);
	int irq, zero = 0;
	dev_t d;
	int unit, i;

	bzero(sc, sizeof(struct pps_data)); /* XXX doesn't newbus do this? */

	/* retrieve the ppbus irq */
	BUS_READ_IVAR(ppbus, dev, PPBUS_IVAR_IRQ, &irq);

	if (irq > 0) {
		/* declare our interrupt handler */
		sc->intr_resource = bus_alloc_resource(dev, SYS_RES_IRQ,
		    &zero, irq, irq, 1, RF_SHAREABLE);
	}
	/* interrupts seem mandatory */
	if (sc->intr_resource == NULL)
		return (ENXIO);

	sc->ppsdev = dev;
	sc->ppbus = ppbus;
	unit = device_get_unit(ppbus);
	d = make_dev(&pps_cdevsw, unit,
	    UID_ROOT, GID_WHEEL, 0644, PPS_NAME "%d", unit);
	sc->devs[0] = d;
	sc->pps[0].ppscap = PPS_CAPTUREASSERT | PPS_ECHOASSERT;
	d->si_drv1 = sc;
	d->si_drv2 = (void*)0;
	pps_init(&sc->pps[0]);

	if (ppb_request_bus(ppbus, dev, PPB_DONTWAIT))
		return (0);

	do {
		i = ppb_set_mode(sc->ppbus, PPB_EPP);
		PRVERBOSE("EPP: %d %d\n", i, PPB_IN_EPP_MODE(sc->ppbus));
		if (i == -1)
			break;
		i = 0;
		ppb_wctr(ppbus, i);
		if (ppstry(ppbus, 0x00, 0x00))
			break;
		if (ppstry(ppbus, 0x55, 0x55))
			break;
		if (ppstry(ppbus, 0xaa, 0xaa))
			break;
		if (ppstry(ppbus, 0xff, 0xff))
			break;

		i = IRQENABLE | PCD | STROBE | nINIT | SELECTIN;
		ppb_wctr(ppbus, i);
		PRVERBOSE("CTR = %02x (%02x)\n", ppb_rctr(ppbus), i);
		if (ppstry(ppbus, 0x00, 0x00))
			break;
		if (ppstry(ppbus, 0x55, 0x00))
			break;
		if (ppstry(ppbus, 0xaa, 0x00))
			break;
		if (ppstry(ppbus, 0xff, 0x00))
			break;

		i = IRQENABLE | PCD | nINIT | SELECTIN;
		ppb_wctr(ppbus, i);
		PRVERBOSE("CTR = %02x (%02x)\n", ppb_rctr(ppbus), i);
		ppstry(ppbus, 0x00, 0xff);
		ppstry(ppbus, 0x55, 0xff);
		ppstry(ppbus, 0xaa, 0xff);
		ppstry(ppbus, 0xff, 0xff);

		for (i = 1; i < 9; i++) {
			d = make_dev(&pps_cdevsw, unit + 0x10000 * i,
			  UID_ROOT, GID_WHEEL, 0644, PPS_NAME "%db%d", unit, i - 1);
			sc->devs[i] = d;
			sc->pps[i].ppscap = PPS_CAPTUREASSERT | PPS_CAPTURECLEAR;
			d->si_drv1 = sc;
			d->si_drv2 = (void*)i;
			pps_init(&sc->pps[i]);
		}
	} while (0);
	i = ppb_set_mode(sc->ppbus, PPB_COMPATIBLE);
	ppb_release_bus(ppbus, dev);

	return (0);
}

static	int
ppsopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct pps_data *sc = dev->si_drv1;
	int subdev = (int)dev->si_drv2;
	int error, i;

	if (!sc->busy) {
		device_t ppsdev = sc->ppsdev;
		device_t ppbus = sc->ppbus;

		if (ppb_request_bus(ppbus, ppsdev, PPB_WAIT|PPB_INTR))
			return (EINTR);

		/* attach the interrupt handler */
		if ((error = BUS_SETUP_INTR(ppbus, ppsdev, sc->intr_resource,
			       INTR_TYPE_TTY, ppsintr, ppsdev,
			       &sc->intr_cookie))) {
			ppb_release_bus(ppbus, ppsdev);
			return (error);
		}

		i = ppb_set_mode(sc->ppbus, PPB_PS2);
		PRVERBOSE("EPP: %d %d\n", i, PPB_IN_EPP_MODE(sc->ppbus));

		i = IRQENABLE | PCD | nINIT | SELECTIN;
		ppb_wctr(ppbus, i);
	} 
	if (subdev > 0 && !(sc->busy & ~1)) {
		sc->timeout = timeout(ppshcpoll, sc, 1);
		sc->lastdata = ppb_rdtr(sc->ppbus);
	}
	sc->busy |= (1 << subdev);
	return(0);
}

static	int
ppsclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct pps_data *sc = dev->si_drv1;
	int subdev = (int)dev->si_drv2;

	sc->pps[subdev].ppsparam.mode = 0;	/* PHK ??? */
	sc->busy &= ~(1 << subdev);
	if (subdev > 0 && !(sc->busy & ~1)) 
		untimeout(ppshcpoll, sc, sc->timeout);
	if (!sc->busy) {
		device_t ppsdev = sc->ppsdev;
		device_t ppbus = sc->ppbus;

		ppb_wdtr(ppbus, 0);
		ppb_wctr(ppbus, 0);

		/* Note: the interrupt handler is automatically detached */
		ppb_set_mode(ppbus, PPB_COMPATIBLE);
		ppb_release_bus(ppbus, ppsdev);
	}
	return(0);
}

static void
ppshcpoll(void *arg)
{
	struct pps_data *sc = arg;
	int i, j, k, l;
	struct timecounter *tc;
	unsigned count;

	if (!(sc->busy & ~1))
		return;
	sc->timeout = timeout(ppshcpoll, sc, 1);
	i = ppb_rdtr(sc->ppbus);
	if (i == sc->lastdata) 
		return;
	tc = timecounter;
	count = timecounter->tc_get_timecount(tc);
	l = sc->lastdata ^ i;
	k = 1;
	for (j = 1; j < 9; j ++) {
		if (l & k) 
			pps_event(&sc->pps[j], tc, count, 
			    i & k ?
			    PPS_CAPTUREASSERT : PPS_CAPTURECLEAR
			);
		k += k;
	}
	sc->lastdata = i;
}

static void
ppsintr(void *arg)
{
	device_t ppsdev = (device_t)arg;
	struct pps_data *sc = DEVTOSOFTC(ppsdev);
	device_t ppbus = sc->ppbus;
	struct timecounter *tc;
	unsigned count;

	tc = timecounter;
	count = timecounter->tc_get_timecount(tc);
	if (!(ppb_rstr(ppbus) & nACK))
		return;
	if (sc->pps[0].ppsparam.mode & PPS_ECHOASSERT) 
		ppb_wctr(ppbus, IRQENABLE | AUTOFEED);
	pps_event(&sc->pps[0], tc, count, PPS_CAPTUREASSERT);
	if (sc->pps[0].ppsparam.mode & PPS_ECHOASSERT) 
		ppb_wctr(ppbus, IRQENABLE);
}

static int
ppsioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	struct pps_data *sc = dev->si_drv1;
	int subdev = (int)dev->si_drv2;

	return (pps_ioctl(cmd, data, &sc->pps[subdev]));
}

static device_method_t pps_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,	ppsidentify),
	DEVMETHOD(device_probe,		ppsprobe),
	DEVMETHOD(device_attach,	ppsattach),

	{ 0, 0 }
};

static driver_t pps_driver = {
	PPS_NAME,
	pps_methods,
	sizeof(struct pps_data),
};
DRIVER_MODULE(pps, ppbus, pps_driver, pps_devclass, 0, 0);
