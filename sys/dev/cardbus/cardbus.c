/*
 * Copyright (c) 2003 M. Warner Losh.  All Rights Reserved.
 * Copyright (c) 2000,2001 Jonathan Chen.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <sys/pciio.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_private.h>

#include <dev/cardbus/cardbusreg.h>
#include <dev/cardbus/cardbusvar.h>
#include <dev/cardbus/cardbus_cis.h>
#include <dev/pccard/pccardvar.h>

#include "power_if.h"
#include "pcib_if.h"

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, cardbus, CTLFLAG_RD, 0, "CardBus parameters");

int    cardbus_debug = 0;
TUNABLE_INT("hw.cardbus.debug", &cardbus_debug);
SYSCTL_INT(_hw_cardbus, OID_AUTO, debug, CTLFLAG_RW,
    &cardbus_debug, 0,
  "CardBus debug");

int    cardbus_cis_debug = 0;
TUNABLE_INT("hw.cardbus.cis_debug", &cardbus_cis_debug);
SYSCTL_INT(_hw_cardbus, OID_AUTO, cis_debug, CTLFLAG_RW,
    &cardbus_cis_debug, 0,
  "CardBus CIS debug");

#define	DPRINTF(a) if (cardbus_debug) printf a
#define	DEVPRINTF(x) if (cardbus_debug) device_printf x


static int	cardbus_attach(device_t cbdev);
static int	cardbus_attach_card(device_t cbdev);
static int	cardbus_detach(device_t cbdev);
static int	cardbus_detach_card(device_t cbdev);
static void	cardbus_device_setup_regs(device_t brdev, int b, int s, int f,
		    pcicfgregs *cfg);
static void	cardbus_driver_added(device_t cbdev, driver_t *driver);
static int	cardbus_probe(device_t cbdev);
static int	cardbus_read_ivar(device_t cbdev, device_t child, int which,
		    uintptr_t *result);
static void	cardbus_release_all_resources(device_t cbdev,
		    struct cardbus_devinfo *dinfo);
static int	cardbus_write_ivar(device_t cbdev, device_t child, int which,
		    uintptr_t value);

/************************************************************************/
/* Probe/Attach								*/
/************************************************************************/

static int
cardbus_probe(device_t cbdev)
{
	device_set_desc(cbdev, "CardBus bus");
	return 0;
}

static int
cardbus_attach(device_t cbdev)
{
	return 0;
}

static int
cardbus_detach(device_t cbdev)
{
	cardbus_detach_card(cbdev);
	return 0;
}

static int
cardbus_suspend(device_t self)
{
	cardbus_detach_card(self);
	return (0);
}

static int
cardbus_resume(device_t self)
{
	return (0);
}

/************************************************************************/
/* Attach/Detach card							*/
/************************************************************************/

static void
cardbus_device_setup_regs(device_t brdev, int b, int s, int f, pcicfgregs *cfg)
{
	PCIB_WRITE_CONFIG(brdev, b, s, f, PCIR_INTLINE,
	    pci_get_irq(device_get_parent(brdev)), 1);
	cfg->intline = PCIB_READ_CONFIG(brdev, b, s, f, PCIR_INTLINE, 1);

	PCIB_WRITE_CONFIG(brdev, b, s, f, PCIR_CACHELNSZ, 0x08, 1);
	cfg->cachelnsz = PCIB_READ_CONFIG(brdev, b, s, f, PCIR_CACHELNSZ, 1);

	PCIB_WRITE_CONFIG(brdev, b, s, f, PCIR_LATTIMER, 0xa8, 1);
	cfg->lattimer = PCIB_READ_CONFIG(brdev, b, s, f, PCIR_LATTIMER, 1);

	PCIB_WRITE_CONFIG(brdev, b, s, f, PCIR_MINGNT, 0x14, 1);
	cfg->mingnt = PCIB_READ_CONFIG(brdev, b, s, f, PCIR_MINGNT, 1);

	PCIB_WRITE_CONFIG(brdev, b, s, f, PCIR_MAXLAT, 0x14, 1);
	cfg->maxlat = PCIB_READ_CONFIG(brdev, b, s, f, PCIR_MAXLAT, 1);
}

