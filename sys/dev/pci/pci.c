/*-
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000, BSDi
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/pci/pci.c,v 1.264.2.9 2005/04/01 22:53:42 jmg Exp $");

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/linker.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <sys/pciio.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include "pcib_if.h"
#include "pci_if.h"

#if (defined(__i386__) && !defined(PC98)) || defined(__amd64__) || \
    defined (__ia64__)
#include <contrib/dev/acpica/acpi.h>
#include "acpi_if.h"
#else
#define ACPI_PWR_FOR_SLEEP(x, y, z)
#endif

static uint32_t		pci_mapbase(unsigned mapreg);
static int		pci_maptype(unsigned mapreg);
static int		pci_mapsize(unsigned testval);
static int		pci_maprange(unsigned mapreg);
static void		pci_fixancient(pcicfgregs *cfg);

static int		pci_porten(device_t pcib, int b, int s, int f);
static int		pci_memen(device_t pcib, int b, int s, int f);
static int		pci_add_map(device_t pcib, device_t bus, device_t dev,
			    int b, int s, int f, int reg,
			    struct resource_list *rl);
static void		pci_add_resources(device_t pcib, device_t bus,
			    device_t dev);
static int		pci_probe(device_t dev);
static int		pci_attach(device_t dev);
static void		pci_load_vendor_data(void);
static int		pci_describe_parse_line(char **ptr, int *vendor, 
			    int *device, char **desc);
static char		*pci_describe_device(device_t dev);
static int		pci_modevent(module_t mod, int what, void *arg);
static void		pci_hdrtypedata(device_t pcib, int b, int s, int f, 
			    pcicfgregs *cfg);
static void		pci_read_extcap(device_t pcib, pcicfgregs *cfg);
static void		pci_cfg_restore(device_t, struct pci_devinfo *);
static void		pci_cfg_save(device_t, struct pci_devinfo *, int);

static device_method_t pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pci_probe),
	DEVMETHOD(device_attach,	pci_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	pci_suspend),
	DEVMETHOD(device_resume,	pci_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	pci_print_child),
	DEVMETHOD(bus_probe_nomatch,	pci_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	pci_read_ivar),
	DEVMETHOD(bus_write_ivar,	pci_write_ivar),
	DEVMETHOD(bus_driver_added,	pci_driver_added),
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
	DEVMETHOD(bus_child_pnpinfo_str, pci_child_pnpinfo_str_method),
	DEVMETHOD(bus_child_location_str, pci_child_location_str_method),

	/* PCI interface */
	DEVMETHOD(pci_read_config,	pci_read_config_method),
	DEVMETHOD(pci_write_config,	pci_write_config_method),
	DEVMETHOD(pci_enable_busmaster,	pci_enable_busmaster_method),
	DEVMETHOD(pci_disable_busmaster, pci_disable_busmaster_method),
	DEVMETHOD(pci_enable_io,	pci_enable_io_method),
	DEVMETHOD(pci_disable_io,	pci_disable_io_method),
	DEVMETHOD(pci_get_powerstate,	pci_get_powerstate_method),
	DEVMETHOD(pci_set_powerstate,	pci_set_powerstate_method),
	DEVMETHOD(pci_assign_interrupt,	pci_assign_interrupt_method),

	{ 0, 0 }
};

DEFINE_CLASS_0(pci, pci_driver, pci_methods, 0);

devclass_t	pci_devclass;
DRIVER_MODULE(pci, pcib, pci_driver, pci_devclass, pci_modevent, 0);
MODULE_VERSION(pci, 1);

static char	*pci_vendordata;
static size_t	pci_vendordata_size;


struct pci_quirk {
	uint32_t devid;	/* Vendor/device of the card */
	int	type;
#define PCI_QUIRK_MAP_REG	1 /* PCI map register in weird place */
	int	arg1;
	int	arg2;
};

struct pci_quirk pci_quirks[] = {
	/* The Intel 82371AB and 82443MX has a map register at offset 0x90. */
	{ 0x71138086, PCI_QUIRK_MAP_REG,	0x90,	 0 },
	{ 0x719b8086, PCI_QUIRK_MAP_REG,	0x90,	 0 },
	/* As does the Serverworks OSB4 (the SMBus mapping register) */
	{ 0x02001166, PCI_QUIRK_MAP_REG,	0x90,	 0 },

	{ 0 }
};

/* map register information */
#define PCI_MAPMEM	0x01	/* memory map */
#define PCI_MAPMEMP	0x02	/* prefetchable memory map */
#define PCI_MAPPORT	0x04	/* port map */

struct devlist pci_devq;
uint32_t pci_generation;
uint32_t pci_numdevs = 0;

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, pci, CTLFLAG_RD, 0, "PCI bus tuning parameters");

static int pci_enable_io_modes = 1;
TUNABLE_INT("hw.pci.enable_io_modes", &pci_enable_io_modes);
SYSCTL_INT(_hw_pci, OID_AUTO, enable_io_modes, CTLFLAG_RW,
    &pci_enable_io_modes, 1,
    "Enable I/O and memory bits in the config register.  Some BIOSes do not\n\
enable these bits correctly.  We'd like to do this all the time, but there\n\
are some peripherals that this causes problems with.");

static int pci_do_powerstate = 0;
TUNABLE_INT("hw.pci.do_powerstate", &pci_do_powerstate);
SYSCTL_INT(_hw_pci, OID_AUTO, do_powerstate, CTLFLAG_RW,
    &pci_do_powerstate, 0,
    "Power down devices into D3 state when no driver attaches to them.\n\
Otherwise, leave the device in D0 state when no driver attaches.");

/* Find a device_t by bus/slot/function */

device_t
pci_find_bsf(uint8_t bus, uint8_t slot, uint8_t func)
{
	struct pci_devinfo *dinfo;

	STAILQ_FOREACH(dinfo, &pci_devq, pci_links) {
		if ((dinfo->cfg.bus == bus) &&
		    (dinfo->cfg.slot == slot) &&
		    (dinfo->cfg.func == func)) {
			return (dinfo->cfg.dev);
		}
	}

	return (NULL);
}

/* Find a device_t by vendor/device ID */

device_t
pci_find_device(uint16_t vendor, uint16_t device)
{
	struct pci_devinfo *dinfo;

	STAILQ_FOREACH(dinfo, &pci_devq, pci_links) {
		if ((dinfo->cfg.vendor == vendor) &&
		    (dinfo->cfg.device == device)) {
			return (dinfo->cfg.dev);
		}
	}

	return (NULL);
}

/* return base address of memory or port map */

static uint32_t
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
	static uint8_t maptype[0x10] = {
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
		cfg->nummaps	    = PCI_MAXMAPS_1;
		break;
	case 2:
		cfg->subvendor      = REG(PCIR_SUBVEND_2, 2);
		cfg->subdevice      = REG(PCIR_SUBDEV_2, 2);
		cfg->nummaps	    = PCI_MAXMAPS_2;
		break;
	}
#undef REG
}

/* read configuration header into pcicfgregs structure */

