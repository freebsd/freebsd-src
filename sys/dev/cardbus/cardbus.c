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

/*
 * Cardbus Bus Driver
 *
 * much of the bus code was stolen directly from sys/pci/pci.c
 *   (Copyright (c) 1997, Stefan Esser <se@freebsd.org>)
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


static struct resource	*cardbus_alloc_resource(device_t cbdev, device_t child,
		    int type, int *rid, u_long start, u_long end, u_long count,
		    u_int flags);
static int	cardbus_attach(device_t cbdev);
static int	cardbus_attach_card(device_t cbdev);
static int	cardbus_child_location_str(device_t cbdev, device_t child,
		    char *, size_t len);
static int	cardbus_child_pnpinfo_str(device_t cbdev, device_t child,
		    char *, size_t len);
static void	cardbus_delete_resource(device_t cbdev, device_t child,
		    int type, int rid);
static void	cardbus_delete_resource_method(device_t cbdev, device_t child,
		    int type, int rid);
static int	cardbus_detach(device_t cbdev);
static int	cardbus_detach_card(device_t cbdev);
static void	cardbus_device_setup_regs(device_t brdev, int b, int s, int f,
		    pcicfgregs *cfg);
static void	cardbus_driver_added(device_t cbdev, driver_t *driver);
static int	cardbus_get_resource(device_t cbdev, device_t child, int type,
		    int rid, u_long *startp, u_long *countp);
static int	cardbus_get_resource_method(device_t cbdev, device_t child,
		    int type, int rid, u_long *startp, u_long *countp);
static int	cardbus_probe(device_t cbdev);
static int	cardbus_read_ivar(device_t cbdev, device_t child, int which,
		    uintptr_t *result);
static void	cardbus_release_all_resources(device_t cbdev,
		    struct cardbus_devinfo *dinfo);
static int	cardbus_release_resource(device_t cbdev, device_t child,
		    int type, int rid, struct resource *r);
static int	cardbus_set_resource(device_t cbdev, device_t child, int type,
		    int rid, u_long start, u_long count, struct resource *res);
static int	cardbus_set_resource_method(device_t cbdev, device_t child,
		    int type, int rid, u_long start, u_long count);
static int	cardbus_setup_intr(device_t cbdev, device_t child,
		    struct resource *irq, int flags, driver_intr_t *intr,
		    void *arg, void **cookiep);
static int	cardbus_teardown_intr(device_t cbdev, device_t child,
		    struct resource *irq, void *cookie);
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
			pci_print_verbose(&dinfo->pci);
			dinfo->pci.cfg.dev = device_add_child(cbdev, NULL, -1);
			if (!dinfo->pci.cfg.dev) {
				DEVPRINTF((cbdev, "Cannot add child!\n"));
				pci_freecfg((struct pci_devinfo *)dinfo);
				continue;
			}
			resource_list_init(&dinfo->pci.resources);
			device_set_ivars(dinfo->pci.cfg.dev, dinfo);
			cardbus_do_cis(cbdev, dinfo->pci.cfg.dev);
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

/************************************************************************/
/* Resources								*/
/************************************************************************/

static int
cardbus_set_resource(device_t cbdev, device_t child, int type, int rid,
    u_long start, u_long count, struct resource *res)
{
	struct cardbus_devinfo *dinfo;
	struct resource_list *rl;
	struct resource_list_entry *rle;

	if (device_get_parent(child) != cbdev)
		return ENOENT;

	dinfo = device_get_ivars(child);
	rl = &dinfo->pci.resources;
	rle = resource_list_find(rl, type, rid);
	if (rle == NULL) {
		resource_list_add(rl, type, rid, start, start + count - 1,
		    count);
		if (res != NULL) {
			rle = resource_list_find(rl, type, rid);
			rle->res = res;
		}
	} else {
		if (rle->res == NULL) {
		} else if (rman_get_device(rle->res) == cbdev &&
		    (!(rman_get_flags(rle->res) & RF_ACTIVE))) {
			int f;
			f = rman_get_flags(rle->res);
			bus_release_resource(cbdev, type, rid, res);
			rle->res = bus_alloc_resource(cbdev, type, &rid,
			    start, start + count - 1,
			    count, f);
		} else {
			device_printf(cbdev, "set_resource: resource busy\n");
			return EBUSY;
		}
		rle->start = start;
		rle->end = start + count - 1;
		rle->count = count;
		if (res != NULL)
			rle->res = res;
	}
	if (device_get_parent(child) == cbdev)
		pci_write_config(child, rid, start, 4);
	return 0;
}

