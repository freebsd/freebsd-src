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

#define	CARDBUS_DEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <sys/pciio.h>

#include <dev/cardbus/cardbusreg.h>
#include <dev/cardbus/cardbusvar.h>
#include <dev/cardbus/cardbus_cis.h>

#include "power_if.h"
#include "card_if.h"
#include "pcib_if.h"

#if defined CARDBUS_DEBUG
#define	DPRINTF(a) printf a
#define	DEVPRINTF(x) device_printf x
#else
#define	DPRINTF(a)
#define	DEVPRINTF(x)
#endif

#if !defined(lint)
static const char rcsid[] =
    "$FreeBSD$";
#endif

static int	cardbus_probe(device_t cbdev);
static int	cardbus_attach(device_t cbdev);
static int	cardbus_detach(device_t cbdev);
static void	device_setup_regs(device_t brdev, int b, int s, int f,
		    pcicfgregs *cfg);
static int	cardbus_attach_card(device_t cbdev);
static int	cardbus_detach_card(device_t cbdev, int flags);
static void	cardbus_driver_added(device_t cbdev, driver_t *driver);
static void	cardbus_read_extcap(device_t cbdev, pcicfgregs *cfg);
static void	cardbus_hdrtypedata(device_t brdev, int b, int s, int f,
		    pcicfgregs *cfg);
static struct cardbus_devinfo	*cardbus_read_device(device_t brdev, int b,
		    int s, int f);
static int	cardbus_freecfg(struct cardbus_devinfo *dinfo);
static void	cardbus_print_verbose(struct cardbus_devinfo *dinfo);
static int	cardbus_set_resource(device_t cbdev, device_t child, int type,
		    int rid, u_long start, u_long count, struct resource *res);
static int	cardbus_get_resource(device_t cbdev, device_t child, int type,
		    int rid, u_long *startp, u_long *countp);
static void	cardbus_delete_resource(device_t cbdev, device_t child,
		    int type, int rid);
static int	cardbus_set_resource_method(device_t cbdev, device_t child,
		    int type, int rid, u_long start, u_long count);
static int	cardbus_get_resource_method(device_t cbdev, device_t child,
		    int type, int rid, u_long *startp, u_long *countp);
static void	cardbus_delete_resource_method(device_t cbdev, device_t child,
		    int type, int rid);
static void	cardbus_release_all_resources(device_t cbdev,
		    struct cardbus_devinfo *dinfo);
static struct resource	*cardbus_alloc_resource(device_t cbdev, device_t child,
		    int type, int *rid, u_long start, u_long end, u_long count,
		    u_int flags);
static int	cardbus_release_resource(device_t cbdev, device_t child,
		    int type, int rid, struct resource *r);
static int	cardbus_setup_intr(device_t cbdev, device_t child,
		    struct resource *irq, int flags, driver_intr_t *intr,
		    void *arg, void **cookiep);
static int	cardbus_teardown_intr(device_t cbdev, device_t child,
		    struct resource *irq, void *cookie);
static int	cardbus_print_resources(struct resource_list *rl,
		    const char *name, int type, const char *format);
static int	cardbus_print_child(device_t cbdev, device_t child);
static void	cardbus_probe_nomatch(device_t cbdev, device_t child);
static int	cardbus_read_ivar(device_t cbdev, device_t child, int which,
		    u_long *result);
static int	cardbus_write_ivar(device_t cbdev, device_t child, int which,
		    uintptr_t value);
static int	cardbus_set_powerstate_method(device_t cbdev, device_t child,
		    int state);
static int	cardbus_get_powerstate_method(device_t cbdev, device_t child);
static u_int32_t cardbus_read_config_method(device_t cbdev,
		    device_t child, int reg, int width);
static void	cardbus_write_config_method(device_t cbdev, device_t child,
		    int reg, u_int32_t val, int width);
static __inline void cardbus_set_command_bit(device_t cbdev, device_t child,
		    u_int16_t bit);
static __inline void cardbus_clear_command_bit(device_t cbdev, device_t child,
		    u_int16_t bit);