struct pci_devinfo *
pci_read_device(device_t pcib, int b, int s, int f, size_t size)
{
#define REG(n, w)	PCIB_READ_CONFIG(pcib, b, s, f, n, w)
	pcicfgregs *cfg = NULL;
	struct pci_devinfo *devlist_entry;
	struct devlist *devlist_head;

	devlist_head = &pci_devq;

	devlist_entry = NULL;

	if (REG(PCIR_DEVVENDOR, 4) != -1) {
		devlist_entry = malloc(size, M_DEVBUF, M_WAITOK | M_ZERO);
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
		cfg->hdrtype		= REG(PCIR_HDRTYPE, 1);
		cfg->cachelnsz		= REG(PCIR_CACHELNSZ, 1);
		cfg->lattimer		= REG(PCIR_LATTIMER, 1);
		cfg->intpin		= REG(PCIR_INTPIN, 1);
		cfg->intline		= REG(PCIR_INTLINE, 1);

		cfg->mingnt		= REG(PCIR_MINGNT, 1);
		cfg->maxlat		= REG(PCIR_MAXLAT, 1);

		cfg->mfdev		= (cfg->hdrtype & PCIM_MFDEV) != 0;
		cfg->hdrtype		&= ~PCIM_MFDEV;

		pci_fixancient(cfg);
		pci_hdrtypedata(pcib, b, s, f, cfg);

		if (REG(PCIR_STATUS, 2) & PCIM_STATUS_CAPPRESENT)
			pci_read_extcap(pcib, cfg);

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

static void
pci_read_extcap(device_t pcib, pcicfgregs *cfg)
{
#define REG(n, w)	PCIB_READ_CONFIG(pcib, cfg->bus, cfg->slot, cfg->func, n, w)
	int	ptr, nextptr, ptrptr;

	switch (cfg->hdrtype & PCIM_HDRTYPE) {
	case 0:
		ptrptr = PCIR_CAP_PTR;
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
		case PCIY_PMG:		/* PCI power management */
			if (cfg->pp.pp_cap == 0) {
				cfg->pp.pp_cap = REG(ptr + PCIR_POWER_CAP, 2);
				cfg->pp.pp_status = ptr + PCIR_POWER_STATUS;
				cfg->pp.pp_pmcsr = ptr + PCIR_POWER_PMCSR;
				if ((nextptr - ptr) > PCIR_POWER_DATA)
					cfg->pp.pp_data = ptr + PCIR_POWER_DATA;
			}
			break;
		case PCIY_MSI:		/* PCI MSI */
			cfg->msi.msi_ctrl = REG(ptr + PCIR_MSI_CTRL, 2);
			if (cfg->msi.msi_ctrl & PCIM_MSICTRL_64BIT)
				cfg->msi.msi_data = PCIR_MSI_DATA_64BIT;
			else
				cfg->msi.msi_data = PCIR_MSI_DATA;
			cfg->msi.msi_msgnum = 1 << ((cfg->msi.msi_ctrl &
						     PCIM_MSICTRL_MMC_MASK)>>1);
		default:
			break;
		}
	}
#undef REG
}

/* free pcicfgregs structure and all depending data structures */

int
pci_freecfg(struct pci_devinfo *dinfo)
{
	struct devlist *devlist_head;

	devlist_head = &pci_devq;

	STAILQ_REMOVE(devlist_head, dinfo, pci_devinfo, pci_links);
	free(dinfo, M_DEVBUF);

	/* increment the generation count */
	pci_generation++;

	/* we're losing one device */
	pci_numdevs--;
	return (0);
}

/*
 * PCI power manangement
 */
int
pci_set_powerstate_method(device_t dev, device_t child, int state)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;
	uint16_t status;
	int result, oldstate, highest, delay;

	if (cfg->pp.pp_cap == 0)
		return (EOPNOTSUPP);

	/*
	 * Optimize a no state change request away.  While it would be OK to
	 * write to the hardware in theory, some devices have shown odd
	 * behavior when going from D3 -> D3.
	 */
	oldstate = pci_get_powerstate(child);
	if (oldstate == state)
		return (0);

	/*
	 * The PCI power management specification states that after a state
	 * transition between PCI power states, system software must
	 * guarantee a minimal delay before the function accesses the device.
	 * Compute the worst case delay that we need to guarantee before we
	 * access the device.  Many devices will be responsive much more
	 * quickly than this delay, but there are some that don't respond
	 * instantly to state changes.  Transitions to/from D3 state require
	 * 10ms, while D2 requires 200us, and D0/1 require none.  The delay
	 * is done below with DELAY rather than a sleeper function because
	 * this function can be called from contexts where we cannot sleep.
	 */
	highest = (oldstate > state) ? oldstate : state;
	if (highest == PCI_POWERSTATE_D3)
	    delay = 10000;
	else if (highest == PCI_POWERSTATE_D2)
	    delay = 200;
	else
	    delay = 0;
	status = PCI_READ_CONFIG(dev, child, cfg->pp.pp_status, 2)
	    & ~PCIM_PSTAT_DMASK;
	result = 0;
	switch (state) {
	case PCI_POWERSTATE_D0:
		status |= PCIM_PSTAT_D0;
		break;
	case PCI_POWERSTATE_D1:
		if ((cfg->pp.pp_cap & PCIM_PCAP_D1SUPP) == 0)
			return (EOPNOTSUPP);
		status |= PCIM_PSTAT_D1;
		break;
	case PCI_POWERSTATE_D2:
		if ((cfg->pp.pp_cap & PCIM_PCAP_D2SUPP) == 0)
			return (EOPNOTSUPP);
		status |= PCIM_PSTAT_D2;
		break;
	case PCI_POWERSTATE_D3:
		status |= PCIM_PSTAT_D3;
		break;
	default:
		return (EINVAL);
	}

	if (bootverbose)
		printf(
		    "pci%d:%d:%d: Transition from D%d to D%d\n",
		    dinfo->cfg.bus, dinfo->cfg.slot, dinfo->cfg.func,
		    oldstate, state);

	PCI_WRITE_CONFIG(dev, child, cfg->pp.pp_status, status, 2);
	if (delay)
		DELAY(delay);
	return (0);
}

