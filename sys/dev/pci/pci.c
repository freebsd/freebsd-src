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
 * $FreeBSD$
 *
 */

#include "opt_bus.h"

#include "opt_simos.h"
#include "opt_compat_oldpci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <machine/md_var.h>		/* For the Alpha */

#include <sys/pciio.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include "pcib_if.h"

#ifdef __alpha__
#include <machine/rpb.h>
#endif

#ifdef APIC_IO
#include <machine/smp.h>
#endif /* APIC_IO */

static devclass_t	pci_devclass;

struct pci_quirk {
	u_int32_t devid;	/* Vendor/device of the card */
	int	type;
#define PCI_QUIRK_MAP_REG	1 /* PCI map register in wierd place */
	int	arg1;
	int	arg2;
};

struct pci_quirk pci_quirks[] = {
	/*
	 * The Intel 82371AB has a map register at offset 0x90.
	 */
	{ 0x71138086, PCI_QUIRK_MAP_REG,	0x90,	 0 },

	{ 0 }
};

/* map register information */
#define PCI_MAPMEM	0x01	/* memory map */
#define PCI_MAPMEMP	0x02	/* prefetchable memory map */
#define PCI_MAPPORT	0x04	/* port map */

static STAILQ_HEAD(devlist, pci_devinfo) pci_devq;
u_int32_t pci_numdevs = 0;
static u_int32_t pci_generation = 0;

/* return base address of memory or port map */

static u_int32_t
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
pci_readppb(device_t pcib, int b, int s, int f)
{
	pcih1cfgregs *p;

	p = malloc(sizeof (pcih1cfgregs), M_DEVBUF, M_WAITOK);
	if (p == NULL)
		return (NULL);

	bzero(p, sizeof *p);

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
pci_readpcb(device_t pcib, int b, int s, int f)
{
	pcih2cfgregs *p;

	p = malloc(sizeof (pcih2cfgregs), M_DEVBUF, M_WAITOK);
	if (p == NULL)
		return (NULL);

	bzero(p, sizeof *p);

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
pci_hdrtypedata(device_t pcib, int b, int s, int f, pcicfgregs *cfg)
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
		cfg->hdrspec        = pci_readppb(pcib, b, s, f);
		break;
	case 2:
		cfg->subvendor      = REG(PCIR_SUBVEND_2, 2);
		cfg->subdevice      = REG(PCIR_SUBDEV_2, 2);
		cfg->secondarybus   = REG(PCIR_SECBUS_2, 1);
		cfg->subordinatebus = REG(PCIR_SUBBUS_2, 1);
		cfg->nummaps	    = PCI_MAXMAPS_2;
		cfg->hdrspec        = pci_readpcb(pcib, b, s, f);
		break;
	}
#undef REG
}

/* read configuration header into pcicfgrect structure */