static void	cardbus_enable_busmaster_method(device_t cbdev, device_t child);
static void	cardbus_disable_busmaster_method(device_t cbdev, device_t child);
static void	cardbus_enable_io_method(device_t cbdev, device_t child,
		    int space);
static void	cardbus_disable_io_method(device_t cbdev, device_t child,
		    int space);

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
		int cardbusfunchigh = 0;
		for (func = 0; func <= cardbusfunchigh; func++) {
			struct cardbus_devinfo *dinfo =
			    cardbus_read_device(brdev, bus, slot, func);

			if (dinfo == NULL)
				continue;
			if (dinfo->cfg.mfdev)
				cardbusfunchigh = CARDBUS_FUNCMAX;
			device_setup_regs(brdev, bus, slot, func, &dinfo->cfg);
			cardbus_print_verbose(dinfo);
			dinfo->cfg.dev = device_add_child(cbdev, NULL, -1);
			if (!dinfo->cfg.dev) {
				DEVPRINTF((cbdev, "Cannot add child!\n"));
				cardbus_freecfg(dinfo);
				continue;
			}
			resource_list_init(&dinfo->resources);
			SLIST_INIT(&dinfo->intrlist);
			device_set_ivars(dinfo->cfg.dev, dinfo);
			cardbus_do_cis(cbdev, dinfo->cfg.dev);
			if (device_probe_and_attach(dinfo->cfg.dev) != 0) {
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

	device_get_children(cbdev, &devlist, &numdevs);

	if (numdevs == 0) {
		if (bootverbose)
			DEVPRINTF((cbdev, "detach_card: no card to detach!\n"));
		POWER_DISABLE_SOCKET(device_get_parent(cbdev), cbdev);
		free(devlist, M_TEMP);
		return ENOENT;
	}

	for (tmp = 0; tmp < numdevs; tmp++) {
		struct cardbus_devinfo *dinfo = device_get_ivars(devlist[tmp]);
		int status = device_get_state(devlist[tmp]);

		if (status == DS_ATTACHED || status == DS_BUSY) {
			if (device_detach(dinfo->cfg.dev) == 0 ||
			    flags & DETACH_FORCE) {
				cardbus_release_all_resources(cbdev, dinfo);
				device_delete_child(cbdev, devlist[tmp]);
			} else {
				err++;
			}
			cardbus_freecfg(dinfo);
		} else {
			cardbus_release_all_resources(cbdev, dinfo);
			device_delete_child(cbdev, devlist[tmp]);
			cardbus_freecfg(dinfo);
		}
	}
	if (err == 0)
		POWER_DISABLE_SOCKET(device_get_parent(cbdev), cbdev);
	free(devlist, M_TEMP);
	return err;
}

static void
cardbus_driver_added(device_t cbdev, driver_t *driver)
{
	/* XXX check if 16-bit or cardbus! */
	int numdevs;
	device_t *devlist;
	int tmp, cardattached;

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
			struct cardbus_devinfo *dinfo;
			dinfo = device_get_ivars(devlist[tmp]);
			cardbus_release_all_resources(cbdev, dinfo);
			resource_list_init(&dinfo->resources);
			cardbus_do_cis(cbdev, dinfo->cfg.dev);
			if (device_probe_and_attach(dinfo->cfg.dev) != 0) {
				cardbus_release_all_resources(cbdev, dinfo);
			} else
				cardattached++;
		}
	}

	free(devlist, M_TEMP);
}

/************************************************************************/
/* PCI-Like config reading (copied from pci.c				*/
/************************************************************************/

/* read configuration header into pcicfgrect structure */