int
pci_get_powerstate_method(device_t dev, device_t child)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;
	uint16_t status;
	int result;

	if (cfg->pp.pp_cap != 0) {
		status = PCI_READ_CONFIG(dev, child, cfg->pp.pp_status, 2);
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

/*
 * Some convenience functions for PCI device drivers.
 */

static __inline void
pci_set_command_bit(device_t dev, device_t child, uint16_t bit)
{
	uint16_t	command;

	command = PCI_READ_CONFIG(dev, child, PCIR_COMMAND, 2);
	command |= bit;
	PCI_WRITE_CONFIG(dev, child, PCIR_COMMAND, command, 2);
}

static __inline void
pci_clear_command_bit(device_t dev, device_t child, uint16_t bit)
{
	uint16_t	command;

	command = PCI_READ_CONFIG(dev, child, PCIR_COMMAND, 2);
	command &= ~bit;
	PCI_WRITE_CONFIG(dev, child, PCIR_COMMAND, command, 2);
}

int
pci_enable_busmaster_method(device_t dev, device_t child)
{
	pci_set_command_bit(dev, child, PCIM_CMD_BUSMASTEREN);
	return (0);
}

int
pci_disable_busmaster_method(device_t dev, device_t child)
{
	pci_clear_command_bit(dev, child, PCIM_CMD_BUSMASTEREN);
	return (0);
}

int
pci_enable_io_method(device_t dev, device_t child, int space)
{
	uint16_t command;
	uint16_t bit;
	char *error;

	bit = 0;
	error = NULL;

	switch(space) {
	case SYS_RES_IOPORT:
		bit = PCIM_CMD_PORTEN;
		error = "port";
		break;
	case SYS_RES_MEMORY:
		bit = PCIM_CMD_MEMEN;
		error = "memory";
		break;
	default:
		return (EINVAL);
	}
	pci_set_command_bit(dev, child, bit);
	/* Some devices seem to need a brief stall here, what do to? */
	command = PCI_READ_CONFIG(dev, child, PCIR_COMMAND, 2);
	if (command & bit)
		return (0);
	device_printf(child, "failed to enable %s mapping!\n", error);
	return (ENXIO);
}

int
pci_disable_io_method(device_t dev, device_t child, int space)
{
	uint16_t command;
	uint16_t bit;
	char *error;

	bit = 0;
	error = NULL;

	switch(space) {
	case SYS_RES_IOPORT:
		bit = PCIM_CMD_PORTEN;
		error = "port";
		break;
	case SYS_RES_MEMORY:
		bit = PCIM_CMD_MEMEN;
		error = "memory";
		break;
	default:
		return (EINVAL);
	}
	pci_clear_command_bit(dev, child, bit);
	command = PCI_READ_CONFIG(dev, child, PCIR_COMMAND, 2);
	if (command & bit) {
		device_printf(child, "failed to disable %s mapping!\n", error);
		return (ENXIO);
	}
	return (0);
}

/*
 * New style pci driver.  Parent device is either a pci-host-bridge or a
 * pci-pci-bridge.  Both kinds are represented by instances of pcib.
 */

void
pci_print_verbose(struct pci_devinfo *dinfo)
{
	if (bootverbose) {
		pcicfgregs *cfg = &dinfo->cfg;

		printf("found->\tvendor=0x%04x, dev=0x%04x, revid=0x%02x\n", 
		    cfg->vendor, cfg->device, cfg->revid);
		printf("\tbus=%d, slot=%d, func=%d\n",
		    cfg->bus, cfg->slot, cfg->func);
		printf("\tclass=%02x-%02x-%02x, hdrtype=0x%02x, mfdev=%d\n",
		    cfg->baseclass, cfg->subclass, cfg->progif, cfg->hdrtype,
		    cfg->mfdev);
		printf("\tcmdreg=0x%04x, statreg=0x%04x, cachelnsz=%d (dwords)\n", 
		    cfg->cmdreg, cfg->statreg, cfg->cachelnsz);
		printf("\tlattimer=0x%02x (%d ns), mingnt=0x%02x (%d ns), maxlat=0x%02x (%d ns)\n",
		    cfg->lattimer, cfg->lattimer * 30, cfg->mingnt,
		    cfg->mingnt * 250, cfg->maxlat, cfg->maxlat * 250);
		if (cfg->intpin > 0)
			printf("\tintpin=%c, irq=%d\n",
			    cfg->intpin +'a' -1, cfg->intline);
		if (cfg->pp.pp_cap) {
			uint16_t status;

			status = pci_read_config(cfg->dev, cfg->pp.pp_status, 2);
			printf("\tpowerspec %d  supports D0%s%s D3  current D%d\n",
			    cfg->pp.pp_cap & PCIM_PCAP_SPEC,
			    cfg->pp.pp_cap & PCIM_PCAP_D1SUPP ? " D1" : "",
			    cfg->pp.pp_cap & PCIM_PCAP_D2SUPP ? " D2" : "",
			    status & PCIM_PSTAT_DMASK);
		}
		if (cfg->msi.msi_data) {
			int ctrl;

			ctrl =  cfg->msi.msi_ctrl;
			printf("\tMSI supports %d message%s%s%s\n",
			    cfg->msi.msi_msgnum,
			    (cfg->msi.msi_msgnum == 1) ? "" : "s",
			    (ctrl & PCIM_MSICTRL_64BIT) ? ", 64 bit" : "",
			    (ctrl & PCIM_MSICTRL_VECTOR) ? ", vector masks":"");
		}
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
pci_add_map(device_t pcib, device_t bus, device_t dev,
    int b, int s, int f, int reg, struct resource_list *rl)
{
	uint32_t map;
	uint64_t base;
	uint64_t start, end, count;
	uint8_t ln2size;
	uint8_t ln2range;
	uint32_t testval;
	uint16_t cmd;
	int type;

	map = PCIB_READ_CONFIG(pcib, b, s, f, reg, 4);
	PCIB_WRITE_CONFIG(pcib, b, s, f, reg, 0xffffffff, 4);
	testval = PCIB_READ_CONFIG(pcib, b, s, f, reg, 4);
	PCIB_WRITE_CONFIG(pcib, b, s, f, reg, map, 4);

	if (pci_maptype(map) & PCI_MAPMEM)
		type = SYS_RES_MEMORY;
	else
		type = SYS_RES_IOPORT;
	ln2size = pci_mapsize(testval);
	ln2range = pci_maprange(testval);
	base = pci_mapbase(map);

	/*
	 * For I/O registers, if bottom bit is set, and the next bit up
	 * isn't clear, we know we have a BAR that doesn't conform to the
	 * spec, so ignore it.  Also, sanity check the size of the data
	 * areas to the type of memory involved.  Memory must be at least
	 * 32 bytes in size, while I/O ranges must be at least 4.
	 */
	if ((testval & 0x1) == 0x1 &&
	    (testval & 0x2) != 0)
		return (1);
	if ((type == SYS_RES_MEMORY && ln2size < 5) ||
	    (type == SYS_RES_IOPORT && ln2size < 2))
		return (1);

	if (ln2range == 64)
		/* Read the other half of a 64bit map register */
		base |= (uint64_t) PCIB_READ_CONFIG(pcib, b, s, f, reg + 4, 4) << 32;

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

	/*
	 * This code theoretically does the right thing, but has
	 * undesirable side effects in some cases where peripherals
	 * respond oddly to having these bits enabled.  Let the user
	 * be able to turn them off (since pci_enable_io_modes is 1 by
	 * default).
	 */
	if (pci_enable_io_modes) {
		/* Turn on resources that have been left off by a lazy BIOS */
		if (type == SYS_RES_IOPORT && !pci_porten(pcib, b, s, f)) {
			cmd = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_COMMAND, 2);
			cmd |= PCIM_CMD_PORTEN;
			PCIB_WRITE_CONFIG(pcib, b, s, f, PCIR_COMMAND, cmd, 2);
		}
		if (type == SYS_RES_MEMORY && !pci_memen(pcib, b, s, f)) {
			cmd = PCIB_READ_CONFIG(pcib, b, s, f, PCIR_COMMAND, 2);
			cmd |= PCIM_CMD_MEMEN;
			PCIB_WRITE_CONFIG(pcib, b, s, f, PCIR_COMMAND, cmd, 2);
		}
	} else {
		if (type == SYS_RES_IOPORT && !pci_porten(pcib, b, s, f))
			return (1);
		if (type == SYS_RES_MEMORY && !pci_memen(pcib, b, s, f))
			return (1);
	}
	/*
	 * If base is 0, then we have problems.  It is best to ignore
	 * such entires for the moment.  These will be allocated later if
	 * the driver specifically requests them.
	 */
	if (base == 0)
		return 1;

	start = base;
	end = base + (1 << ln2size) - 1;
	count = 1 << ln2size;
	resource_list_add(rl, type, reg, start, end, count);

	/*
	 * Not quite sure what to do on failure of allocating the resource
	 * since I can postulate several right answers.
	 */
	resource_list_alloc(rl, bus, dev, type, &reg, start, end, count, 0);
	return ((ln2range == 64) ? 2 : 1);
}

/*
 * For ATA devices we need to decide early what addressing mode to use.
 * Legacy demands that the primary and secondary ATA ports sits on the
 * same addresses that old ISA hardware did. This dictates that we use
 * those addresses and ignore the BAR's if we cannot set PCI native 
 * addressing mode.
 */
static void
pci_ata_maps(device_t pcib, device_t bus, device_t dev, int b,
	     int s, int f, struct resource_list *rl)
{
	int rid, type, progif;
#if 0
	/* if this device supports PCI native addressing use it */
	progif = pci_read_config(dev, PCIR_PROGIF, 1);
	if ((progif & 0x8a) == 0x8a) {
		if (pci_mapbase(pci_read_config(dev, PCIR_BAR(0), 4)) &&
		    pci_mapbase(pci_read_config(dev, PCIR_BAR(2), 4))) {
			printf("Trying ATA native PCI addressing mode\n");
			pci_write_config(dev, PCIR_PROGIF, progif | 0x05, 1);
		}
	}
#endif
	progif = pci_read_config(dev, PCIR_PROGIF, 1);
	type = SYS_RES_IOPORT;
	if (progif & PCIP_STORAGE_IDE_MODEPRIM) {
		pci_add_map(pcib, bus, dev, b, s, f, PCIR_BAR(0), rl);
		pci_add_map(pcib, bus, dev, b, s, f, PCIR_BAR(1), rl);
	}
	else {
		rid = PCIR_BAR(0);
		resource_list_add(rl, type, rid, 0x1f0, 0x1f7, 8);
		resource_list_alloc(rl, bus, dev, type, &rid, 0x1f0, 0x1f7,8,0);
		rid = PCIR_BAR(1);
		resource_list_add(rl, type, rid, 0x3f6, 0x3f6, 1);
		resource_list_alloc(rl, bus, dev, type, &rid, 0x3f6, 0x3f6,1,0);
	}
	if (progif & PCIP_STORAGE_IDE_MODESEC) {
		pci_add_map(pcib, bus, dev, b, s, f, PCIR_BAR(2), rl);
		pci_add_map(pcib, bus, dev, b, s, f, PCIR_BAR(3), rl);
	}
	else {
		rid = PCIR_BAR(2);
		resource_list_add(rl, type, rid, 0x170, 0x177, 8);
		resource_list_alloc(rl, bus, dev, type, &rid, 0x170, 0x177,8,0);
		rid = PCIR_BAR(3);
		resource_list_add(rl, type, rid, 0x376, 0x376, 1);
		resource_list_alloc(rl, bus, dev, type, &rid, 0x376, 0x376,1,0);
	}
	pci_add_map(pcib, bus, dev, b, s, f, PCIR_BAR(4), rl);
}

static void
pci_add_resources(device_t pcib, device_t bus, device_t dev)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	pcicfgregs *cfg = &dinfo->cfg;
	struct resource_list *rl = &dinfo->resources;
	struct pci_quirk *q;
	int b, i, irq, f, s;

	b = cfg->bus;
	s = cfg->slot;
	f = cfg->func;

	/* ATA devices needs special map treatment */
	if ((pci_get_class(dev) == PCIC_STORAGE) &&
	    (pci_get_subclass(dev) == PCIS_STORAGE_IDE) &&
	    (pci_get_progif(dev) & PCIP_STORAGE_IDE_MASTERDEV))
		pci_ata_maps(pcib, bus, dev, b, s, f, rl);
	else
		for (i = 0; i < cfg->nummaps;)
			i += pci_add_map(pcib, bus, dev, b, s, f, PCIR_BAR(i),
			    rl);

	for (q = &pci_quirks[0]; q->devid; q++) {
		if (q->devid == ((cfg->device << 16) | cfg->vendor)
		    && q->type == PCI_QUIRK_MAP_REG)
			pci_add_map(pcib, bus, dev, b, s, f, q->arg1, rl);
	}

	if (cfg->intpin > 0 && PCI_INTERRUPT_VALID(cfg->intline)) {
#if defined(__ia64__) || defined(__i386__) || defined(__amd64__) || \
		defined(__alpha__)
		/*
		 * Try to re-route interrupts. Sometimes the BIOS or
		 * firmware may leave bogus values in these registers.
		 * If the re-route fails, then just stick with what we
		 * have.
		 */
		irq = PCI_ASSIGN_INTERRUPT(bus, dev);
		if (PCI_INTERRUPT_VALID(irq)) {
			pci_write_config(dev, PCIR_INTLINE, irq, 1);
			cfg->intline = irq;
		} else
#endif
			irq = cfg->intline;
		resource_list_add(rl, SYS_RES_IRQ, 0, irq, irq, 1);
	}
}

void
pci_add_children(device_t dev, int busno, size_t dinfo_size)
{
#define REG(n, w)	PCIB_READ_CONFIG(pcib, busno, s, f, n, w)
	device_t pcib = device_get_parent(dev);
	struct pci_devinfo *dinfo;
	int maxslots;
	int s, f, pcifunchigh;
	uint8_t hdrtype;

	KASSERT(dinfo_size >= sizeof(struct pci_devinfo),
	    ("dinfo_size too small"));
	maxslots = PCIB_MAXSLOTS(pcib);	
	for (s = 0; s <= maxslots; s++) {
		pcifunchigh = 0;
		f = 0;
		hdrtype = REG(PCIR_HDRTYPE, 1);
		if ((hdrtype & PCIM_HDRTYPE) > PCI_MAXHDRTYPE)
			continue;
		if (hdrtype & PCIM_MFDEV)
			pcifunchigh = PCI_FUNCMAX;
		for (f = 0; f <= pcifunchigh; f++) {
			dinfo = pci_read_device(pcib, busno, s, f, dinfo_size);
			if (dinfo != NULL) {
				pci_add_child(dev, dinfo);
			}
		}
	}
#undef REG
}

void
pci_add_child(device_t bus, struct pci_devinfo *dinfo)
{
	device_t pcib;

	pcib = device_get_parent(bus);
	dinfo->cfg.dev = device_add_child(bus, NULL, -1);
	device_set_ivars(dinfo->cfg.dev, dinfo);
	pci_cfg_save(dinfo->cfg.dev, dinfo, 0);
	pci_cfg_restore(dinfo->cfg.dev, dinfo);
	pci_add_resources(pcib, bus, dinfo->cfg.dev);
	pci_print_verbose(dinfo);
}

static int
pci_probe(device_t dev)
{

	device_set_desc(dev, "PCI bus");

	/* Allow other subclasses to override this driver. */
	return (-1000);
}

static int
pci_attach(device_t dev)
{
	int busno;

	/*
	 * Since there can be multiple independantly numbered PCI
	 * busses on some large alpha systems, we can't use the unit
	 * number to decide what bus we are probing. We ask the parent 
	 * pcib what our bus number is.
	 */
	busno = pcib_get_bus(dev);
	if (bootverbose)
		device_printf(dev, "physical bus=%d\n", busno);

	pci_add_children(dev, busno, sizeof(struct pci_devinfo));

	return (bus_generic_attach(dev));
}

int
pci_suspend(device_t dev)
{
	int dstate, error, i, numdevs;
	device_t acpi_dev, child, *devlist;
	struct pci_devinfo *dinfo;

	/*
	 * Save the PCI configuration space for each child and set the
	 * device in the appropriate power state for this sleep state.
	 */
	acpi_dev = NULL;
	if (pci_do_powerstate)
		acpi_dev = devclass_get_device(devclass_find("acpi"), 0);
	device_get_children(dev, &devlist, &numdevs);
	for (i = 0; i < numdevs; i++) {
		child = devlist[i];
		dinfo = (struct pci_devinfo *) device_get_ivars(child);
		pci_cfg_save(child, dinfo, 0);
	}

	/* Suspend devices before potentially powering them down. */
	error = bus_generic_suspend(dev);
	if (error) {
		free(devlist, M_TEMP);
		return (error);
	}

	/*
	 * Always set the device to D3.  If ACPI suggests a different
	 * power state, use it instead.  If ACPI is not present, the
	 * firmware is responsible for managing device power.  Skip
	 * children who aren't attached since they are powered down
	 * separately.  Only manage type 0 devices for now.
	 */
	for (i = 0; acpi_dev && i < numdevs; i++) {
		child = devlist[i];
		dinfo = (struct pci_devinfo *) device_get_ivars(child);
		if (device_is_attached(child) && dinfo->cfg.hdrtype == 0) {
			dstate = PCI_POWERSTATE_D3;
			ACPI_PWR_FOR_SLEEP(acpi_dev, child, &dstate);
			pci_set_powerstate(child, dstate);
		}
	}
	free(devlist, M_TEMP);
	return (0);
}

int
pci_resume(device_t dev)
{
	int i, numdevs;
	device_t acpi_dev, child, *devlist;
	struct pci_devinfo *dinfo;

	/*
	 * Set each child to D0 and restore its PCI configuration space.
	 */
	acpi_dev = NULL;
	if (pci_do_powerstate)
		acpi_dev = devclass_get_device(devclass_find("acpi"), 0);
	device_get_children(dev, &devlist, &numdevs);
	for (i = 0; i < numdevs; i++) {
		/*
		 * Notify ACPI we're going to D0 but ignore the result.  If
		 * ACPI is not present, the firmware is responsible for
		 * managing device power.  Only manage type 0 devices for now.
		 */
		child = devlist[i];
		dinfo = (struct pci_devinfo *) device_get_ivars(child);
		if (acpi_dev && device_is_attached(child) &&
		    dinfo->cfg.hdrtype == 0) {
			ACPI_PWR_FOR_SLEEP(acpi_dev, child, NULL);
			pci_set_powerstate(child, PCI_POWERSTATE_D0);
		}

		/* Now the device is powered up, restore its config space. */
		pci_cfg_restore(child, dinfo);
	}
	free(devlist, M_TEMP);
	return (bus_generic_resume(dev));
}

static void
pci_load_vendor_data(void)
{
	caddr_t vendordata, info;

	if ((vendordata = preload_search_by_type("pci_vendor_data")) != NULL) {
		info = preload_search_info(vendordata, MODINFO_ADDR);
		pci_vendordata = *(char **)info;
		info = preload_search_info(vendordata, MODINFO_SIZE);
		pci_vendordata_size = *(size_t *)info;
		/* terminate the database */
		pci_vendordata[pci_vendordata_size] = '\n';
	}
}

void
pci_driver_added(device_t dev, driver_t *driver)
{
	int numdevs;
	device_t *devlist;
	device_t child;
	struct pci_devinfo *dinfo;
	int i;

	if (bootverbose)
		device_printf(dev, "driver added\n");
	DEVICE_IDENTIFY(driver, dev);
	device_get_children(dev, &devlist, &numdevs);
	for (i = 0; i < numdevs; i++) {
		child = devlist[i];
		if (device_get_state(child) != DS_NOTPRESENT)
			continue;
		dinfo = device_get_ivars(child);
		pci_print_verbose(dinfo);
/*XXX???*/	/* resource_list_init(&dinfo->cfg.resources); */
		if (bootverbose)
			printf("pci%d:%d:%d: reprobing on driver added\n",
			    dinfo->cfg.bus, dinfo->cfg.slot, dinfo->cfg.func);
		pci_cfg_restore(child, dinfo);
		if (device_probe_and_attach(child) != 0)
			pci_cfg_save(child, dinfo, 1);
	}
	free(devlist, M_TEMP);
}

int
pci_print_child(device_t dev, device_t child)
{
	struct pci_devinfo *dinfo;
	struct resource_list *rl;
	int retval = 0;

	dinfo = device_get_ivars(child);
	rl = &dinfo->resources;

	retval += bus_print_child_header(dev, child);

	retval += resource_list_print_type(rl, "port", SYS_RES_IOPORT, "%#lx");
	retval += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");
	if (device_get_flags(dev))
		retval += printf(" flags %#x", device_get_flags(dev));

	retval += printf(" at device %d.%d", pci_get_slot(child),
	    pci_get_function(child));

	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static struct
{
	int	class;
	int	subclass;
	char	*desc;
} pci_nomatch_tab[] = {
	{PCIC_OLD,		-1,			"old"},
	{PCIC_OLD,		PCIS_OLD_NONVGA,	"non-VGA display device"},
	{PCIC_OLD,		PCIS_OLD_VGA,		"VGA-compatible display device"},
	{PCIC_STORAGE,		-1,			"mass storage"},
	{PCIC_STORAGE,		PCIS_STORAGE_SCSI,	"SCSI"},
	{PCIC_STORAGE,		PCIS_STORAGE_IDE,	"ATA"},
	{PCIC_STORAGE,		PCIS_STORAGE_FLOPPY,	"floppy disk"},
	{PCIC_STORAGE,		PCIS_STORAGE_IPI,	"IPI"},
	{PCIC_STORAGE,		PCIS_STORAGE_RAID,	"RAID"},
	{PCIC_NETWORK,		-1,			"network"},
	{PCIC_NETWORK,		PCIS_NETWORK_ETHERNET,	"ethernet"},
	{PCIC_NETWORK,		PCIS_NETWORK_TOKENRING,	"token ring"},
	{PCIC_NETWORK,		PCIS_NETWORK_FDDI,	"fddi"},
	{PCIC_NETWORK,		PCIS_NETWORK_ATM,	"ATM"},
	{PCIC_NETWORK,		PCIS_NETWORK_ISDN,	"ISDN"},
	{PCIC_DISPLAY,		-1,			"display"},
	{PCIC_DISPLAY,		PCIS_DISPLAY_VGA,	"VGA"},
	{PCIC_DISPLAY,		PCIS_DISPLAY_XGA,	"XGA"},
	{PCIC_DISPLAY,		PCIS_DISPLAY_3D,	"3D"},
	{PCIC_MULTIMEDIA,	-1,			"multimedia"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_VIDEO,	"video"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_AUDIO,	"audio"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_TELE,	"telephony"},
	{PCIC_MEMORY,		-1,			"memory"},
	{PCIC_MEMORY,		PCIS_MEMORY_RAM,	"RAM"},
	{PCIC_MEMORY,		PCIS_MEMORY_FLASH,	"flash"},
	{PCIC_BRIDGE,		-1,			"bridge"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_HOST,	"HOST-PCI"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_ISA,	"PCI-ISA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_EISA,	"PCI-EISA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_MCA,	"PCI-MCA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_PCI,	"PCI-PCI"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_PCMCIA,	"PCI-PCMCIA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_NUBUS,	"PCI-NuBus"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_CARDBUS,	"PCI-CardBus"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_RACEWAY,	"PCI-RACEway"},
	{PCIC_SIMPLECOMM,	-1,			"simple comms"},
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_UART,	"UART"},	/* could detect 16550 */
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_PAR,	"parallel port"},
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_MULSER,	"multiport serial"},
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_MODEM,	"generic modem"},
	{PCIC_BASEPERIPH,	-1,			"base peripheral"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_PIC,	"interrupt controller"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_DMA,	"DMA controller"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_TIMER,	"timer"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_RTC,	"realtime clock"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_PCIHOT,	"PCI hot-plug controller"},
	{PCIC_INPUTDEV,		-1,			"input device"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_KEYBOARD,	"keyboard"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_DIGITIZER,"digitizer"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_MOUSE,	"mouse"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_SCANNER,	"scanner"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_GAMEPORT,	"gameport"},
	{PCIC_DOCKING,		-1,			"docking station"},
	{PCIC_PROCESSOR,	-1,			"processor"},
	{PCIC_SERIALBUS,	-1,			"serial bus"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_FW,	"FireWire"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_ACCESS,	"AccessBus"},	 
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_SSA,	"SSA"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_USB,	"USB"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_FC,	"Fibre Channel"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_SMBUS,	"SMBus"},
	{PCIC_WIRELESS,		-1,			"wireless controller"},
	{PCIC_WIRELESS,		PCIS_WIRELESS_IRDA,	"iRDA"},
	{PCIC_WIRELESS,		PCIS_WIRELESS_IR,	"IR"},
	{PCIC_WIRELESS,		PCIS_WIRELESS_RF,	"RF"},
	{PCIC_INTELLIIO,	-1,			"intelligent I/O controller"},
	{PCIC_INTELLIIO,	PCIS_INTELLIIO_I2O,	"I2O"},
	{PCIC_SATCOM,		-1,			"satellite communication"},
	{PCIC_SATCOM,		PCIS_SATCOM_TV,		"sat TV"},
	{PCIC_SATCOM,		PCIS_SATCOM_AUDIO,	"sat audio"},
	{PCIC_SATCOM,		PCIS_SATCOM_VOICE,	"sat voice"},
	{PCIC_SATCOM,		PCIS_SATCOM_DATA,	"sat data"},
	{PCIC_CRYPTO,		-1,			"encrypt/decrypt"},
	{PCIC_CRYPTO,		PCIS_CRYPTO_NETCOMP,	"network/computer crypto"},
	{PCIC_CRYPTO,		PCIS_CRYPTO_NETCOMP,	"entertainment crypto"},
	{PCIC_DASP,		-1,			"dasp"},
	{PCIC_DASP,		PCIS_DASP_DPIO,		"DPIO module"},
	{0, 0,		NULL}
};

