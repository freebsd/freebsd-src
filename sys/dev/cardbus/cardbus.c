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

#define CARDBUS_DEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <sys/pciio.h>

#include <dev/cardbus/cardbusreg.h>
#include <dev/cardbus/cardbusvar.h>
#include <dev/cardbus/cardbus_cis.h>

#include "power_if.h"
#include "card_if.h"
#include "pcib_if.h"

#if defined CARDBUS_DEBUG
#define STATIC
#define DPRINTF(a) printf a
#define DEVPRINTF(x) device_printf x
#else
#define STATIC static
#define DPRINTF(a)
#define DEVPRINTF(x)
#endif

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif

struct cardbus_quirk {
	u_int32_t devid;	/* Vendor/device of the card */
	int	type;
#define CARDBUS_QUIRK_MAP_REG	1 /* PCI map register in wierd place */
	int	arg1;
	int	arg2;
};

struct cardbus_quirk cardbus_quirks[] = {
	{ 0 }
};

static int cardbus_probe(device_t dev);
static int cardbus_attach(device_t dev);
static void device_setup_regs(device_t cbdev, int b, int s, int f,
			      pcicfgregs *cfg);
static int cardbus_attach_card(device_t dev);
static int cardbus_detach_card(device_t dev, int flags);
static struct cardbus_devinfo *cardbus_read_device(device_t pcib,
						   int b, int s, int f);
static void *cardbus_readppb(device_t pcib, int b, int s, int f);
static void *cardbus_readpcb(device_t pcib, int b, int s, int f);
static void cardbus_hdrtypedata(device_t pcib, int b, int s, int f,
				pcicfgregs *cfg);
static int cardbus_freecfg(struct cardbus_devinfo *dinfo);
static void cardbus_print_verbose(struct cardbus_devinfo *dinfo);
static int cardbus_set_resource(device_t dev, device_t child, int type,
				int rid, u_long start, u_long count);
static int cardbus_get_resource(device_t dev, device_t child, int type,
				int rid, u_long *startp, u_long *countp);
static void cardbus_delete_resource(device_t dev, device_t child, int type,
				    int rid);
static int cardbus_set_resource_method(device_t dev, device_t child, int type,
				       int rid, u_long start, u_long count);
static int cardbus_get_resource_method(device_t dev, device_t child, int type,
				      int rid, u_long *startp, u_long *countp);
static void cardbus_add_map(device_t bdev, device_t dev,
			    pcicfgregs *cfg, int reg);
static void cardbus_add_resources(device_t dev, pcicfgregs* cfg);
static void cardbus_release_all_resources(device_t dev,
					  struct resource_list *rl);
static struct resource* cardbus_alloc_resource(device_t self, device_t child,
					       int type, int* rid,u_long start,
					       u_long end, u_long count,
					       u_int flags);
static int cardbus_release_resource(device_t dev, device_t child, int type,
				    int rid, struct resource *r);
static int cardbus_print_resources(struct resource_list *rl, const char *name,
				   int type, const char *format);
static int cardbus_print_child(device_t dev, device_t child);
static void cardbus_probe_nomatch(device_t dev, device_t child);
static int cardbus_read_ivar(device_t dev, device_t child, int which,
			     u_long *result);
static int cardbus_write_ivar(device_t dev, device_t child, int which,
			      uintptr_t value);
static u_int32_t cardbus_read_config_method(device_t dev, device_t child,
					    int reg, int width);
static void cardbus_write_config_method(device_t dev, device_t child, int reg,
					u_int32_t val, int width);

/************************************************************************/
/* Probe/Attach								*/
/************************************************************************/

static int
cardbus_probe(device_t dev)
{
	device_set_desc(dev, "Cardbus bus (newcard)");
	return 0;
}

static int
cardbus_attach(device_t dev)
{
	return 0;
}

static int
cardbus_detach(device_t dev)
{
	cardbus_detach_card(dev, DETACH_FORCE);
	return 0;
}

/************************************************************************/
/* Attach/Detach card							*/
/************************************************************************/