static void
cardbus_read_extcap(device_t cbdev, pcicfgregs *cfg)
{
#define	REG(n, w) PCIB_READ_CONFIG(cbdev, cfg->bus, cfg->slot, cfg->func, n, w)
	int ptr, nextptr, ptrptr;

	switch (cfg->hdrtype) {
	case 0:
		ptrptr = 0x34;
		break;
	case 2:
		ptrptr = 0x14;
		break;
	default:
		return;		/* no extended capabilities support */
	}
	nextptr = REG(ptrptr, 1);	/* sanity check? */

	/*
	 * Read capability entries.
	 */
	while (nextptr != 0) {
		/* Sanity check */
		if (nextptr > 255) {
			printf("illegal PCI extended capability offset %d\n",
			    nextptr);
			return;
		}
		/* Find the next entry */
		ptr = nextptr;
		nextptr = REG(ptr + 1, 1);

		/* Process this entry */
		switch (REG(ptr, 1)) {
		case 0x01:		/* PCI power management */
			if (cfg->pp_cap == 0) {
				cfg->pp_cap = REG(ptr + PCIR_POWER_CAP, 2);
				cfg->pp_status = ptr + PCIR_POWER_STATUS;
				cfg->pp_pmcsr = ptr + PCIR_POWER_PMCSR;
				if ((nextptr - ptr) > PCIR_POWER_DATA)
					cfg->pp_data = ptr + PCIR_POWER_DATA;
			}
			break;
		default:
			break;
		}
	}
#undef	REG
}

/* extract header type specific config data */

static void
cardbus_hdrtypedata(device_t brdev, int b, int s, int f, pcicfgregs *cfg)
{
#define	REG(n, w)	PCIB_READ_CONFIG(brdev, b, s, f, n, w)
	switch (cfg->hdrtype) {
	case 0:
		cfg->subvendor	= REG(PCIR_SUBVEND_0, 2);
		cfg->subdevice	= REG(PCIR_SUBDEV_0, 2);
		cfg->nummaps	= PCI_MAXMAPS_0;
		break;
	case 1:
		cfg->subvendor	= REG(PCIR_SUBVEND_1, 2);
		cfg->subdevice	= REG(PCIR_SUBDEV_1, 2);
		cfg->nummaps	= PCI_MAXMAPS_1;
		break;
	case 2:
		cfg->subvendor	= REG(PCIR_SUBVEND_2, 2);
		cfg->subdevice	= REG(PCIR_SUBDEV_2, 2);
		cfg->nummaps	= PCI_MAXMAPS_2;
		break;
	}
#undef	REG
}