void
pci_probe_nomatch(device_t dev, device_t child)
{
	int	i;
	char	*cp, *scp, *device;
	
	/*
	 * Look for a listing for this device in a loaded device database.
	 */
	if ((device = pci_describe_device(child)) != NULL) {
		device_printf(dev, "<%s>", device);
		free(device, M_DEVBUF);
	} else {
		/*
		 * Scan the class/subclass descriptions for a general
		 * description.
		 */
		cp = "unknown";
		scp = NULL;
		for (i = 0; pci_nomatch_tab[i].desc != NULL; i++) {
			if (pci_nomatch_tab[i].class == pci_get_class(child)) {
				if (pci_nomatch_tab[i].subclass == -1) {
					cp = pci_nomatch_tab[i].desc;
				} else if (pci_nomatch_tab[i].subclass ==
				    pci_get_subclass(child)) {
					scp = pci_nomatch_tab[i].desc;
				}
			}
		}
		device_printf(dev, "<%s%s%s>", 
		    cp ? cp : "",
		    ((cp != NULL) && (scp != NULL)) ? ", " : "",
		    scp ? scp : "");
	}
	printf(" at device %d.%d (no driver attached)\n",
	    pci_get_slot(child), pci_get_function(child));
	if (pci_do_powerstate)
		pci_cfg_save(child,
		    (struct pci_devinfo *) device_get_ivars(child), 1);
	return;
}

