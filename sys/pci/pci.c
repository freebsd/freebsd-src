/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: pci.c,v 1.93 1999/01/19 23:29:18 se Exp $
 *
 */

#include "pci.h"
#if NPCI > 0

#include "opt_devfs.h"
#include "opt_simos.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/buf.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /* DEVFS */

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <pci/pci_ioctl.h>

#ifdef APIC_IO
#include <machine/smp.h>
#endif /* APIC_IO */

STAILQ_HEAD(devlist, pci_devinfo) pci_devq;
u_int32_t pci_numdevs = 0;
u_int32_t pci_generation = 0;

/* return highest PCI bus number known to be used, or -1 if none */

static int
pci_bushigh(void)
{
	if (pci_cfgopen() == 0)
		return (-1);
	return (0);
}

/* return base address of memory or port map */

static int
pci_mapbase(unsigned mapreg)
{
	int mask = 0x03;
	if ((mapreg & 0x01) == 0)
		mask = 0x0f;
	return (mapreg & ~mask);
}

/* return map type of memory or port map */

static int
pci_maptype(unsigned mapreg)
{
	static u_int8_t maptype[0x10] = {
		PCI_MAPMEM,		PCI_MAPPORT,
		PCI_MAPMEM,		0,
		PCI_MAPMEM,		PCI_MAPPORT,
		0,			0,
		PCI_MAPMEM|PCI_MAPMEMP,	PCI_MAPPORT,
		PCI_MAPMEM|PCI_MAPMEMP, 0,
		PCI_MAPMEM|PCI_MAPMEMP,	PCI_MAPPORT,
		0,			0,
	};

	return maptype[mapreg & 0x0f];
}

/* return log2 of map size decoded for memory or port map */

static int
pci_mapsize(unsigned testval)
{
	int ln2size;

	testval = pci_mapbase(testval);
	ln2size = 0;
	if (testval != 0) {
		while ((testval & 1) == 0)
		{
			ln2size++;
			testval >>= 1;
		}
	}
	return (ln2size);
}

/* return log2 of address range supported by map register */

static int
pci_maprange(unsigned mapreg)
{
	int ln2range = 0;
	switch (mapreg & 0x07) {
	case 0x00:
	case 0x01:
	case 0x05:
		ln2range = 32;
		break;
	case 0x02:
		ln2range = 20;
		break;
	case 0x04:
		ln2range = 64;
		break;
	}
	return (ln2range);
}

/* extract map parameters into newly allocated array of pcimap structures */

static pcimap *
pci_readmaps(pcicfgregs *cfg, int maxmaps)
{
	int i, j = 0;
	pcimap *map;
	int map64 = 0;
	int reg = PCIR_MAPS;

	for (i = 0; i < maxmaps; i++) {
		int reg = PCIR_MAPS + i*4;
		u_int32_t base;
		u_int32_t ln2range;

		base = pci_cfgread(cfg, reg, 4);
		ln2range = pci_maprange(base);

		if (base == 0 || ln2range == 0 || base == 0xffffffff)
			continue; /* skip invalid entry */
		else {
			j++;
			if (ln2range > 32) {
				i++;
				j++;
			}
		}
	}

	map = malloc(j * sizeof (pcimap), M_DEVBUF, M_WAITOK);
	if (map != NULL) {
		bzero(map, sizeof(pcimap) * j);
		cfg->nummaps = j;

		for (i = 0, j = 0; i < maxmaps; i++, reg += 4) {
			u_int32_t base;
			u_int32_t testval;

			base = pci_cfgread(cfg, reg, 4);

			if (map64 == 0) {
				if (base == 0 || base == 0xffffffff)
					continue; /* skip invalid entry */
				pci_cfgwrite(cfg, reg, 0xffffffff, 4);
				testval = pci_cfgread(cfg, reg, 4);
				pci_cfgwrite(cfg, reg, base, 4);

				map[j].reg	= reg;
				map[j].base     = pci_mapbase(base);
				map[j].type     = pci_maptype(base);
				map[j].ln2size  = pci_mapsize(testval);
				map[j].ln2range = pci_maprange(testval);
				map64 = map[j].ln2range == 64;
			} else {
				/* only fill in base, other fields are 0 */
				map[j].base     = base;
				map64 = 0;
			}
			j++;
		}
	}
	return (map);
}