static int
cardbus_attach_card(device_t cbdev)
{
	device_t brdev = device_get_parent(cbdev);
	int cardattached = 0;
	static int curr_bus_number = 2; /* XXX EVILE BAD (see below) */
	int bus, slot, func;

	cardbus_detach_card(cbdev); /* detach existing cards */

	POWER_ENABLE_SOCKET(brdev, cbdev);
	bus = pcib_get_bus(cbdev);
	if (bus == 0) {
		/*
		 * XXX EVILE BAD XXX
		 * Not all BIOSes initialize the secondary bus number properly,
		 * so if the default is bad, we just put one in and hope it
		 * works.
		 */
		bus = curr_bus_number;
		pci_write_config(brdev, PCIR_SECBUS_2, curr_bus_number, 1);
		pci_write_config(brdev, PCIR_SUBBUS_2, curr_bus_number + 2, 1);
		curr_bus_number += 3;
	}
	/* For each function, set it up and try to attach a driver to it */
	for (slot = 0; slot <= CARDBUS_SLOTMAX; slot++) {
		int cardbusfunchigh = 0;
		for (func = 0; func <= cardbusfunchigh; func++) {
			struct cardbus_devinfo *dinfo;

			dinfo = (struct cardbus_devinfo *)
			    pci_read_device(brdev, bus, slot, func,
				sizeof(struct cardbus_devinfo));
			if (dinfo == NULL)
				continue;
			if (dinfo->pci.cfg.mfdev)
				cardbusfunchigh = CARDBUS_FUNCMAX;

			cardbus_device_setup_regs(brdev, bus, slot, func,
			    &dinfo->pci.cfg);
			dinfo->pci.cfg.dev = device_add_child(cbdev, NULL, -1);
			if (!dinfo->pci.cfg.dev) {
				DEVPRINTF((cbdev, "Cannot add child!\n"));
				pci_freecfg((struct pci_devinfo *)dinfo);
				continue;
			}
			resource_list_init(&dinfo->pci.resources);
			device_set_ivars(dinfo->pci.cfg.dev, dinfo);
			cardbus_do_cis(cbdev, dinfo->pci.cfg.dev);
			pci_print_verbose(&dinfo->pci);
			if (device_probe_and_attach(dinfo->pci.cfg.dev) != 0)
				cardbus_release_all_resources(cbdev, dinfo);
			else
				cardattached++;
		}
	}

	if (cardattached > 0)
		return (0);
	POWER_DISABLE_SOCKET(brdev, cbdev);
	return (ENOENT);
}

static int
cardbus_detach_card(device_t cbdev)
{
	int numdevs;
	device_t *devlist;
	int tmp;
	int err = 0;

	device_get_children(cbdev, &devlist, &numdevs);

	if (numdevs == 0) {
		free(devlist, M_TEMP);
		return (ENOENT);
	}

	for (tmp = 0; tmp < numdevs; tmp++) {
		struct cardbus_devinfo *dinfo = device_get_ivars(devlist[tmp]);
		int status = device_get_state(devlist[tmp]);

		if (dinfo->pci.cfg.dev != devlist[tmp])
			device_printf(cbdev, "devinfo dev mismatch\n");
		if (status == DS_ATTACHED || status == DS_BUSY)
			device_detach(devlist[tmp]);
		cardbus_release_all_resources(cbdev, dinfo);
		device_delete_child(cbdev, devlist[tmp]);
		pci_freecfg((struct pci_devinfo *)dinfo);
	}
	POWER_DISABLE_SOCKET(device_get_parent(cbdev), cbdev);
	free(devlist, M_TEMP);
	return (err);
}

static void
cardbus_driver_added(device_t cbdev, driver_t *driver)
{
	int numdevs;
	device_t *devlist;
	device_t dev;
	int i;
	struct cardbus_devinfo *dinfo;

	DEVICE_IDENTIFY(driver, cbdev);
	device_get_children(cbdev, &devlist, &numdevs);
	/*
	 * If there are no drivers attached, but there are children,
	 * then power the card up.
	 */
	for (i = 0; i < numdevs; i++) {
		dev = devlist[i];
		if (device_get_state(dev) != DS_NOTPRESENT)
		    break;
	}
	if (i > 0 && i == numdevs)
		POWER_ENABLE_SOCKET(device_get_parent(cbdev), cbdev);
	for (i = 0; i < numdevs; i++) {
		dev = devlist[i];
		if (device_get_state(dev) != DS_NOTPRESENT)
			continue;
		dinfo = device_get_ivars(dev);
		pci_print_verbose(&dinfo->pci);
		resource_list_init(&dinfo->pci.resources);
		cardbus_do_cis(cbdev, dev);
		if (device_probe_and_attach(dev) != 0)
			cardbus_release_all_resources(cbdev, dinfo);
	}
	free(devlist, M_TEMP);
}