/*
 * Parse the PCI device database, if loaded, and return a pointer to a 
 * description of the device.
 *
 * The database is flat text formatted as follows:
 *
 * Any line not in a valid format is ignored.
 * Lines are terminated with newline '\n' characters.
 * 
 * A VENDOR line consists of the 4 digit (hex) vendor code, a TAB, then
 * the vendor name.
 * 
 * A DEVICE line is entered immediately below the corresponding VENDOR ID.
 * - devices cannot be listed without a corresponding VENDOR line.
 * A DEVICE line consists of a TAB, the 4 digit (hex) device code,
 * another TAB, then the device name.                                            
 */

/*
 * Assuming (ptr) points to the beginning of a line in the database,
 * return the vendor or device and description of the next entry.
 * The value of (vendor) or (device) inappropriate for the entry type
 * is set to -1.  Returns nonzero at the end of the database.
 *
 * Note that this is slightly unrobust in the face of corrupt data;
 * we attempt to safeguard against this by spamming the end of the
 * database with a newline when we initialise.
 */
static int
pci_describe_parse_line(char **ptr, int *vendor, int *device, char **desc) 
{
	char	*cp = *ptr;
	int	left;

	*device = -1;
	*vendor = -1;
	**desc = '\0';
	for (;;) {
		left = pci_vendordata_size - (cp - pci_vendordata);
		if (left <= 0) {
			*ptr = cp;
			return(1);
		}

		/* vendor entry? */
		if (*cp != '\t' &&
		    sscanf(cp, "%x\t%80[^\n]", vendor, *desc) == 2)
			break;
		/* device entry? */
		if (*cp == '\t' &&
		    sscanf(cp, "%x\t%80[^\n]", device, *desc) == 2)
			break;
		
		/* skip to next line */
		while (*cp != '\n' && left > 0) {
			cp++;
			left--;
		}
		if (*cp == '\n') {
			cp++;
			left--;
		}
	}
	/* skip to next line */
	while (*cp != '\n' && left > 0) {
		cp++;
		left--;
	}
	if (*cp == '\n' && left > 0)
		cp++;
	*ptr = cp;
	return(0);
}