static void
device_setup_regs(device_t bdev, int b, int s, int f, pcicfgregs *cfg)
{
	PCIB_WRITE_CONFIG(bdev, b, s, f, PCIR_COMMAND,
			  PCIB_READ_CONFIG(bdev, b, s, f, PCIR_COMMAND, 2) |
			  PCIM_CMD_MEMEN|PCIM_CMD_PORTEN|PCIM_CMD_BUSMASTEREN,
			  2);
	
	PCIB_WRITE_CONFIG(bdev, b, s, f, PCIR_INTLINE,
			  pci_get_irq(device_get_parent(bdev)), 1);
	cfg->intline = PCIB_READ_CONFIG(bdev, b, s, f, PCIR_INTLINE, 1);
	
	PCIB_WRITE_CONFIG(bdev, b, s, f, PCIR_CACHELNSZ, 0x08, 1);
	cfg->cachelnsz = PCIB_READ_CONFIG(bdev, b, s, f, PCIR_CACHELNSZ, 1);

	PCIB_WRITE_CONFIG(bdev, b, s, f, PCIR_LATTIMER, 0xa8, 1);
	cfg->lattimer = PCIB_READ_CONFIG(bdev, b, s, f, PCIR_LATTIMER, 1);

	PCIB_WRITE_CONFIG(bdev, b, s, f, PCIR_MINGNT, 0x14, 1);
	cfg->mingnt = PCIB_READ_CONFIG(bdev, b, s, f, PCIR_MINGNT, 1);

	PCIB_WRITE_CONFIG(bdev, b, s, f, PCIR_MAXLAT, 0x14, 1);
	cfg->maxlat = PCIB_READ_CONFIG(bdev, b, s, f, PCIR_MAXLAT, 1);
}

static int
cardbus_attach_card(device_t dev)
{
	device_t bdev = device_get_parent(dev);
	int cardattached = 0;
	static int curr_bus_number = 2; /* XXX EVILE BAD (see below) */
	int bus, slot, func;

	POWER_ENABLE_SOCKET(bdev, dev);

	bus = pci_get_secondarybus(bdev);
	if (bus == 0) {
		/*
		 * XXX EVILE BAD XXX
		 * Not all BIOSes initialize the secondary bus number properly,
		 * so if the default is bad, we just put one in and hope it
		 * works.
		 */
		bus = curr_bus_number;
		pci_write_config (bdev, PCIR_SECBUS_2, curr_bus_number, 1);
		pci_write_config (bdev, PCIR_SUBBUS_2, curr_bus_number+2, 1);
		curr_bus_number += 3;
	}

	for (slot = 0; slot <= CARDBUS_SLOTMAX; slot++) {
		int cardbusfunchigh = 0;
		for (func = 0; func <= cardbusfunchigh; func++) {
			struct cardbus_devinfo *dinfo =
				cardbus_read_device(bdev, bus, slot, func);

			if (dinfo == NULL) continue;
			if (dinfo->cfg.mfdev)
				cardbusfunchigh = CARDBUS_FUNCMAX;
			device_setup_regs(bdev, bus, slot, func, &dinfo->cfg);
			cardbus_print_verbose(dinfo);
			dinfo->cfg.dev = device_add_child(dev, NULL, -1);
			if (!dinfo->cfg.dev) {
				DEVPRINTF((dev, "Cannot add child!\n"));
				cardbus_freecfg(dinfo);
				continue;
			}
			resource_list_init(&dinfo->resources);
			device_set_ivars(dinfo->cfg.dev, dinfo);
			cardbus_add_resources(dinfo->cfg.dev, &dinfo->cfg);
			cardbus_do_cis(dev, dinfo->cfg.dev);
			if (device_probe_and_attach(dinfo->cfg.dev) != 0) {
				cardbus_release_all_resources(dinfo->cfg.dev,
						      &dinfo->resources);
				device_delete_child(dev, dinfo->cfg.dev);
				cardbus_freecfg(dinfo);
			} else
				cardattached++;
		}
	}

	if (cardattached > 0) return 0;
	POWER_DISABLE_SOCKET(bdev, dev);
	return ENOENT;
}