static int
cardbus_get_resource(device_t cbdev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct cardbus_devinfo *dinfo;
	struct resource_list *rl;
	struct resource_list_entry *rle;

	if (device_get_parent(child) != cbdev)
		return ENOENT;

	dinfo = device_get_ivars(child);
	rl = &dinfo->pci.resources;
	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return ENOENT;
	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;
	return 0;
}

static void
cardbus_delete_resource(device_t cbdev, device_t child, int type, int rid)
{
	struct cardbus_devinfo *dinfo;
	struct resource_list *rl;
	struct resource_list_entry *rle;

	if (device_get_parent(child) != cbdev)
		return;

	dinfo = device_get_ivars(child);
	rl = &dinfo->pci.resources;
	rle = resource_list_find(rl, type, rid);
	if (rle) {
		if (rle->res) {
			if (rman_get_device(rle->res) != cbdev ||
			    rman_get_flags(rle->res) & RF_ACTIVE) {
				device_printf(cbdev, "delete_resource: "
				    "Resource still owned by child, oops. "
				    "(type=%d, rid=%d, addr=%lx)\n",
				    rle->type, rle->rid,
				    rman_get_start(rle->res));
				return;
			}
			bus_release_resource(cbdev, type, rid, rle->res);
		}
		resource_list_delete(rl, type, rid);
	}
	if (device_get_parent(child) == cbdev)
		pci_write_config(child, rid, 0, 4);
}

static int
cardbus_set_resource_method(device_t cbdev, device_t child, int type, int rid,
    u_long start, u_long count)
{
	int ret;
	ret = cardbus_set_resource(cbdev, child, type, rid, start, count, NULL);
	if (ret != 0)
		return ret;
	return BUS_SET_RESOURCE(device_get_parent(cbdev), child, type, rid,
	    start, count);
}

static int
cardbus_get_resource_method(device_t cbdev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	int ret;
	ret = cardbus_get_resource(cbdev, child, type, rid, startp, countp);
	if (ret != 0)
		return ret;
	return BUS_GET_RESOURCE(device_get_parent(cbdev), child, type, rid,
	    startp, countp);
}

