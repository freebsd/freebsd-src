/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include "opt_bus.h"
#include "opt_pci.h"
#include "opt_smp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bus.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

static unsigned piix_get_timecount(struct timecounter *tc);

static u_int32_t piix_timecounter_address;

static struct timecounter piix_timecounter = {
	piix_get_timecount,
	0,
	0xffffff,
	14318182/4,
	"PIIX"
};

SYSCTL_OPAQUE(_debug, OID_AUTO, piix_timecounter, CTLFLAG_RD,
	&piix_timecounter, sizeof(piix_timecounter), "S,timecounter", "");

static unsigned
piix_get_timecount(struct timecounter *tc)
{
	return (inl(piix_timecounter_address));
}

static int
piix_probe (device_t dev)
{
	u_int32_t	d;

	switch (pci_get_devid(dev)) {
	case 0x71138086:
		d = pci_read_config(dev, 0x4, 2);
		if (!(d & 1))
			return 0;	/* IO space not mapped */
		d = pci_read_config(dev, 0x40, 4);
		piix_timecounter_address = (d & 0xffc0) + 8;
		init_timecounter(&piix_timecounter);
		return (ENXIO);
	};
	return (ENXIO);
}

static int
piix_attach (device_t dev)
{
	
	return 0;
}

static device_method_t piix_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		piix_probe),
	DEVMETHOD(device_attach,	piix_attach),
	{ 0, 0 }
};

static driver_t piix_driver = {
	"piix",
	piix_methods,
	1,
};

static devclass_t piix_devclass;

DRIVER_MODULE(piix, pci, piix_driver, piix_devclass, 0, 0);