static struct pci_devinfo *
pci_read_device(device_t pcib, int b, int s, int f)
{
#define REG(n, w)	PCIB_READ_CONFIG(pcib, b, s, f, n, w)
	pcicfgregs *cfg = NULL;
	struct pci_devinfo *devlist_entry;
	struct devlist *devlist_head;

	devlist_head = &pci_devq;

	devlist_entry = NULL;

	if (PCIB_READ_CONFIG(pcib, b, s, f, PCIR_DEVVENDOR, 4) != -1) {
		devlist_entry = malloc(sizeof(struct pci_devinfo),
				       M_DEVBUF, M_WAITOK);
		if (devlist_entry == NULL)
			return (NULL);
		bzero(devlist_entry, sizeof *devlist_entry);

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

		pci_fixancient(cfg);
		pci_hdrtypedata(pcib, b, s, f, cfg);

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
#undef REG
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
	device_t pci, pcib;
	struct pci_io *io;
	const char *name;
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
				       (int)sizeof(struct pci_match_conf),
				       (int)sizeof(struct pci_match_conf) * 
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
			if (!useracc((caddr_t)cio->patterns,
				    cio->pat_buf_len, VM_PROT_READ)) {
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
		if (!useracc((caddr_t)cio->matches,
			     cio->match_buf_len, VM_PROT_WRITE)) {
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

			/* Populate pd_name and pd_unit */
			name = NULL;
			if (dinfo->cfg.dev && dinfo->conf.pd_name[0] == '\0')
				name = device_get_name(dinfo->cfg.dev);
			if (name) {
				strncpy(dinfo->conf.pd_name, name,
					sizeof(dinfo->conf.pd_name));
				dinfo->conf.pd_name[PCI_MAXNAMELEN] = 0;
				dinfo->conf.pd_unit =
					device_get_unit(dinfo->cfg.dev);
			}

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
		case 4:
		case 2:
		case 1:
			/*
			 * Assume that the user-level bus number is
			 * actually the pciN instance number. We map
			 * from that to the real pcib+bus combination.
			 */
			pci = devclass_get_device(pci_devclass,
						  io->pi_sel.pc_bus);
			if (pci) {
				int b = pcib_get_bus(pci);
				pcib = device_get_parent(pci);
				io->pi_data =
					PCIB_READ_CONFIG(pcib,
							 b,
							 io->pi_sel.pc_dev,
							 io->pi_sel.pc_func,
							 io->pi_reg,
							 io->pi_width);
				error = 0;
			} else {
				error = ENODEV;
			}
			break;
		default:
			error = ENODEV;
			break;
		}
		break;

	case PCIOCWRITE:
		io = (struct pci_io *)data;
		switch(io->pi_width) {
		case 4:
		case 2:
		case 1:
			/*
			 * Assume that the user-level bus number is
			 * actually the pciN instance number. We map
			 * from that to the real pcib+bus combination.
			 */
			pci = devclass_get_device(pci_devclass,
						  io->pi_sel.pc_bus);
			if (pci) {
				int b = pcib_get_bus(pci);
				pcib = device_get_parent(pci);
				PCIB_WRITE_CONFIG(pcib,
						  b,
						  io->pi_sel.pc_dev,
						  io->pi_sel.pc_func,
						  io->pi_reg,
						  io->pi_data,
						  io->pi_width);
				error = 0;
			} else {
				error = ENODEV;
			}
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
	/* open */	pci_open,
	/* close */	pci_close,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	pci_ioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"pci",
	/* maj */	PCI_CDEV,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};

#include "pci_if.h"

/*
 * New style pci driver.  Parent device is either a pci-host-bridge or a
 * pci-pci-bridge.  Both kinds are represented by instances of pcib.
 */

static void
pci_print_verbose(struct pci_devinfo *dinfo)
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
#ifdef PCI_DEBUG
		printf("\tcmdreg=0x%04x, statreg=0x%04x, cachelnsz=%d (dwords)\n", 
		       cfg->cmdreg, cfg->statreg, cfg->cachelnsz);
		printf("\tlattimer=0x%02x (%d ns), mingnt=0x%02x (%d ns), maxlat=0x%02x (%d ns)\n",
		       cfg->lattimer, cfg->lattimer * 30, 
		       cfg->mingnt, cfg->mingnt * 250, cfg->maxlat, cfg->maxlat * 250);
#endif /* PCI_DEBUG */
		if (cfg->intpin > 0)
			printf("\tintpin=%c, irq=%d\n", cfg->intpin +'a' -1, cfg->intline);
	}
}

static int
pci_porten(device_t pcib, int b, int s, int f)
{
	return (PCIB_READ_CONFIG(pcib, b, s, f, PCIR_COMMAND, 2)
		& PCIM_CMD_PORTEN) != 0;
}

static int
pci_memen(device_t pcib, int b, int s, int f)
{
	return (PCIB_READ_CONFIG(pcib, b, s, f, PCIR_COMMAND, 2)
		& PCIM_CMD_MEMEN) != 0;
}

/*
 * Add a resource based on a pci map register. Return 1 if the map
 * register is a 32bit map register or 2 if it is a 64bit register.
 */
static int
pci_add_map(device_t pcib, int b, int s, int f, int reg,
	    struct resource_list *rl)
{
	u_int32_t map;
	u_int64_t base;
	u_int8_t ln2size;
	u_int8_t ln2range;
	u_int32_t testval;
		
	int type;

	map = PCIB_READ_CONFIG(pcib, b, s, f, reg, 4);

	if (map == 0 || map == 0xffffffff)
		return 1; /* skip invalid entry */

	PCIB_WRITE_CONFIG(pcib, b, s, f, reg, 0xffffffff, 4);
	testval = PCIB_READ_CONFIG(pcib, b, s, f, reg, 4);
	PCIB_WRITE_CONFIG(pcib, b, s, f, reg, map, 4);

	base = pci_mapbase(map);
	if (pci_maptype(map) & PCI_MAPMEM)
		type = SYS_RES_MEMORY;
	else
		type = SYS_RES_IOPORT;
	ln2size = pci_mapsize(testval);
	ln2range = pci_maprange(testval);
	if (ln2range == 64) {
		/* Read the other half of a 64bit map register */
		base |= (u_int64_t) PCIB_READ_CONFIG(pcib, b, s, f, reg + 4, 4) << 32;
	}

	if (bootverbose) {
		printf("\tmap[%02x]: type %x, range %2d, base %08x, size %2d",
		       reg, pci_maptype(map), ln2range,
		       (unsigned int) base, ln2size);
		if (type == SYS_RES_IOPORT && !pci_porten(pcib, b, s, f))
			printf(", port disabled\n");
		else if (type == SYS_RES_MEMORY && !pci_memen(pcib, b, s, f))
			printf(", memory disabled\n");
		else
			printf(", enabled\n");
	}

	if (type == SYS_RES_IOPORT && !pci_porten(pcib, b, s, f))
		return 1;
	if (type == SYS_RES_MEMORY && !pci_memen(pcib, b, s, f))
		return 1;

	resource_list_add(rl, type, reg,
			  base, base + (1 << ln2size) - 1,
			  (1 << ln2size));

	return (ln2range == 64) ? 2 : 1;
}

static void
pci_add_resources(device_t pcib, int b, int s, int f, device_t dev)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	pcicfgregs *cfg = &dinfo->cfg;
	struct resource_list *rl = &dinfo->resources;
	struct pci_quirk *q;
	int i;

	for (i = 0; i < cfg->nummaps;) {
		i += pci_add_map(pcib, b, s, f, PCIR_MAPS + i*4, rl);
	}

	for (q = &pci_quirks[0]; q->devid; q++) {
		if (q->devid == ((cfg->device << 16) | cfg->vendor)
		    && q->type == PCI_QUIRK_MAP_REG)
			pci_add_map(pcib, b, s, f, q->arg1, rl);
	}

	if (cfg->intpin > 0 && cfg->intline != 255)
		resource_list_add(rl, SYS_RES_IRQ, 0,
				  cfg->intline, cfg->intline, 1);
}