static struct cardbus_devinfo *
cardbus_read_device(device_t brdev, int b, int s, int f)
{
#define	REG(n, w)	PCIB_READ_CONFIG(brdev, b, s, f, n, w)
	pcicfgregs *cfg = NULL;
	struct cardbus_devinfo *devlist_entry = NULL;

	if (REG(PCIR_DEVVENDOR, 4) != -1) {
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

		cfg->mingnt		= REG(PCIR_MINGNT, 1);
		cfg->maxlat		= REG(PCIR_MAXLAT, 1);

		cfg->mfdev		= (cfg->hdrtype & PCIM_MFDEV) != 0;
		cfg->hdrtype		&= ~PCIM_MFDEV;

		cardbus_hdrtypedata(brdev, b, s, f, cfg);

		if (REG(PCIR_STATUS, 2) & PCIM_STATUS_CAPPRESENT)
			cardbus_read_extcap(brdev, cfg);

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
#undef	REG
}

/* free pcicfgregs structure and all depending data structures */

static int
cardbus_freecfg(struct cardbus_devinfo *dinfo)
{
	free(dinfo, M_DEVBUF);

	return (0);
}

static void
cardbus_print_verbose(struct cardbus_devinfo *dinfo)
{
#ifndef CARDBUS_DEBUG
	if (bootverbose)
#endif /* CARDBUS_DEBUG */
	{
		pcicfgregs *cfg = &dinfo->cfg;

		printf("found->\tvendor=0x%04x, dev=0x%04x, revid=0x%02x\n",
		    cfg->vendor, cfg->device, cfg->revid);
		printf("\tclass=%02x-%02x-%02x, hdrtype=0x%02x, mfdev=%d\n",
		    cfg->baseclass, cfg->subclass, cfg->progif,
		    cfg->hdrtype, cfg->mfdev);
#ifdef CARDBUS_DEBUG
		printf("\tcmdreg=0x%04x, statreg=0x%04x, "
		    "cachelnsz=%d (dwords)\n",
		    cfg->cmdreg, cfg->statreg, cfg->cachelnsz);
		printf("\tlattimer=0x%02x (%d ns), mingnt=0x%02x (%d ns), "
		    "maxlat=0x%02x (%d ns)\n",
		    cfg->lattimer, cfg->lattimer * 30,
		    cfg->mingnt, cfg->mingnt * 250, cfg->maxlat,
		    cfg->maxlat * 250);
#endif /* CARDBUS_DEBUG */
		if (cfg->intpin > 0)
			printf("\tintpin=%c, irq=%d\n",
			    cfg->intpin + 'a' - 1, cfg->intline);
	}
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
	rl = &dinfo->resources;
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
		} else if (rle->res->r_dev == cbdev &&
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
	rl = &dinfo->resources;
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
	rl = &dinfo->resources;
	rle = resource_list_find(rl, type, rid);
	if (rle) {
		if (rle->res) {
			if (rle->res->r_dev != cbdev ||
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
	struct cardbus_intrlist *ile;

	/* Remove any interrupt handlers */
	while (NULL != (ile = SLIST_FIRST(&dinfo->intrlist))) {
		device_printf(cbdev, "release_all_resource: "
		    "intr handler still active, removing.\n");
		bus_teardown_intr(ile->dev, ile->irq, ile->cookie);
		SLIST_REMOVE_HEAD(&dinfo->intrlist, link);
		free(ile, M_DEVBUF);
	}

	/* Free all allocated resources */
	SLIST_FOREACH(rle, &dinfo->resources, link) {
		if (rle->res) {
			if (rle->res->r_dev != cbdev)
				device_printf(cbdev, "release_all_resource: "
				    "Resource still owned by child, oops. "
				    "(type=%d, rid=%d, addr=%lx)\n",
				    rle->type, rle->rid,
				    rman_get_start(rle->res));
			BUS_RELEASE_RESOURCE(device_get_parent(cbdev),
			    rle->res->r_dev,
			    rle->type, rle->rid,
			    rle->res);
			rle->res = NULL;
			/*
			 * zero out config so the card won't acknowledge
			 * access to the space anymore
			 */
			pci_write_config(dinfo->cfg.dev, rle->rid, 0, 4);
		}
	}
	resource_list_free(&dinfo->resources);
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
	rle = resource_list_find(&dinfo->resources, type, *rid);

	if (!rle)
		return NULL;		/* no resource of that type/rid */

	if (!rle->res) {
		device_printf(cbdev, "WARNING: Resource not reserved by bus\n");
		return NULL;
	} else {
		/* Release the cardbus hold on the resource */
		if (rle->res->r_dev != cbdev)
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
		return resource_list_alloc(&dinfo->resources, cbdev, child,
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

	rle = resource_list_find(&dinfo->resources, type, rid);

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
	struct cardbus_intrlist *ile;
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

	/* record interrupt handler */
	ile = malloc(sizeof(struct cardbus_intrlist), M_DEVBUF, M_NOWAIT);
	ile->dev = child;
	ile->irq = irq;
	ile->cookie = *cookiep;

	SLIST_INSERT_HEAD(&dinfo->intrlist, ile, link);
	return 0;
}

static int
cardbus_teardown_intr(device_t cbdev, device_t child, struct resource *irq,
    void *cookie)
{
	int ret;
	struct cardbus_intrlist *ile;
	device_t cdev;
	struct cardbus_devinfo *dinfo;

	ret = bus_generic_teardown_intr(cbdev, child, irq, cookie);
	if (ret != 0)
		return ret;

	for (cdev = child; cbdev != device_get_parent(cdev);
	    cdev = device_get_parent(cdev))
		/* NOTHING */;
	dinfo = device_get_ivars(cdev);

	/* remove interrupt handler from record */
	SLIST_FOREACH(ile, &dinfo->intrlist, link) {
		if (ile->irq == irq && ile->cookie == cookie) {
			SLIST_REMOVE(&dinfo->intrlist, ile, cardbus_intrlist,
			    link);
			free(ile, M_DEVBUF);
			return 0;
		}
	}
	device_printf(cbdev, "teardown_intr: intr handler not recorded.\n");
	return ENOENT;
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
cardbus_print_child(device_t cbdev, device_t child)
{
	struct cardbus_devinfo *dinfo;
	struct resource_list *rl;
	pcicfgregs *cfg;
	int retval = 0;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->cfg;
	rl = &dinfo->resources;

	retval += bus_print_child_header(cbdev, child);

	retval += cardbus_print_resources(rl, "port", SYS_RES_IOPORT, "%#lx");
	retval += cardbus_print_resources(rl, "mem", SYS_RES_MEMORY, "%#lx");
	retval += cardbus_print_resources(rl, "irq", SYS_RES_IRQ, "%ld");
	if (device_get_flags(cbdev))
		retval += printf(" flags %#x", device_get_flags(cbdev));

	retval += printf(" at device %d.%d", pci_get_slot(child),
	    pci_get_function(child));

	retval += bus_print_child_footer(cbdev, child);

	return (retval);
}

static void
cardbus_probe_nomatch(device_t cbdev, device_t child)
{
	struct cardbus_devinfo *dinfo;
	pcicfgregs *cfg;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->cfg;
	device_printf(cbdev, "<unknown card>");
	printf(" (vendor=0x%04x, dev=0x%04x)", cfg->vendor, cfg->device);
	printf(" at %d.%d", pci_get_slot(child), pci_get_function(child));
	if (cfg->intpin > 0 && cfg->intline != 255) {
		printf(" irq %d", cfg->intline);
	}
	printf("\n");

	return;
}

static int
cardbus_read_ivar(device_t cbdev, device_t child, int which, u_long *result)
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
	default:
		return ENOENT;
	}
	return 0;
}

static int
cardbus_write_ivar(device_t cbdev, device_t child, int which, uintptr_t value)
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
	default:
		return ENOENT;
	}
	return 0;
}

/************************************************************************/
/* Compatibility with PCI bus (XXX: Do we need this?)			*/
/************************************************************************/

/*
 * PCI power manangement
 */
static int
cardbus_set_powerstate_method(device_t cbdev, device_t child, int state)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;
	u_int16_t status;
	int result;

	if (cfg->pp_cap != 0) {
		status = PCI_READ_CONFIG(cbdev, child, cfg->pp_status, 2)
		    & ~PCIM_PSTAT_DMASK;
		result = 0;
		switch (state) {
		case PCI_POWERSTATE_D0:
			status |= PCIM_PSTAT_D0;
			break;
		case PCI_POWERSTATE_D1:
			if (cfg->pp_cap & PCIM_PCAP_D1SUPP) {
				status |= PCIM_PSTAT_D1;
			} else {
				result = EOPNOTSUPP;
			}
			break;
		case PCI_POWERSTATE_D2:
			if (cfg->pp_cap & PCIM_PCAP_D2SUPP) {
				status |= PCIM_PSTAT_D2;
			} else {
				result = EOPNOTSUPP;
			}
			break;
		case PCI_POWERSTATE_D3:
			status |= PCIM_PSTAT_D3;
			break;
		default:
			result = EINVAL;
		}
		if (result == 0)
			PCI_WRITE_CONFIG(cbdev, child, cfg->pp_status,
			    status, 2);
	} else {
		result = ENXIO;
	}
	return (result);
}

static int
cardbus_get_powerstate_method(device_t cbdev, device_t child)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;
	u_int16_t status;
	int result;

	if (cfg->pp_cap != 0) {
		status = PCI_READ_CONFIG(cbdev, child, cfg->pp_status, 2);
		switch (status & PCIM_PSTAT_DMASK) {
		case PCIM_PSTAT_D0:
			result = PCI_POWERSTATE_D0;
			break;
		case PCIM_PSTAT_D1:
			result = PCI_POWERSTATE_D1;
			break;
		case PCIM_PSTAT_D2:
			result = PCI_POWERSTATE_D2;
			break;
		case PCIM_PSTAT_D3:
			result = PCI_POWERSTATE_D3;
			break;
		default:
			result = PCI_POWERSTATE_UNKNOWN;
			break;
		}
	} else {
		/* No support, device is always at D0 */
		result = PCI_POWERSTATE_D0;
	}
	return (result);
}

static u_int32_t
cardbus_read_config_method(device_t cbdev, device_t child, int reg, int width)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	return PCIB_READ_CONFIG(device_get_parent(cbdev),
	    cfg->bus, cfg->slot, cfg->func, reg, width);
}