/* adjust some values from PCI 1.0 devices to match 2.0 standards ... */

static void
pci_fixancient(pcicfgregs *cfg)
{
	if (cfg->hdrtype != 0)
		return;

	/* PCI to PCI bridges use header type 1 */
	if (cfg->baseclass == PCIC_BRIDGE && cfg->subclass == PCIS_BRIDGE_PCI)
		cfg->hdrtype = 1;
}

/* read config data specific to header type 1 device (PCI to PCI bridge) */

static void *
pci_readppb(pcicfgregs *cfg)
{
	pcih1cfgregs *p;

	p = malloc(sizeof (pcih1cfgregs), M_DEVBUF, M_WAITOK);
	if (p == NULL)
		return (NULL);

	bzero(p, sizeof *p);

	p->secstat = pci_cfgread(cfg, PCIR_SECSTAT_1, 2);
	p->bridgectl = pci_cfgread(cfg, PCIR_BRIDGECTL_1, 2);

	p->seclat = pci_cfgread(cfg, PCIR_SECLAT_1, 1);

	p->iobase = PCI_PPBIOBASE (pci_cfgread(cfg, PCIR_IOBASEH_1, 2),
				   pci_cfgread(cfg, PCIR_IOBASEL_1, 1));
	p->iolimit = PCI_PPBIOLIMIT (pci_cfgread(cfg, PCIR_IOLIMITH_1, 2),
				     pci_cfgread(cfg, PCIR_IOLIMITL_1, 1));

	p->membase = PCI_PPBMEMBASE (0,
				     pci_cfgread(cfg, PCIR_MEMBASE_1, 2));
	p->memlimit = PCI_PPBMEMLIMIT (0,
				       pci_cfgread(cfg, PCIR_MEMLIMIT_1, 2));

	p->pmembase = PCI_PPBMEMBASE (
		(pci_addr_t)pci_cfgread(cfg, PCIR_PMBASEH_1, 4),
		pci_cfgread(cfg, PCIR_PMBASEL_1, 2));

	p->pmemlimit = PCI_PPBMEMLIMIT (
		(pci_addr_t)pci_cfgread(cfg, PCIR_PMLIMITH_1, 4),
		pci_cfgread(cfg, PCIR_PMLIMITL_1, 2));
	return (p);
}

/* read config data specific to header type 2 device (PCI to CardBus bridge) */

static void *
pci_readpcb(pcicfgregs *cfg)
{
	pcih2cfgregs *p;

	p = malloc(sizeof (pcih2cfgregs), M_DEVBUF, M_WAITOK);
	if (p == NULL)
		return (NULL);

	bzero(p, sizeof *p);

	p->secstat = pci_cfgread(cfg, PCIR_SECSTAT_2, 2);
	p->bridgectl = pci_cfgread(cfg, PCIR_BRIDGECTL_2, 2);
	
	p->seclat = pci_cfgread(cfg, PCIR_SECLAT_2, 1);

	p->membase0 = pci_cfgread(cfg, PCIR_MEMBASE0_2, 4);
	p->memlimit0 = pci_cfgread(cfg, PCIR_MEMLIMIT0_2, 4);
	p->membase1 = pci_cfgread(cfg, PCIR_MEMBASE1_2, 4);
	p->memlimit1 = pci_cfgread(cfg, PCIR_MEMLIMIT1_2, 4);

	p->iobase0 = pci_cfgread(cfg, PCIR_IOBASE0_2, 4);
	p->iolimit0 = pci_cfgread(cfg, PCIR_IOLIMIT0_2, 4);
	p->iobase1 = pci_cfgread(cfg, PCIR_IOBASE1_2, 4);
	p->iolimit1 = pci_cfgread(cfg, PCIR_IOLIMIT1_2, 4);

	p->pccardif = pci_cfgread(cfg, PCIR_PCCARDIF_2, 4);
	return p;
}

/* extract header type specific config data */