static void
pci_add_children(device_t dev, int busno)
{
	device_t pcib = device_get_parent(dev);
	int maxslots;
	int s, f;

	maxslots = PCIB_MAXSLOTS(pcib);
	
	for (s = 0; s <= maxslots; s++) {
		int pcifunchigh = 0;
		for (f = 0; f <= pcifunchigh; f++) {
			struct pci_devinfo *dinfo =
				pci_read_device(pcib, busno, s, f);
			if (dinfo != NULL) {
				if (dinfo->cfg.mfdev)
					pcifunchigh = 7;

				pci_print_verbose(dinfo);
				dinfo->cfg.dev = device_add_child(dev, NULL, -1);
				device_set_ivars(dinfo->cfg.dev, dinfo);
				pci_add_resources(pcib, busno, s, f,
						  dinfo->cfg.dev);
			}
		}
	}
}

static int
pci_probe(device_t dev)
{
	static int once;

	device_set_desc(dev, "PCI bus");

	if (bootverbose)
		device_printf(dev, "physical bus=%d\n", pcib_get_bus(dev));

	/*
	 * Since there can be multiple independantly numbered PCI
	 * busses on some large alpha systems, we can't use the unit
	 * number to decide what bus we are probing. We ask the parent 
	 * pcib what our bus number is.
	 */
	pci_add_children(dev, pcib_get_bus(dev));

	if (!once) {
		make_dev(&pcicdev, 0, UID_ROOT, GID_WHEEL, 0644, "pci");
		once++;
	}

	return 0;
}