static int
cardbus_detach_card(device_t dev, int flags)
{
	int numdevs;
	device_t *devlist;
	int tmp;
	int err=0;

	device_get_children(dev, &devlist, &numdevs);

	if (numdevs == 0) {
		DEVPRINTF((dev, "Detaching card: no cards to detach!\n"));
		POWER_DISABLE_SOCKET(device_get_parent(dev), dev);
		return ENOENT;
	}

	for (tmp = 0; tmp < numdevs; tmp++) {
		struct cardbus_devinfo *dinfo = device_get_ivars(devlist[tmp]);
		if (device_detach(dinfo->cfg.dev) == 0 || flags & DETACH_FORCE){
			cardbus_release_all_resources(dinfo->cfg.dev,
						      &dinfo->resources);
			device_delete_child(dev, devlist[tmp]);
		} else
			err++;
		cardbus_freecfg(dinfo);
	}
	if (err == 0)
		POWER_DISABLE_SOCKET(device_get_parent(dev), dev);
	return err;
}

static void
cardbus_driver_added(device_t dev, driver_t *driver)
{
	/* 
	 * For this to work, we should:
	 * 1) power up the slot if it isn't powered.
	 *    (Is this necessary?  Can we assume _probe() doesn't need power?)
	 * 2) probe (we should probe even though we already have child?)
	 * 3) power up if we haven't done so and probe succeeds
	 * 4) attach if probe succeeds.
	 * 5) power down if probe or attach failed, and the slot was powered
	 *    down to begin with.
	 */
	printf("I see you added a driver that could be a child of cardbus...\n");
	printf("If this is for a cardbus card, please remove and reinsert the card.\n");
	printf("(there is no current support for adding a driver like this)\n");
}

/************************************************************************/
/* PCI-Like config reading (copied from pci.c				*/
/************************************************************************/

/* read configuration header into pcicfgrect structure */

static struct cardbus_devinfo *
cardbus_read_device(device_t pcib, int b, int s, int f)
{
#define REG(n, w)	PCIB_READ_CONFIG(pcib, b, s, f, n, w)
	pcicfgregs *cfg = NULL;
	struct cardbus_devinfo *devlist_entry = NULL;

	if (PCIB_READ_CONFIG(pcib, b, s, f, PCIR_DEVVENDOR, 4) != -1) {
		devlist_entry = malloc(sizeof(struct cardbus_devinfo),
				       M_DEVBUF, M_WAITOK | M_ZERO);
		if (devlist_entry == NULL)
			return (NULL);

		cfg = &devlist_entry->cfg;
		
		cfg->bus		= b;
		cfg->slot		= s;
		cfg->func		= f;
		cfg->vendor		= REG(PCIR_VENDOR, 2);
		cfg->device		= REG(PCIR_DEVICE, 2);
		cfg->cmdreg		= REG(PCIR_COMMAND, 2);
		cfg->statreg		= REG(PCIR_STATUS, 2);
		cfg->baseclass		= REG(PCIR_CLASS, 1);
		cfg->subclass		= REG(PCIR_SUBCLASS, 1);
		cfg->progif		= REG(PCIR_PROGIF, 1);
		cfg->revid		= REG(PCIR_REVID, 1);
		cfg->hdrtype		= REG(PCIR_HEADERTYPE, 1);
		cfg->cachelnsz		= REG(PCIR_CACHELNSZ, 1);
		cfg->lattimer		= REG(PCIR_LATTIMER, 1);
		cfg->intpin		= REG(PCIR_INTPIN, 1);
		cfg->intline		= REG(PCIR_INTLINE, 1);
#ifdef __alpha__
		alpha_platform_assign_pciintr(cfg);
#endif

#ifdef APIC_IO
		if (cfg->intpin != 0) {
			int airq;

			airq = pci_apic_irq(cfg->bus, cfg->slot, cfg->intpin);
			if (airq >= 0) {
				/* PCI specific entry found in MP table */
				if (airq != cfg->intline) {
					undirect_pci_irq(cfg->intline);
					cfg->intline = airq;
				}
			} else {
				/* 
				 * PCI interrupts might be redirected to the
				 * ISA bus according to some MP tables. Use the
				 * same methods as used by the ISA devices
				 * devices to find the proper IOAPIC int pin.
				 */
				airq = isa_apic_irq(cfg->intline);
				if ((airq >= 0) && (airq != cfg->intline)) {
					/* XXX: undirect_pci_irq() ? */
					undirect_isa_irq(cfg->intline);
					cfg->intline = airq;
				}
			}
		}
#endif /* APIC_IO */

		cfg->mingnt		= REG(PCIR_MINGNT, 1);
		cfg->maxlat		= REG(PCIR_MAXLAT, 1);

		cfg->mfdev		= (cfg->hdrtype & PCIM_MFDEV) != 0;
		cfg->hdrtype		&= ~PCIM_MFDEV;

		cardbus_hdrtypedata(pcib, b, s, f, cfg);

		devlist_entry->conf.pc_sel.pc_bus = cfg->bus;
		devlist_entry->conf.pc_sel.pc_dev = cfg->slot;
		devlist_entry->conf.pc_sel.pc_func = cfg->func;
		devlist_entry->conf.pc_hdr = cfg->hdrtype;

		devlist_entry->conf.pc_subvendor = cfg->subvendor;
		devlist_entry->conf.pc_subdevice = cfg->subdevice;
		devlist_entry->conf.pc_vendor = cfg->vendor;
		devlist_entry->conf.pc_device = cfg->device;

		devlist_entry->conf.pc_class = cfg->baseclass;
		devlist_entry->conf.pc_subclass = cfg->subclass;
		devlist_entry->conf.pc_progif = cfg->progif;
		devlist_entry->conf.pc_revid = cfg->revid;
	}
	return (devlist_entry);
#undef REG
}