static char *
pci_describe_device(device_t dev)
{
	int	vendor, device;
	char	*desc, *vp, *dp, *line;

	desc = vp = dp = NULL;
	
	/*
	 * If we have no vendor data, we can't do anything.
	 */
	if (pci_vendordata == NULL)
		goto out;

	/*
	 * Scan the vendor data looking for this device
	 */
	line = pci_vendordata;
	if ((vp = malloc(80, M_DEVBUF, M_NOWAIT)) == NULL)
		goto out;
	for (;;) {
		if (pci_describe_parse_line(&line, &vendor, &device, &vp))
			goto out;
		if (vendor == pci_get_vendor(dev))
			break;
	}
	if ((dp = malloc(80, M_DEVBUF, M_NOWAIT)) == NULL)
		goto out;
	for (;;) {
		if (pci_describe_parse_line(&line, &vendor, &device, &dp)) {
			*dp = 0;
			break;
		}
		if (vendor != -1) {
			*dp = 0;
			break;
		}
		if (device == pci_get_device(dev))
			break;
	}
	if (dp[0] == '\0')
		snprintf(dp, 80, "0x%x", pci_get_device(dev));
	if ((desc = malloc(strlen(vp) + strlen(dp) + 3, M_DEVBUF, M_NOWAIT)) !=
	    NULL)
		sprintf(desc, "%s, %s", vp, dp);
 out:
	if (vp != NULL)
		free(vp, M_DEVBUF);
	if (dp != NULL)
		free(dp, M_DEVBUF);
	return(desc);
}

int
pci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct pci_devinfo *dinfo;
	pcicfgregs *cfg;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->cfg;

	switch (which) {
	case PCI_IVAR_ETHADDR:
		/*
		 * The generic accessor doesn't deal with failure, so
		 * we set the return value, then return an error.
		 */
		*((uint8_t **) result) = NULL;
		return (EINVAL);
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
		return (ENOENT);
	}
	return (0);
}

int
pci_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct pci_devinfo *dinfo;

	dinfo = device_get_ivars(child);

	switch (which) {
	case PCI_IVAR_INTPIN:
		dinfo->cfg.intpin = value;
		return (0);
	case PCI_IVAR_ETHADDR:
	case PCI_IVAR_SUBVENDOR:
	case PCI_IVAR_SUBDEVICE:
	case PCI_IVAR_VENDOR:
	case PCI_IVAR_DEVICE:
	case PCI_IVAR_DEVID:
	case PCI_IVAR_CLASS:
	case PCI_IVAR_SUBCLASS:
	case PCI_IVAR_PROGIF:
	case PCI_IVAR_REVID:
	case PCI_IVAR_IRQ:
	case PCI_IVAR_BUS:
	case PCI_IVAR_SLOT:
	case PCI_IVAR_FUNCTION:
		return (EINVAL);	/* disallow for now */

	default:
		return (ENOENT);
	}
}


#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>
#include <sys/cons.h>

/*
 * List resources based on pci map registers, used for within ddb
 */

