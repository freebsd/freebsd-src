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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include "pci_if.h"

static MALLOC_DEFINE(M_MUSYCC, "musycc", "MUSYCC related");

/*
 * Device driver initialization stuff
 */

static devclass_t musycc_devclass;

#define dev2unit(devt) (minor(devt) & 0xff)
#define dev2pps(devt) ((minor(devt) >> 16)-1)

struct softc {
	int	unit, bus, slot;
	device_t f0, f1;

	vm_offset_t vir0base, phys0base;
	vm_offset_t vir1base, phys1base;
	LIST_ENTRY(softc) list;
};

static LIST_HEAD(, softc) sc_list = LIST_HEAD_INITIALIZER(&sc_list);


/*
 * PCI initialization stuff
 */

static int
musycc_probe(device_t self)
{
	char desc[40];

	switch (pci_get_devid(self)) {
	case 0x8471109e: strcpy(desc, "CN8471 MUSYCC"); break;
	case 0x8472109e: strcpy(desc, "CN8472 MUSYCC"); break;
	case 0x8474109e: strcpy(desc, "CN8474 MUSYCC"); break;
	case 0x8478109e: strcpy(desc, "CN8478 MUSYCC"); break;
	default:
		return (ENXIO);
	}

	switch (pci_get_function(self)) {
	case 0: strcat(desc, " Network controller"); break;
	case 1: strcat(desc, " Ebus bridge"); break;
	default:
		return (ENXIO);
	}

	device_set_desc_copy(self, desc);
	return 0;
}

static int
musycc_attach(device_t self)
{
	struct softc *sc;
	struct resource *res;
	int rid;
	u_int32_t	*u32p;

#if 0
	printf("subvendor      %04x\n", pci_get_subvendor(self));
	printf("subdevice      %04x\n", pci_get_subdevice(self));
	printf("devid          %08x\n", pci_get_devid(self));
	printf("class          %02x\n", pci_get_class(self));
	printf("subclass       %02x\n", pci_get_subclass(self));
	printf("progif         %02x\n", pci_get_progif(self));
	printf("revid          %02x\n", pci_get_revid(self));
	printf("bus            %02x\n", pci_get_bus(self));
	printf("slot           %02x\n", pci_get_slot(self));
	printf("function       %02x\n", pci_get_function(self));
	printf("secondarybus   %02x\n", pci_get_secondarybus(self));
	printf("subordinatebus %02x\n", pci_get_subordinatebus(self));
	printf("hose           %08x\n", pci_get_hose(self));
#endif

	/* For function zero allocate a softc */
	if (pci_get_function(self) == 0) {
		MALLOC(sc, struct softc *, sizeof(*sc), M_MUSYCC, M_WAITOK);
		bzero(sc, sizeof(*sc));
		sc->bus = pci_get_bus(self);
		sc->slot = pci_get_slot(self);
		sc->f0 = self;
		device_set_softc(self, sc);
		rid = PCIR_MAPS;
		res = bus_alloc_resource(self, SYS_RES_MEMORY, &rid,
					 0, ~0, 1, RF_ACTIVE);
		if (res == NULL) {
			device_printf(self, "Could not map memory\n");
			return ENXIO;
		}
		sc->vir0base = (vm_offset_t)rman_get_virtual(res);
		sc->phys0base = rman_get_start(res);
		LIST_INSERT_HEAD(&sc_list, sc, list);
		return (0);
	}

	/* ... and have function one match it up */
	LIST_FOREACH(sc, &sc_list, list) {
		if (sc->bus != pci_get_bus(self))
			continue;
		if (sc->slot != pci_get_slot(self))
			continue;
		break;
	}
	sc->f1 = self;
	device_set_softc(self, sc);
	rid = PCIR_MAPS;
	res = bus_alloc_resource(self, SYS_RES_MEMORY, &rid,
				 0, ~0, 1, RF_ACTIVE);
	if (res == NULL) {
		device_printf(self, "Could not map memory\n");
		return ENXIO;
	}
	sc->vir1base = (vm_offset_t)rman_get_virtual(res);
	sc->phys1base = rman_get_start(res);

	printf("f0: %p %08x %08x\n", sc->f0, sc->vir0base, sc->phys0base);
	printf("f1: %p %08x %08x\n", sc->f1, sc->vir1base, sc->phys1base);

	u32p = (u_int32_t *)sc->vir0base;
	u32p[0x180] = 0x3f30;
	u32p = (u_int32_t *)sc->vir1base;
	if ((u32p[0x1200] & 0xffffff00) != 0x13760400) {
		printf("Not a LMC1504 (ID is 0x%08x).  Bailing out.\n",
		    u32p[0x1200]);
		return(ENXIO);
	}
	printf("Found <LanMedia LMC1504>\n");
	return 0;
}

static device_method_t musycc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		musycc_probe),
	DEVMETHOD(device_attach,	musycc_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	{0, 0}
};
 
static driver_t musycc_driver = {
	"musycc",
	musycc_methods,
	0
};

DRIVER_MODULE(musycc, pci, musycc_driver, musycc_devclass, 0, 0);