/* read config data specific to header type 1 device (PCI to PCI bridge) */

static void *
cardbus_readppb(device_t pcib, int b, int s, int f)
{
	pcih1cfgregs *p;

	p = malloc(sizeof (pcih1cfgregs), M_DEVBUF, M_WAITOK | M_ZERO);
	if (p == NULL)
		return (NULL);

	p->secstat = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_SECSTAT_1, 2);
	p->bridgectl = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_BRIDGECTL_1, 2);

	p->seclat = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_SECLAT_1, 1);

	p->iobase = PCI_PPBIOBASE (PCIB_READ_CONFIG(pcib, b, s, f,
						    PCIR_IOBASEH_1, 2),
				   PCIB_READ_CONFIG(pcib, b, s, f,
						    PCIR_IOBASEL_1, 1));
	p->iolimit = PCI_PPBIOLIMIT (PCIB_READ_CONFIG(pcib, b, s, f,
						      PCIR_IOLIMITH_1, 2),
				     PCIB_READ_CONFIG(pcib, b, s, f,
						      PCIR_IOLIMITL_1, 1));

	p->membase = PCI_PPBMEMBASE (0,
				     PCIB_READ_CONFIG(pcib, b, s, f,
						      PCIR_MEMBASE_1, 2));
	p->memlimit = PCI_PPBMEMLIMIT (0,
				       PCIB_READ_CONFIG(pcib, b, s, f,
							PCIR_MEMLIMIT_1, 2));

	p->pmembase = PCI_PPBMEMBASE (
		(pci_addr_t)PCIB_READ_CONFIG(pcib, b, s, f, PCIR_PMBASEH_1, 4),
		PCIB_READ_CONFIG(pcib, b, s, f, PCIR_PMBASEL_1, 2));

	p->pmemlimit = PCI_PPBMEMLIMIT (
		(pci_addr_t)PCIB_READ_CONFIG(pcib, b, s, f,
					     PCIR_PMLIMITH_1, 4),
		PCIB_READ_CONFIG(pcib, b, s, f, PCIR_PMLIMITL_1, 2));

	return (p);
}

/* read config data specific to header type 2 device (PCI to CardBus bridge) */

static void *
cardbus_readpcb(device_t pcib, int b, int s, int f)
{
	pcih2cfgregs *p;

	p = malloc(sizeof (pcih2cfgregs), M_DEVBUF, M_WAITOK | M_ZERO);
	if (p == NULL)
		return (NULL);

	p->secstat = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_SECSTAT_2, 2);
	p->bridgectl = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_BRIDGECTL_2, 2);
	
	p->seclat = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_SECLAT_2, 1);

	p->membase0 = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_MEMBASE0_2, 4);
	p->memlimit0 = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_MEMLIMIT0_2, 4);
	p->membase1 = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_MEMBASE1_2, 4);
	p->memlimit1 = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_MEMLIMIT1_2, 4);

	p->iobase0 = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_IOBASE0_2, 4);
	p->iolimit0 = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_IOLIMIT0_2, 4);
	p->iobase1 = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_IOBASE1_2, 4);
	p->iolimit1 = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_IOLIMIT1_2, 4);

	p->pccardif = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_PCCARDIF_2, 4);
	return p;
}