static void
pci_hdrtypedata(pcicfgregs *cfg)
{
	switch (cfg->hdrtype) {
	case 0:
		cfg->subvendor      = pci_cfgread(cfg, PCIR_SUBVEND_0, 2);
		cfg->subdevice      = pci_cfgread(cfg, PCIR_SUBDEV_0, 2);
		cfg->map            = pci_readmaps(cfg, PCI_MAXMAPS_0);
		break;
	case 1:
		cfg->subvendor      = pci_cfgread(cfg, PCIR_SUBVEND_1, 2);
		cfg->subdevice      = pci_cfgread(cfg, PCIR_SUBDEV_1, 2);
		cfg->secondarybus   = pci_cfgread(cfg, PCIR_SECBUS_1, 1);
		cfg->subordinatebus = pci_cfgread(cfg, PCIR_SUBBUS_1, 1);
		cfg->map            = pci_readmaps(cfg, PCI_MAXMAPS_1);
		cfg->hdrspec        = pci_readppb(cfg);
		break;
	case 2:
		cfg->subvendor      = pci_cfgread(cfg, PCIR_SUBVEND_2, 2);
		cfg->subdevice      = pci_cfgread(cfg, PCIR_SUBDEV_2, 2);
		cfg->secondarybus   = pci_cfgread(cfg, PCIR_SECBUS_2, 1);
		cfg->subordinatebus = pci_cfgread(cfg, PCIR_SUBBUS_2, 1);
		cfg->map            = pci_readmaps(cfg, PCI_MAXMAPS_2);
		cfg->hdrspec        = pci_readpcb(cfg);
		break;
	}
}

/* read configuration header into pcicfgrect structure */

static struct pci_devinfo *
pci_readcfg(pcicfgregs *probe)
{
	pcicfgregs *cfg = NULL;
	struct pci_devinfo *devlist_entry;
	struct devlist *devlist_head;

	devlist_head = &pci_devq;

	devlist_entry = NULL;

	if (pci_cfgread(probe, PCIR_DEVVENDOR, 4) != -1) {
		devlist_entry = malloc(sizeof(struct pci_devinfo),
				       M_DEVBUF, M_WAITOK);
		if (devlist_entry == NULL)
			return (NULL);

		cfg = &devlist_entry->cfg;

		bzero(cfg, sizeof *cfg);

		cfg->bus		= probe->bus;
		cfg->slot		= probe->slot;
		cfg->func		= probe->func;
		cfg->vendor		= pci_cfgread(cfg, PCIR_VENDOR, 2);
		cfg->device		= pci_cfgread(cfg, PCIR_DEVICE, 2);
		cfg->cmdreg		= pci_cfgread(cfg, PCIR_COMMAND, 2);
		cfg->statreg		= pci_cfgread(cfg, PCIR_STATUS, 2);
		cfg->baseclass		= pci_cfgread(cfg, PCIR_CLASS, 1);
		cfg->subclass		= pci_cfgread(cfg, PCIR_SUBCLASS, 1);
		cfg->progif		= pci_cfgread(cfg, PCIR_PROGIF, 1);
		cfg->revid		= pci_cfgread(cfg, PCIR_REVID, 1);
		cfg->hdrtype		= pci_cfgread(cfg, PCIR_HEADERTYPE, 1);
		cfg->cachelnsz		= pci_cfgread(cfg, PCIR_CACHELNSZ, 1);
		cfg->lattimer		= pci_cfgread(cfg, PCIR_LATTIMER, 1);
		cfg->intpin		= pci_cfgread(cfg, PCIR_INTPIN, 1);
		cfg->intline		= pci_cfgread(cfg, PCIR_INTLINE, 1);
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

		cfg->mingnt		= pci_cfgread(cfg, PCIR_MINGNT, 1);
		cfg->maxlat		= pci_cfgread(cfg, PCIR_MAXLAT, 1);

		cfg->mfdev		= (cfg->hdrtype & PCIM_MFDEV) != 0;
		cfg->hdrtype		&= ~PCIM_MFDEV;

		pci_fixancient(cfg);
		pci_hdrtypedata(cfg);

		STAILQ_INSERT_TAIL(devlist_head, devlist_entry, pci_links);

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

		pci_numdevs++;
		pci_generation++;
	}
	return (devlist_entry);
}

#if 0
/* free pcicfgregs structure and all depending data structures */

static int
pci_freecfg(struct pci_devinfo *dinfo)
{
	struct devlist *devlist_head;

	devlist_head = &pci_devq;

	if (dinfo->cfg.hdrspec != NULL)
		free(dinfo->cfg.hdrspec, M_DEVBUF);
	if (dinfo->cfg.map != NULL)
		free(dinfo->cfg.map, M_DEVBUF);
	/* XXX this hasn't been tested */
	STAILQ_REMOVE(devlist_head, dinfo, pci_devinfo, pci_links);
	free(dinfo, M_DEVBUF);

	/* increment the generation count */
	pci_generation++;

	/* we're losing one device */
	pci_numdevs--;
	return (0);
}
#endif