static void
cardbus_release_all_resources(device_t cbdev, struct cardbus_devinfo *dinfo)
{
	struct resource_list_entry *rle;

	/* Free all allocated resources */
	SLIST_FOREACH(rle, &dinfo->pci.resources, link) {
		if (rle->res) {
			if (rman_get_device(rle->res) != cbdev)
				device_printf(cbdev, "release_all_resource: "
				    "Resource still owned by child, oops. "
				    "(type=%d, rid=%d, addr=%lx)\n",
				    rle->type, rle->rid,
				    rman_get_start(rle->res));
			BUS_RELEASE_RESOURCE(device_get_parent(cbdev),
			    cbdev, rle->type, rle->rid, rle->res);
			rle->res = NULL;
			/*
			 * zero out config so the card won't acknowledge
			 * access to the space anymore
			 */
			pci_write_config(dinfo->pci.cfg.dev, rle->rid, 0, 4);
		}
	}
	resource_list_free(&dinfo->pci.resources);
}

/************************************************************************/
/* Other Bus Methods							*/
/************************************************************************/

static int
cardbus_read_ivar(device_t cbdev, device_t child, int which, uintptr_t *result)
{
	struct cardbus_devinfo *dinfo;
	pcicfgregs *cfg;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->pci.cfg;

	switch (which) {
	case PCI_IVAR_ETHADDR:
		/*
		 * The generic accessor doesn't deal with failure, so
		 * we set the return value, then return an error.
		 */
		if ((dinfo->fepresent & (1 << TPL_FUNCE_LAN_NID)) == 0) {
			*((uint8_t **) result) = NULL;
			return (EINVAL);
		}
		*((uint8_t **) result) = dinfo->funce.lan.nid;
		break;
	default:
		return (pci_read_ivar(cbdev, child, which, result));
	}
	return 0;
}

static int
cardbus_write_ivar(device_t cbdev, device_t child, int which, uintptr_t value)
{
	return(pci_write_ivar(cbdev, child, which, value));
}

static device_method_t cardbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cardbus_probe),
	DEVMETHOD(device_attach,	cardbus_attach),
	DEVMETHOD(device_detach,	cardbus_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	cardbus_suspend),
	DEVMETHOD(device_resume,	cardbus_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	pci_print_child),
	DEVMETHOD(bus_probe_nomatch,	pci_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	cardbus_read_ivar),
	DEVMETHOD(bus_write_ivar,	cardbus_write_ivar),
	DEVMETHOD(bus_driver_added,	cardbus_driver_added),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_get_resource_list,pci_get_resource_list),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_delete_resource,	pci_delete_resource),
	DEVMETHOD(bus_alloc_resource,	pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_child_pnpinfo_str, pci_child_pnpinfo_str_method),
	DEVMETHOD(bus_child_location_str, pci_child_location_str_method),

	/* Card Interface */
	DEVMETHOD(card_attach_card,	cardbus_attach_card),
	DEVMETHOD(card_detach_card,	cardbus_detach_card),
	DEVMETHOD(card_cis_read,	cardbus_cis_read),
	DEVMETHOD(card_cis_free,	cardbus_cis_free),

	/* Cardbus/PCI interface */
	DEVMETHOD(pci_read_config,	pci_read_config_method),
	DEVMETHOD(pci_write_config,	pci_write_config_method),
	DEVMETHOD(pci_enable_busmaster,	pci_enable_busmaster_method),
	DEVMETHOD(pci_disable_busmaster, pci_disable_busmaster_method),
	DEVMETHOD(pci_enable_io,	pci_enable_io_method),
	DEVMETHOD(pci_disable_io,	pci_disable_io_method),
	DEVMETHOD(pci_get_powerstate,	pci_get_powerstate_method),
	DEVMETHOD(pci_set_powerstate,	pci_set_powerstate_method),
	DEVMETHOD(pci_assign_interrupt, pci_assign_interrupt_method),

	{0,0}
};

static driver_t cardbus_driver = {
	"cardbus",
	cardbus_methods,
	0 /* no softc */
};

static devclass_t cardbus_devclass;

DRIVER_MODULE(cardbus, cbb, cardbus_driver, cardbus_devclass, 0, 0);
MODULE_VERSION(cardbus, 1);