static int
pci_print_resources(struct resource_list *rl, const char *name, int type,
		    const char *format)
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
pci_print_child(device_t dev, device_t child)
{
	struct pci_devinfo *dinfo;
	struct resource_list *rl;
	pcicfgregs *cfg;
	int retval = 0;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->cfg;
	rl = &dinfo->resources;

	retval += bus_print_child_header(dev, child);

	retval += pci_print_resources(rl, "port", SYS_RES_IOPORT, "%#lx");
	retval += pci_print_resources(rl, "mem", SYS_RES_MEMORY, "%#lx");
	retval += pci_print_resources(rl, "irq", SYS_RES_IRQ, "%ld");
	if (device_get_flags(dev))
		retval += printf(" flags %#x", device_get_flags(dev));

	retval += printf(" at device %d.%d", pci_get_slot(child),
			 pci_get_function(child));

	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static void
pci_probe_nomatch(device_t dev, device_t child)
{
	struct pci_devinfo *dinfo;
	pcicfgregs *cfg;
	const char *desc;
	int unknown;

	unknown = 0;
	dinfo = device_get_ivars(child);
	cfg = &dinfo->cfg;
	desc = pci_ata_match(child);
	if (!desc) desc = pci_usb_match(child);
	if (!desc) desc = pci_vga_match(child);
	if (!desc) desc = pci_chip_match(child);
	if (!desc) {
		desc = "unknown card";
		unknown++;
	}
	device_printf(dev, "<%s>", desc);
	if (bootverbose || unknown) {
		printf(" (vendor=0x%04x, dev=0x%04x)",
			cfg->vendor,
			cfg->device);
	}
	printf(" at %d.%d",
		pci_get_slot(child),
		pci_get_function(child));
	if (cfg->intpin > 0 && cfg->intline != 255) {
		printf(" irq %d", cfg->intline);
	}
	printf("\n");
                                      
	return;
}

static int
pci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct pci_devinfo *dinfo;
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
pci_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct pci_devinfo *dinfo;
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

static struct resource *
pci_alloc_resource(device_t dev, device_t child, int type, int *rid,
		   u_long start, u_long end, u_long count, u_int flags)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->resources;

	return resource_list_alloc(rl, dev, child, type, rid,
				   start, end, count, flags);
}

static int
pci_release_resource(device_t dev, device_t child, int type, int rid,
		     struct resource *r)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->resources;

	return resource_list_release(rl, dev, child, type, rid, r);
}

static int
pci_set_resource(device_t dev, device_t child, int type, int rid,
		 u_long start, u_long count)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->resources;

	resource_list_add(rl, type, rid, start, start + count - 1, count);
	return 0;
}

static int
pci_get_resource(device_t dev, device_t child, int type, int rid,
		 u_long *startp, u_long *countp)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
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
pci_delete_resource(device_t dev, device_t child, int type, int rid)
{
	printf("pci_delete_resource: PCI resources can not be deleted\n");
}

static u_int32_t
pci_read_config_method(device_t dev, device_t child, int reg, int width)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	return PCIB_READ_CONFIG(device_get_parent(dev),
				cfg->bus, cfg->slot, cfg->func,
				reg, width);
}

static void
pci_write_config_method(device_t dev, device_t child, int reg,
			u_int32_t val, int width)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	PCIB_WRITE_CONFIG(device_get_parent(dev),
			  cfg->bus, cfg->slot, cfg->func,
			  reg, val, width);
}

static int
pci_modevent(module_t mod, int what, void *arg)
{
	switch (what) {
	case MOD_LOAD:
		STAILQ_INIT(&pci_devq);
		break;

	case MOD_UNLOAD:
		break;
	}

	return 0;
}

static device_method_t pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pci_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	pci_print_child),
	DEVMETHOD(bus_probe_nomatch,	pci_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	pci_read_ivar),
	DEVMETHOD(bus_write_ivar,	pci_write_ivar),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_alloc_resource,	pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	pci_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_set_resource,	pci_set_resource),
	DEVMETHOD(bus_get_resource,	pci_get_resource),
	DEVMETHOD(bus_delete_resource,	pci_delete_resource),

	/* PCI interface */
	DEVMETHOD(pci_read_config,	pci_read_config_method),
	DEVMETHOD(pci_write_config,	pci_write_config_method),

	{ 0, 0 }
};

static driver_t pci_driver = {
	"pci",
	pci_methods,
	1,			/* no softc */
};
DRIVER_MODULE(pci, pcib, pci_driver, pci_devclass, pci_modevent, 0);
