/*-
 * Copyright (c) 2003 M. Warner Losh.  All Rights Reserved.
 * Copyright (c) 2000,2001 Jonathan Chen.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
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
#include <dev/pccard/pccard_cis.h>
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


static void	cardbus_add_map(device_t cbdev, device_t child, int reg);
static int	cardbus_alloc_resources(device_t cbdev, device_t child);
static int	cardbus_attach(device_t cbdev);
static int	cardbus_attach_card(device_t cbdev);
static int	cardbus_barsort(const void *a, const void *b);
static int	cardbus_detach(device_t cbdev);
static int	cardbus_detach_card(device_t cbdev);
static void	cardbus_device_setup_regs(device_t brdev, int b, int s, int f,
		    pcicfgregs *cfg);
static void	cardbus_driver_added(device_t cbdev, driver_t *driver);
static void	cardbus_pickup_maps(device_t cbdev, device_t child);
static int	cardbus_probe(device_t cbdev);
static int	cardbus_read_ivar(device_t cbdev, device_t child, int which,
		    uintptr_t *result);
static void	cardbus_release_all_resources(device_t cbdev,
		    struct cardbus_devinfo *dinfo);
static int	cardbus_write_ivar(device_t cbdev, device_t child, int which,
		    uintptr_t value);

/*
 * Resource allocation
 */
/*
 * Adding a memory/io resource (sans CIS)
 */

static void
cardbus_add_map(device_t cbdev, device_t child, int reg)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	struct resource_list_entry *rle;
	uint32_t size;
	uint32_t testval;
	int type;

	STAILQ_FOREACH(rle, &dinfo->pci.resources, link) {
		if (rle->rid == reg)
			return;
	}

	if (reg == CARDBUS_ROM_REG)
		testval = CARDBUS_ROM_ADDRMASK;
	else
		testval = ~0;

	pci_write_config(child, reg, testval, 4);
	testval = pci_read_config(child, reg, 4);

	if (testval == ~0 || testval == 0)
		return;

	if ((testval & 1) == 0)
		type = SYS_RES_MEMORY;
	else
		type = SYS_RES_IOPORT;

	size = CARDBUS_MAPREG_MEM_SIZE(testval);
	device_printf(cbdev, "Resource not specified in CIS: id=%x, size=%x\n",
	    reg, size);
	resource_list_add(&dinfo->pci.resources, type, reg, 0UL, ~0UL, size);
}

static void
cardbus_pickup_maps(device_t cbdev, device_t child)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	int reg;

	/*
	 * Try to pick up any resources that was not specified in CIS.
	 * Maybe this isn't any longer necessary now that we have fixed
	 * CIS parsing and we should filter things here?  XXX
	 */
	for (reg = 0; reg < dinfo->pci.cfg.nummaps; reg++)
		cardbus_add_map(cbdev, child, PCIR_BAR(reg));
}

static void
cardbus_do_res(struct resource_list_entry *rle, device_t child, uint32_t start)
{
	rle->start = start;
	rle->end = start + rle->count - 1;
	pci_write_config(child, rle->rid, rle->start, 4);
}

static int
cardbus_barsort(const void *a, const void *b)
{
	return ((*(const struct resource_list_entry * const *)b)->count -
	    (*(const struct resource_list_entry * const *)a)->count);
}