/* extract header type specific config data */

static void
cardbus_hdrtypedata(device_t pcib, int b, int s, int f, pcicfgregs *cfg)
{
#define REG(n, w)	PCIB_READ_CONFIG(pcib, b, s, f, n, w)
	switch (cfg->hdrtype) {
	case 0:
		cfg->subvendor      = REG(PCIR_SUBVEND_0, 2);
		cfg->subdevice      = REG(PCIR_SUBDEV_0, 2);
		cfg->nummaps	    = PCI_MAXMAPS_0;
		break;
	case 1:
		cfg->subvendor      = REG(PCIR_SUBVEND_1, 2);
		cfg->subdevice      = REG(PCIR_SUBDEV_1, 2);
		cfg->secondarybus   = REG(PCIR_SECBUS_1, 1);
		cfg->subordinatebus = REG(PCIR_SUBBUS_1, 1);
		cfg->nummaps	    = PCI_MAXMAPS_1;
		cfg->hdrspec        = cardbus_readppb(pcib, b, s, f);
		break;
	case 2:
		cfg->subvendor      = REG(PCIR_SUBVEND_2, 2);
		cfg->subdevice      = REG(PCIR_SUBDEV_2, 2);
		cfg->secondarybus   = REG(PCIR_SECBUS_2, 1);
		cfg->subordinatebus = REG(PCIR_SUBBUS_2, 1);
		cfg->nummaps	    = PCI_MAXMAPS_2;
		cfg->hdrspec        = cardbus_readpcb(pcib, b, s, f);
		break;
	}
#undef REG
}

/* free pcicfgregs structure and all depending data structures */

static int
cardbus_freecfg(struct cardbus_devinfo *dinfo)
{
	if (dinfo->cfg.hdrspec != NULL)
		free(dinfo->cfg.hdrspec, M_DEVBUF);
	free(dinfo, M_DEVBUF);

	return (0);
}

static void
cardbus_print_verbose(struct cardbus_devinfo *dinfo)
{
	if (bootverbose) {
		pcicfgregs *cfg = &dinfo->cfg;

		printf("found->\tvendor=0x%04x, dev=0x%04x, revid=0x%02x\n",
		       cfg->vendor, cfg->device, cfg->revid);
		printf("\tclass=%02x-%02x-%02x, hdrtype=0x%02x, mfdev=%d\n",
		       cfg->baseclass, cfg->subclass, cfg->progif,
		       cfg->hdrtype, cfg->mfdev);
		printf("\tsubordinatebus=%x \tsecondarybus=%x\n",
		       cfg->subordinatebus, cfg->secondarybus);
#ifdef CARDBUS_DEBUG
		printf("\tcmdreg=0x%04x, statreg=0x%04x, cachelnsz=%d (dwords)\n",
		       cfg->cmdreg, cfg->statreg, cfg->cachelnsz);
		printf("\tlattimer=0x%02x (%d ns), mingnt=0x%02x (%d ns), maxlat=0x%02x (%d ns)\n",
		       cfg->lattimer, cfg->lattimer * 30,
		       cfg->mingnt, cfg->mingnt * 250, cfg->maxlat, cfg->maxlat * 250);
#endif /* CARDBUS_DEBUG */
		if (cfg->intpin > 0)
			printf("\tintpin=%c, irq=%d\n", cfg->intpin +'a' -1, cfg->intline);
	}
}

/************************************************************************/
/* Resources								*/
/************************************************************************/

static int
cardbus_set_resource(device_t dev, device_t child, int type, int rid,
		     u_long start, u_long count)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->resources;
	resource_list_add(rl, type, rid, start, start + count - 1, count);
	if (rid == CARDBUS_ROM_REG) start |= 1;
	if (device_get_parent(child) == dev)
		pci_write_config(child, rid, start, 4);
	return 0;
}