static void
cardbus_write_config_method(device_t cbdev, device_t child, int reg,
    u_int32_t val, int width)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	PCIB_WRITE_CONFIG(device_get_parent(cbdev),
	    cfg->bus, cfg->slot, cfg->func, reg, val, width);
}

static __inline void
cardbus_set_command_bit(device_t cbdev, device_t child, u_int16_t bit)
{
	u_int16_t command;

	command = PCI_READ_CONFIG(cbdev, child, PCIR_COMMAND, 2);
	command |= bit;
	PCI_WRITE_CONFIG(cbdev, child, PCIR_COMMAND, command, 2);
}

static __inline void
cardbus_clear_command_bit(device_t cbdev, device_t child, u_int16_t bit)
{
	u_int16_t command;

	command = PCI_READ_CONFIG(cbdev, child, PCIR_COMMAND, 2);
	command &= ~bit;
	PCI_WRITE_CONFIG(cbdev, child, PCIR_COMMAND, command, 2);
}

static void
cardbus_enable_busmaster_method(device_t cbdev, device_t child)
{
	cardbus_set_command_bit(cbdev, child, PCIM_CMD_BUSMASTEREN);
}

static void
cardbus_disable_busmaster_method(device_t cbdev, device_t child)
{
	cardbus_clear_command_bit(cbdev, child, PCIM_CMD_BUSMASTEREN);
}

