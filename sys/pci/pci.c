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
 * $Id: pci.c,v 1.75 1997/05/28 10:01:03 se Exp $
 *
 */

#include "pci.h"
#if NPCI > 0

#include <stddef.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /* DEVFS */

#include <vm/vm.h>
#include <vm/pmap.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <pci/pci_ioctl.h>

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
	int i;
	pcimap *map;
	int map64 = 0;

	for (i = 0; i < maxmaps; i++) {
		int reg = PCIR_MAPS + i*4;
		u_int32_t base;
		u_int32_t ln2range;

		base = pci_cfgread(cfg, reg, 4);
		ln2range = pci_maprange(base);

		if (base == 0 || ln2range == 0)
			maxmaps = i;
		else if (ln2range > 32)
			i++;
	}

	map = malloc(maxmaps * sizeof (pcimap), M_DEVBUF, M_WAITOK);
	if (map != NULL) {
		bzero(map, sizeof(pcimap) * maxmaps);

		for (i = 0; i < maxmaps; i++) {
			int reg = PCIR_MAPS + i*4;
			u_int32_t base;
			u_int32_t testval;

			base = pci_cfgread(cfg, reg, 4);

			if (map64 == 0) {
				pci_cfgwrite(cfg, reg, 0xffffffff, 4);
				testval = pci_cfgread(cfg, reg, 4);
				pci_cfgwrite(cfg, reg, base, 4);

				map[i].base     = pci_mapbase(base);
				map[i].type     = pci_maptype(base);
				map[i].ln2size  = pci_mapsize(testval);
				map[i].ln2range = pci_maprange(testval);
				map64 = map[i].ln2range == 64;
			} else {
				/* only fill in base, other fields are 0 */
				map[i].base     = base;
				map64 = 0;
			}
		}
		cfg->nummaps = maxmaps;
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
	if (cfg->class == PCIC_BRIDGE && cfg->subclass == PCIS_BRIDGE_PCI)
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

static pcicfgregs *
pci_readcfg(pcicfgregs *probe)
{
	pcicfgregs *cfg = NULL;

	if (pci_cfgread(probe, PCIR_DEVVENDOR, 4) != -1) {
		cfg = malloc(sizeof (pcicfgregs), M_DEVBUF, M_WAITOK);
		if (cfg == NULL)
			return (cfg);

		bzero(cfg, sizeof *cfg);

		cfg->bus		= probe->bus;
		cfg->slot		= probe->slot;
		cfg->func		= probe->func;
		cfg->parent		= probe->parent;

		cfg->vendor		= pci_cfgread(cfg, PCIR_VENDOR, 2);
		cfg->device		= pci_cfgread(cfg, PCIR_DEVICE, 2);
		cfg->cmdreg		= pci_cfgread(cfg, PCIR_COMMAND, 2);
		cfg->statreg		= pci_cfgread(cfg, PCIR_STATUS, 2);
		cfg->class		= pci_cfgread(cfg, PCIR_CLASS, 1);
		cfg->subclass		= pci_cfgread(cfg, PCIR_SUBCLASS, 1);
		cfg->progif		= pci_cfgread(cfg, PCIR_PROGIF, 1);
		cfg->revid		= pci_cfgread(cfg, PCIR_REVID, 1);
		cfg->hdrtype		= pci_cfgread(cfg, PCIR_HEADERTYPE, 1);
		cfg->cachelnsz		= pci_cfgread(cfg, PCIR_CACHELNSZ, 1);
		cfg->lattimer		= pci_cfgread(cfg, PCIR_LATTIMER, 1);
		cfg->intpin		= pci_cfgread(cfg, PCIR_INTPIN, 1);
		cfg->intline		= pci_cfgread(cfg, PCIR_INTLINE, 1);

#ifdef APIC_IO
		if (cfg->intpin != 0) {
			int airq;

			airq = get_pci_apic_irq(cfg->bus,
						cfg->slot, cfg->intpin);

			if ((airq >= 0) && (airq != cfg->intline)) {
				undirect_pci_irq(cfg->intline);
				cfg->intline = airq;
			}
		}
#endif  /* APIC_IO */

		cfg->mingnt		= pci_cfgread(cfg, PCIR_MINGNT, 1);
		cfg->maxlat		= pci_cfgread(cfg, PCIR_MAXLAT, 1);

		cfg->mfdev		= (cfg->hdrtype & PCIM_MFDEV) != 0;
		cfg->hdrtype		&= ~PCIM_MFDEV;

		pci_fixancient(cfg);
		pci_hdrtypedata(cfg);
	}
	return (cfg);
}

/* free pcicfgregs structure and all depending data structures */

static int
pci_freecfg(pcicfgregs *cfg)
{
	if (cfg->hdrspec != NULL)
		free(cfg->hdrspec, M_DEVBUF);
	if (cfg->map != NULL)
		free(cfg->map, M_DEVBUF);
	free(cfg, M_DEVBUF);
	return (0);
}

static void
pci_addcfg(pcicfgregs *cfg)
{
	if (bootverbose) {
		int i;
		printf("found->\tvendor=0x%04x, dev=0x%04x, revid=0x%02x\n", 
		       cfg->vendor, cfg->device, cfg->revid);
		printf("\tclass=%02x-%02x-%02x, hdrtype=0x%02x, mfdev=%d\n",
		       cfg->class, cfg->subclass, cfg->progif, cfg->hdrtype, cfg->mfdev);
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
	pci_drvattach(cfg); /* XXX currently defined in pci_compat.c */
}

/* return pointer to device that is a bridge to this bus */

static pcicfgregs *
pci_bridgeto(int bus)
{
	return (NULL); /* XXX not yet implemented */
}

/* scan one PCI bus for devices */

static int
pci_probebus(int bus)
{
	pcicfgregs probe;
	int bushigh = bus;

	bzero(&probe, sizeof probe);
	probe.parent = pci_bridgeto(bus);
	probe.bus = bus;
	for (probe.slot = 0; probe.slot <= PCI_SLOTMAX; probe.slot++) {
		int pcifunchigh = 0;
		for (probe.func = 0; probe.func <= pcifunchigh; probe.func++) {
			pcicfgregs *cfg = pci_readcfg(&probe);
			if (cfg != NULL) {
				if (cfg->mfdev)
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
				pci_addcfg(cfg);

				if (bushigh < cfg->subordinatebus)
					bushigh = cfg->subordinatebus;

				cfg = NULL; /* we don't own this anymore ... */
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

static int
pci_ioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	struct pci_conf_io *cio;
	struct pci_io *io;
	size_t iolen;
	int error;

	if (cmd != PCIOCGETCONF && !(flag & FWRITE))
		return EPERM;

	switch(cmd) {
	case PCIOCGETCONF:
#ifdef NOTYET
static struct pci_conf *pci_dev_list;
static unsigned pci_dev_list_count;
static unsigned pci_dev_list_size;

		cio = (struct pci_conf_io *)data;
		iolen = min(cio->pci_len, 
			    pci_dev_list_count * sizeof(struct pci_conf));
		cio->pci_len = pci_dev_list_count * sizeof(struct pci_conf);

		error = copyout(pci_dev_list, cio->pci_buf, iolen);
#else
		error = ENODEV;
#endif
		break;
		
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
	nodevtotty, noselect, nommap, nostrategy, "pci", 0, PCI_CDEV
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