DB_SHOW_COMMAND(pciregs, db_pci_dump)
{
	struct pci_devinfo *dinfo;
	struct devlist *devlist_head;
	struct pci_conf *p;
	const char *name;
	int i, error, none_count, quit;

	none_count = 0;
	/* get the head of the device queue */
	devlist_head = &pci_devq;

	/*
	 * Go through the list of devices and print out devices
	 */
	db_setup_paging(db_simple_pager, &quit, db_lines_per_page);
	for (error = 0, i = 0, quit = 0,
	     dinfo = STAILQ_FIRST(devlist_head);
	     (dinfo != NULL) && (error == 0) && (i < pci_numdevs) && !quit;
	     dinfo = STAILQ_NEXT(dinfo, pci_links), i++) {

		/* Populate pd_name and pd_unit */
		name = NULL;
		if (dinfo->cfg.dev)
			name = device_get_name(dinfo->cfg.dev);

		p = &dinfo->conf;
		db_printf("%s%d@pci%d:%d:%d:\tclass=0x%06x card=0x%08x "
			"chip=0x%08x rev=0x%02x hdr=0x%02x\n",
			(name && *name) ? name : "none",
			(name && *name) ? (int)device_get_unit(dinfo->cfg.dev) :
			none_count++,
			p->pc_sel.pc_bus, p->pc_sel.pc_dev,
			p->pc_sel.pc_func, (p->pc_class << 16) |
			(p->pc_subclass << 8) | p->pc_progif,
			(p->pc_subdevice << 16) | p->pc_subvendor,
			(p->pc_device << 16) | p->pc_vendor,
			p->pc_revid, p->pc_hdr);
	}
}
#endif /* DDB */

static struct resource *
pci_alloc_map(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->resources;
	struct resource_list_entry *rle;
	struct resource *res;
	uint32_t map, testval;
	int mapsize;

	/*
	 * Weed out the bogons, and figure out how large the BAR/map
	 * is.  Bars that read back 0 here are bogus and unimplemented.
	 * Note: atapci in legacy mode are special and handled elsewhere
	 * in the code.  If you have a atapci device in legacy mode and
	 * it fails here, that other code is broken.
	 */
	res = NULL;
	map = pci_read_config(child, *rid, 4);
	pci_write_config(child, *rid, 0xffffffff, 4);
	testval = pci_read_config(child, *rid, 4);
	if (testval == 0)
		return (NULL);
	if (pci_maptype(testval) & PCI_MAPMEM) {
		if (type != SYS_RES_MEMORY) {
			device_printf(child,
			    "failed: rid %#x is memory, requested %d\n",
			    *rid, type);
			goto out;
		}
	} else {
		if (type != SYS_RES_IOPORT) {
			device_printf(child,
			    "failed: rid %#x is ioport, requested %d\n",
			    *rid, type);
			goto out;
		}
	}
	/*
	 * For real BARs, we need to override the size that
	 * the driver requests, because that's what the BAR
	 * actually uses and we would otherwise have a
	 * situation where we might allocate the excess to
	 * another driver, which won't work.
	 */
	mapsize = pci_mapsize(testval);
	count = 1 << mapsize;
	if (RF_ALIGNMENT(flags) < mapsize)
		flags = (flags & ~RF_ALIGNMENT_MASK) | RF_ALIGNMENT_LOG2(mapsize);
	
	/*
	 * Allocate enough resource, and then write back the
	 * appropriate bar for that resource.
	 */
	res = BUS_ALLOC_RESOURCE(device_get_parent(dev), child, type, rid,
	    start, end, count, flags);
	if (res == NULL) {
		device_printf(child, "%#lx bytes of rid %#x res %d failed.\n",
		    count, *rid, type);
		goto out;
	}
	resource_list_add(rl, type, *rid, start, end, count);
	rle = resource_list_find(rl, type, *rid);
	if (rle == NULL)
		panic("pci_alloc_map: unexpectedly can't find resource.");
	rle->res = res;
	if (bootverbose)
		device_printf(child,
		    "Lazy allocation of %#lx bytes rid %#x type %d at %#lx\n",
		    count, *rid, type, rman_get_start(res));
	map = rman_get_start(res);
out:;
	pci_write_config(child, *rid, map, 4);
	return (res);
}


struct resource *
pci_alloc_resource(device_t dev, device_t child, int type, int *rid,
		   u_long start, u_long end, u_long count, u_int flags)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->resources;
	struct resource_list_entry *rle;
	pcicfgregs *cfg = &dinfo->cfg;

	/*
	 * Perform lazy resource allocation
	 */
	if (device_get_parent(child) == dev) {
		switch (type) {
		case SYS_RES_IRQ:
			/*
			 * If the child device doesn't have an
			 * interrupt routed and is deserving of an
			 * interrupt, try to assign it one.
			 */
			if (!PCI_INTERRUPT_VALID(cfg->intline) &&
			    (cfg->intpin != 0)) {
				cfg->intline = PCI_ASSIGN_INTERRUPT(dev, child);
				if (PCI_INTERRUPT_VALID(cfg->intline)) {
					pci_write_config(child, PCIR_INTLINE,
					    cfg->intline, 1);
					resource_list_add(rl, SYS_RES_IRQ, 0,
					    cfg->intline, cfg->intline, 1);
				}
			}
			break;
		case SYS_RES_IOPORT:
		case SYS_RES_MEMORY:
			if (*rid < PCIR_BAR(cfg->nummaps)) {
				/*
				 * Enable the I/O mode.  We should
				 * also be assigning resources too
				 * when none are present.  The
				 * resource_list_alloc kind of sorta does
				 * this...
				 */
				if (PCI_ENABLE_IO(dev, child, type))
					return (NULL);
			}
			rle = resource_list_find(rl, type, *rid);
			if (rle == NULL)
				return (pci_alloc_map(dev, child, type, rid,
				    start, end, count, flags));
			break;
		}
		/*
		 * If we've already allocated the resource, then
		 * return it now.  But first we may need to activate
		 * it, since we don't allocate the resource as active
		 * above.  Normally this would be done down in the
		 * nexus, but since we short-circuit that path we have
		 * to do its job here.  Not sure if we should free the
		 * resource if it fails to activate.
		 */
		rle = resource_list_find(rl, type, *rid);
		if (rle != NULL && rle->res != NULL) {
			if (bootverbose)
				device_printf(child,
			    "Reserved %#lx bytes for rid %#x type %d at %#lx\n",
				    rman_get_size(rle->res), *rid, type,
				    rman_get_start(rle->res));
			if ((flags & RF_ACTIVE) && 
			    bus_generic_activate_resource(dev, child, type,
			    *rid, rle->res) != 0)
				return NULL;
			return (rle->res);
		}
	}
	return (resource_list_alloc(rl, dev, child, type, rid,
	    start, end, count, flags));
}

void
pci_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct pci_devinfo *dinfo;
	struct resource_list *rl;
	struct resource_list_entry *rle;

	if (device_get_parent(child) != dev)
		return;

	dinfo = device_get_ivars(child);
	rl = &dinfo->resources;
	rle = resource_list_find(rl, type, rid);
	if (rle) {
		if (rle->res) {
			if (rman_get_device(rle->res) != dev ||
			    rman_get_flags(rle->res) & RF_ACTIVE) {
				device_printf(dev, "delete_resource: "
				    "Resource still owned by child, oops. "
				    "(type=%d, rid=%d, addr=%lx)\n",
				    rle->type, rle->rid,
				    rman_get_start(rle->res));
				return;
			}
			bus_release_resource(dev, type, rid, rle->res);
		}
		resource_list_delete(rl, type, rid);
	}
	/* 
	 * Why do we turn off the PCI configuration BAR when we delete a
	 * resource? -- imp
	 */
	pci_write_config(child, rid, 0, 4);
	BUS_DELETE_RESOURCE(device_get_parent(dev), child, type, rid);
}

struct resource_list *
pci_get_resource_list (device_t dev, device_t child)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);

	return (&dinfo->resources);
}

