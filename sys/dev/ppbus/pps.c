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

struct pps_data {
	int	pps_open;
	struct	ppb_device pps_dev;	
	struct	pps_state pps;

	struct resource *intr_resource;	/* interrupt resource */
	void *intr_cookie;		/* interrupt registration cookie */
};

static void	ppsintr(void *arg);

#define DEVTOSOFTC(dev) \
	((struct pps_data *)device_get_softc(dev))
#define UNITOSOFTC(unit) \
	((struct pps_data *)devclass_get_softc(pps_devclass, (unit)))
#define UNITODEVICE(unit) \
	(devclass_get_device(pps_devclass, (unit)))

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

	BUS_ADD_CHILD(parent, 0, PPS_NAME, 0);
}

static int
ppsprobe(device_t ppsdev)
{
	struct pps_data *sc;
	dev_t dev;
	int unit;

	sc = DEVTOSOFTC(ppsdev);
	bzero(sc, sizeof(struct pps_data));

	unit = device_get_unit(ppsdev);
	dev = make_dev(&pps_cdevsw, unit,
	    UID_ROOT, GID_WHEEL, 0644, PPS_NAME "%d", unit);

	device_set_desc(ppsdev, "Pulse per second Timing Interface");

	sc->pps.ppscap = PPS_CAPTUREASSERT | PPS_ECHOASSERT;
	pps_init(&sc->pps);
	return (0);
}

static int
ppsattach(device_t dev)
{
	struct pps_data *sc = DEVTOSOFTC(dev);
	device_t ppbus = device_get_parent(dev);
	int irq, zero = 0;

	/* retrieve the ppbus irq */
	BUS_READ_IVAR(ppbus, dev, PPBUS_IVAR_IRQ, &irq);

	if (irq > 0) {
		/* declare our interrupt handler */
		sc->intr_resource = bus_alloc_resource(dev, SYS_RES_IRQ,
				       &zero, irq, irq, 1, RF_SHAREABLE);
	}
	/* interrupts seem mandatory */
	if (sc->intr_resource == 0)
		return (ENXIO);

	return (0);
}

static	int
ppsopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	u_int unit = minor(dev);
	struct pps_data *sc = UNITOSOFTC(unit);
	device_t ppsdev = UNITODEVICE(unit);
	device_t ppbus = device_get_parent(ppsdev);
	int error;

	if (!sc->pps_open) {
		if (ppb_request_bus(ppbus, ppsdev, PPB_WAIT|PPB_INTR))
			return (EINTR);

		/* attach the interrupt handler */
		if ((error = BUS_SETUP_INTR(ppbus, ppsdev, sc->intr_resource,
			       INTR_TYPE_TTY, ppsintr, ppsdev,
			       &sc->intr_cookie))) {
			ppb_release_bus(ppbus, ppsdev);
			return (error);
		}

		ppb_wctr(ppbus, 0);
		ppb_wctr(ppbus, IRQENABLE);
		sc->pps_open = 1;
	}

	return(0);
}

static	int
ppsclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	u_int unit = minor(dev);
	struct pps_data *sc = UNITOSOFTC(unit);
	device_t ppsdev = UNITODEVICE(unit);
	device_t ppbus = device_get_parent(ppsdev);

	sc->pps.ppsparam.mode = 0;	/* PHK ??? */

	ppb_wdtr(ppbus, 0);
	ppb_wctr(ppbus, 0);

	/* Note: the interrupt handler is automatically detached */
	ppb_release_bus(ppbus, ppsdev);
	sc->pps_open = 0;
	return(0);
}

static void
ppsintr(void *arg)
{
	device_t ppsdev = (device_t)arg;
	device_t ppbus = device_get_parent(ppsdev);
	struct pps_data *sc = DEVTOSOFTC(ppsdev);
	struct timecounter *tc;
	unsigned count;

	tc = timecounter;
	count = timecounter->tc_get_timecount(tc);
	if (!(ppb_rstr(ppbus) & nACK))
		return;
	if (sc->pps.ppsparam.mode & PPS_ECHOASSERT) 
		ppb_wctr(ppbus, IRQENABLE | AUTOFEED);
	pps_event(&sc->pps, tc, count, PPS_CAPTUREASSERT);
	if (sc->pps.ppsparam.mode & PPS_ECHOASSERT) 
		ppb_wctr(ppbus, IRQENABLE);
}

static int
ppsioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	u_int unit = minor(dev);
	struct pps_data *sc = UNITOSOFTC(unit);

	return (pps_ioctl(cmd, data, &sc->pps));
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