/* XXX this function is too long */
static int
cardbus_alloc_resources(device_t cbdev, device_t child)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	int count;
	struct resource_list_entry *rle;
	struct resource_list_entry **barlist;
	int tmp;
	uint32_t mem_psize = 0, mem_nsize = 0, io_size = 0;
	struct resource *res;
	uint32_t start,end;
	int rid, flags;

	count = 0;
	STAILQ_FOREACH(rle, &dinfo->pci.resources, link) {
		count++;
	}
	if (count == 0)
		return (0);
	barlist = malloc(sizeof(struct resource_list_entry*) * count, M_DEVBUF,
	    M_WAITOK);
	count = 0;
	STAILQ_FOREACH(rle, &dinfo->pci.resources, link) {
		barlist[count] = rle;
		if (rle->type == SYS_RES_IOPORT) {
			io_size += rle->count;
		} else if (rle->type == SYS_RES_MEMORY) {
			if (dinfo->mprefetchable & BARBIT(rle->rid))
				mem_psize += rle->count;
			else
				mem_nsize += rle->count;
		}
		count++;
	}

	/*
	 * We want to allocate the largest resource first, so that our
	 * allocated memory is packed.
	 */
	qsort(barlist, count, sizeof(struct resource_list_entry *),
	    cardbus_barsort);

	/* Allocate prefetchable memory */
	flags = 0;
	for (tmp = 0; tmp < count; tmp++) {
		rle = barlist[tmp];
		if (rle->res == NULL &&
		    rle->type == SYS_RES_MEMORY &&
		    dinfo->mprefetchable & BARBIT(rle->rid)) {
			flags = rman_make_alignment_flags(rle->count);
			break;
		}
	}
	if (flags > 0) { /* If any prefetchable memory is requested... */
		/*
		 * First we allocate one big space for all resources of this
		 * type.  We do this because our parent, pccbb, needs to open
		 * a window to forward all addresses within the window, and
		 * it would be best if nobody else has resources allocated
		 * within the window.
		 * (XXX: Perhaps there might be a better way to do this?)
		 */
		rid = 0;
		res = bus_alloc_resource(cbdev, SYS_RES_MEMORY, &rid, 0,
		    (dinfo->mprefetchable & dinfo->mbelow1mb)?0xFFFFF:~0UL,
		    mem_psize, flags);
		if (res == NULL) {
			device_printf(cbdev,
			    "Can't get memory for prefetch mem\n");
			free(barlist, M_DEVBUF);
			return (EIO);
		}
		start = rman_get_start(res);
		end = rman_get_end(res);
		DEVPRINTF((cbdev, "Prefetchable memory at %x-%x\n", start, end));
		/*
		 * Now that we know the region is free, release it and hand it
		 * out piece by piece.
		 */
		bus_release_resource(cbdev, SYS_RES_MEMORY, rid, res);
		for (tmp = 0; tmp < count; tmp++) {
			rle = barlist[tmp];
			if (rle->type == SYS_RES_MEMORY &&
			    dinfo->mprefetchable & BARBIT(rle->rid)) {
				cardbus_do_res(rle, child, start);
				start += rle->count;
			}
		}
	}

	/* Allocate non-prefetchable memory */
	flags = 0;
	for (tmp = 0; tmp < count; tmp++) {
		rle = barlist[tmp];
		if (rle->type == SYS_RES_MEMORY &&
		    (dinfo->mprefetchable & BARBIT(rle->rid)) == 0) {
			flags = rman_make_alignment_flags(rle->count);
			break;
		}
	}
	if (flags > 0) { /* If any non-prefetchable memory is requested... */
		/*
		 * First we allocate one big space for all resources of this
		 * type.  We do this because our parent, pccbb, needs to open
		 * a window to forward all addresses within the window, and
		 * it would be best if nobody else has resources allocated
		 * within the window.
		 * (XXX: Perhaps there might be a better way to do this?)
		 */
		rid = 0;
		res = bus_alloc_resource(cbdev, SYS_RES_MEMORY, &rid, 0,
		    ((~dinfo->mprefetchable) & dinfo->mbelow1mb)?0xFFFFF:~0UL,
		    mem_nsize, flags);
		if (res == NULL) {
			device_printf(cbdev,
			    "Can't get memory for non-prefetch mem\n");
			free(barlist, M_DEVBUF);
			return (EIO);
		}
		start = rman_get_start(res);
		end = rman_get_end(res);
		DEVPRINTF((cbdev, "Non-prefetchable memory at %x-%x\n",
		    start, end));
		/*
		 * Now that we know the region is free, release it and hand it
		 * out piece by piece.
		 */
		bus_release_resource(cbdev, SYS_RES_MEMORY, rid, res);
		for (tmp = 0; tmp < count; tmp++) {
			rle = barlist[tmp];
			if (rle->type == SYS_RES_MEMORY &&
			    (dinfo->mprefetchable & BARBIT(rle->rid)) == 0) {
				cardbus_do_res(rle, child, start);
				start += rle->count;
			}
		}
	}

	/* Allocate IO ports */
	flags = 0;
	for (tmp = 0; tmp < count; tmp++) {
		rle = barlist[tmp];
		if (rle->type == SYS_RES_IOPORT) {
			flags = rman_make_alignment_flags(rle->count);
			break;
		}
	}
	if (flags > 0) { /* If any IO port is requested... */
		/*
		 * First we allocate one big space for all resources of this
		 * type.  We do this because our parent, pccbb, needs to open
		 * a window to forward all addresses within the window, and
		 * it would be best if nobody else has resources allocated
		 * within the window.
		 * (XXX: Perhaps there might be a better way to do this?)
		 */
		rid = 0;
		res = bus_alloc_resource(cbdev, SYS_RES_IOPORT, &rid, 0,
		    (dinfo->ibelow1mb)?0xFFFFF:~0UL, io_size, flags);
		if (res == NULL) {
			device_printf(cbdev,
			    "Can't get memory for IO ports\n");
			free(barlist, M_DEVBUF);
			return (EIO);
		}
		start = rman_get_start(res);
		end = rman_get_end(res);
		DEVPRINTF((cbdev, "IO port at %x-%x\n", start, end));
		/*
		 * Now that we know the region is free, release it and hand it
		 * out piece by piece.
		 */
		bus_release_resource(cbdev, SYS_RES_IOPORT, rid, res);
		for (tmp = 0; tmp < count; tmp++) {
			rle = barlist[tmp];
			if (rle->type == SYS_RES_IOPORT) {
				cardbus_do_res(rle, child, start);
				start += rle->count;
			}
		}
	}

	/* Allocate IRQ */
	rid = 0;
	res = bus_alloc_resource_any(cbdev, SYS_RES_IRQ, &rid, RF_SHAREABLE);
	if (res == NULL) {
		device_printf(cbdev, "Can't get memory for irq\n");
		free(barlist, M_DEVBUF);
		return (EIO);
	}
	start = rman_get_start(res);
	end = rman_get_end(res);
	bus_release_resource(cbdev, SYS_RES_IRQ, rid, res);
	resource_list_add(&dinfo->pci.resources, SYS_RES_IRQ, rid, start, end,
	    1);
	dinfo->pci.cfg.intline = start;
	pci_write_config(child, PCIR_INTLINE, start, 1);

	free(barlist, M_DEVBUF);
	return (0);
}

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
	device_t child;
	int cardattached = 0;
	int bus, slot, func;

	cardbus_detach_card(cbdev); /* detach existing cards */
	POWER_ENABLE_SOCKET(brdev, cbdev);
	bus = pcib_get_bus(cbdev);
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
			child = device_add_child(cbdev, NULL, -1);
			if (child == NULL) {
				DEVPRINTF((cbdev, "Cannot add child!\n"));
				pci_freecfg((struct pci_devinfo *)dinfo);
				continue;
			}
			dinfo->pci.cfg.dev = child;
			resource_list_init(&dinfo->pci.resources);
			device_set_ivars(child, dinfo);
			if (cardbus_do_cis(cbdev, child) != 0) {
				DEVPRINTF((cbdev, "Can't parse cis\n"));
				pci_freecfg((struct pci_devinfo *)dinfo);
				continue;
			}
			cardbus_pickup_maps(cbdev, child);
			cardbus_alloc_resources(cbdev, child);
			pci_print_verbose(&dinfo->pci);
			if (device_probe_and_attach(child) != 0)
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
		cardbus_pickup_maps(cbdev, dev);
		cardbus_alloc_resources(cbdev, dev);
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
	STAILQ_FOREACH(rle, &dinfo->pci.resources, link) {
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
		if (dinfo->fepresent & (1 << PCCARD_TPLFE_TYPE_LAN_NID)) {
			*((uint8_t **) result) = dinfo->funce.lan.nid;
			break;
		}
		*((uint8_t **) result) = NULL;
		return (EINVAL);
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
	DEVMETHOD(device_suspend,	cardbus_suspend),
	DEVMETHOD(device_resume,	cardbus_resume),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	cardbus_read_ivar),
	DEVMETHOD(bus_write_ivar,	cardbus_write_ivar),
	DEVMETHOD(bus_driver_added,	cardbus_driver_added),

	/* Card Interface */
	DEVMETHOD(card_attach_card,	cardbus_attach_card),
	DEVMETHOD(card_detach_card,	cardbus_detach_card),

	{0,0}
};

DECLARE_CLASS(pci_driver);
DEFINE_CLASS_1(cardbus, cardbus_driver, cardbus_methods, 0, pci_driver);

static devclass_t cardbus_devclass;

DRIVER_MODULE(cardbus, cbb, cardbus_driver, cardbus_devclass, 0, 0);
MODULE_VERSION(cardbus, 1);