uint32_t
pci_read_config_method(device_t dev, device_t child, int reg, int width)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	return (PCIB_READ_CONFIG(device_get_parent(dev),
	    cfg->bus, cfg->slot, cfg->func, reg, width));
}

void
pci_write_config_method(device_t dev, device_t child, int reg, 
    uint32_t val, int width)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	PCIB_WRITE_CONFIG(device_get_parent(dev),
	    cfg->bus, cfg->slot, cfg->func, reg, val, width);
}

int
pci_child_location_str_method(device_t dev, device_t child, char *buf,
    size_t buflen)
{
	struct pci_devinfo *dinfo;

	dinfo = device_get_ivars(child);
	snprintf(buf, buflen, "slot=%d function=%d", pci_get_slot(child),
	    pci_get_function(child));
	return (0);
}

int
pci_child_pnpinfo_str_method(device_t dev, device_t child, char *buf,
    size_t buflen)
{
	struct pci_devinfo *dinfo;
	pcicfgregs *cfg;

	dinfo = device_get_ivars(child);
	cfg = &dinfo->cfg;
	snprintf(buf, buflen, "vendor=0x%04x device=0x%04x subvendor=0x%04x "
	    "subdevice=0x%04x class=0x%02x%02x%02x", cfg->vendor, cfg->device,
	    cfg->subvendor, cfg->subdevice, cfg->baseclass, cfg->subclass,
	    cfg->progif);
	return (0);
}

int
pci_assign_interrupt_method(device_t dev, device_t child)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	return (PCIB_ROUTE_INTERRUPT(device_get_parent(dev), child,
	    cfg->intpin));
}

static int
pci_modevent(module_t mod, int what, void *arg)
{
	static struct cdev *pci_cdev;

	switch (what) {
	case MOD_LOAD:
		STAILQ_INIT(&pci_devq);
		pci_generation = 0;
		pci_cdev = make_dev(&pcicdev, 0, UID_ROOT, GID_WHEEL, 0644,
		    "pci");
		pci_load_vendor_data();
		break;

	case MOD_UNLOAD:
		destroy_dev(pci_cdev);
		break;
	}

	return (0);
}

static void
pci_cfg_restore(device_t dev, struct pci_devinfo *dinfo)
{
	int i;

	/*
	 * Only do header type 0 devices.  Type 1 devices are bridges,
	 * which we know need special treatment.  Type 2 devices are
	 * cardbus bridges which also require special treatment.
	 * Other types are unknown, and we err on the side of safety
	 * by ignoring them.
	 */
	if (dinfo->cfg.hdrtype != 0)
		return;

	/*
	 * Restore the device to full power mode.  We must do this
	 * before we restore the registers because moving from D3 to
	 * D0 will cause the chip's BARs and some other registers to
	 * be reset to some unknown power on reset values.  Cut down
	 * the noise on boot by doing nothing if we are already in
	 * state D0.
	 */
	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	}
	for (i = 0; i < dinfo->cfg.nummaps; i++)
		pci_write_config(dev, PCIR_BAR(i), dinfo->cfg.bar[i], 4);
	pci_write_config(dev, PCIR_BIOS, dinfo->cfg.bios, 4);
	pci_write_config(dev, PCIR_COMMAND, dinfo->cfg.cmdreg, 2);
	pci_write_config(dev, PCIR_INTLINE, dinfo->cfg.intline, 1);
	pci_write_config(dev, PCIR_INTPIN, dinfo->cfg.intpin, 1);
	pci_write_config(dev, PCIR_MINGNT, dinfo->cfg.mingnt, 1);
	pci_write_config(dev, PCIR_MAXLAT, dinfo->cfg.maxlat, 1);
	pci_write_config(dev, PCIR_CACHELNSZ, dinfo->cfg.cachelnsz, 1);
	pci_write_config(dev, PCIR_LATTIMER, dinfo->cfg.lattimer, 1);
	pci_write_config(dev, PCIR_PROGIF, dinfo->cfg.progif, 1);
	pci_write_config(dev, PCIR_REVID, dinfo->cfg.revid, 1);
}

static void
pci_cfg_save(device_t dev, struct pci_devinfo *dinfo, int setstate)
{
	int i;
	uint32_t cls;
	int ps;

	/*
	 * Only do header type 0 devices.  Type 1 devices are bridges, which
	 * we know need special treatment.  Type 2 devices are cardbus bridges
	 * which also require special treatment.  Other types are unknown, and
	 * we err on the side of safety by ignoring them.  Powering down
	 * bridges should not be undertaken lightly.
	 */
	if (dinfo->cfg.hdrtype != 0)
		return;
	for (i = 0; i < dinfo->cfg.nummaps; i++)
		dinfo->cfg.bar[i] = pci_read_config(dev, PCIR_BAR(i), 4);
	dinfo->cfg.bios = pci_read_config(dev, PCIR_BIOS, 4);

	/*
	 * Some drivers apparently write to these registers w/o updating our
	 * cached copy.  No harm happens if we update the copy, so do so here
	 * so we can restore them.  The COMMAND register is modified by the
	 * bus w/o updating the cache.  This should represent the normally
	 * writable portion of the 'defined' part of type 0 headers.  In
	 * theory we also need to save/restore the PCI capability structures
	 * we know about, but apart from power we don't know any that are
	 * writable.
	 */
	dinfo->cfg.subvendor = pci_read_config(dev, PCIR_SUBVEND_0, 2);
	dinfo->cfg.subdevice = pci_read_config(dev, PCIR_SUBDEV_0, 2);
	dinfo->cfg.vendor = pci_read_config(dev, PCIR_VENDOR, 2);
	dinfo->cfg.device = pci_read_config(dev, PCIR_DEVICE, 2);
	dinfo->cfg.cmdreg = pci_read_config(dev, PCIR_COMMAND, 2);
	dinfo->cfg.intline = pci_read_config(dev, PCIR_INTLINE, 1);
	dinfo->cfg.intpin = pci_read_config(dev, PCIR_INTPIN, 1);
	dinfo->cfg.mingnt = pci_read_config(dev, PCIR_MINGNT, 1);
	dinfo->cfg.maxlat = pci_read_config(dev, PCIR_MAXLAT, 1);
	dinfo->cfg.cachelnsz = pci_read_config(dev, PCIR_CACHELNSZ, 1);
	dinfo->cfg.lattimer = pci_read_config(dev, PCIR_LATTIMER, 1);
	dinfo->cfg.baseclass = pci_read_config(dev, PCIR_CLASS, 1);
	dinfo->cfg.subclass = pci_read_config(dev, PCIR_SUBCLASS, 1);
	dinfo->cfg.progif = pci_read_config(dev, PCIR_PROGIF, 1);
	dinfo->cfg.revid = pci_read_config(dev, PCIR_REVID, 1);

	/*
	 * don't set the state for display devices, base peripherals and
	 * memory devices since bad things happen when they are powered down.
	 * We should (a) have drivers that can easily detach and (b) use
	 * generic drivers for these devices so that some device actually
	 * attaches.  We need to make sure that when we implement (a) we don't
	 * power the device down on a reattach.
	 */
	cls = pci_get_class(dev);
	if (setstate && cls != PCIC_DISPLAY && cls != PCIC_MEMORY &&
	    cls != PCIC_BASEPERIPH) {
		/*
		 * PCI spec says we can only go into D3 state from D0 state.
		 * Transition from D[12] into D0 before going to D3 state.
		 */
		ps = pci_get_powerstate(dev);
		if (ps != PCI_POWERSTATE_D0 && ps != PCI_POWERSTATE_D3) {
			pci_set_powerstate(dev, PCI_POWERSTATE_D0);
		}
		if (pci_get_powerstate(dev) != PCI_POWERSTATE_D3) {
			pci_set_powerstate(dev, PCI_POWERSTATE_D3);
		}
	}
}