static int
cardbus_get_resource(device_t dev, device_t child, int type, int rid,
		     u_long *startp, u_long *countp)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->resources;
	struct resource_list_entry *rle;
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
cardbus_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->resources;
	struct resource_list_entry *rle;
	rle = resource_list_find(rl, type, rid);
	if (rle) {
		if (rle->res)
			bus_generic_release_resource(dev, child, type, rid,
						     rle->res);
		resource_list_delete(rl, type, rid);
	}
	if (device_get_parent(child) == dev)
		pci_write_config(child, rid, 0, 4);
}

static int
cardbus_set_resource_method(device_t dev, device_t child, int type, int rid,
			    u_long start, u_long count)
{
	int ret;
	ret = cardbus_set_resource(dev, child, type, rid, start, count);
	if (ret != 0) return ret;
	return BUS_SET_RESOURCE(device_get_parent(dev), child, type, rid,
				start, count);
}

static int
cardbus_get_resource_method(device_t dev, device_t child, int type, int rid,
			    u_long *startp, u_long *countp)
{
	int ret;
	ret = cardbus_get_resource(dev, child, type, rid, startp, countp);
	if (ret != 0) return ret;
	return BUS_GET_RESOURCE(device_get_parent(dev), child, type, rid,
				startp, countp);
}

static void
cardbus_delete_resource_method(device_t dev, device_t child,
					   int type, int rid)
{
	cardbus_delete_resource(dev, child, type, rid);
	BUS_DELETE_RESOURCE(device_get_parent(dev), child, type, rid);
}

static void
cardbus_add_map(device_t cbdev, device_t dev, pcicfgregs *cfg, int reg)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(dev);
	struct resource_list *rl = &dinfo->resources;
	struct resource_list_entry *rle;
	device_t bdev = device_get_parent(cbdev);
	u_int32_t size;
	u_int32_t testval;
	int type;
	struct resource *res;

	PCIB_WRITE_CONFIG(bdev, cfg->bus, cfg->slot, cfg->func,
			  reg, 0xfffffff0, 4);
	
	testval = PCIB_READ_CONFIG(bdev, cfg->bus, cfg->slot, cfg->func,
				   reg, 4);
	if (testval == 0xfffffff0 || testval == 0) return;

	if ((testval&1) == 0)
		type = SYS_RES_MEMORY;
	else
		type = SYS_RES_IOPORT;

	size = CARDBUS_MAPREG_MEM_SIZE(testval);
	res = bus_generic_alloc_resource(cbdev, dev, type, &reg, 0, ~0, size,
					 rman_make_alignment_flags(size));
	if (res) {
		u_int32_t start = rman_get_start(res);
		u_int32_t end = rman_get_end(res);
		cardbus_set_resource(cbdev, dev, type, reg, start,end-start+1);
		rle = resource_list_find(rl, type, reg);
		rle->res = res;
	} else {
		device_printf(dev, "Unable to add map %02x\n", reg);
	}
}

static void
cardbus_add_resources(device_t dev, pcicfgregs* cfg)
{
	device_t cbdev = device_get_parent(dev);
	struct cardbus_devinfo *dinfo = device_get_ivars(dev);
	struct resource_list *rl = &dinfo->resources;
	struct cardbus_quirk *q;
	struct resource_list_entry *rle;
	struct resource *res;
	int rid;
	int i;

	for (i = 0; i < cfg->nummaps; i++) {
		cardbus_add_map(cbdev, dev, cfg, PCIR_MAPS + i*4);
	}
	cardbus_add_map(cbdev, dev, cfg, CARDBUS_ROM_REG);

	for (q = &cardbus_quirks[0]; q->devid; q++) {
		if (q->devid == ((cfg->device << 16) | cfg->vendor)
		    && q->type == CARDBUS_QUIRK_MAP_REG)
			cardbus_add_map(cbdev, dev, cfg, q->arg1);
	}

	rid = 0;
	res = bus_generic_alloc_resource(cbdev, dev, SYS_RES_IRQ,
					 &rid, 0, ~0, 1, RF_SHAREABLE);

	if (res == NULL)
		panic("Cannot allocate IRQ for card\n");

	resource_list_add(rl, SYS_RES_IRQ, rid,
			  rman_get_start(res), rman_get_start(res), 1);
	rle = resource_list_find(rl, SYS_RES_IRQ, rid);
	rle->res = res;
}