static void
pci_addcfg(struct pci_devinfo *dinfo)
{
	if (bootverbose) {
		int i;
		pcicfgregs *cfg = &dinfo->cfg;

		printf("found->\tvendor=0x%04x, dev=0x%04x, revid=0x%02x\n", 
		       cfg->vendor, cfg->device, cfg->revid);
		printf("\tclass=%02x-%02x-%02x, hdrtype=0x%02x, mfdev=%d\n",
		       cfg->baseclass, cfg->subclass, cfg->progif,
		       cfg->hdrtype, cfg->mfdev);
		printf("\tsubordinatebus=%x \tsecondarybus=%x\n",
		       cfg->subordinatebus, cfg->secondarybus);
#ifdef PCI_DEBUG
		printf("\tcmdreg=0x%04x, statreg=0x%04x, cachelnsz=%d (dwords)\n", 
		       cfg->cmdreg, cfg->statreg, cfg->cachelnsz);
		printf("\tlattimer=0x%02x (%d ns), mingnt=0x%02x (%d ns), maxlat=0x%02x (%d ns)\n",
		       cfg->lattimer, cfg->lattimer * 30, 
		       cfg->mingnt, cfg->mingnt * 250, cfg->maxlat, cfg->maxlat * 250);
#endif /* PCI_DEBUG */
		if (cfg->intpin > 0)
			printf("\tintpin=%c, irq=%d\n", cfg->intpin +'a' -1, cfg->intline);

		for (i = 0; i < cfg->nummaps; i++) {
			pcimap *m = &cfg->map[i];
			printf("\tmap[%d]: type %x, range %2d, base %08x, size %2d\n",
			       i, m->type, m->ln2range, m->base, m->ln2size);
		}
	}
	pci_drvattach(dinfo); /* XXX currently defined in pci_compat.c */
}

/* scan one PCI bus for devices */

static int
pci_probebus(int bus)
{
	pcicfgregs probe;
	int bushigh = bus;

#ifdef SIMOS
#undef PCI_SLOTMAX
#define PCI_SLOTMAX 0
#endif

	bzero(&probe, sizeof probe);
	/* XXX KDM */
	/* probe.parent = pci_bridgeto(bus); */
	probe.bus = bus;
	for (probe.slot = 0; probe.slot <= PCI_SLOTMAX; probe.slot++) {
		int pcifunchigh = 0;
		for (probe.func = 0; probe.func <= pcifunchigh; probe.func++) {
			struct pci_devinfo *dinfo = pci_readcfg(&probe);
			if (dinfo != NULL) {
				if (dinfo->cfg.mfdev)
					pcifunchigh = 7;
				/*
				 * XXX: Temporarily move pci_addcfg() up before
				 * the use of cfg->subordinatebus. This is 
				 * necessary, since pci_addcfg() calls the 
				 * device's probe(), which may read the bus#
				 * from some device dependent register of
				 * some host to PCI bridges. The probe will 
				 * eventually be moved to pci_readcfg(), and 
				 * pci_addcfg() will then be moved back down
				 * below the conditional statement ...
				 */
				pci_addcfg(dinfo);

				if (bushigh < dinfo->cfg.subordinatebus)
					bushigh = dinfo->cfg.subordinatebus;
				if (bushigh < dinfo->cfg.secondarybus)
					bushigh = dinfo->cfg.secondarybus;

				/* XXX KDM */
				/* cfg = NULL; we don't own this anymore ... */
			}
		}
	}
	return (bushigh);
}

/* scan a PCI bus tree reached through one PCI attachment point */

int
pci_probe(pciattach *parent)
{
	int bushigh;
	int bus = 0;

	STAILQ_INIT(&pci_devq);

	bushigh = pci_bushigh();
	while (bus <= bushigh) {
		int newbushigh;

		printf("Probing for devices on PCI bus %d:\n", bus);
		newbushigh = pci_probebus(bus);

		if (bushigh < newbushigh)
			bushigh = newbushigh;
		bus++;
	}
	return (bushigh);
}

/*
 * This is the user interface to PCI configuration space.
 */
  