static void
cardbus_enable_io_method(device_t cbdev, device_t child, int space)
{
	switch (space) {
	case SYS_RES_IOPORT:
		cardbus_set_command_bit(cbdev, child, PCIM_CMD_PORTEN);
		break;
	case SYS_RES_MEMORY:
		cardbus_set_command_bit(cbdev, child, PCIM_CMD_MEMEN);
		break;
	}
}

static void
cardbus_disable_io_method(device_t cbdev, device_t child, int space)
{
	switch (space) {
	case SYS_RES_IOPORT:
		cardbus_clear_command_bit(cbdev, child, PCIM_CMD_PORTEN);
		break;
	case SYS_RES_MEMORY:
		cardbus_clear_command_bit(cbdev, child, PCIM_CMD_MEMEN);
		break;
	}
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
	DEVMETHOD(bus_print_child,	cardbus_print_child),
	DEVMETHOD(bus_probe_nomatch,	cardbus_probe_nomatch),
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

	/* Card Interface */
	DEVMETHOD(card_attach_card,	cardbus_attach_card),
	DEVMETHOD(card_detach_card,	cardbus_detach_card),
	DEVMETHOD(card_cis_read,	cardbus_cis_read),
	DEVMETHOD(card_cis_free,	cardbus_cis_free),

	/* Cardbus/PCI interface */
	DEVMETHOD(pci_read_config,	cardbus_read_config_method),
	DEVMETHOD(pci_write_config,	cardbus_write_config_method),
	DEVMETHOD(pci_enable_busmaster,	cardbus_enable_busmaster_method),
	DEVMETHOD(pci_disable_busmaster, cardbus_disable_busmaster_method),
	DEVMETHOD(pci_enable_io,	cardbus_enable_io_method),
	DEVMETHOD(pci_disable_io,	cardbus_disable_io_method),
	DEVMETHOD(pci_get_powerstate,	cardbus_get_powerstate_method),
	DEVMETHOD(pci_set_powerstate,	cardbus_set_powerstate_method),

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