static void
cardbus_release_all_resources(device_t dev, struct resource_list *rl)
{
	struct resource_list_entry *rle;

	SLIST_FOREACH(rle, rl, link) {
		if (rle->res) {
			bus_generic_release_resource(device_get_parent(dev),
						     dev, rle->type, rle->rid,
						     rle->res);
		}
	}
}

static struct resource*
cardbus_alloc_resource(device_t self, device_t child, int type,
				 int* rid, u_long start, u_long end,
				 u_long count, u_int flags)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->resources;
	struct resource_list_entry *rle = NULL;
	struct resource *res;

	if (device_get_parent(child) == self || child == self)
		rle = resource_list_find(rl, type, *rid);
	if (rle) {
		if (flags & RF_ACTIVE)
			if (bus_activate_resource(child, type, *rid,
						  rle->res)) {
				return NULL;
			}
		return rle->res; /* XXX: check if range within start/end */
	} else {
		res = bus_generic_alloc_resource(self, child, type, rid,
						 start, end, count, flags);
		if (res) {
			start = rman_get_start(res);
			end = rman_get_end(res);
			cardbus_set_resource(self, child, type, *rid, start,
					     end-start+1);
			rle = resource_list_find(rl, type, *rid);
			rle->res = res;
			return res;
		} else {
			device_printf(self, "Resource Allocation Failed!\n");
			return NULL;
		}
	}
}

static int
cardbus_release_resource(device_t dev, device_t child, int type, int rid,
			 struct resource *r)
{
	return bus_deactivate_resource(child, type, rid, r);
}

/************************************************************************/
/* Other Bus Methods							*/
/************************************************************************/

static int
cardbus_print_resources(struct resource_list *rl, const char *name,
			int type, const char *format)
{
	struct resource_list_entry *rle;
	int printed, retval;

	printed = 0;
	retval = 0;
	/* Yes, this is kinda cheating */
	SLIST_FOREACH(rle, rl, link) {
		if (rle->type == type) {
			if (printed == 0)
				retval += printf(" %s ", name);
			else if (printed > 0)
				retval += printf(",");
			printed++;
			retval += printf(format, rle->start);
			if (rle->count > 1) {
				retval += printf("-");
				retval += printf(format, rle->start +
						 rle->count - 1);
			}
		}
	}
	return retval;
}

static int
cardbus_print_child(device_t dev, device_t child)
{
	struct cardbus_devinfo *dinfo;
	struct resource_list *rl;
	pcicfgregs *cfg;
	int retval = 0;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->cfg;
	rl = &dinfo->resources;

	retval += bus_print_child_header(dev, child);

	retval += cardbus_print_resources(rl, "port", SYS_RES_IOPORT, "%#lx");
	retval += cardbus_print_resources(rl, "mem", SYS_RES_MEMORY, "%#lx");
	retval += cardbus_print_resources(rl, "irq", SYS_RES_IRQ, "%ld");
	if (device_get_flags(dev))
		retval += printf(" flags %#x", device_get_flags(dev));

	retval += printf(" at device %d.%d", pci_get_slot(child),
			 pci_get_function(child));

	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static void cardbus_probe_nomatch(device_t dev, device_t child) {
	struct cardbus_devinfo *dinfo;
	pcicfgregs *cfg;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->cfg;
	device_printf(dev, "<unknown card>");
	printf(" (vendor=0x%04x, dev=0x%04x)", cfg->vendor, cfg->device);
	printf(" at %d.%d", pci_get_slot(child), pci_get_function(child));
	if (cfg->intpin > 0 && cfg->intline != 255) {
		printf(" irq %d", cfg->intline);
	}
	printf("\n");

	return;
}

static int
cardbus_read_ivar(device_t dev, device_t child, int which, u_long *result)
{
	struct cardbus_devinfo *dinfo;
	pcicfgregs *cfg;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->cfg;

	switch (which) {
	case PCI_IVAR_SUBVENDOR:
		*result = cfg->subvendor;
		break;
	case PCI_IVAR_SUBDEVICE:
		*result = cfg->subdevice;
		break;
	case PCI_IVAR_VENDOR:
		*result = cfg->vendor;
		break;
	case PCI_IVAR_DEVICE:
		*result = cfg->device;
		break;
	case PCI_IVAR_DEVID:
		*result = (cfg->device << 16) | cfg->vendor;
		break;
	case PCI_IVAR_CLASS:
		*result = cfg->baseclass;
		break;
	case PCI_IVAR_SUBCLASS:
		*result = cfg->subclass;
		break;
	case PCI_IVAR_PROGIF:
		*result = cfg->progif;
		break;
	case PCI_IVAR_REVID:
		*result = cfg->revid;
		break;
	case PCI_IVAR_INTPIN:
		*result = cfg->intpin;
		break;
	case PCI_IVAR_IRQ:
		*result = cfg->intline;
		break;
	case PCI_IVAR_BUS:
		*result = cfg->bus;
		break;
	case PCI_IVAR_SLOT:
		*result = cfg->slot;
		break;
	case PCI_IVAR_FUNCTION:
		*result = cfg->func;
		break;
	case PCI_IVAR_SECONDARYBUS:
		*result = cfg->secondarybus;
		break;
	case PCI_IVAR_SUBORDINATEBUS:
		*result = cfg->subordinatebus;
		break;
	default:
		return ENOENT;
	}
	return 0;
}

static int
cardbus_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct cardbus_devinfo *dinfo;
	pcicfgregs *cfg;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->cfg;

	switch (which) {
	case PCI_IVAR_SUBVENDOR:
	case PCI_IVAR_SUBDEVICE:
	case PCI_IVAR_VENDOR:
	case PCI_IVAR_DEVICE:
	case PCI_IVAR_DEVID:
	case PCI_IVAR_CLASS:
	case PCI_IVAR_SUBCLASS:
	case PCI_IVAR_PROGIF:
	case PCI_IVAR_REVID:
	case PCI_IVAR_INTPIN:
	case PCI_IVAR_IRQ:
	case PCI_IVAR_BUS:
	case PCI_IVAR_SLOT:
	case PCI_IVAR_FUNCTION:
		return EINVAL;	/* disallow for now */
	case PCI_IVAR_SECONDARYBUS:
		cfg->secondarybus = value;
		break;
	case PCI_IVAR_SUBORDINATEBUS:
		cfg->subordinatebus = value;
		break;
	default:
		return ENOENT;
	}
	return 0;
}