static int
pci_open(dev_t dev, int oflags, int devtype, struct proc *p)
{
	if ((oflags & FWRITE) && securelevel > 0) {
		return EPERM;
	}
	return 0;
}

static int
pci_close(dev_t dev, int flag, int devtype, struct proc *p)
{
	return 0;
}

/*
 * Match a single pci_conf structure against an array of pci_match_conf
 * structures.  The first argument, 'matches', is an array of num_matches
 * pci_match_conf structures.  match_buf is a pointer to the pci_conf
 * structure that will be compared to every entry in the matches array.
 * This function returns 1 on failure, 0 on success.
 */
static int
pci_conf_match(struct pci_match_conf *matches, int num_matches, 
	       struct pci_conf *match_buf)
{
	int i;

	if ((matches == NULL) || (match_buf == NULL) || (num_matches <= 0))
		return(1);

	for (i = 0; i < num_matches; i++) {
		/*
		 * I'm not sure why someone would do this...but...
		 */
		if (matches[i].flags == PCI_GETCONF_NO_MATCH)
			continue;

		/*
		 * Look at each of the match flags.  If it's set, do the
		 * comparison.  If the comparison fails, we don't have a
		 * match, go on to the next item if there is one.
		 */
		if (((matches[i].flags & PCI_GETCONF_MATCH_BUS) != 0)
		 && (match_buf->pc_sel.pc_bus != matches[i].pc_sel.pc_bus))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_DEV) != 0)
		 && (match_buf->pc_sel.pc_dev != matches[i].pc_sel.pc_dev))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_FUNC) != 0)
		 && (match_buf->pc_sel.pc_func != matches[i].pc_sel.pc_func))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_VENDOR) != 0) 
		 && (match_buf->pc_vendor != matches[i].pc_vendor))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_DEVICE) != 0)
		 && (match_buf->pc_device != matches[i].pc_device))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_CLASS) != 0)
		 && (match_buf->pc_class != matches[i].pc_class))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_UNIT) != 0)
		 && (match_buf->pd_unit != matches[i].pd_unit))
			continue;

		if (((matches[i].flags & PCI_GETCONF_MATCH_NAME) != 0)
		 && (strncmp(matches[i].pd_name, match_buf->pd_name,
			     sizeof(match_buf->pd_name)) != 0))
			continue;

		return(0);
	}

	return(1);
}

/*
 * Locate the parent of a PCI device by scanning the PCI devlist
 * and return the entry for the parent.
 * For devices on PCI Bus 0 (the host bus), this is the PCI Host.
 * For devices on secondary PCI busses, this is that bus' PCI-PCI Bridge.
 */

pcicfgregs *
pci_devlist_get_parent(pcicfgregs *cfg)
{
	struct devlist *devlist_head;
	struct pci_devinfo *dinfo;
	pcicfgregs *bridge_cfg;
	int i;

	dinfo = STAILQ_FIRST(devlist_head = &pci_devq);

	/* If the device is on PCI bus 0, look for the host */
	if (cfg->bus == 0) {
		for (i = 0; (dinfo != NULL) && (i < pci_numdevs);
		dinfo = STAILQ_NEXT(dinfo, pci_links), i++) {
			bridge_cfg = &dinfo->cfg;
			if (bridge_cfg->baseclass == PCIC_BRIDGE
				&& bridge_cfg->subclass == PCIS_BRIDGE_HOST
		    		&& bridge_cfg->bus == cfg->bus) {
				return bridge_cfg;
			}
		}
	}

	/* If the device is not on PCI bus 0, look for the PCI-PCI bridge */
	if (cfg->bus > 0) {
		for (i = 0; (dinfo != NULL) && (i < pci_numdevs);
		dinfo = STAILQ_NEXT(dinfo, pci_links), i++) {
			bridge_cfg = &dinfo->cfg;
			if (bridge_cfg->baseclass == PCIC_BRIDGE
				&& bridge_cfg->subclass == PCIS_BRIDGE_PCI
				&& bridge_cfg->secondarybus == cfg->bus) {
				return bridge_cfg;
			}
		}
	}

	return NULL; 
}