static void
cardbus_delete_resource_method(device_t cbdev, device_t child,
    int type, int rid)
{
	cardbus_delete_resource(cbdev, child, type, rid);
	BUS_DELETE_RESOURCE(device_get_parent(cbdev), child, type, rid);
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

static struct resource *
cardbus_alloc_resource(device_t cbdev, device_t child, int type,
    int *rid, u_long start, u_long end, u_long count, u_int flags)
{
	struct cardbus_devinfo *dinfo;
	struct resource_list_entry *rle = 0;
	int passthrough = (device_get_parent(child) != cbdev);

	if (passthrough) {
		return (BUS_ALLOC_RESOURCE(device_get_parent(cbdev), child,
		    type, rid, start, end, count, flags));
	}

	dinfo = device_get_ivars(child);
	rle = resource_list_find(&dinfo->pci.resources, type, *rid);

	if (!rle)
		return NULL;		/* no resource of that type/rid */

	if (!rle->res) {
		device_printf(cbdev, "WARNING: Resource not reserved by bus\n");
		return NULL;
	} else {
		/* Release the cardbus hold on the resource */
		if (rman_get_device(rle->res) != cbdev)
			return NULL;
		bus_release_resource(cbdev, type, *rid, rle->res);
		rle->res = NULL;
		switch (type) {
		case SYS_RES_IOPORT:
		case SYS_RES_MEMORY:
			if (!(flags & RF_ALIGNMENT_MASK))
				flags |= rman_make_alignment_flags(rle->count);
			break;
		case SYS_RES_IRQ:
			flags |= RF_SHAREABLE;
			break;
		}
		/* Allocate the resource to the child */
		return resource_list_alloc(&dinfo->pci.resources, cbdev, child,
		    type, rid, rle->start, rle->end, rle->count, flags);
	}
}

static int
cardbus_release_resource(device_t cbdev, device_t child, int type, int rid,
    struct resource *r)
{
	struct cardbus_devinfo *dinfo;
	int passthrough = (device_get_parent(child) != cbdev);
	struct resource_list_entry *rle = 0;
	int flags;
	int ret;

	if (passthrough) {
		return BUS_RELEASE_RESOURCE(device_get_parent(cbdev), child,
		    type, rid, r);
	}

	dinfo = device_get_ivars(child);
	/*
	 * According to the PCI 2.2 spec, devices may share an address
	 * decoder between memory mapped ROM access and memory
	 * mapped register access.  To be safe, disable ROM access
	 * whenever it is released.
	 */
	if (rid == CARDBUS_ROM_REG) {
		uint32_t rom_reg;

		rom_reg = pci_read_config(child, rid, 4);
		rom_reg &= ~CARDBUS_ROM_ENABLE;
		pci_write_config(child, rid, rom_reg, 4);
	}

	rle = resource_list_find(&dinfo->pci.resources, type, rid);

	if (!rle) {
		device_printf(cbdev, "Allocated resource not found\n");
		return ENOENT;
	}
	if (!rle->res) {
		device_printf(cbdev, "Allocated resource not recorded\n");
		return ENOENT;
	}

	ret = BUS_RELEASE_RESOURCE(device_get_parent(cbdev), child,
	    type, rid, r);
	switch (type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		flags = rman_make_alignment_flags(rle->count);
		break;
	case SYS_RES_IRQ:
		flags = RF_SHAREABLE;
		break;
	default:
		flags = 0;
	}
	/* Restore cardbus hold on the resource */
	rle->res = bus_alloc_resource(cbdev, type, &rid,
	    rle->start, rle->end, rle->count, flags);
	if (rle->res == NULL)
		device_printf(cbdev, "release_resource: "
		    "unable to reacquire resource\n");
	return ret;
}

static int
cardbus_setup_intr(device_t cbdev, device_t child, struct resource *irq,
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	int ret;
	device_t cdev;
	struct cardbus_devinfo *dinfo;

	ret = bus_generic_setup_intr(cbdev, child, irq, flags, intr, arg,
	    cookiep);
	if (ret != 0)
		return ret;

	for (cdev = child; cbdev != device_get_parent(cdev);
	    cdev = device_get_parent(cdev))
		/* NOTHING */;
	dinfo = device_get_ivars(cdev);

	return 0;
}

static int
cardbus_teardown_intr(device_t cbdev, device_t child, struct resource *irq,
    void *cookie)
{
	int ret;
	device_t cdev;
	struct cardbus_devinfo *dinfo;

	ret = bus_generic_teardown_intr(cbdev, child, irq, cookie);
	if (ret != 0)
		return ret;

	for (cdev = child; cbdev != device_get_parent(cdev);
	    cdev = device_get_parent(cdev))
		/* NOTHING */;
	dinfo = device_get_ivars(cdev);

	return (0);
}


/************************************************************************/
/* Other Bus Methods							*/
/************************************************************************/

static int
cardbus_child_location_str(device_t cbdev, device_t child, char *buf,
    size_t buflen)
{
	struct cardbus_devinfo *dinfo;
	pcicfgregs *cfg;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->pci.cfg;
	snprintf(buf, buflen, "slot=%d function=%d", pci_get_slot(child),
	    pci_get_function(child));
	return (0);
}

static int
cardbus_child_pnpinfo_str(device_t cbdev, device_t child, char *buf,
    size_t buflen)
{
	struct cardbus_devinfo *dinfo;
	pcicfgregs *cfg;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->pci.cfg;
	snprintf(buf, buflen, "vendor=0x%04x device=0x%04x subvendor=0x%04x "
	    "subdevice=0x%04x", cfg->vendor, cfg->device, cfg->subvendor,
	    cfg->subdevice);
	return (0);
}

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

/************************************************************************/
/* Compatibility with PCI bus (XXX: Do we need this?)			*/
/************************************************************************/

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
	DEVMETHOD(bus_alloc_resource,	cardbus_alloc_resource),
	DEVMETHOD(bus_release_resource,	cardbus_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	cardbus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	cardbus_teardown_intr),

	DEVMETHOD(bus_set_resource,	cardbus_set_resource_method),
	DEVMETHOD(bus_get_resource,	cardbus_get_resource_method),
	DEVMETHOD(bus_delete_resource,	cardbus_delete_resource_method),
	DEVMETHOD(bus_child_pnpinfo_str, cardbus_child_pnpinfo_str),
	DEVMETHOD(bus_child_location_str, cardbus_child_location_str),

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

DRIVER_MODULE(cardbus, cbb, cardbus_driver, cardbus_devclass, 0, 0);
MODULE_VERSION(cardbus, 1);
MODULE_DEPEND(cardbus, exca, 1, 1, 1);
/*
MODULE_DEPEND(cardbus, pccbb, 1, 1, 1);
*/