/************************************************************************/
/* Compatibility with PCI bus (XXX: Do we need this?)			*/
/************************************************************************/

static u_int32_t
cardbus_read_config_method(device_t dev, device_t child, int reg, int width)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	return PCIB_READ_CONFIG(device_get_parent(dev),
				cfg->bus, cfg->slot, cfg->func,
				reg, width);
}

static void
cardbus_write_config_method(device_t dev, device_t child, int reg,
			    u_int32_t val, int width)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	PCIB_WRITE_CONFIG(device_get_parent(dev),
			  cfg->bus, cfg->slot, cfg->func,
			  reg, val, width);
}

static device_method_t cardbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cardbus_probe),
	DEVMETHOD(device_attach,	cardbus_attach),
	DEVMETHOD(device_detach,	cardbus_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	cardbus_print_child),
	DEVMETHOD(bus_probe_nomatch,	cardbus_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	cardbus_read_ivar),
	DEVMETHOD(bus_write_ivar,	cardbus_write_ivar),
	DEVMETHOD(bus_driver_added,	cardbus_driver_added),
	DEVMETHOD(bus_alloc_resource,	cardbus_alloc_resource),
	DEVMETHOD(bus_release_resource,	cardbus_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	DEVMETHOD(bus_set_resource,	cardbus_set_resource_method),
	DEVMETHOD(bus_get_resource,	cardbus_get_resource_method),
	DEVMETHOD(bus_delete_resource,	cardbus_delete_resource_method),

	/* Card Interface */
	DEVMETHOD(card_attach_card,	cardbus_attach_card),
	DEVMETHOD(card_detach_card,	cardbus_detach_card),

	/* Cardbus/PCI interface */
	DEVMETHOD(pci_read_config,	cardbus_read_config_method),
	DEVMETHOD(pci_write_config,	cardbus_write_config_method),

	{0,0}
};

static driver_t cardbus_driver = {
	"cardbus",
	cardbus_methods,
	0 /* no softc */
};

static devclass_t cardbus_devclass;

DRIVER_MODULE(cardbus, pccbb, cardbus_driver, cardbus_devclass, 0, 0);
MODULE_DEPEND(cardbus, pccbb, 1, 1, 1);