static int
pci_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct pci_io *io;
	int error;

	if (!(flag & FWRITE))
		return EPERM;


	switch(cmd) {
	case PCIOCGETCONF:
		{
		struct pci_devinfo *dinfo;
		struct pci_conf_io *cio;
		struct devlist *devlist_head;
		struct pci_match_conf *pattern_buf;
		int num_patterns;
		size_t iolen;
		int ionum, i;

		cio = (struct pci_conf_io *)data;

		num_patterns = 0;
		dinfo = NULL;

		/*
		 * Hopefully the user won't pass in a null pointer, but it
		 * can't hurt to check.
		 */
		if (cio == NULL) {
			error = EINVAL;
			break;
		}

		/*
		 * If the user specified an offset into the device list,
		 * but the list has changed since they last called this
		 * ioctl, tell them that the list has changed.  They will
		 * have to get the list from the beginning.
		 */
		if ((cio->offset != 0)
		 && (cio->generation != pci_generation)){
			cio->num_matches = 0;	
			cio->status = PCI_GETCONF_LIST_CHANGED;
			error = 0;
			break;
		}

		/*
		 * Check to see whether the user has asked for an offset
		 * past the end of our list.
		 */
		if (cio->offset >= pci_numdevs) {
			cio->num_matches = 0;
			cio->status = PCI_GETCONF_LAST_DEVICE;
			error = 0;
			break;
		}

		/* get the head of the device queue */
		devlist_head = &pci_devq;

		/*
		 * Determine how much room we have for pci_conf structures.
		 * Round the user's buffer size down to the nearest
		 * multiple of sizeof(struct pci_conf) in case the user
		 * didn't specify a multiple of that size.
		 */
		iolen = min(cio->match_buf_len - 
			    (cio->match_buf_len % sizeof(struct pci_conf)),
			    pci_numdevs * sizeof(struct pci_conf));

		/*
		 * Since we know that iolen is a multiple of the size of
		 * the pciconf union, it's okay to do this.
		 */
		ionum = iolen / sizeof(struct pci_conf);

		/*
		 * If this test is true, the user wants the pci_conf
		 * structures returned to match the supplied entries.
		 */
		if ((cio->num_patterns > 0)
		 && (cio->pat_buf_len > 0)) {
			/*
			 * pat_buf_len needs to be:
			 * num_patterns * sizeof(struct pci_match_conf)
			 * While it is certainly possible the user just
			 * allocated a large buffer, but set the number of
			 * matches correctly, it is far more likely that
			 * their kernel doesn't match the userland utility
			 * they're using.  It's also possible that the user
			 * forgot to initialize some variables.  Yes, this
			 * may be overly picky, but I hazard to guess that
			 * it's far more likely to just catch folks that
			 * updated their kernel but not their userland.
			 */
			if ((cio->num_patterns *
			    sizeof(struct pci_match_conf)) != cio->pat_buf_len){
				/* The user made a mistake, return an error*/
				cio->status = PCI_GETCONF_ERROR;
				printf("pci_ioctl: pat_buf_len %d != "
				       "num_patterns (%d) * sizeof(struct "
				       "pci_match_conf) (%d)\npci_ioctl: "
				       "pat_buf_len should be = %d\n",
				       cio->pat_buf_len, cio->num_patterns,
				       sizeof(struct pci_match_conf),
				       sizeof(struct pci_match_conf) * 
				       cio->num_patterns);
				printf("pci_ioctl: do your headers match your "
				       "kernel?\n");
				cio->num_matches = 0;
				error = EINVAL;
				break;
			}

			/*
			 * Check the user's buffer to make sure it's readable.
			 */
			if ((error = useracc((caddr_t)cio->patterns,
			                     cio->pat_buf_len, B_READ)) != 1){
				printf("pci_ioctl: pattern buffer %p, "
				       "length %u isn't user accessible for"
				       " READ\n", cio->patterns,
				       cio->pat_buf_len);
				error = EACCES;
				break;
			}
			/*
			 * Allocate a buffer to hold the patterns.
			 */
			pattern_buf = malloc(cio->pat_buf_len, M_TEMP,
					     M_WAITOK);
			error = copyin(cio->patterns, pattern_buf,
				       cio->pat_buf_len);
			if (error != 0)
				break;
			num_patterns = cio->num_patterns;

		} else if ((cio->num_patterns > 0)
			|| (cio->pat_buf_len > 0)) {
			/*
			 * The user made a mistake, spit out an error.
			 */
			cio->status = PCI_GETCONF_ERROR;
			cio->num_matches = 0;
			printf("pci_ioctl: invalid GETCONF arguments\n");
			error = EINVAL;
			break;
		} else
			pattern_buf = NULL;

		/*
		 * Make sure we can write to the match buffer.
		 */
		if ((error = useracc((caddr_t)cio->matches, cio->match_buf_len,
				     B_WRITE)) != 1) {
			printf("pci_ioctl: match buffer %p, length %u "
			       "isn't user accessible for WRITE\n",
			       cio->matches, cio->match_buf_len);
			error = EACCES;
			break;
		}

		/*
		 * Go through the list of devices and copy out the devices
		 * that match the user's criteria.
		 */
		for (cio->num_matches = 0, error = 0, i = 0,
		     dinfo = STAILQ_FIRST(devlist_head);
		     (dinfo != NULL) && (cio->num_matches < ionum)
		     && (error == 0) && (i < pci_numdevs);
		     dinfo = STAILQ_NEXT(dinfo, pci_links), i++) {

			if (i < cio->offset)
				continue;

			if ((pattern_buf == NULL) ||
			    (pci_conf_match(pattern_buf, num_patterns,
					    &dinfo->conf) == 0)) {

				/*
				 * If we've filled up the user's buffer,
				 * break out at this point.  Since we've
				 * got a match here, we'll pick right back
				 * up at the matching entry.  We can also
				 * tell the user that there are more matches
				 * left.
				 */
				if (cio->num_matches >= ionum)
					break;

				error = copyout(&dinfo->conf,
					        &cio->matches[cio->num_matches],
						sizeof(struct pci_conf));
				cio->num_matches++;
			}
		}

		/*
		 * Set the pointer into the list, so if the user is getting
		 * n records at a time, where n < pci_numdevs,
		 */
		cio->offset = i;

		/*
		 * Set the generation, the user will need this if they make
		 * another ioctl call with offset != 0.
		 */
		cio->generation = pci_generation;
		
		/*
		 * If this is the last device, inform the user so he won't
		 * bother asking for more devices.  If dinfo isn't NULL, we
		 * know that there are more matches in the list because of
		 * the way the traversal is done.
		 */
		if (dinfo == NULL)
			cio->status = PCI_GETCONF_LAST_DEVICE;
		else
			cio->status = PCI_GETCONF_MORE_DEVS;

		if (pattern_buf != NULL)
			free(pattern_buf, M_TEMP);

		break;
		}
	case PCIOCREAD:
		io = (struct pci_io *)data;
		switch(io->pi_width) {
			pcicfgregs probe;
		case 4:
		case 2:
		case 1:
			probe.bus = io->pi_sel.pc_bus;
			probe.slot = io->pi_sel.pc_dev;
			probe.func = io->pi_sel.pc_func;
			io->pi_data = pci_cfgread(&probe, 
						  io->pi_reg, io->pi_width);
			error = 0;
			break;
		default:
			error = ENODEV;
			break;
		}
		break;

	case PCIOCWRITE:
		io = (struct pci_io *)data;
		switch(io->pi_width) {
			pcicfgregs probe;
		case 4:
		case 2:
		case 1:
			probe.bus = io->pi_sel.pc_bus;
			probe.slot = io->pi_sel.pc_dev;
			probe.func = io->pi_sel.pc_func;
			pci_cfgwrite(&probe, 
				    io->pi_reg, io->pi_data, io->pi_width);
			error = 0;
			break;
		default:
			error = ENODEV;
			break;
		}
		break;

	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

#define	PCI_CDEV	78

static struct cdevsw pcicdev = {
	pci_open, pci_close, noread, nowrite, pci_ioctl, nostop, noreset,
	nodevtotty, seltrue, nommap, nostrategy, "pci", 0, PCI_CDEV
};

#ifdef DEVFS
static void *pci_devfs_token;
#endif

static void
pci_cdevinit(void *dummy)
{
	dev_t dev;

	dev = makedev(PCI_CDEV, 0);
	cdevsw_add(&dev, &pcicdev, NULL);
#ifdef	DEVFS
	pci_devfs_token = devfs_add_devswf(&pcicdev, 0, DV_CHR,
					   UID_ROOT, GID_WHEEL, 0644, "pci");
#endif
}

SYSINIT(pcidev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE+PCI_CDEV, pci_cdevinit, NULL);

#endif /* NPCI > 0 */
