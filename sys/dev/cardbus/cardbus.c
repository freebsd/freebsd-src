/*
 * Copyright (c) 2000,2001 Jonathan Chen.
 * All rights reserved.
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
 * $FreeBSD$
 */

/* XXX pci_cfgfree vs cardbus_cfgfree */

/*
 * Cardbus Bus Driver
 *
 * Written by Jonathan Chen <jon@freebsd.org>
 */

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

#include "power_if.h"
#include "card_if.h"
#include "pcib_if.h"

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, cardbus, CTLFLAG_RD, 0, "CardBus parameters");

int	cardbus_debug = 0;
TUNABLE_INT("hw.cardbus.debug", &cardbus_debug);
SYSCTL_INT(_hw_cardbus, OID_AUTO, debug, CTLFLAG_RW,
    &cardbus_debug, 0,
  "CardBus debug");

int	cardbus_cis_debug = 0;
TUNABLE_INT("hw.cardbus.cis_debug", &cardbus_cis_debug);
SYSCTL_INT(_hw_cardbus, OID_AUTO, cis_debug, CTLFLAG_RW,
    &cardbus_cis_debug, 0,
  "CardBus CIS debug");

#define	DPRINTF(a) if (cardbus_debug) printf a
#define	DEVPRINTF(x) if (cardbus_debug) device_printf x

static int	cardbus_probe(device_t cbdev);
static int	cardbus_attach(device_t cbdev);
static int	cardbus_detach(device_t cbdev);
static void	device_setup_regs(device_t brdev, int b, int s, int f,
		    pcicfgregs *cfg);
static int	cardbus_attach_card(device_t cbdev);
static int	cardbus_detach_card(device_t cbdev, int flags);
static void	cardbus_driver_added(device_t cbdev, driver_t *driver);
static void	cardbus_release_all_resources(device_t cbdev,
		    struct cardbus_devinfo *dinfo);

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
	cardbus_detach_card(cbdev, DETACH_FORCE);
	return 0;
}

static int
cardbus_suspend(device_t self)
{
	cardbus_detach_card(self, DETACH_FORCE);
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
device_setup_regs(device_t brdev, int b, int s, int f, pcicfgregs *cfg)
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
	struct cardbus_devinfo *dinfo;
	int cardbusfunchigh = 0;
	device_t kid;

	cardbus_detach_card(cbdev, 0); /* detach existing cards */

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
		cardbusfunchigh = 0;
		for (func = 0; func <= cardbusfunchigh; func++) {
			dinfo = (struct cardbus_devinfo *)
			  pci_read_device(brdev, bus, slot, func,
			  sizeof(struct cardbus_devinfo));
			if (dinfo == NULL)
				continue;
			if (dinfo->pci.cfg.mfdev)
				cardbusfunchigh = CARDBUS_FUNCMAX;
			device_setup_regs(brdev, bus, slot, func,
			    &dinfo->pci.cfg);
			kid = device_add_child(cbdev, NULL, -1);
			if (kid == NULL) {
				DEVPRINTF((cbdev, "Cannot add child!\n"));
				pci_freecfg(&dinfo->pci);
				continue;
			}
			dinfo->pci.cfg.dev = kid;
			device_set_ivars(kid, &dinfo->pci);
			cardbus_do_cis(cbdev, kid);
			pci_print_verbose(&dinfo->pci);
			if (device_probe_and_attach(kid) != 0) {
				/* when fail, release all resources */
				cardbus_release_all_resources(cbdev, dinfo);
			} else
				cardattached++;
		}
	}

	if (cardattached > 0)
		return 0;
	POWER_DISABLE_SOCKET(brdev, cbdev);
	return ENOENT;
}

static int
cardbus_detach_card(device_t cbdev, int flags)
{
	int numdevs;
	device_t *devlist;
	int tmp;
	int err = 0;
	struct cardbus_devinfo *dinfo;
	int status;

	device_get_children(cbdev, &devlist, &numdevs);

	if (numdevs == 0) {
		if (bootverbose)
			DEVPRINTF((cbdev, "detach_card: no card to detach!\n"));
		POWER_DISABLE_SOCKET(device_get_parent(cbdev), cbdev);
		free(devlist, M_TEMP);
		return ENOENT;
	}

	for (tmp = 0; tmp < numdevs; tmp++) {
		dinfo = device_get_ivars(devlist[tmp]);
		status = device_get_state(devlist[tmp]);

		if (status == DS_ATTACHED || status == DS_BUSY) {
			if (device_detach(dinfo->pci.cfg.dev) == 0 ||
			    flags & DETACH_FORCE) {
				cardbus_release_all_resources(cbdev, dinfo);
				device_delete_child(cbdev, devlist[tmp]);
			} else {
				err++;
			}
		} else {
			cardbus_release_all_resources(cbdev, dinfo);
			device_delete_child(cbdev, devlist[tmp]);
		}
		pci_freecfg(&dinfo->pci);
	}
	if (err == 0)
		POWER_DISABLE_SOCKET(device_get_parent(cbdev), cbdev);
	free(devlist, M_TEMP);
	return err;
}

static void
cardbus_driver_added(device_t cbdev, driver_t *driver)
{
	int numdevs;
	device_t *devlist;
	int tmp, cardattached;
	struct cardbus_devinfo *dinfo;

	device_get_children(cbdev, &devlist, &numdevs);

	cardattached = 0;
	for (tmp = 0; tmp < numdevs; tmp++) {
		if (device_get_state(devlist[tmp]) != DS_NOTPRESENT)
			cardattached++;
	}

	if (cardattached == 0) {
		free(devlist, M_TEMP);
		CARD_REPROBE_CARD(device_get_parent(cbdev), cbdev);
		return;
	}

	DEVICE_IDENTIFY(driver, cbdev);
	for (tmp = 0; tmp < numdevs; tmp++) {
		if (device_get_state(devlist[tmp]) == DS_NOTPRESENT) {
			dinfo = device_get_ivars(devlist[tmp]);
			cardbus_release_all_resources(cbdev, dinfo);
			resource_list_init(&dinfo->pci.resources);
			cardbus_do_cis(cbdev, dinfo->pci.cfg.dev);
			if (device_probe_and_attach(dinfo->pci.cfg.dev) != 0)
				cardbus_release_all_resources(cbdev, dinfo);
			else
				cardattached++;
		}
	}

	free(devlist, M_TEMP);
}

/************************************************************************/
/* Resources								*/
/************************************************************************/

static void
cardbus_release_all_resources(device_t cbdev, struct cardbus_devinfo *dinfo)
{
	resource_list_free(&dinfo->pci.resources);
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
	DEVMETHOD(bus_read_ivar,	pci_read_ivar),
	DEVMETHOD(bus_write_ivar,	pci_write_ivar),
	DEVMETHOD(bus_driver_added,	cardbus_driver_added),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	DEVMETHOD(bus_get_resource_list,pci_get_resource_list),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_delete_resource,	pci_delete_resource),
	DEVMETHOD(bus_alloc_resource,	pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),

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

	{0,0}
};

static driver_t cardbus_driver = {
	"cardbus",
	cardbus_methods,
	0 /* no softc */
};

static devclass_t cardbus_devclass;

DRIVER_MODULE(cardbus, pccbb, cardbus_driver, cardbus_devclass, 0, 0);
/*
MODULE_DEPEND(cardbus, pccbb, 1, 1, 1);
*/
