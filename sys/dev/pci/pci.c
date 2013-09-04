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
__FBSDID("$FreeBSD$");

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
#include <sys/endian.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#if defined(__i386__) || defined(__amd64__) || defined(__powerpc__)
#include <machine/intr_machdep.h>
#endif

#include <sys/pciio.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include <dev/usb/controller/xhcireg.h>
#include <dev/usb/controller/ehcireg.h>
#include <dev/usb/controller/ohcireg.h>
#include <dev/usb/controller/uhcireg.h>

#include "pcib_if.h"
#include "pci_if.h"

#if (BUS_SPACE_MAXADDR > 0xFFFFFFFF)
#define	PCI_DMA_BOUNDARY	0x100000000
#endif

#define	PCIR_IS_BIOS(cfg, reg)						\
	(((cfg)->hdrtype == PCIM_HDRTYPE_NORMAL && reg == PCIR_BIOS) ||	\
	 ((cfg)->hdrtype == PCIM_HDRTYPE_BRIDGE && reg == PCIR_BIOS_1))

static int		pci_has_quirk(uint32_t devid, int quirk);
static pci_addr_t	pci_mapbase(uint64_t mapreg);
static const char	*pci_maptype(uint64_t mapreg);
static int		pci_mapsize(uint64_t testval);
static int		pci_maprange(uint64_t mapreg);
static pci_addr_t	pci_rombase(uint64_t mapreg);
static int		pci_romsize(uint64_t testval);
static void		pci_fixancient(pcicfgregs *cfg);
static int		pci_printf(pcicfgregs *cfg, const char *fmt, ...);

static int		pci_porten(device_t dev);
static int		pci_memen(device_t dev);
static void		pci_assign_interrupt(device_t bus, device_t dev,
			    int force_route);
static int		pci_add_map(device_t bus, device_t dev, int reg,
			    struct resource_list *rl, int force, int prefetch);
static int		pci_probe(device_t dev);
static int		pci_attach(device_t dev);
static void		pci_load_vendor_data(void);
static int		pci_describe_parse_line(char **ptr, int *vendor,
			    int *device, char **desc);
static char		*pci_describe_device(device_t dev);
static bus_dma_tag_t	pci_get_dma_tag(device_t bus, device_t dev);
static int		pci_modevent(module_t mod, int what, void *arg);
static void		pci_hdrtypedata(device_t pcib, int b, int s, int f,
			    pcicfgregs *cfg);
static void		pci_read_cap(device_t pcib, pcicfgregs *cfg);
static int		pci_read_vpd_reg(device_t pcib, pcicfgregs *cfg,
			    int reg, uint32_t *data);
#if 0
static int		pci_write_vpd_reg(device_t pcib, pcicfgregs *cfg,
			    int reg, uint32_t data);
#endif
static void		pci_read_vpd(device_t pcib, pcicfgregs *cfg);
static void		pci_disable_msi(device_t dev);
static void		pci_enable_msi(device_t dev, uint64_t address,
			    uint16_t data);
static void		pci_enable_msix(device_t dev, u_int index,
			    uint64_t address, uint32_t data);
static void		pci_mask_msix(device_t dev, u_int index);
static void		pci_unmask_msix(device_t dev, u_int index);
static int		pci_msi_blacklisted(void);
static int		pci_msix_blacklisted(void);
static void		pci_resume_msi(device_t dev);
static void		pci_resume_msix(device_t dev);
static int		pci_remap_intr_method(device_t bus, device_t dev,
			    u_int irq);

static device_method_t pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pci_probe),
	DEVMETHOD(device_attach,	pci_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	pci_suspend),
	DEVMETHOD(device_resume,	pci_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	pci_print_child),
	DEVMETHOD(bus_probe_nomatch,	pci_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	pci_read_ivar),
	DEVMETHOD(bus_write_ivar,	pci_write_ivar),
	DEVMETHOD(bus_driver_added,	pci_driver_added),
	DEVMETHOD(bus_setup_intr,	pci_setup_intr),
	DEVMETHOD(bus_teardown_intr,	pci_teardown_intr),

	DEVMETHOD(bus_get_dma_tag,	pci_get_dma_tag),
	DEVMETHOD(bus_get_resource_list,pci_get_resource_list),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_delete_resource,	pci_delete_resource),
	DEVMETHOD(bus_alloc_resource,	pci_alloc_resource),
	DEVMETHOD(bus_adjust_resource,	bus_generic_adjust_resource),
	DEVMETHOD(bus_release_resource,	pci_release_resource),
	DEVMETHOD(bus_activate_resource, pci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pci_deactivate_resource),
	DEVMETHOD(bus_child_detached,	pci_child_detached),
	DEVMETHOD(bus_child_pnpinfo_str, pci_child_pnpinfo_str_method),
	DEVMETHOD(bus_child_location_str, pci_child_location_str_method),
	DEVMETHOD(bus_remap_intr,	pci_remap_intr_method),

	/* PCI interface */
	DEVMETHOD(pci_read_config,	pci_read_config_method),
	DEVMETHOD(pci_write_config,	pci_write_config_method),
	DEVMETHOD(pci_enable_busmaster,	pci_enable_busmaster_method),
	DEVMETHOD(pci_disable_busmaster, pci_disable_busmaster_method),
	DEVMETHOD(pci_enable_io,	pci_enable_io_method),
	DEVMETHOD(pci_disable_io,	pci_disable_io_method),
	DEVMETHOD(pci_get_vpd_ident,	pci_get_vpd_ident_method),
	DEVMETHOD(pci_get_vpd_readonly,	pci_get_vpd_readonly_method),
	DEVMETHOD(pci_get_powerstate,	pci_get_powerstate_method),
	DEVMETHOD(pci_set_powerstate,	pci_set_powerstate_method),
	DEVMETHOD(pci_assign_interrupt,	pci_assign_interrupt_method),
	DEVMETHOD(pci_find_cap,		pci_find_cap_method),
	DEVMETHOD(pci_find_extcap,	pci_find_extcap_method),
	DEVMETHOD(pci_find_htcap,	pci_find_htcap_method),
	DEVMETHOD(pci_alloc_msi,	pci_alloc_msi_method),
	DEVMETHOD(pci_alloc_msix,	pci_alloc_msix_method),
	DEVMETHOD(pci_remap_msix,	pci_remap_msix_method),
	DEVMETHOD(pci_release_msi,	pci_release_msi_method),
	DEVMETHOD(pci_msi_count,	pci_msi_count_method),
	DEVMETHOD(pci_msix_count,	pci_msix_count_method),

	DEVMETHOD_END
};

DEFINE_CLASS_0(pci, pci_driver, pci_methods, sizeof(struct pci_softc));

static devclass_t pci_devclass;
DRIVER_MODULE(pci, pcib, pci_driver, pci_devclass, pci_modevent, NULL);
MODULE_VERSION(pci, 1);

static char	*pci_vendordata;
static size_t	pci_vendordata_size;

struct pci_quirk {
	uint32_t devid;	/* Vendor/device of the card */
	int	type;
#define	PCI_QUIRK_MAP_REG	1 /* PCI map register in weird place */
#define	PCI_QUIRK_DISABLE_MSI	2 /* Neither MSI nor MSI-X work */
#define	PCI_QUIRK_ENABLE_MSI_VM	3 /* Older chipset in VM where MSI works */
#define	PCI_QUIRK_UNMAP_REG	4 /* Ignore PCI map register */
#define	PCI_QUIRK_DISABLE_MSIX	5 /* MSI-X doesn't work */
	int	arg1;
	int	arg2;
};

static const struct pci_quirk pci_quirks[] = {
	/* The Intel 82371AB and 82443MX have a map register at offset 0x90. */
	{ 0x71138086, PCI_QUIRK_MAP_REG,	0x90,	 0 },
	{ 0x719b8086, PCI_QUIRK_MAP_REG,	0x90,	 0 },
	/* As does the Serverworks OSB4 (the SMBus mapping register) */
	{ 0x02001166, PCI_QUIRK_MAP_REG,	0x90,	 0 },

	/*
	 * MSI doesn't work with the ServerWorks CNB20-HE Host Bridge
	 * or the CMIC-SL (AKA ServerWorks GC_LE).
	 */
	{ 0x00141166, PCI_QUIRK_DISABLE_MSI,	0,	0 },
	{ 0x00171166, PCI_QUIRK_DISABLE_MSI,	0,	0 },

	/*
	 * MSI doesn't work on earlier Intel chipsets including
	 * E7500, E7501, E7505, 845, 865, 875/E7210, and 855.
	 */
	{ 0x25408086, PCI_QUIRK_DISABLE_MSI,	0,	0 },
	{ 0x254c8086, PCI_QUIRK_DISABLE_MSI,	0,	0 },
	{ 0x25508086, PCI_QUIRK_DISABLE_MSI,	0,	0 },
	{ 0x25608086, PCI_QUIRK_DISABLE_MSI,	0,	0 },
	{ 0x25708086, PCI_QUIRK_DISABLE_MSI,	0,	0 },
	{ 0x25788086, PCI_QUIRK_DISABLE_MSI,	0,	0 },
	{ 0x35808086, PCI_QUIRK_DISABLE_MSI,	0,	0 },

	/*
	 * MSI doesn't work with devices behind the AMD 8131 HT-PCIX
	 * bridge.
	 */
	{ 0x74501022, PCI_QUIRK_DISABLE_MSI,	0,	0 },

	/*
	 * MSI-X allocation doesn't work properly for devices passed through
	 * by VMware up to at least ESXi 5.1.
	 */
	{ 0x079015ad, PCI_QUIRK_DISABLE_MSIX,	0,	0 }, /* PCI/PCI-X */
	{ 0x07a015ad, PCI_QUIRK_DISABLE_MSIX,	0,	0 }, /* PCIe */

	/*
	 * Some virtualization environments emulate an older chipset
	 * but support MSI just fine.  QEMU uses the Intel 82440.
	 */
	{ 0x12378086, PCI_QUIRK_ENABLE_MSI_VM,	0,	0 },

	/*
	 * HPET MMIO base address may appear in Bar1 for AMD SB600 SMBus
	 * controller depending on SoftPciRst register (PM_IO 0x55 [7]).
	 * It prevents us from attaching hpet(4) when the bit is unset.
	 * Note this quirk only affects SB600 revision A13 and earlier.
	 * For SB600 A21 and later, firmware must set the bit to hide it.
	 * For SB700 and later, it is unused and hardcoded to zero.
	 */
	{ 0x43851002, PCI_QUIRK_UNMAP_REG,	0x14,	0 },

	{ 0 }
};

/* map register information */
#define	PCI_MAPMEM	0x01	/* memory map */
#define	PCI_MAPMEMP	0x02	/* prefetchable memory map */
#define	PCI_MAPPORT	0x04	/* port map */

struct devlist pci_devq;
uint32_t pci_generation;
uint32_t pci_numdevs = 0;
static int pcie_chipset, pcix_chipset;

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, pci, CTLFLAG_RD, 0, "PCI bus tuning parameters");

static int pci_enable_io_modes = 1;
TUNABLE_INT("hw.pci.enable_io_modes", &pci_enable_io_modes);
SYSCTL_INT(_hw_pci, OID_AUTO, enable_io_modes, CTLFLAG_RW,
    &pci_enable_io_modes, 1,
    "Enable I/O and memory bits in the config register.  Some BIOSes do not\n\
enable these bits correctly.  We'd like to do this all the time, but there\n\
are some peripherals that this causes problems with.");

static int pci_do_realloc_bars = 0;
TUNABLE_INT("hw.pci.realloc_bars", &pci_do_realloc_bars);
SYSCTL_INT(_hw_pci, OID_AUTO, realloc_bars, CTLFLAG_RW,
    &pci_do_realloc_bars, 0,
    "Attempt to allocate a new range for any BARs whose original firmware-assigned ranges fail to allocate during the initial device scan.");

static int pci_do_power_nodriver = 0;
TUNABLE_INT("hw.pci.do_power_nodriver", &pci_do_power_nodriver);
SYSCTL_INT(_hw_pci, OID_AUTO, do_power_nodriver, CTLFLAG_RW,
    &pci_do_power_nodriver, 0,
  "Place a function into D3 state when no driver attaches to it.  0 means\n\
disable.  1 means conservatively place devices into D3 state.  2 means\n\
agressively place devices into D3 state.  3 means put absolutely everything\n\
in D3 state.");

int pci_do_power_resume = 1;
TUNABLE_INT("hw.pci.do_power_resume", &pci_do_power_resume);
SYSCTL_INT(_hw_pci, OID_AUTO, do_power_resume, CTLFLAG_RW,
    &pci_do_power_resume, 1,
  "Transition from D3 -> D0 on resume.");

int pci_do_power_suspend = 1;
TUNABLE_INT("hw.pci.do_power_suspend", &pci_do_power_suspend);
SYSCTL_INT(_hw_pci, OID_AUTO, do_power_suspend, CTLFLAG_RW,
    &pci_do_power_suspend, 1,
  "Transition from D0 -> D3 on suspend.");

static int pci_do_msi = 1;
TUNABLE_INT("hw.pci.enable_msi", &pci_do_msi);
SYSCTL_INT(_hw_pci, OID_AUTO, enable_msi, CTLFLAG_RW, &pci_do_msi, 1,
    "Enable support for MSI interrupts");

static int pci_do_msix = 1;
TUNABLE_INT("hw.pci.enable_msix", &pci_do_msix);
SYSCTL_INT(_hw_pci, OID_AUTO, enable_msix, CTLFLAG_RW, &pci_do_msix, 1,
    "Enable support for MSI-X interrupts");

static int pci_honor_msi_blacklist = 1;
TUNABLE_INT("hw.pci.honor_msi_blacklist", &pci_honor_msi_blacklist);
SYSCTL_INT(_hw_pci, OID_AUTO, honor_msi_blacklist, CTLFLAG_RD,
    &pci_honor_msi_blacklist, 1, "Honor chipset blacklist for MSI/MSI-X");

#if defined(__i386__) || defined(__amd64__)
static int pci_usb_takeover = 1;
#else
static int pci_usb_takeover = 0;
#endif
TUNABLE_INT("hw.pci.usb_early_takeover", &pci_usb_takeover);
SYSCTL_INT(_hw_pci, OID_AUTO, usb_early_takeover, CTLFLAG_RDTUN,
    &pci_usb_takeover, 1, "Enable early takeover of USB controllers.\n\
Disable this if you depend on BIOS emulation of USB devices, that is\n\
you use USB devices (like keyboard or mouse) but do not load USB drivers");

static int
pci_has_quirk(uint32_t devid, int quirk)
{
	const struct pci_quirk *q;

	for (q = &pci_quirks[0]; q->devid; q++) {
		if (q->devid == devid && q->type == quirk)
			return (1);
	}
	return (0);
}

/* Find a device_t by bus/slot/function in domain 0 */

device_t
pci_find_bsf(uint8_t bus, uint8_t slot, uint8_t func)
{

	return (pci_find_dbsf(0, bus, slot, func));
}

/* Find a device_t by domain/bus/slot/function */

device_t
pci_find_dbsf(uint32_t domain, uint8_t bus, uint8_t slot, uint8_t func)
{
	struct pci_devinfo *dinfo;

	STAILQ_FOREACH(dinfo, &pci_devq, pci_links) {
		if ((dinfo->cfg.domain == domain) &&
		    (dinfo->cfg.bus == bus) &&
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

device_t
pci_find_class(uint8_t class, uint8_t subclass)
{
	struct pci_devinfo *dinfo;

	STAILQ_FOREACH(dinfo, &pci_devq, pci_links) {
		if (dinfo->cfg.baseclass == class &&
		    dinfo->cfg.subclass == subclass) {
			return (dinfo->cfg.dev);
		}
	}

	return (NULL);
}

static int
pci_printf(pcicfgregs *cfg, const char *fmt, ...)
{
	va_list ap;
	int retval;

	retval = printf("pci%d:%d:%d:%d: ", cfg->domain, cfg->bus, cfg->slot,
	    cfg->func);
	va_start(ap, fmt);
	retval += vprintf(fmt, ap);
	va_end(ap);
	return (retval);
}

/* return base address of memory or port map */

static pci_addr_t
pci_mapbase(uint64_t mapreg)
{

	if (PCI_BAR_MEM(mapreg))
		return (mapreg & PCIM_BAR_MEM_BASE);
	else
		return (mapreg & PCIM_BAR_IO_BASE);
}

/* return map type of memory or port map */

static const char *
pci_maptype(uint64_t mapreg)
{

	if (PCI_BAR_IO(mapreg))
		return ("I/O Port");
	if (mapreg & PCIM_BAR_MEM_PREFETCH)
		return ("Prefetchable Memory");
	return ("Memory");
}

/* return log2 of map size decoded for memory or port map */

static int
pci_mapsize(uint64_t testval)
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

/* return base address of device ROM */

static pci_addr_t
pci_rombase(uint64_t mapreg)
{

	return (mapreg & PCIM_BIOS_ADDR_MASK);
}

/* return log2 of map size decided for device ROM */

static int
pci_romsize(uint64_t testval)
{
	int ln2size;

	testval = pci_rombase(testval);
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
pci_maprange(uint64_t mapreg)
{
	int ln2range = 0;

	if (PCI_BAR_IO(mapreg))
		ln2range = 32;
	else
		switch (mapreg & PCIM_BAR_MEM_TYPE) {
		case PCIM_BAR_MEM_32:
			ln2range = 32;
			break;
		case PCIM_BAR_MEM_1MB:
			ln2range = 20;
			break;
		case PCIM_BAR_MEM_64:
			ln2range = 64;
			break;
		}
	return (ln2range);
}

/* adjust some values from PCI 1.0 devices to match 2.0 standards ... */

static void
pci_fixancient(pcicfgregs *cfg)
{
	if ((cfg->hdrtype & PCIM_HDRTYPE) != PCIM_HDRTYPE_NORMAL)
		return;

	/* PCI to PCI bridges use header type 1 */
	if (cfg->baseclass == PCIC_BRIDGE && cfg->subclass == PCIS_BRIDGE_PCI)
		cfg->hdrtype = PCIM_HDRTYPE_BRIDGE;
}

/* extract header type specific config data */

static void
pci_hdrtypedata(device_t pcib, int b, int s, int f, pcicfgregs *cfg)
{
#define	REG(n, w)	PCIB_READ_CONFIG(pcib, b, s, f, n, w)
	switch (cfg->hdrtype & PCIM_HDRTYPE) {
	case PCIM_HDRTYPE_NORMAL:
		cfg->subvendor      = REG(PCIR_SUBVEND_0, 2);
		cfg->subdevice      = REG(PCIR_SUBDEV_0, 2);
		cfg->nummaps	    = PCI_MAXMAPS_0;
		break;
	case PCIM_HDRTYPE_BRIDGE:
		cfg->nummaps	    = PCI_MAXMAPS_1;
		break;
	case PCIM_HDRTYPE_CARDBUS:
		cfg->subvendor      = REG(PCIR_SUBVEND_2, 2);
		cfg->subdevice      = REG(PCIR_SUBDEV_2, 2);
		cfg->nummaps	    = PCI_MAXMAPS_2;
		break;
	}
#undef REG
}

/* read configuration header into pcicfgregs structure */
struct pci_devinfo *
pci_read_device(device_t pcib, int d, int b, int s, int f, size_t size)
{
#define	REG(n, w)	PCIB_READ_CONFIG(pcib, b, s, f, n, w)
	pcicfgregs *cfg = NULL;
	struct pci_devinfo *devlist_entry;
	struct devlist *devlist_head;

	devlist_head = &pci_devq;

	devlist_entry = NULL;

	if (REG(PCIR_DEVVENDOR, 4) != 0xfffffffful) {
		devlist_entry = malloc(size, M_DEVBUF, M_WAITOK | M_ZERO);
		if (devlist_entry == NULL)
			return (NULL);

		cfg = &devlist_entry->cfg;

		cfg->domain		= d;
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
		STAILQ_INIT(&cfg->maps);

		pci_fixancient(cfg);
		pci_hdrtypedata(pcib, b, s, f, cfg);

		if (REG(PCIR_STATUS, 2) & PCIM_STATUS_CAPPRESENT)
			pci_read_cap(pcib, cfg);

		STAILQ_INSERT_TAIL(devlist_head, devlist_entry, pci_links);

		devlist_entry->conf.pc_sel.pc_domain = cfg->domain;
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
pci_read_cap(device_t pcib, pcicfgregs *cfg)
{
#define	REG(n, w)	PCIB_READ_CONFIG(pcib, cfg->bus, cfg->slot, cfg->func, n, w)
#define	WREG(n, v, w)	PCIB_WRITE_CONFIG(pcib, cfg->bus, cfg->slot, cfg->func, n, v, w)
#if defined(__i386__) || defined(__amd64__) || defined(__powerpc__)
	uint64_t addr;
#endif
	uint32_t val;
	int	ptr, nextptr, ptrptr;

	switch (cfg->hdrtype & PCIM_HDRTYPE) {
	case PCIM_HDRTYPE_NORMAL:
	case PCIM_HDRTYPE_BRIDGE:
		ptrptr = PCIR_CAP_PTR;
		break;
	case PCIM_HDRTYPE_CARDBUS:
		ptrptr = PCIR_CAP_PTR_2;	/* cardbus capabilities ptr */
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
		nextptr = REG(ptr + PCICAP_NEXTPTR, 1);

		/* Process this entry */
		switch (REG(ptr + PCICAP_ID, 1)) {
		case PCIY_PMG:		/* PCI power management */
			if (cfg->pp.pp_cap == 0) {
				cfg->pp.pp_cap = REG(ptr + PCIR_POWER_CAP, 2);
				cfg->pp.pp_status = ptr + PCIR_POWER_STATUS;
				cfg->pp.pp_bse = ptr + PCIR_POWER_BSE;
				if ((nextptr - ptr) > PCIR_POWER_DATA)
					cfg->pp.pp_data = ptr + PCIR_POWER_DATA;
			}
			break;
		case PCIY_HT:		/* HyperTransport */
			/* Determine HT-specific capability type. */
			val = REG(ptr + PCIR_HT_COMMAND, 2);

			if ((val & 0xe000) == PCIM_HTCAP_SLAVE)
				cfg->ht.ht_slave = ptr;

#if defined(__i386__) || defined(__amd64__) || defined(__powerpc__)
			switch (val & PCIM_HTCMD_CAP_MASK) {
			case PCIM_HTCAP_MSI_MAPPING:
				if (!(val & PCIM_HTCMD_MSI_FIXED)) {
					/* Sanity check the mapping window. */
					addr = REG(ptr + PCIR_HTMSI_ADDRESS_HI,
					    4);
					addr <<= 32;
					addr |= REG(ptr + PCIR_HTMSI_ADDRESS_LO,
					    4);
					if (addr != MSI_INTEL_ADDR_BASE)
						device_printf(pcib,
	    "HT device at pci%d:%d:%d:%d has non-default MSI window 0x%llx\n",
						    cfg->domain, cfg->bus,
						    cfg->slot, cfg->func,
						    (long long)addr);
				} else
					addr = MSI_INTEL_ADDR_BASE;

				cfg->ht.ht_msimap = ptr;
				cfg->ht.ht_msictrl = val;
				cfg->ht.ht_msiaddr = addr;
				break;
			}
#endif
			break;
		case PCIY_MSI:		/* PCI MSI */
			cfg->msi.msi_location = ptr;
			cfg->msi.msi_ctrl = REG(ptr + PCIR_MSI_CTRL, 2);
			cfg->msi.msi_msgnum = 1 << ((cfg->msi.msi_ctrl &
						     PCIM_MSICTRL_MMC_MASK)>>1);
			break;
		case PCIY_MSIX:		/* PCI MSI-X */
			cfg->msix.msix_location = ptr;
			cfg->msix.msix_ctrl = REG(ptr + PCIR_MSIX_CTRL, 2);
			cfg->msix.msix_msgnum = (cfg->msix.msix_ctrl &
			    PCIM_MSIXCTRL_TABLE_SIZE) + 1;
			val = REG(ptr + PCIR_MSIX_TABLE, 4);
			cfg->msix.msix_table_bar = PCIR_BAR(val &
			    PCIM_MSIX_BIR_MASK);
			cfg->msix.msix_table_offset = val & ~PCIM_MSIX_BIR_MASK;
			val = REG(ptr + PCIR_MSIX_PBA, 4);
			cfg->msix.msix_pba_bar = PCIR_BAR(val &
			    PCIM_MSIX_BIR_MASK);
			cfg->msix.msix_pba_offset = val & ~PCIM_MSIX_BIR_MASK;
			break;
		case PCIY_VPD:		/* PCI Vital Product Data */
			cfg->vpd.vpd_reg = ptr;
			break;
		case PCIY_SUBVENDOR:
			/* Should always be true. */
			if ((cfg->hdrtype & PCIM_HDRTYPE) ==
			    PCIM_HDRTYPE_BRIDGE) {
				val = REG(ptr + PCIR_SUBVENDCAP_ID, 4);
				cfg->subvendor = val & 0xffff;
				cfg->subdevice = val >> 16;
			}
			break;
		case PCIY_PCIX:		/* PCI-X */
			/*
			 * Assume we have a PCI-X chipset if we have
			 * at least one PCI-PCI bridge with a PCI-X
			 * capability.  Note that some systems with
			 * PCI-express or HT chipsets might match on
			 * this check as well.
			 */
			if ((cfg->hdrtype & PCIM_HDRTYPE) ==
			    PCIM_HDRTYPE_BRIDGE)
				pcix_chipset = 1;
			cfg->pcix.pcix_location = ptr;
			break;
		case PCIY_EXPRESS:	/* PCI-express */
			/*
			 * Assume we have a PCI-express chipset if we have
			 * at least one PCI-express device.
			 */
			pcie_chipset = 1;
			cfg->pcie.pcie_location = ptr;
			val = REG(ptr + PCIER_FLAGS, 2);
			cfg->pcie.pcie_type = val & PCIEM_FLAGS_TYPE;
			break;
		default:
			break;
		}
	}

#if defined(__powerpc__)
	/*
	 * Enable the MSI mapping window for all HyperTransport
	 * slaves.  PCI-PCI bridges have their windows enabled via
	 * PCIB_MAP_MSI().
	 */
	if (cfg->ht.ht_slave != 0 && cfg->ht.ht_msimap != 0 &&
	    !(cfg->ht.ht_msictrl & PCIM_HTCMD_MSI_ENABLE)) {
		device_printf(pcib,
	    "Enabling MSI window for HyperTransport slave at pci%d:%d:%d:%d\n",
		    cfg->domain, cfg->bus, cfg->slot, cfg->func);
		 cfg->ht.ht_msictrl |= PCIM_HTCMD_MSI_ENABLE;
		 WREG(cfg->ht.ht_msimap + PCIR_HT_COMMAND, cfg->ht.ht_msictrl,
		     2);
	}
#endif
/* REG and WREG use carry through to next functions */
}

/*
 * PCI Vital Product Data
 */

#define	PCI_VPD_TIMEOUT		1000000

static int
pci_read_vpd_reg(device_t pcib, pcicfgregs *cfg, int reg, uint32_t *data)
{
	int count = PCI_VPD_TIMEOUT;

	KASSERT((reg & 3) == 0, ("VPD register must by 4 byte aligned"));

	WREG(cfg->vpd.vpd_reg + PCIR_VPD_ADDR, reg, 2);

	while ((REG(cfg->vpd.vpd_reg + PCIR_VPD_ADDR, 2) & 0x8000) != 0x8000) {
		if (--count < 0)
			return (ENXIO);
		DELAY(1);	/* limit looping */
	}
	*data = (REG(cfg->vpd.vpd_reg + PCIR_VPD_DATA, 4));

	return (0);
}

#if 0
static int
pci_write_vpd_reg(device_t pcib, pcicfgregs *cfg, int reg, uint32_t data)
{
	int count = PCI_VPD_TIMEOUT;

	KASSERT((reg & 3) == 0, ("VPD register must by 4 byte aligned"));

	WREG(cfg->vpd.vpd_reg + PCIR_VPD_DATA, data, 4);
	WREG(cfg->vpd.vpd_reg + PCIR_VPD_ADDR, reg | 0x8000, 2);
	while ((REG(cfg->vpd.vpd_reg + PCIR_VPD_ADDR, 2) & 0x8000) == 0x8000) {
		if (--count < 0)
			return (ENXIO);
		DELAY(1);	/* limit looping */
	}

	return (0);
}
#endif

#undef PCI_VPD_TIMEOUT

struct vpd_readstate {
	device_t	pcib;
	pcicfgregs	*cfg;
	uint32_t	val;
	int		bytesinval;
	int		off;
	uint8_t		cksum;
};

static int
vpd_nextbyte(struct vpd_readstate *vrs, uint8_t *data)
{
	uint32_t reg;
	uint8_t byte;

	if (vrs->bytesinval == 0) {
		if (pci_read_vpd_reg(vrs->pcib, vrs->cfg, vrs->off, &reg))
			return (ENXIO);
		vrs->val = le32toh(reg);
		vrs->off += 4;
		byte = vrs->val & 0xff;
		vrs->bytesinval = 3;
	} else {
		vrs->val = vrs->val >> 8;
		byte = vrs->val & 0xff;
		vrs->bytesinval--;
	}

	vrs->cksum += byte;
	*data = byte;
	return (0);
}

static void
pci_read_vpd(device_t pcib, pcicfgregs *cfg)
{
	struct vpd_readstate vrs;
	int state;
	int name;
	int remain;
	int i;
	int alloc, off;		/* alloc/off for RO/W arrays */
	int cksumvalid;
	int dflen;
	uint8_t byte;
	uint8_t byte2;

	/* init vpd reader */
	vrs.bytesinval = 0;
	vrs.off = 0;
	vrs.pcib = pcib;
	vrs.cfg = cfg;
	vrs.cksum = 0;

	state = 0;
	name = remain = i = 0;	/* shut up stupid gcc */
	alloc = off = 0;	/* shut up stupid gcc */
	dflen = 0;		/* shut up stupid gcc */
	cksumvalid = -1;
	while (state >= 0) {
		if (vpd_nextbyte(&vrs, &byte)) {
			state = -2;
			break;
		}
#if 0
		printf("vpd: val: %#x, off: %d, bytesinval: %d, byte: %#hhx, " \
		    "state: %d, remain: %d, name: %#x, i: %d\n", vrs.val,
		    vrs.off, vrs.bytesinval, byte, state, remain, name, i);
#endif
		switch (state) {
		case 0:		/* item name */
			if (byte & 0x80) {
				if (vpd_nextbyte(&vrs, &byte2)) {
					state = -2;
					break;
				}
				remain = byte2;
				if (vpd_nextbyte(&vrs, &byte2)) {
					state = -2;
					break;
				}
				remain |= byte2 << 8;
				if (remain > (0x7f*4 - vrs.off)) {
					state = -1;
					pci_printf(cfg,
					    "invalid VPD data, remain %#x\n",
					    remain);
				}
				name = byte & 0x7f;
			} else {
				remain = byte & 0x7;
				name = (byte >> 3) & 0xf;
			}
			switch (name) {
			case 0x2:	/* String */
				cfg->vpd.vpd_ident = malloc(remain + 1,
				    M_DEVBUF, M_WAITOK);
				i = 0;
				state = 1;
				break;
			case 0xf:	/* End */
				state = -1;
				break;
			case 0x10:	/* VPD-R */
				alloc = 8;
				off = 0;
				cfg->vpd.vpd_ros = malloc(alloc *
				    sizeof(*cfg->vpd.vpd_ros), M_DEVBUF,
				    M_WAITOK | M_ZERO);
				state = 2;
				break;
			case 0x11:	/* VPD-W */
				alloc = 8;
				off = 0;
				cfg->vpd.vpd_w = malloc(alloc *
				    sizeof(*cfg->vpd.vpd_w), M_DEVBUF,
				    M_WAITOK | M_ZERO);
				state = 5;
				break;
			default:	/* Invalid data, abort */
				state = -1;
				break;
			}
			break;

		case 1:	/* Identifier String */
			cfg->vpd.vpd_ident[i++] = byte;
			remain--;
			if (remain == 0)  {
				cfg->vpd.vpd_ident[i] = '\0';
				state = 0;
			}
			break;

		case 2:	/* VPD-R Keyword Header */
			if (off == alloc) {
				cfg->vpd.vpd_ros = reallocf(cfg->vpd.vpd_ros,
				    (alloc *= 2) * sizeof(*cfg->vpd.vpd_ros),
				    M_DEVBUF, M_WAITOK | M_ZERO);
			}
			cfg->vpd.vpd_ros[off].keyword[0] = byte;
			if (vpd_nextbyte(&vrs, &byte2)) {
				state = -2;
				break;
			}
			cfg->vpd.vpd_ros[off].keyword[1] = byte2;
			if (vpd_nextbyte(&vrs, &byte2)) {
				state = -2;
				break;
			}
			dflen = byte2;
			if (dflen == 0 &&
			    strncmp(cfg->vpd.vpd_ros[off].keyword, "RV",
			    2) == 0) {
				/*
				 * if this happens, we can't trust the rest
				 * of the VPD.
				 */
				pci_printf(cfg, "bad keyword length: %d\n",
				    dflen);
				cksumvalid = 0;
				state = -1;
				break;
			} else if (dflen == 0) {
				cfg->vpd.vpd_ros[off].value = malloc(1 *
				    sizeof(*cfg->vpd.vpd_ros[off].value),
				    M_DEVBUF, M_WAITOK);
				cfg->vpd.vpd_ros[off].value[0] = '\x00';
			} else
				cfg->vpd.vpd_ros[off].value = malloc(
				    (dflen + 1) *
				    sizeof(*cfg->vpd.vpd_ros[off].value),
				    M_DEVBUF, M_WAITOK);
			remain -= 3;
			i = 0;
			/* keep in sync w/ state 3's transistions */
			if (dflen == 0 && remain == 0)
				state = 0;
			else if (dflen == 0)
				state = 2;
			else
				state = 3;
			break;

		case 3:	/* VPD-R Keyword Value */
			cfg->vpd.vpd_ros[off].value[i++] = byte;
			if (strncmp(cfg->vpd.vpd_ros[off].keyword,
			    "RV", 2) == 0 && cksumvalid == -1) {
				if (vrs.cksum == 0)
					cksumvalid = 1;
				else {
					if (bootverbose)
						pci_printf(cfg,
					    "bad VPD cksum, remain %hhu\n",
						    vrs.cksum);
					cksumvalid = 0;
					state = -1;
					break;
				}
			}
			dflen--;
			remain--;
			/* keep in sync w/ state 2's transistions */
			if (dflen == 0)
				cfg->vpd.vpd_ros[off++].value[i++] = '\0';
			if (dflen == 0 && remain == 0) {
				cfg->vpd.vpd_rocnt = off;
				cfg->vpd.vpd_ros = reallocf(cfg->vpd.vpd_ros,
				    off * sizeof(*cfg->vpd.vpd_ros),
				    M_DEVBUF, M_WAITOK | M_ZERO);
				state = 0;
			} else if (dflen == 0)
				state = 2;
			break;

		case 4:
			remain--;
			if (remain == 0)
				state = 0;
			break;

		case 5:	/* VPD-W Keyword Header */
			if (off == alloc) {
				cfg->vpd.vpd_w = reallocf(cfg->vpd.vpd_w,
				    (alloc *= 2) * sizeof(*cfg->vpd.vpd_w),
				    M_DEVBUF, M_WAITOK | M_ZERO);
			}
			cfg->vpd.vpd_w[off].keyword[0] = byte;
			if (vpd_nextbyte(&vrs, &byte2)) {
				state = -2;
				break;
			}
			cfg->vpd.vpd_w[off].keyword[1] = byte2;
			if (vpd_nextbyte(&vrs, &byte2)) {
				state = -2;
				break;
			}
			cfg->vpd.vpd_w[off].len = dflen = byte2;
			cfg->vpd.vpd_w[off].start = vrs.off - vrs.bytesinval;
			cfg->vpd.vpd_w[off].value = malloc((dflen + 1) *
			    sizeof(*cfg->vpd.vpd_w[off].value),
			    M_DEVBUF, M_WAITOK);
			remain -= 3;
			i = 0;
			/* keep in sync w/ state 6's transistions */
			if (dflen == 0 && remain == 0)
				state = 0;
			else if (dflen == 0)
				state = 5;
			else
				state = 6;
			break;

		case 6:	/* VPD-W Keyword Value */
			cfg->vpd.vpd_w[off].value[i++] = byte;
			dflen--;
			remain--;
			/* keep in sync w/ state 5's transistions */
			if (dflen == 0)
				cfg->vpd.vpd_w[off++].value[i++] = '\0';
			if (dflen == 0 && remain == 0) {
				cfg->vpd.vpd_wcnt = off;
				cfg->vpd.vpd_w = reallocf(cfg->vpd.vpd_w,
				    off * sizeof(*cfg->vpd.vpd_w),
				    M_DEVBUF, M_WAITOK | M_ZERO);
				state = 0;
			} else if (dflen == 0)
				state = 5;
			break;

		default:
			pci_printf(cfg, "invalid state: %d\n", state);
			state = -1;
			break;
		}
	}

	if (cksumvalid == 0 || state < -1) {
		/* read-only data bad, clean up */
		if (cfg->vpd.vpd_ros != NULL) {
			for (off = 0; cfg->vpd.vpd_ros[off].value; off++)
				free(cfg->vpd.vpd_ros[off].value, M_DEVBUF);
			free(cfg->vpd.vpd_ros, M_DEVBUF);
			cfg->vpd.vpd_ros = NULL;
		}
	}
	if (state < -1) {
		/* I/O error, clean up */
		pci_printf(cfg, "failed to read VPD data.\n");
		if (cfg->vpd.vpd_ident != NULL) {
			free(cfg->vpd.vpd_ident, M_DEVBUF);
			cfg->vpd.vpd_ident = NULL;
		}
		if (cfg->vpd.vpd_w != NULL) {
			for (off = 0; cfg->vpd.vpd_w[off].value; off++)
				free(cfg->vpd.vpd_w[off].value, M_DEVBUF);
			free(cfg->vpd.vpd_w, M_DEVBUF);
			cfg->vpd.vpd_w = NULL;
		}
	}
	cfg->vpd.vpd_cached = 1;
#undef REG
#undef WREG
}

int
pci_get_vpd_ident_method(device_t dev, device_t child, const char **identptr)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;

	if (!cfg->vpd.vpd_cached && cfg->vpd.vpd_reg != 0)
		pci_read_vpd(device_get_parent(dev), cfg);

	*identptr = cfg->vpd.vpd_ident;

	if (*identptr == NULL)
		return (ENXIO);

	return (0);
}

int
pci_get_vpd_readonly_method(device_t dev, device_t child, const char *kw,
	const char **vptr)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;
	int i;

	if (!cfg->vpd.vpd_cached && cfg->vpd.vpd_reg != 0)
		pci_read_vpd(device_get_parent(dev), cfg);

	for (i = 0; i < cfg->vpd.vpd_rocnt; i++)
		if (memcmp(kw, cfg->vpd.vpd_ros[i].keyword,
		    sizeof(cfg->vpd.vpd_ros[i].keyword)) == 0) {
			*vptr = cfg->vpd.vpd_ros[i].value;
			return (0);
		}

	*vptr = NULL;
	return (ENXIO);
}

/*
 * Find the requested HyperTransport capability and return the offset
 * in configuration space via the pointer provided.  The function
 * returns 0 on success and an error code otherwise.
 */
int
pci_find_htcap_method(device_t dev, device_t child, int capability, int *capreg)
{
	int ptr, error;
	uint16_t val;

	error = pci_find_cap(child, PCIY_HT, &ptr);
	if (error)
		return (error);

	/*
	 * Traverse the capabilities list checking each HT capability
	 * to see if it matches the requested HT capability.
	 */
	while (ptr != 0) {
		val = pci_read_config(child, ptr + PCIR_HT_COMMAND, 2);
		if (capability == PCIM_HTCAP_SLAVE ||
		    capability == PCIM_HTCAP_HOST)
			val &= 0xe000;
		else
			val &= PCIM_HTCMD_CAP_MASK;
		if (val == capability) {
			if (capreg != NULL)
				*capreg = ptr;
			return (0);
		}

		/* Skip to the next HT capability. */
		while (ptr != 0) {
			ptr = pci_read_config(child, ptr + PCICAP_NEXTPTR, 1);
			if (pci_read_config(child, ptr + PCICAP_ID, 1) ==
			    PCIY_HT)
				break;
		}
	}
	return (ENOENT);
}

/*
 * Find the requested capability and return the offset in
 * configuration space via the pointer provided.  The function returns
 * 0 on success and an error code otherwise.
 */
int
pci_find_cap_method(device_t dev, device_t child, int capability,
    int *capreg)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;
	u_int32_t status;
	u_int8_t ptr;

	/*
	 * Check the CAP_LIST bit of the PCI status register first.
	 */
	status = pci_read_config(child, PCIR_STATUS, 2);
	if (!(status & PCIM_STATUS_CAPPRESENT))
		return (ENXIO);

	/*
	 * Determine the start pointer of the capabilities list.
	 */
	switch (cfg->hdrtype & PCIM_HDRTYPE) {
	case PCIM_HDRTYPE_NORMAL:
	case PCIM_HDRTYPE_BRIDGE:
		ptr = PCIR_CAP_PTR;
		break;
	case PCIM_HDRTYPE_CARDBUS:
		ptr = PCIR_CAP_PTR_2;
		break;
	default:
		/* XXX: panic? */
		return (ENXIO);		/* no extended capabilities support */
	}
	ptr = pci_read_config(child, ptr, 1);

	/*
	 * Traverse the capabilities list.
	 */
	while (ptr != 0) {
		if (pci_read_config(child, ptr + PCICAP_ID, 1) == capability) {
			if (capreg != NULL)
				*capreg = ptr;
			return (0);
		}
		ptr = pci_read_config(child, ptr + PCICAP_NEXTPTR, 1);
	}

	return (ENOENT);
}

/*
 * Find the requested extended capability and return the offset in
 * configuration space via the pointer provided.  The function returns
 * 0 on success and an error code otherwise.
 */
int
pci_find_extcap_method(device_t dev, device_t child, int capability,
    int *capreg)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;
	uint32_t ecap;
	uint16_t ptr;

	/* Only supported for PCI-express devices. */
	if (cfg->pcie.pcie_location == 0)
		return (ENXIO);

	ptr = PCIR_EXTCAP;
	ecap = pci_read_config(child, ptr, 4);
	if (ecap == 0xffffffff || ecap == 0)
		return (ENOENT);
	for (;;) {
		if (PCI_EXTCAP_ID(ecap) == capability) {
			if (capreg != NULL)
				*capreg = ptr;
			return (0);
		}
		ptr = PCI_EXTCAP_NEXTPTR(ecap);
		if (ptr == 0)
			break;
		ecap = pci_read_config(child, ptr, 4);
	}

	return (ENOENT);
}

/*
 * Support for MSI-X message interrupts.
 */
void
pci_enable_msix(device_t dev, u_int index, uint64_t address, uint32_t data)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	struct pcicfg_msix *msix = &dinfo->cfg.msix;
	uint32_t offset;

	KASSERT(msix->msix_table_len > index, ("bogus index"));
	offset = msix->msix_table_offset + index * 16;
	bus_write_4(msix->msix_table_res, offset, address & 0xffffffff);
	bus_write_4(msix->msix_table_res, offset + 4, address >> 32);
	bus_write_4(msix->msix_table_res, offset + 8, data);

	/* Enable MSI -> HT mapping. */
	pci_ht_map_msi(dev, address);
}

void
pci_mask_msix(device_t dev, u_int index)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	struct pcicfg_msix *msix = &dinfo->cfg.msix;
	uint32_t offset, val;

	KASSERT(msix->msix_msgnum > index, ("bogus index"));
	offset = msix->msix_table_offset + index * 16 + 12;
	val = bus_read_4(msix->msix_table_res, offset);
	if (!(val & PCIM_MSIX_VCTRL_MASK)) {
		val |= PCIM_MSIX_VCTRL_MASK;
		bus_write_4(msix->msix_table_res, offset, val);
	}
}

void
pci_unmask_msix(device_t dev, u_int index)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	struct pcicfg_msix *msix = &dinfo->cfg.msix;
	uint32_t offset, val;

	KASSERT(msix->msix_table_len > index, ("bogus index"));
	offset = msix->msix_table_offset + index * 16 + 12;
	val = bus_read_4(msix->msix_table_res, offset);
	if (val & PCIM_MSIX_VCTRL_MASK) {
		val &= ~PCIM_MSIX_VCTRL_MASK;
		bus_write_4(msix->msix_table_res, offset, val);
	}
}

int
pci_pending_msix(device_t dev, u_int index)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	struct pcicfg_msix *msix = &dinfo->cfg.msix;
	uint32_t offset, bit;

	KASSERT(msix->msix_table_len > index, ("bogus index"));
	offset = msix->msix_pba_offset + (index / 32) * 4;
	bit = 1 << index % 32;
	return (bus_read_4(msix->msix_pba_res, offset) & bit);
}

/*
 * Restore MSI-X registers and table during resume.  If MSI-X is
 * enabled then walk the virtual table to restore the actual MSI-X
 * table.
 */
static void
pci_resume_msix(device_t dev)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	struct pcicfg_msix *msix = &dinfo->cfg.msix;
	struct msix_table_entry *mte;
	struct msix_vector *mv;
	int i;

	if (msix->msix_alloc > 0) {
		/* First, mask all vectors. */
		for (i = 0; i < msix->msix_msgnum; i++)
			pci_mask_msix(dev, i);

		/* Second, program any messages with at least one handler. */
		for (i = 0; i < msix->msix_table_len; i++) {
			mte = &msix->msix_table[i];
			if (mte->mte_vector == 0 || mte->mte_handlers == 0)
				continue;
			mv = &msix->msix_vectors[mte->mte_vector - 1];
			pci_enable_msix(dev, i, mv->mv_address, mv->mv_data);
			pci_unmask_msix(dev, i);
		}
	}
	pci_write_config(dev, msix->msix_location + PCIR_MSIX_CTRL,
	    msix->msix_ctrl, 2);
}

/*
 * Attempt to allocate *count MSI-X messages.  The actual number allocated is
 * returned in *count.  After this function returns, each message will be
 * available to the driver as SYS_RES_IRQ resources starting at rid 1.
 */
int
pci_alloc_msix_method(device_t dev, device_t child, int *count)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;
	struct resource_list_entry *rle;
	int actual, error, i, irq, max;

	/* Don't let count == 0 get us into trouble. */
	if (*count == 0)
		return (EINVAL);

	/* If rid 0 is allocated, then fail. */
	rle = resource_list_find(&dinfo->resources, SYS_RES_IRQ, 0);
	if (rle != NULL && rle->res != NULL)
		return (ENXIO);

	/* Already have allocated messages? */
	if (cfg->msi.msi_alloc != 0 || cfg->msix.msix_alloc != 0)
		return (ENXIO);

	/* If MSI-X is blacklisted for this system, fail. */
	if (pci_msix_blacklisted())
		return (ENXIO);

	/* MSI-X capability present? */
	if (cfg->msix.msix_location == 0 || !pci_do_msix)
		return (ENODEV);

	/* Make sure the appropriate BARs are mapped. */
	rle = resource_list_find(&dinfo->resources, SYS_RES_MEMORY,
	    cfg->msix.msix_table_bar);
	if (rle == NULL || rle->res == NULL ||
	    !(rman_get_flags(rle->res) & RF_ACTIVE))
		return (ENXIO);
	cfg->msix.msix_table_res = rle->res;
	if (cfg->msix.msix_pba_bar != cfg->msix.msix_table_bar) {
		rle = resource_list_find(&dinfo->resources, SYS_RES_MEMORY,
		    cfg->msix.msix_pba_bar);
		if (rle == NULL || rle->res == NULL ||
		    !(rman_get_flags(rle->res) & RF_ACTIVE))
			return (ENXIO);
	}
	cfg->msix.msix_pba_res = rle->res;

	if (bootverbose)
		device_printf(child,
		    "attempting to allocate %d MSI-X vectors (%d supported)\n",
		    *count, cfg->msix.msix_msgnum);
	max = min(*count, cfg->msix.msix_msgnum);
	for (i = 0; i < max; i++) {
		/* Allocate a message. */
		error = PCIB_ALLOC_MSIX(device_get_parent(dev), child, &irq);
		if (error) {
			if (i == 0)
				return (error);
			break;
		}
		resource_list_add(&dinfo->resources, SYS_RES_IRQ, i + 1, irq,
		    irq, 1);
	}
	actual = i;

	if (bootverbose) {
		rle = resource_list_find(&dinfo->resources, SYS_RES_IRQ, 1);
		if (actual == 1)
			device_printf(child, "using IRQ %lu for MSI-X\n",
			    rle->start);
		else {
			int run;

			/*
			 * Be fancy and try to print contiguous runs of
			 * IRQ values as ranges.  'irq' is the previous IRQ.
			 * 'run' is true if we are in a range.
			 */
			device_printf(child, "using IRQs %lu", rle->start);
			irq = rle->start;
			run = 0;
			for (i = 1; i < actual; i++) {
				rle = resource_list_find(&dinfo->resources,
				    SYS_RES_IRQ, i + 1);

				/* Still in a run? */
				if (rle->start == irq + 1) {
					run = 1;
					irq++;
					continue;
				}

				/* Finish previous range. */
				if (run) {
					printf("-%d", irq);
					run = 0;
				}

				/* Start new range. */
				printf(",%lu", rle->start);
				irq = rle->start;
			}

			/* Unfinished range? */
			if (run)
				printf("-%d", irq);
			printf(" for MSI-X\n");
		}
	}

	/* Mask all vectors. */
	for (i = 0; i < cfg->msix.msix_msgnum; i++)
		pci_mask_msix(child, i);

	/* Allocate and initialize vector data and virtual table. */
	cfg->msix.msix_vectors = malloc(sizeof(struct msix_vector) * actual,
	    M_DEVBUF, M_WAITOK | M_ZERO);
	cfg->msix.msix_table = malloc(sizeof(struct msix_table_entry) * actual,
	    M_DEVBUF, M_WAITOK | M_ZERO);
	for (i = 0; i < actual; i++) {
		rle = resource_list_find(&dinfo->resources, SYS_RES_IRQ, i + 1);
		cfg->msix.msix_vectors[i].mv_irq = rle->start;
		cfg->msix.msix_table[i].mte_vector = i + 1;
	}

	/* Update control register to enable MSI-X. */
	cfg->msix.msix_ctrl |= PCIM_MSIXCTRL_MSIX_ENABLE;
	pci_write_config(child, cfg->msix.msix_location + PCIR_MSIX_CTRL,
	    cfg->msix.msix_ctrl, 2);

	/* Update counts of alloc'd messages. */
	cfg->msix.msix_alloc = actual;
	cfg->msix.msix_table_len = actual;
	*count = actual;
	return (0);
}

/*
 * By default, pci_alloc_msix() will assign the allocated IRQ
 * resources consecutively to the first N messages in the MSI-X table.
 * However, device drivers may want to use different layouts if they
 * either receive fewer messages than they asked for, or they wish to
 * populate the MSI-X table sparsely.  This method allows the driver
 * to specify what layout it wants.  It must be called after a
 * successful pci_alloc_msix() but before any of the associated
 * SYS_RES_IRQ resources are allocated via bus_alloc_resource().
 *
 * The 'vectors' array contains 'count' message vectors.  The array
 * maps directly to the MSI-X table in that index 0 in the array
 * specifies the vector for the first message in the MSI-X table, etc.
 * The vector value in each array index can either be 0 to indicate
 * that no vector should be assigned to a message slot, or it can be a
 * number from 1 to N (where N is the count returned from a
 * succcessful call to pci_alloc_msix()) to indicate which message
 * vector (IRQ) to be used for the corresponding message.
 *
 * On successful return, each message with a non-zero vector will have
 * an associated SYS_RES_IRQ whose rid is equal to the array index +
 * 1.  Additionally, if any of the IRQs allocated via the previous
 * call to pci_alloc_msix() are not used in the mapping, those IRQs
 * will be freed back to the system automatically.
 *
 * For example, suppose a driver has a MSI-X table with 6 messages and
 * asks for 6 messages, but pci_alloc_msix() only returns a count of
 * 3.  Call the three vectors allocated by pci_alloc_msix() A, B, and
 * C.  After the call to pci_alloc_msix(), the device will be setup to
 * have an MSI-X table of ABC--- (where - means no vector assigned).
 * If the driver then passes a vector array of { 1, 0, 1, 2, 0, 2 },
 * then the MSI-X table will look like A-AB-B, and the 'C' vector will
 * be freed back to the system.  This device will also have valid
 * SYS_RES_IRQ rids of 1, 3, 4, and 6.
 *
 * In any case, the SYS_RES_IRQ rid X will always map to the message
 * at MSI-X table index X - 1 and will only be valid if a vector is
 * assigned to that table entry.
 */
int
pci_remap_msix_method(device_t dev, device_t child, int count,
    const u_int *vectors)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct pcicfg_msix *msix = &dinfo->cfg.msix;
	struct resource_list_entry *rle;
	int i, irq, j, *used;

	/*
	 * Have to have at least one message in the table but the
	 * table can't be bigger than the actual MSI-X table in the
	 * device.
	 */
	if (count == 0 || count > msix->msix_msgnum)
		return (EINVAL);

	/* Sanity check the vectors. */
	for (i = 0; i < count; i++)
		if (vectors[i] > msix->msix_alloc)
			return (EINVAL);

	/*
	 * Make sure there aren't any holes in the vectors to be used.
	 * It's a big pain to support it, and it doesn't really make
	 * sense anyway.  Also, at least one vector must be used.
	 */
	used = malloc(sizeof(int) * msix->msix_alloc, M_DEVBUF, M_WAITOK |
	    M_ZERO);
	for (i = 0; i < count; i++)
		if (vectors[i] != 0)
			used[vectors[i] - 1] = 1;
	for (i = 0; i < msix->msix_alloc - 1; i++)
		if (used[i] == 0 && used[i + 1] == 1) {
			free(used, M_DEVBUF);
			return (EINVAL);
		}
	if (used[0] != 1) {
		free(used, M_DEVBUF);
		return (EINVAL);
	}
	
	/* Make sure none of the resources are allocated. */
	for (i = 0; i < msix->msix_table_len; i++) {
		if (msix->msix_table[i].mte_vector == 0)
			continue;
		if (msix->msix_table[i].mte_handlers > 0)
			return (EBUSY);
		rle = resource_list_find(&dinfo->resources, SYS_RES_IRQ, i + 1);
		KASSERT(rle != NULL, ("missing resource"));
		if (rle->res != NULL)
			return (EBUSY);
	}

	/* Free the existing resource list entries. */
	for (i = 0; i < msix->msix_table_len; i++) {
		if (msix->msix_table[i].mte_vector == 0)
			continue;
		resource_list_delete(&dinfo->resources, SYS_RES_IRQ, i + 1);
	}

	/*
	 * Build the new virtual table keeping track of which vectors are
	 * used.
	 */
	free(msix->msix_table, M_DEVBUF);
	msix->msix_table = malloc(sizeof(struct msix_table_entry) * count,
	    M_DEVBUF, M_WAITOK | M_ZERO);
	for (i = 0; i < count; i++)
		msix->msix_table[i].mte_vector = vectors[i];
	msix->msix_table_len = count;

	/* Free any unused IRQs and resize the vectors array if necessary. */
	j = msix->msix_alloc - 1;
	if (used[j] == 0) {
		struct msix_vector *vec;

		while (used[j] == 0) {
			PCIB_RELEASE_MSIX(device_get_parent(dev), child,
			    msix->msix_vectors[j].mv_irq);
			j--;
		}
		vec = malloc(sizeof(struct msix_vector) * (j + 1), M_DEVBUF,
		    M_WAITOK);
		bcopy(msix->msix_vectors, vec, sizeof(struct msix_vector) *
		    (j + 1));
		free(msix->msix_vectors, M_DEVBUF);
		msix->msix_vectors = vec;
		msix->msix_alloc = j + 1;
	}
	free(used, M_DEVBUF);

	/* Map the IRQs onto the rids. */
	for (i = 0; i < count; i++) {
		if (vectors[i] == 0)
			continue;
		irq = msix->msix_vectors[vectors[i]].mv_irq;
		resource_list_add(&dinfo->resources, SYS_RES_IRQ, i + 1, irq,
		    irq, 1);
	}

	if (bootverbose) {
		device_printf(child, "Remapped MSI-X IRQs as: ");
		for (i = 0; i < count; i++) {
			if (i != 0)
				printf(", ");
			if (vectors[i] == 0)
				printf("---");
			else
				printf("%d",
				    msix->msix_vectors[vectors[i]].mv_irq);
		}
		printf("\n");
	}

	return (0);
}

static int
pci_release_msix(device_t dev, device_t child)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct pcicfg_msix *msix = &dinfo->cfg.msix;
	struct resource_list_entry *rle;
	int i;

	/* Do we have any messages to release? */
	if (msix->msix_alloc == 0)
		return (ENODEV);

	/* Make sure none of the resources are allocated. */
	for (i = 0; i < msix->msix_table_len; i++) {
		if (msix->msix_table[i].mte_vector == 0)
			continue;
		if (msix->msix_table[i].mte_handlers > 0)
			return (EBUSY);
		rle = resource_list_find(&dinfo->resources, SYS_RES_IRQ, i + 1);
		KASSERT(rle != NULL, ("missing resource"));
		if (rle->res != NULL)
			return (EBUSY);
	}

	/* Update control register to disable MSI-X. */
	msix->msix_ctrl &= ~PCIM_MSIXCTRL_MSIX_ENABLE;
	pci_write_config(child, msix->msix_location + PCIR_MSIX_CTRL,
	    msix->msix_ctrl, 2);

	/* Free the resource list entries. */
	for (i = 0; i < msix->msix_table_len; i++) {
		if (msix->msix_table[i].mte_vector == 0)
			continue;
		resource_list_delete(&dinfo->resources, SYS_RES_IRQ, i + 1);
	}
	free(msix->msix_table, M_DEVBUF);
	msix->msix_table_len = 0;

	/* Release the IRQs. */
	for (i = 0; i < msix->msix_alloc; i++)
		PCIB_RELEASE_MSIX(device_get_parent(dev), child,
		    msix->msix_vectors[i].mv_irq);
	free(msix->msix_vectors, M_DEVBUF);
	msix->msix_alloc = 0;
	return (0);
}

/*
 * Return the max supported MSI-X messages this device supports.
 * Basically, assuming the MD code can alloc messages, this function
 * should return the maximum value that pci_alloc_msix() can return.
 * Thus, it is subject to the tunables, etc.
 */
int
pci_msix_count_method(device_t dev, device_t child)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct pcicfg_msix *msix = &dinfo->cfg.msix;

	if (pci_do_msix && msix->msix_location != 0)
		return (msix->msix_msgnum);
	return (0);
}

/*
 * HyperTransport MSI mapping control
 */
void
pci_ht_map_msi(device_t dev, uint64_t addr)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	struct pcicfg_ht *ht = &dinfo->cfg.ht;

	if (!ht->ht_msimap)
		return;

	if (addr && !(ht->ht_msictrl & PCIM_HTCMD_MSI_ENABLE) &&
	    ht->ht_msiaddr >> 20 == addr >> 20) {
		/* Enable MSI -> HT mapping. */
		ht->ht_msictrl |= PCIM_HTCMD_MSI_ENABLE;
		pci_write_config(dev, ht->ht_msimap + PCIR_HT_COMMAND,
		    ht->ht_msictrl, 2);
	}

	if (!addr && ht->ht_msictrl & PCIM_HTCMD_MSI_ENABLE) {
		/* Disable MSI -> HT mapping. */
		ht->ht_msictrl &= ~PCIM_HTCMD_MSI_ENABLE;
		pci_write_config(dev, ht->ht_msimap + PCIR_HT_COMMAND,
		    ht->ht_msictrl, 2);
	}
}

int
pci_get_max_read_req(device_t dev)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	int cap;
	uint16_t val;

	cap = dinfo->cfg.pcie.pcie_location;
	if (cap == 0)
		return (0);
	val = pci_read_config(dev, cap + PCIER_DEVICE_CTL, 2);
	val &= PCIEM_CTL_MAX_READ_REQUEST;
	val >>= 12;
	return (1 << (val + 7));
}

int
pci_set_max_read_req(device_t dev, int size)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	int cap;
	uint16_t val;

	cap = dinfo->cfg.pcie.pcie_location;
	if (cap == 0)
		return (0);
	if (size < 128)
		size = 128;
	if (size > 4096)
		size = 4096;
	size = (1 << (fls(size) - 1));
	val = pci_read_config(dev, cap + PCIER_DEVICE_CTL, 2);
	val &= ~PCIEM_CTL_MAX_READ_REQUEST;
	val |= (fls(size) - 8) << 12;
	pci_write_config(dev, cap + PCIER_DEVICE_CTL, val, 2);
	return (size);
}

/*
 * Support for MSI message signalled interrupts.
 */
void
pci_enable_msi(device_t dev, uint64_t address, uint16_t data)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	struct pcicfg_msi *msi = &dinfo->cfg.msi;

	/* Write data and address values. */
	pci_write_config(dev, msi->msi_location + PCIR_MSI_ADDR,
	    address & 0xffffffff, 4);
	if (msi->msi_ctrl & PCIM_MSICTRL_64BIT) {
		pci_write_config(dev, msi->msi_location + PCIR_MSI_ADDR_HIGH,
		    address >> 32, 4);
		pci_write_config(dev, msi->msi_location + PCIR_MSI_DATA_64BIT,
		    data, 2);
	} else
		pci_write_config(dev, msi->msi_location + PCIR_MSI_DATA, data,
		    2);

	/* Enable MSI in the control register. */
	msi->msi_ctrl |= PCIM_MSICTRL_MSI_ENABLE;
	pci_write_config(dev, msi->msi_location + PCIR_MSI_CTRL, msi->msi_ctrl,
	    2);

	/* Enable MSI -> HT mapping. */
	pci_ht_map_msi(dev, address);
}

void
pci_disable_msi(device_t dev)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	struct pcicfg_msi *msi = &dinfo->cfg.msi;

	/* Disable MSI -> HT mapping. */
	pci_ht_map_msi(dev, 0);

	/* Disable MSI in the control register. */
	msi->msi_ctrl &= ~PCIM_MSICTRL_MSI_ENABLE;
	pci_write_config(dev, msi->msi_location + PCIR_MSI_CTRL, msi->msi_ctrl,
	    2);
}

/*
 * Restore MSI registers during resume.  If MSI is enabled then
 * restore the data and address registers in addition to the control
 * register.
 */
static void
pci_resume_msi(device_t dev)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	struct pcicfg_msi *msi = &dinfo->cfg.msi;
	uint64_t address;
	uint16_t data;

	if (msi->msi_ctrl & PCIM_MSICTRL_MSI_ENABLE) {
		address = msi->msi_addr;
		data = msi->msi_data;
		pci_write_config(dev, msi->msi_location + PCIR_MSI_ADDR,
		    address & 0xffffffff, 4);
		if (msi->msi_ctrl & PCIM_MSICTRL_64BIT) {
			pci_write_config(dev, msi->msi_location +
			    PCIR_MSI_ADDR_HIGH, address >> 32, 4);
			pci_write_config(dev, msi->msi_location +
			    PCIR_MSI_DATA_64BIT, data, 2);
		} else
			pci_write_config(dev, msi->msi_location + PCIR_MSI_DATA,
			    data, 2);
	}
	pci_write_config(dev, msi->msi_location + PCIR_MSI_CTRL, msi->msi_ctrl,
	    2);
}

static int
pci_remap_intr_method(device_t bus, device_t dev, u_int irq)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	pcicfgregs *cfg = &dinfo->cfg;
	struct resource_list_entry *rle;
	struct msix_table_entry *mte;
	struct msix_vector *mv;
	uint64_t addr;
	uint32_t data;	
	int error, i, j;

	/*
	 * Handle MSI first.  We try to find this IRQ among our list
	 * of MSI IRQs.  If we find it, we request updated address and
	 * data registers and apply the results.
	 */
	if (cfg->msi.msi_alloc > 0) {

		/* If we don't have any active handlers, nothing to do. */
		if (cfg->msi.msi_handlers == 0)
			return (0);
		for (i = 0; i < cfg->msi.msi_alloc; i++) {
			rle = resource_list_find(&dinfo->resources, SYS_RES_IRQ,
			    i + 1);
			if (rle->start == irq) {
				error = PCIB_MAP_MSI(device_get_parent(bus),
				    dev, irq, &addr, &data);
				if (error)
					return (error);
				pci_disable_msi(dev);
				dinfo->cfg.msi.msi_addr = addr;
				dinfo->cfg.msi.msi_data = data;
				pci_enable_msi(dev, addr, data);
				return (0);
			}
		}
		return (ENOENT);
	}

	/*
	 * For MSI-X, we check to see if we have this IRQ.  If we do,
	 * we request the updated mapping info.  If that works, we go
	 * through all the slots that use this IRQ and update them.
	 */
	if (cfg->msix.msix_alloc > 0) {
		for (i = 0; i < cfg->msix.msix_alloc; i++) {
			mv = &cfg->msix.msix_vectors[i];
			if (mv->mv_irq == irq) {
				error = PCIB_MAP_MSI(device_get_parent(bus),
				    dev, irq, &addr, &data);
				if (error)
					return (error);
				mv->mv_address = addr;
				mv->mv_data = data;
				for (j = 0; j < cfg->msix.msix_table_len; j++) {
					mte = &cfg->msix.msix_table[j];
					if (mte->mte_vector != i + 1)
						continue;
					if (mte->mte_handlers == 0)
						continue;
					pci_mask_msix(dev, j);
					pci_enable_msix(dev, j, addr, data);
					pci_unmask_msix(dev, j);
				}
			}
		}
		return (ENOENT);
	}

	return (ENOENT);
}

/*
 * Returns true if the specified device is blacklisted because MSI
 * doesn't work.
 */
int
pci_msi_device_blacklisted(device_t dev)
{

	if (!pci_honor_msi_blacklist)
		return (0);

	return (pci_has_quirk(pci_get_devid(dev), PCI_QUIRK_DISABLE_MSI));
}

/*
 * Determine if MSI is blacklisted globally on this system.  Currently,
 * we just check for blacklisted chipsets as represented by the
 * host-PCI bridge at device 0:0:0.  In the future, it may become
 * necessary to check other system attributes, such as the kenv values
 * that give the motherboard manufacturer and model number.
 */
static int
pci_msi_blacklisted(void)
{
	device_t dev;

	if (!pci_honor_msi_blacklist)
		return (0);

	/* Blacklist all non-PCI-express and non-PCI-X chipsets. */
	if (!(pcie_chipset || pcix_chipset)) {
		if (vm_guest != VM_GUEST_NO) {
			/*
			 * Whitelist older chipsets in virtual
			 * machines known to support MSI.
			 */
			dev = pci_find_bsf(0, 0, 0);
			if (dev != NULL)
				return (!pci_has_quirk(pci_get_devid(dev),
					PCI_QUIRK_ENABLE_MSI_VM));
		}
		return (1);
	}

	dev = pci_find_bsf(0, 0, 0);
	if (dev != NULL)
		return (pci_msi_device_blacklisted(dev));
	return (0);
}

/*
 * Returns true if the specified device is blacklisted because MSI-X
 * doesn't work.  Note that this assumes that if MSI doesn't work,
 * MSI-X doesn't either.
 */
int
pci_msix_device_blacklisted(device_t dev)
{

	if (!pci_honor_msi_blacklist)
		return (0);

	if (pci_has_quirk(pci_get_devid(dev), PCI_QUIRK_DISABLE_MSIX))
		return (1);

	return (pci_msi_device_blacklisted(dev));
}

/*
 * Determine if MSI-X is blacklisted globally on this system.  If MSI
 * is blacklisted, assume that MSI-X is as well.  Check for additional
 * chipsets where MSI works but MSI-X does not.
 */
static int
pci_msix_blacklisted(void)
{
	device_t dev;

	if (!pci_honor_msi_blacklist)
		return (0);

	dev = pci_find_bsf(0, 0, 0);
	if (dev != NULL && pci_has_quirk(pci_get_devid(dev),
	    PCI_QUIRK_DISABLE_MSIX))
		return (1);

	return (pci_msi_blacklisted());
}

/*
 * Attempt to allocate *count MSI messages.  The actual number allocated is
 * returned in *count.  After this function returns, each message will be
 * available to the driver as SYS_RES_IRQ resources starting at a rid 1.
 */
int
pci_alloc_msi_method(device_t dev, device_t child, int *count)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	pcicfgregs *cfg = &dinfo->cfg;
	struct resource_list_entry *rle;
	int actual, error, i, irqs[32];
	uint16_t ctrl;

	/* Don't let count == 0 get us into trouble. */
	if (*count == 0)
		return (EINVAL);

	/* If rid 0 is allocated, then fail. */
	rle = resource_list_find(&dinfo->resources, SYS_RES_IRQ, 0);
	if (rle != NULL && rle->res != NULL)
		return (ENXIO);

	/* Already have allocated messages? */
	if (cfg->msi.msi_alloc != 0 || cfg->msix.msix_alloc != 0)
		return (ENXIO);

	/* If MSI is blacklisted for this system, fail. */
	if (pci_msi_blacklisted())
		return (ENXIO);

	/* MSI capability present? */
	if (cfg->msi.msi_location == 0 || !pci_do_msi)
		return (ENODEV);

	if (bootverbose)
		device_printf(child,
		    "attempting to allocate %d MSI vectors (%d supported)\n",
		    *count, cfg->msi.msi_msgnum);

	/* Don't ask for more than the device supports. */
	actual = min(*count, cfg->msi.msi_msgnum);

	/* Don't ask for more than 32 messages. */
	actual = min(actual, 32);

	/* MSI requires power of 2 number of messages. */
	if (!powerof2(actual))
		return (EINVAL);

	for (;;) {
		/* Try to allocate N messages. */
		error = PCIB_ALLOC_MSI(device_get_parent(dev), child, actual,
		    actual, irqs);
		if (error == 0)
			break;
		if (actual == 1)
			return (error);

		/* Try N / 2. */
		actual >>= 1;
	}

	/*
	 * We now have N actual messages mapped onto SYS_RES_IRQ
	 * resources in the irqs[] array, so add new resources
	 * starting at rid 1.
	 */
	for (i = 0; i < actual; i++)
		resource_list_add(&dinfo->resources, SYS_RES_IRQ, i + 1,
		    irqs[i], irqs[i], 1);

	if (bootverbose) {
		if (actual == 1)
			device_printf(child, "using IRQ %d for MSI\n", irqs[0]);
		else {
			int run;

			/*
			 * Be fancy and try to print contiguous runs
			 * of IRQ values as ranges.  'run' is true if
			 * we are in a range.
			 */
			device_printf(child, "using IRQs %d", irqs[0]);
			run = 0;
			for (i = 1; i < actual; i++) {

				/* Still in a run? */
				if (irqs[i] == irqs[i - 1] + 1) {
					run = 1;
					continue;
				}

				/* Finish previous range. */
				if (run) {
					printf("-%d", irqs[i - 1]);
					run = 0;
				}

				/* Start new range. */
				printf(",%d", irqs[i]);
			}

			/* Unfinished range? */
			if (run)
				printf("-%d", irqs[actual - 1]);
			printf(" for MSI\n");
		}
	}

	/* Update control register with actual count. */
	ctrl = cfg->msi.msi_ctrl;
	ctrl &= ~PCIM_MSICTRL_MME_MASK;
	ctrl |= (ffs(actual) - 1) << 4;
	cfg->msi.msi_ctrl = ctrl;
	pci_write_config(child, cfg->msi.msi_location + PCIR_MSI_CTRL, ctrl, 2);

	/* Update counts of alloc'd messages. */
	cfg->msi.msi_alloc = actual;
	cfg->msi.msi_handlers = 0;
	*count = actual;
	return (0);
}

/* Release the MSI messages associated with this device. */
int
pci_release_msi_method(device_t dev, device_t child)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct pcicfg_msi *msi = &dinfo->cfg.msi;
	struct resource_list_entry *rle;
	int error, i, irqs[32];

	/* Try MSI-X first. */
	error = pci_release_msix(dev, child);
	if (error != ENODEV)
		return (error);

	/* Do we have any messages to release? */
	if (msi->msi_alloc == 0)
		return (ENODEV);
	KASSERT(msi->msi_alloc <= 32, ("more than 32 alloc'd messages"));

	/* Make sure none of the resources are allocated. */
	if (msi->msi_handlers > 0)
		return (EBUSY);
	for (i = 0; i < msi->msi_alloc; i++) {
		rle = resource_list_find(&dinfo->resources, SYS_RES_IRQ, i + 1);
		KASSERT(rle != NULL, ("missing MSI resource"));
		if (rle->res != NULL)
			return (EBUSY);
		irqs[i] = rle->start;
	}

	/* Update control register with 0 count. */
	KASSERT(!(msi->msi_ctrl & PCIM_MSICTRL_MSI_ENABLE),
	    ("%s: MSI still enabled", __func__));
	msi->msi_ctrl &= ~PCIM_MSICTRL_MME_MASK;
	pci_write_config(child, msi->msi_location + PCIR_MSI_CTRL,
	    msi->msi_ctrl, 2);

	/* Release the messages. */
	PCIB_RELEASE_MSI(device_get_parent(dev), child, msi->msi_alloc, irqs);
	for (i = 0; i < msi->msi_alloc; i++)
		resource_list_delete(&dinfo->resources, SYS_RES_IRQ, i + 1);

	/* Update alloc count. */
	msi->msi_alloc = 0;
	msi->msi_addr = 0;
	msi->msi_data = 0;
	return (0);
}

/*
 * Return the max supported MSI messages this device supports.
 * Basically, assuming the MD code can alloc messages, this function
 * should return the maximum value that pci_alloc_msi() can return.
 * Thus, it is subject to the tunables, etc.
 */
int
pci_msi_count_method(device_t dev, device_t child)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct pcicfg_msi *msi = &dinfo->cfg.msi;

	if (pci_do_msi && msi->msi_location != 0)
		return (msi->msi_msgnum);
	return (0);
}

/* free pcicfgregs structure and all depending data structures */

int
pci_freecfg(struct pci_devinfo *dinfo)
{
	struct devlist *devlist_head;
	struct pci_map *pm, *next;
	int i;

	devlist_head = &pci_devq;

	if (dinfo->cfg.vpd.vpd_reg) {
		free(dinfo->cfg.vpd.vpd_ident, M_DEVBUF);
		for (i = 0; i < dinfo->cfg.vpd.vpd_rocnt; i++)
			free(dinfo->cfg.vpd.vpd_ros[i].value, M_DEVBUF);
		free(dinfo->cfg.vpd.vpd_ros, M_DEVBUF);
		for (i = 0; i < dinfo->cfg.vpd.vpd_wcnt; i++)
			free(dinfo->cfg.vpd.vpd_w[i].value, M_DEVBUF);
		free(dinfo->cfg.vpd.vpd_w, M_DEVBUF);
	}
	STAILQ_FOREACH_SAFE(pm, &dinfo->cfg.maps, pm_link, next) {
		free(pm, M_DEVBUF);
	}
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
		pci_printf(cfg, "Transition from D%d to D%d\n", oldstate,
		    state);

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
	uint16_t bit;

	switch(space) {
	case SYS_RES_IOPORT:
		bit = PCIM_CMD_PORTEN;
		break;
	case SYS_RES_MEMORY:
		bit = PCIM_CMD_MEMEN;
		break;
	default:
		return (EINVAL);
	}
	pci_set_command_bit(dev, child, bit);
	return (0);
}

int
pci_disable_io_method(device_t dev, device_t child, int space)
{
	uint16_t bit;

	switch(space) {
	case SYS_RES_IOPORT:
		bit = PCIM_CMD_PORTEN;
		break;
	case SYS_RES_MEMORY:
		bit = PCIM_CMD_MEMEN;
		break;
	default:
		return (EINVAL);
	}
	pci_clear_command_bit(dev, child, bit);
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
		printf("\tdomain=%d, bus=%d, slot=%d, func=%d\n",
		    cfg->domain, cfg->bus, cfg->slot, cfg->func);
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
		if (cfg->msi.msi_location) {
			int ctrl;

			ctrl = cfg->msi.msi_ctrl;
			printf("\tMSI supports %d message%s%s%s\n",
			    cfg->msi.msi_msgnum,
			    (cfg->msi.msi_msgnum == 1) ? "" : "s",
			    (ctrl & PCIM_MSICTRL_64BIT) ? ", 64 bit" : "",
			    (ctrl & PCIM_MSICTRL_VECTOR) ? ", vector masks":"");
		}
		if (cfg->msix.msix_location) {
			printf("\tMSI-X supports %d message%s ",
			    cfg->msix.msix_msgnum,
			    (cfg->msix.msix_msgnum == 1) ? "" : "s");
			if (cfg->msix.msix_table_bar == cfg->msix.msix_pba_bar)
				printf("in map 0x%x\n",
				    cfg->msix.msix_table_bar);
			else
				printf("in maps 0x%x and 0x%x\n",
				    cfg->msix.msix_table_bar,
				    cfg->msix.msix_pba_bar);
		}
	}
}

static int
pci_porten(device_t dev)
{
	return (pci_read_config(dev, PCIR_COMMAND, 2) & PCIM_CMD_PORTEN) != 0;
}

static int
pci_memen(device_t dev)
{
	return (pci_read_config(dev, PCIR_COMMAND, 2) & PCIM_CMD_MEMEN) != 0;
}

static void
pci_read_bar(device_t dev, int reg, pci_addr_t *mapp, pci_addr_t *testvalp)
{
	struct pci_devinfo *dinfo;
	pci_addr_t map, testval;
	int ln2range;
	uint16_t cmd;

	/*
	 * The device ROM BAR is special.  It is always a 32-bit
	 * memory BAR.  Bit 0 is special and should not be set when
	 * sizing the BAR.
	 */
	dinfo = device_get_ivars(dev);
	if (PCIR_IS_BIOS(&dinfo->cfg, reg)) {
		map = pci_read_config(dev, reg, 4);
		pci_write_config(dev, reg, 0xfffffffe, 4);
		testval = pci_read_config(dev, reg, 4);
		pci_write_config(dev, reg, map, 4);
		*mapp = map;
		*testvalp = testval;
		return;
	}

	map = pci_read_config(dev, reg, 4);
	ln2range = pci_maprange(map);
	if (ln2range == 64)
		map |= (pci_addr_t)pci_read_config(dev, reg + 4, 4) << 32;

	/*
	 * Disable decoding via the command register before
	 * determining the BAR's length since we will be placing it in
	 * a weird state.
	 */
	cmd = pci_read_config(dev, PCIR_COMMAND, 2);
	pci_write_config(dev, PCIR_COMMAND,
	    cmd & ~(PCI_BAR_MEM(map) ? PCIM_CMD_MEMEN : PCIM_CMD_PORTEN), 2);

	/*
	 * Determine the BAR's length by writing all 1's.  The bottom
	 * log_2(size) bits of the BAR will stick as 0 when we read
	 * the value back.
	 */
	pci_write_config(dev, reg, 0xffffffff, 4);
	testval = pci_read_config(dev, reg, 4);
	if (ln2range == 64) {
		pci_write_config(dev, reg + 4, 0xffffffff, 4);
		testval |= (pci_addr_t)pci_read_config(dev, reg + 4, 4) << 32;
	}

	/*
	 * Restore the original value of the BAR.  We may have reprogrammed
	 * the BAR of the low-level console device and when booting verbose,
	 * we need the console device addressable.
	 */
	pci_write_config(dev, reg, map, 4);
	if (ln2range == 64)
		pci_write_config(dev, reg + 4, map >> 32, 4);
	pci_write_config(dev, PCIR_COMMAND, cmd, 2);

	*mapp = map;
	*testvalp = testval;
}

static void
pci_write_bar(device_t dev, struct pci_map *pm, pci_addr_t base)
{
	struct pci_devinfo *dinfo;
	int ln2range;

	/* The device ROM BAR is always a 32-bit memory BAR. */
	dinfo = device_get_ivars(dev);
	if (PCIR_IS_BIOS(&dinfo->cfg, pm->pm_reg))
		ln2range = 32;
	else
		ln2range = pci_maprange(pm->pm_value);
	pci_write_config(dev, pm->pm_reg, base, 4);
	if (ln2range == 64)
		pci_write_config(dev, pm->pm_reg + 4, base >> 32, 4);
	pm->pm_value = pci_read_config(dev, pm->pm_reg, 4);
	if (ln2range == 64)
		pm->pm_value |= (pci_addr_t)pci_read_config(dev,
		    pm->pm_reg + 4, 4) << 32;
}

struct pci_map *
pci_find_bar(device_t dev, int reg)
{
	struct pci_devinfo *dinfo;
	struct pci_map *pm;

	dinfo = device_get_ivars(dev);
	STAILQ_FOREACH(pm, &dinfo->cfg.maps, pm_link) {
		if (pm->pm_reg == reg)
			return (pm);
	}
	return (NULL);
}

int
pci_bar_enabled(device_t dev, struct pci_map *pm)
{
	struct pci_devinfo *dinfo;
	uint16_t cmd;

	dinfo = device_get_ivars(dev);
	if (PCIR_IS_BIOS(&dinfo->cfg, pm->pm_reg) &&
	    !(pm->pm_value & PCIM_BIOS_ENABLE))
		return (0);
	cmd = pci_read_config(dev, PCIR_COMMAND, 2);
	if (PCIR_IS_BIOS(&dinfo->cfg, pm->pm_reg) || PCI_BAR_MEM(pm->pm_value))
		return ((cmd & PCIM_CMD_MEMEN) != 0);
	else
		return ((cmd & PCIM_CMD_PORTEN) != 0);
}

static struct pci_map *
pci_add_bar(device_t dev, int reg, pci_addr_t value, pci_addr_t size)
{
	struct pci_devinfo *dinfo;
	struct pci_map *pm, *prev;

	dinfo = device_get_ivars(dev);
	pm = malloc(sizeof(*pm), M_DEVBUF, M_WAITOK | M_ZERO);
	pm->pm_reg = reg;
	pm->pm_value = value;
	pm->pm_size = size;
	STAILQ_FOREACH(prev, &dinfo->cfg.maps, pm_link) {
		KASSERT(prev->pm_reg != pm->pm_reg, ("duplicate map %02x",
		    reg));
		if (STAILQ_NEXT(prev, pm_link) == NULL ||
		    STAILQ_NEXT(prev, pm_link)->pm_reg > pm->pm_reg)
			break;
	}
	if (prev != NULL)
		STAILQ_INSERT_AFTER(&dinfo->cfg.maps, prev, pm, pm_link);
	else
		STAILQ_INSERT_TAIL(&dinfo->cfg.maps, pm, pm_link);
	return (pm);
}

static void
pci_restore_bars(device_t dev)
{
	struct pci_devinfo *dinfo;
	struct pci_map *pm;
	int ln2range;

	dinfo = device_get_ivars(dev);
	STAILQ_FOREACH(pm, &dinfo->cfg.maps, pm_link) {
		if (PCIR_IS_BIOS(&dinfo->cfg, pm->pm_reg))
			ln2range = 32;
		else
			ln2range = pci_maprange(pm->pm_value);
		pci_write_config(dev, pm->pm_reg, pm->pm_value, 4);
		if (ln2range == 64)
			pci_write_config(dev, pm->pm_reg + 4,
			    pm->pm_value >> 32, 4);
	}
}

/*
 * Add a resource based on a pci map register. Return 1 if the map
 * register is a 32bit map register or 2 if it is a 64bit register.
 */
static int
pci_add_map(device_t bus, device_t dev, int reg, struct resource_list *rl,
    int force, int prefetch)
{
	struct pci_map *pm;
	pci_addr_t base, map, testval;
	pci_addr_t start, end, count;
	int barlen, basezero, maprange, mapsize, type;
	uint16_t cmd;
	struct resource *res;

	/*
	 * The BAR may already exist if the device is a CardBus card
	 * whose CIS is stored in this BAR.
	 */
	pm = pci_find_bar(dev, reg);
	if (pm != NULL) {
		maprange = pci_maprange(pm->pm_value);
		barlen = maprange == 64 ? 2 : 1;
		return (barlen);
	}

	pci_read_bar(dev, reg, &map, &testval);
	if (PCI_BAR_MEM(map)) {
		type = SYS_RES_MEMORY;
		if (map & PCIM_BAR_MEM_PREFETCH)
			prefetch = 1;
	} else
		type = SYS_RES_IOPORT;
	mapsize = pci_mapsize(testval);
	base = pci_mapbase(map);
#ifdef __PCI_BAR_ZERO_VALID
	basezero = 0;
#else
	basezero = base == 0;
#endif
	maprange = pci_maprange(map);
	barlen = maprange == 64 ? 2 : 1;

	/*
	 * For I/O registers, if bottom bit is set, and the next bit up
	 * isn't clear, we know we have a BAR that doesn't conform to the
	 * spec, so ignore it.  Also, sanity check the size of the data
	 * areas to the type of memory involved.  Memory must be at least
	 * 16 bytes in size, while I/O ranges must be at least 4.
	 */
	if (PCI_BAR_IO(testval) && (testval & PCIM_BAR_IO_RESERVED) != 0)
		return (barlen);
	if ((type == SYS_RES_MEMORY && mapsize < 4) ||
	    (type == SYS_RES_IOPORT && mapsize < 2))
		return (barlen);

	/* Save a record of this BAR. */
	pm = pci_add_bar(dev, reg, map, mapsize);
	if (bootverbose) {
		printf("\tmap[%02x]: type %s, range %2d, base %#jx, size %2d",
		    reg, pci_maptype(map), maprange, (uintmax_t)base, mapsize);
		if (type == SYS_RES_IOPORT && !pci_porten(dev))
			printf(", port disabled\n");
		else if (type == SYS_RES_MEMORY && !pci_memen(dev))
			printf(", memory disabled\n");
		else
			printf(", enabled\n");
	}

	/*
	 * If base is 0, then we have problems if this architecture does
	 * not allow that.  It is best to ignore such entries for the
	 * moment.  These will be allocated later if the driver specifically
	 * requests them.  However, some removable busses look better when
	 * all resources are allocated, so allow '0' to be overriden.
	 *
	 * Similarly treat maps whose values is the same as the test value
	 * read back.  These maps have had all f's written to them by the
	 * BIOS in an attempt to disable the resources.
	 */
	if (!force && (basezero || map == testval))
		return (barlen);
	if ((u_long)base != base) {
		device_printf(bus,
		    "pci%d:%d:%d:%d bar %#x too many address bits",
		    pci_get_domain(dev), pci_get_bus(dev), pci_get_slot(dev),
		    pci_get_function(dev), reg);
		return (barlen);
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
		if (type == SYS_RES_IOPORT && !pci_porten(dev)) {
			cmd = pci_read_config(dev, PCIR_COMMAND, 2);
			cmd |= PCIM_CMD_PORTEN;
			pci_write_config(dev, PCIR_COMMAND, cmd, 2);
		}
		if (type == SYS_RES_MEMORY && !pci_memen(dev)) {
			cmd = pci_read_config(dev, PCIR_COMMAND, 2);
			cmd |= PCIM_CMD_MEMEN;
			pci_write_config(dev, PCIR_COMMAND, cmd, 2);
		}
	} else {
		if (type == SYS_RES_IOPORT && !pci_porten(dev))
			return (barlen);
		if (type == SYS_RES_MEMORY && !pci_memen(dev))
			return (barlen);
	}

	count = (pci_addr_t)1 << mapsize;
	if (basezero || base == pci_mapbase(testval)) {
		start = 0;	/* Let the parent decide. */
		end = ~0ul;
	} else {
		start = base;
		end = base + count - 1;
	}
	resource_list_add(rl, type, reg, start, end, count);

	/*
	 * Try to allocate the resource for this BAR from our parent
	 * so that this resource range is already reserved.  The
	 * driver for this device will later inherit this resource in
	 * pci_alloc_resource().
	 */
	res = resource_list_reserve(rl, bus, dev, type, &reg, start, end, count,
	    prefetch ? RF_PREFETCHABLE : 0);
	if (pci_do_realloc_bars && res == NULL && (start != 0 || end != ~0ul)) {
		/*
		 * If the allocation fails, try to allocate a resource for
		 * this BAR using any available range.  The firmware felt
		 * it was important enough to assign a resource, so don't
		 * disable decoding if we can help it.
		 */
		resource_list_delete(rl, type, reg);
		resource_list_add(rl, type, reg, 0, ~0ul, count);
		res = resource_list_reserve(rl, bus, dev, type, &reg, 0, ~0ul,
		    count, prefetch ? RF_PREFETCHABLE : 0);
	}
	if (res == NULL) {
		/*
		 * If the allocation fails, delete the resource list entry
		 * and disable decoding for this device.
		 *
		 * If the driver requests this resource in the future,
		 * pci_reserve_map() will try to allocate a fresh
		 * resource range.
		 */
		resource_list_delete(rl, type, reg);
		pci_disable_io(dev, type);
		if (bootverbose)
			device_printf(bus,
			    "pci%d:%d:%d:%d bar %#x failed to allocate\n",
			    pci_get_domain(dev), pci_get_bus(dev),
			    pci_get_slot(dev), pci_get_function(dev), reg);
	} else {
		start = rman_get_start(res);
		pci_write_bar(dev, pm, start);
	}
	return (barlen);
}

/*
 * For ATA devices we need to decide early what addressing mode to use.
 * Legacy demands that the primary and secondary ATA ports sits on the
 * same addresses that old ISA hardware did. This dictates that we use
 * those addresses and ignore the BAR's if we cannot set PCI native
 * addressing mode.
 */
static void
pci_ata_maps(device_t bus, device_t dev, struct resource_list *rl, int force,
    uint32_t prefetchmask)
{
	struct resource *r;
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
		pci_add_map(bus, dev, PCIR_BAR(0), rl, force,
		    prefetchmask & (1 << 0));
		pci_add_map(bus, dev, PCIR_BAR(1), rl, force,
		    prefetchmask & (1 << 1));
	} else {
		rid = PCIR_BAR(0);
		resource_list_add(rl, type, rid, 0x1f0, 0x1f7, 8);
		r = resource_list_reserve(rl, bus, dev, type, &rid, 0x1f0,
		    0x1f7, 8, 0);
		rid = PCIR_BAR(1);
		resource_list_add(rl, type, rid, 0x3f6, 0x3f6, 1);
		r = resource_list_reserve(rl, bus, dev, type, &rid, 0x3f6,
		    0x3f6, 1, 0);
	}
	if (progif & PCIP_STORAGE_IDE_MODESEC) {
		pci_add_map(bus, dev, PCIR_BAR(2), rl, force,
		    prefetchmask & (1 << 2));
		pci_add_map(bus, dev, PCIR_BAR(3), rl, force,
		    prefetchmask & (1 << 3));
	} else {
		rid = PCIR_BAR(2);
		resource_list_add(rl, type, rid, 0x170, 0x177, 8);
		r = resource_list_reserve(rl, bus, dev, type, &rid, 0x170,
		    0x177, 8, 0);
		rid = PCIR_BAR(3);
		resource_list_add(rl, type, rid, 0x376, 0x376, 1);
		r = resource_list_reserve(rl, bus, dev, type, &rid, 0x376,
		    0x376, 1, 0);
	}
	pci_add_map(bus, dev, PCIR_BAR(4), rl, force,
	    prefetchmask & (1 << 4));
	pci_add_map(bus, dev, PCIR_BAR(5), rl, force,
	    prefetchmask & (1 << 5));
}

static void
pci_assign_interrupt(device_t bus, device_t dev, int force_route)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	pcicfgregs *cfg = &dinfo->cfg;
	char tunable_name[64];
	int irq;

	/* Has to have an intpin to have an interrupt. */
	if (cfg->intpin == 0)
		return;

	/* Let the user override the IRQ with a tunable. */
	irq = PCI_INVALID_IRQ;
	snprintf(tunable_name, sizeof(tunable_name),
	    "hw.pci%d.%d.%d.INT%c.irq",
	    cfg->domain, cfg->bus, cfg->slot, cfg->intpin + 'A' - 1);
	if (TUNABLE_INT_FETCH(tunable_name, &irq) && (irq >= 255 || irq <= 0))
		irq = PCI_INVALID_IRQ;

	/*
	 * If we didn't get an IRQ via the tunable, then we either use the
	 * IRQ value in the intline register or we ask the bus to route an
	 * interrupt for us.  If force_route is true, then we only use the
	 * value in the intline register if the bus was unable to assign an
	 * IRQ.
	 */
	if (!PCI_INTERRUPT_VALID(irq)) {
		if (!PCI_INTERRUPT_VALID(cfg->intline) || force_route)
			irq = PCI_ASSIGN_INTERRUPT(bus, dev);
		if (!PCI_INTERRUPT_VALID(irq))
			irq = cfg->intline;
	}

	/* If after all that we don't have an IRQ, just bail. */
	if (!PCI_INTERRUPT_VALID(irq))
		return;

	/* Update the config register if it changed. */
	if (irq != cfg->intline) {
		cfg->intline = irq;
		pci_write_config(dev, PCIR_INTLINE, irq, 1);
	}

	/* Add this IRQ as rid 0 interrupt resource. */
	resource_list_add(&dinfo->resources, SYS_RES_IRQ, 0, irq, irq, 1);
}

/* Perform early OHCI takeover from SMM. */
static void
ohci_early_takeover(device_t self)
{
	struct resource *res;
	uint32_t ctl;
	int rid;
	int i;

	rid = PCIR_BAR(0);
	res = bus_alloc_resource_any(self, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (res == NULL)
		return;

	ctl = bus_read_4(res, OHCI_CONTROL);
	if (ctl & OHCI_IR) {
		if (bootverbose)
			printf("ohci early: "
			    "SMM active, request owner change\n");
		bus_write_4(res, OHCI_COMMAND_STATUS, OHCI_OCR);
		for (i = 0; (i < 100) && (ctl & OHCI_IR); i++) {
			DELAY(1000);
			ctl = bus_read_4(res, OHCI_CONTROL);
		}
		if (ctl & OHCI_IR) {
			if (bootverbose)
				printf("ohci early: "
				    "SMM does not respond, resetting\n");
			bus_write_4(res, OHCI_CONTROL, OHCI_HCFS_RESET);
		}
		/* Disable interrupts */
		bus_write_4(res, OHCI_INTERRUPT_DISABLE, OHCI_ALL_INTRS);
	}

	bus_release_resource(self, SYS_RES_MEMORY, rid, res);
}

/* Perform early UHCI takeover from SMM. */
static void
uhci_early_takeover(device_t self)
{
	struct resource *res;
	int rid;

	/*
	 * Set the PIRQD enable bit and switch off all the others. We don't
	 * want legacy support to interfere with us XXX Does this also mean
	 * that the BIOS won't touch the keyboard anymore if it is connected
	 * to the ports of the root hub?
	 */
	pci_write_config(self, PCI_LEGSUP, PCI_LEGSUP_USBPIRQDEN, 2);

	/* Disable interrupts */
	rid = PCI_UHCI_BASE_REG;
	res = bus_alloc_resource_any(self, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (res != NULL) {
		bus_write_2(res, UHCI_INTR, 0);
		bus_release_resource(self, SYS_RES_IOPORT, rid, res);
	}
}

/* Perform early EHCI takeover from SMM. */
static void
ehci_early_takeover(device_t self)
{
	struct resource *res;
	uint32_t cparams;
	uint32_t eec;
	uint8_t eecp;
	uint8_t bios_sem;
	uint8_t offs;
	int rid;
	int i;

	rid = PCIR_BAR(0);
	res = bus_alloc_resource_any(self, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (res == NULL)
		return;

	cparams = bus_read_4(res, EHCI_HCCPARAMS);

	/* Synchronise with the BIOS if it owns the controller. */
	for (eecp = EHCI_HCC_EECP(cparams); eecp != 0;
	    eecp = EHCI_EECP_NEXT(eec)) {
		eec = pci_read_config(self, eecp, 4);
		if (EHCI_EECP_ID(eec) != EHCI_EC_LEGSUP) {
			continue;
		}
		bios_sem = pci_read_config(self, eecp +
		    EHCI_LEGSUP_BIOS_SEM, 1);
		if (bios_sem == 0) {
			continue;
		}
		if (bootverbose)
			printf("ehci early: "
			    "SMM active, request owner change\n");

		pci_write_config(self, eecp + EHCI_LEGSUP_OS_SEM, 1, 1);

		for (i = 0; (i < 100) && (bios_sem != 0); i++) {
			DELAY(1000);
			bios_sem = pci_read_config(self, eecp +
			    EHCI_LEGSUP_BIOS_SEM, 1);
		}

		if (bios_sem != 0) {
			if (bootverbose)
				printf("ehci early: "
				    "SMM does not respond\n");
		}
		/* Disable interrupts */
		offs = EHCI_CAPLENGTH(bus_read_4(res, EHCI_CAPLEN_HCIVERSION));
		bus_write_4(res, offs + EHCI_USBINTR, 0);
	}
	bus_release_resource(self, SYS_RES_MEMORY, rid, res);
}

/* Perform early XHCI takeover from SMM. */
static void
xhci_early_takeover(device_t self)
{
	struct resource *res;
	uint32_t cparams;
	uint32_t eec;
	uint8_t eecp;
	uint8_t bios_sem;
	uint8_t offs;
	int rid;
	int i;

	rid = PCIR_BAR(0);
	res = bus_alloc_resource_any(self, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (res == NULL)
		return;

	cparams = bus_read_4(res, XHCI_HCSPARAMS0);

	eec = -1;

	/* Synchronise with the BIOS if it owns the controller. */
	for (eecp = XHCI_HCS0_XECP(cparams) << 2; eecp != 0 && XHCI_XECP_NEXT(eec);
	    eecp += XHCI_XECP_NEXT(eec) << 2) {
		eec = bus_read_4(res, eecp);

		if (XHCI_XECP_ID(eec) != XHCI_ID_USB_LEGACY)
			continue;

		bios_sem = bus_read_1(res, eecp + XHCI_XECP_BIOS_SEM);
		if (bios_sem == 0)
			continue;

		if (bootverbose)
			printf("xhci early: "
			    "SMM active, request owner change\n");

		bus_write_1(res, eecp + XHCI_XECP_OS_SEM, 1);

		/* wait a maximum of 5 second */

		for (i = 0; (i < 5000) && (bios_sem != 0); i++) {
			DELAY(1000);
			bios_sem = bus_read_1(res, eecp +
			    XHCI_XECP_BIOS_SEM);
		}

		if (bios_sem != 0) {
			if (bootverbose)
				printf("xhci early: "
				    "SMM does not respond\n");
		}

		/* Disable interrupts */
		offs = bus_read_1(res, XHCI_CAPLENGTH);
		bus_write_4(res, offs + XHCI_USBCMD, 0);
		bus_read_4(res, offs + XHCI_USBSTS);
	}
	bus_release_resource(self, SYS_RES_MEMORY, rid, res);
}

void
pci_add_resources(device_t bus, device_t dev, int force, uint32_t prefetchmask)
{
	struct pci_devinfo *dinfo;
	pcicfgregs *cfg;
	struct resource_list *rl;
	const struct pci_quirk *q;
	uint32_t devid;
	int i;

	dinfo = device_get_ivars(dev);
	cfg = &dinfo->cfg;
	rl = &dinfo->resources;
	devid = (cfg->device << 16) | cfg->vendor;

	/* ATA devices needs special map treatment */
	if ((pci_get_class(dev) == PCIC_STORAGE) &&
	    (pci_get_subclass(dev) == PCIS_STORAGE_IDE) &&
	    ((pci_get_progif(dev) & PCIP_STORAGE_IDE_MASTERDEV) ||
	     (!pci_read_config(dev, PCIR_BAR(0), 4) &&
	      !pci_read_config(dev, PCIR_BAR(2), 4))) )
		pci_ata_maps(bus, dev, rl, force, prefetchmask);
	else
		for (i = 0; i < cfg->nummaps;) {
			/*
			 * Skip quirked resources.
			 */
			for (q = &pci_quirks[0]; q->devid != 0; q++)
				if (q->devid == devid &&
				    q->type == PCI_QUIRK_UNMAP_REG &&
				    q->arg1 == PCIR_BAR(i))
					break;
			if (q->devid != 0) {
				i++;
				continue;
			}
			i += pci_add_map(bus, dev, PCIR_BAR(i), rl, force,
			    prefetchmask & (1 << i));
		}

	/*
	 * Add additional, quirked resources.
	 */
	for (q = &pci_quirks[0]; q->devid != 0; q++)
		if (q->devid == devid && q->type == PCI_QUIRK_MAP_REG)
			pci_add_map(bus, dev, q->arg1, rl, force, 0);

	if (cfg->intpin > 0 && PCI_INTERRUPT_VALID(cfg->intline)) {
#ifdef __PCI_REROUTE_INTERRUPT
		/*
		 * Try to re-route interrupts. Sometimes the BIOS or
		 * firmware may leave bogus values in these registers.
		 * If the re-route fails, then just stick with what we
		 * have.
		 */
		pci_assign_interrupt(bus, dev, 1);
#else
		pci_assign_interrupt(bus, dev, 0);
#endif
	}

	if (pci_usb_takeover && pci_get_class(dev) == PCIC_SERIALBUS &&
	    pci_get_subclass(dev) == PCIS_SERIALBUS_USB) {
		if (pci_get_progif(dev) == PCIP_SERIALBUS_USB_XHCI)
			xhci_early_takeover(dev);
		else if (pci_get_progif(dev) == PCIP_SERIALBUS_USB_EHCI)
			ehci_early_takeover(dev);
		else if (pci_get_progif(dev) == PCIP_SERIALBUS_USB_OHCI)
			ohci_early_takeover(dev);
		else if (pci_get_progif(dev) == PCIP_SERIALBUS_USB_UHCI)
			uhci_early_takeover(dev);
	}
}

void
pci_add_children(device_t dev, int domain, int busno, size_t dinfo_size)
{
#define	REG(n, w)	PCIB_READ_CONFIG(pcib, busno, s, f, n, w)
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
		DELAY(1);
		hdrtype = REG(PCIR_HDRTYPE, 1);
		if ((hdrtype & PCIM_HDRTYPE) > PCI_MAXHDRTYPE)
			continue;
		if (hdrtype & PCIM_MFDEV)
			pcifunchigh = PCI_FUNCMAX;
		for (f = 0; f <= pcifunchigh; f++) {
			dinfo = pci_read_device(pcib, domain, busno, s, f,
			    dinfo_size);
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
	dinfo->cfg.dev = device_add_child(bus, NULL, -1);
	device_set_ivars(dinfo->cfg.dev, dinfo);
	resource_list_init(&dinfo->resources);
	pci_cfg_save(dinfo->cfg.dev, dinfo, 0);
	pci_cfg_restore(dinfo->cfg.dev, dinfo);
	pci_print_verbose(dinfo);
	pci_add_resources(bus, dinfo->cfg.dev, 0, 0);
}

static int
pci_probe(device_t dev)
{

	device_set_desc(dev, "PCI bus");

	/* Allow other subclasses to override this driver. */
	return (BUS_PROBE_GENERIC);
}

int
pci_attach_common(device_t dev)
{
	struct pci_softc *sc;
	int busno, domain;
#ifdef PCI_DMA_BOUNDARY
	int error, tag_valid;
#endif

	sc = device_get_softc(dev);
	domain = pcib_get_domain(dev);
	busno = pcib_get_bus(dev);
	if (bootverbose)
		device_printf(dev, "domain=%d, physical bus=%d\n",
		    domain, busno);
#ifdef PCI_DMA_BOUNDARY
	tag_valid = 0;
	if (device_get_devclass(device_get_parent(device_get_parent(dev))) !=
	    devclass_find("pci")) {
		error = bus_dma_tag_create(bus_get_dma_tag(dev), 1,
		    PCI_DMA_BOUNDARY, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,
		    NULL, NULL, BUS_SPACE_MAXSIZE, BUS_SPACE_UNRESTRICTED,
		    BUS_SPACE_MAXSIZE, 0, NULL, NULL, &sc->sc_dma_tag);
		if (error)
			device_printf(dev, "Failed to create DMA tag: %d\n",
			    error);
		else
			tag_valid = 1;
	}
	if (!tag_valid)
#endif
		sc->sc_dma_tag = bus_get_dma_tag(dev);
	return (0);
}

static int
pci_attach(device_t dev)
{
	int busno, domain, error;

	error = pci_attach_common(dev);
	if (error)
		return (error);

	/*
	 * Since there can be multiple independantly numbered PCI
	 * busses on systems with multiple PCI domains, we can't use
	 * the unit number to decide which bus we are probing. We ask
	 * the parent pcib what our domain and bus numbers are.
	 */
	domain = pcib_get_domain(dev);
	busno = pcib_get_bus(dev);
	pci_add_children(dev, domain, busno, sizeof(struct pci_devinfo));
	return (bus_generic_attach(dev));
}

static void
pci_set_power_children(device_t dev, device_t *devlist, int numdevs,
    int state)
{
	device_t child, pcib;
	struct pci_devinfo *dinfo;
	int dstate, i;

	/*
	 * Set the device to the given state.  If the firmware suggests
	 * a different power state, use it instead.  If power management
	 * is not present, the firmware is responsible for managing
	 * device power.  Skip children who aren't attached since they
	 * are handled separately.
	 */
	pcib = device_get_parent(dev);
	for (i = 0; i < numdevs; i++) {
		child = devlist[i];
		dinfo = device_get_ivars(child);
		dstate = state;
		if (device_is_attached(child) &&
		    PCIB_POWER_FOR_SLEEP(pcib, dev, &dstate) == 0)
			pci_set_powerstate(child, dstate);
	}
}

int
pci_suspend(device_t dev)
{
	device_t child, *devlist;
	struct pci_devinfo *dinfo;
	int error, i, numdevs;

	/*
	 * Save the PCI configuration space for each child and set the
	 * device in the appropriate power state for this sleep state.
	 */
	if ((error = device_get_children(dev, &devlist, &numdevs)) != 0)
		return (error);
	for (i = 0; i < numdevs; i++) {
		child = devlist[i];
		dinfo = device_get_ivars(child);
		pci_cfg_save(child, dinfo, 0);
	}

	/* Suspend devices before potentially powering them down. */
	error = bus_generic_suspend(dev);
	if (error) {
		free(devlist, M_TEMP);
		return (error);
	}
	if (pci_do_power_suspend)
		pci_set_power_children(dev, devlist, numdevs,
		    PCI_POWERSTATE_D3);
	free(devlist, M_TEMP);
	return (0);
}

int
pci_resume(device_t dev)
{
	device_t child, *devlist;
	struct pci_devinfo *dinfo;
	int error, i, numdevs;

	/*
	 * Set each child to D0 and restore its PCI configuration space.
	 */
	if ((error = device_get_children(dev, &devlist, &numdevs)) != 0)
		return (error);
	if (pci_do_power_resume)
		pci_set_power_children(dev, devlist, numdevs,
		    PCI_POWERSTATE_D0);

	/* Now the device is powered up, restore its config space. */
	for (i = 0; i < numdevs; i++) {
		child = devlist[i];
		dinfo = device_get_ivars(child);

		pci_cfg_restore(child, dinfo);
		if (!device_is_attached(child))
			pci_cfg_save(child, dinfo, 1);
	}

	/*
	 * Resume critical devices first, then everything else later.
	 */
	for (i = 0; i < numdevs; i++) {
		child = devlist[i];
		switch (pci_get_class(child)) {
		case PCIC_DISPLAY:
		case PCIC_MEMORY:
		case PCIC_BRIDGE:
		case PCIC_BASEPERIPH:
			DEVICE_RESUME(child);
			break;
		}
	}
	for (i = 0; i < numdevs; i++) {
		child = devlist[i];
		switch (pci_get_class(child)) {
		case PCIC_DISPLAY:
		case PCIC_MEMORY:
		case PCIC_BRIDGE:
		case PCIC_BASEPERIPH:
			break;
		default:
			DEVICE_RESUME(child);
		}
	}
	free(devlist, M_TEMP);
	return (0);
}

static void
pci_load_vendor_data(void)
{
	caddr_t data;
	void *ptr;
	size_t sz;

	data = preload_search_by_type("pci_vendor_data");
	if (data != NULL) {
		ptr = preload_fetch_addr(data);
		sz = preload_fetch_size(data);
		if (ptr != NULL && sz != 0) {
			pci_vendordata = ptr;
			pci_vendordata_size = sz;
			/* terminate the database */
			pci_vendordata[pci_vendordata_size] = '\n';
		}
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
	if (device_get_children(dev, &devlist, &numdevs) != 0)
		return;
	for (i = 0; i < numdevs; i++) {
		child = devlist[i];
		if (device_get_state(child) != DS_NOTPRESENT)
			continue;
		dinfo = device_get_ivars(child);
		pci_print_verbose(dinfo);
		if (bootverbose)
			pci_printf(&dinfo->cfg, "reprobing on driver added\n");
		pci_cfg_restore(child, dinfo);
		if (device_probe_and_attach(child) != 0)
			pci_child_detached(dev, child);
	}
	free(devlist, M_TEMP);
}

int
pci_setup_intr(device_t dev, device_t child, struct resource *irq, int flags,
    driver_filter_t *filter, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct pci_devinfo *dinfo;
	struct msix_table_entry *mte;
	struct msix_vector *mv;
	uint64_t addr;
	uint32_t data;
	void *cookie;
	int error, rid;

	error = bus_generic_setup_intr(dev, child, irq, flags, filter, intr,
	    arg, &cookie);
	if (error)
		return (error);

	/* If this is not a direct child, just bail out. */
	if (device_get_parent(child) != dev) {
		*cookiep = cookie;
		return(0);
	}

	rid = rman_get_rid(irq);
	if (rid == 0) {
		/* Make sure that INTx is enabled */
		pci_clear_command_bit(dev, child, PCIM_CMD_INTxDIS);
	} else {
		/*
		 * Check to see if the interrupt is MSI or MSI-X.
		 * Ask our parent to map the MSI and give
		 * us the address and data register values.
		 * If we fail for some reason, teardown the
		 * interrupt handler.
		 */
		dinfo = device_get_ivars(child);
		if (dinfo->cfg.msi.msi_alloc > 0) {
			if (dinfo->cfg.msi.msi_addr == 0) {
				KASSERT(dinfo->cfg.msi.msi_handlers == 0,
			    ("MSI has handlers, but vectors not mapped"));
				error = PCIB_MAP_MSI(device_get_parent(dev),
				    child, rman_get_start(irq), &addr, &data);
				if (error)
					goto bad;
				dinfo->cfg.msi.msi_addr = addr;
				dinfo->cfg.msi.msi_data = data;
			}
			if (dinfo->cfg.msi.msi_handlers == 0)
				pci_enable_msi(child, dinfo->cfg.msi.msi_addr,
				    dinfo->cfg.msi.msi_data);
			dinfo->cfg.msi.msi_handlers++;
		} else {
			KASSERT(dinfo->cfg.msix.msix_alloc > 0,
			    ("No MSI or MSI-X interrupts allocated"));
			KASSERT(rid <= dinfo->cfg.msix.msix_table_len,
			    ("MSI-X index too high"));
			mte = &dinfo->cfg.msix.msix_table[rid - 1];
			KASSERT(mte->mte_vector != 0, ("no message vector"));
			mv = &dinfo->cfg.msix.msix_vectors[mte->mte_vector - 1];
			KASSERT(mv->mv_irq == rman_get_start(irq),
			    ("IRQ mismatch"));
			if (mv->mv_address == 0) {
				KASSERT(mte->mte_handlers == 0,
		    ("MSI-X table entry has handlers, but vector not mapped"));
				error = PCIB_MAP_MSI(device_get_parent(dev),
				    child, rman_get_start(irq), &addr, &data);
				if (error)
					goto bad;
				mv->mv_address = addr;
				mv->mv_data = data;
			}
			if (mte->mte_handlers == 0) {
				pci_enable_msix(child, rid - 1, mv->mv_address,
				    mv->mv_data);
				pci_unmask_msix(child, rid - 1);
			}
			mte->mte_handlers++;
		}

		/* Make sure that INTx is disabled if we are using MSI/MSIX */
		pci_set_command_bit(dev, child, PCIM_CMD_INTxDIS);
	bad:
		if (error) {
			(void)bus_generic_teardown_intr(dev, child, irq,
			    cookie);
			return (error);
		}
	}
	*cookiep = cookie;
	return (0);
}

int
pci_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	struct msix_table_entry *mte;
	struct resource_list_entry *rle;
	struct pci_devinfo *dinfo;
	int error, rid;

	if (irq == NULL || !(rman_get_flags(irq) & RF_ACTIVE))
		return (EINVAL);

	/* If this isn't a direct child, just bail out */
	if (device_get_parent(child) != dev)
		return(bus_generic_teardown_intr(dev, child, irq, cookie));

	rid = rman_get_rid(irq);
	if (rid == 0) {
		/* Mask INTx */
		pci_set_command_bit(dev, child, PCIM_CMD_INTxDIS);
	} else {
		/*
		 * Check to see if the interrupt is MSI or MSI-X.  If so,
		 * decrement the appropriate handlers count and mask the
		 * MSI-X message, or disable MSI messages if the count
		 * drops to 0.
		 */
		dinfo = device_get_ivars(child);
		rle = resource_list_find(&dinfo->resources, SYS_RES_IRQ, rid);
		if (rle->res != irq)
			return (EINVAL);
		if (dinfo->cfg.msi.msi_alloc > 0) {
			KASSERT(rid <= dinfo->cfg.msi.msi_alloc,
			    ("MSI-X index too high"));
			if (dinfo->cfg.msi.msi_handlers == 0)
				return (EINVAL);
			dinfo->cfg.msi.msi_handlers--;
			if (dinfo->cfg.msi.msi_handlers == 0)
				pci_disable_msi(child);
		} else {
			KASSERT(dinfo->cfg.msix.msix_alloc > 0,
			    ("No MSI or MSI-X interrupts allocated"));
			KASSERT(rid <= dinfo->cfg.msix.msix_table_len,
			    ("MSI-X index too high"));
			mte = &dinfo->cfg.msix.msix_table[rid - 1];
			if (mte->mte_handlers == 0)
				return (EINVAL);
			mte->mte_handlers--;
			if (mte->mte_handlers == 0)
				pci_mask_msix(child, rid - 1);
		}
	}
	error = bus_generic_teardown_intr(dev, child, irq, cookie);
	if (rid > 0)
		KASSERT(error == 0,
		    ("%s: generic teardown failed for MSI/MSI-X", __func__));
	return (error);
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

static const struct
{
	int		class;
	int		subclass;
	const char	*desc;
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
	{PCIC_STORAGE,		PCIS_STORAGE_ATA_ADMA,	"ATA (ADMA)"},
	{PCIC_STORAGE,		PCIS_STORAGE_SATA,	"SATA"},
	{PCIC_STORAGE,		PCIS_STORAGE_SAS,	"SAS"},
	{PCIC_STORAGE,		PCIS_STORAGE_NVM,	"NVM"},
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
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_HDA,	"HDA"},
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
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_SDHC,	"SD host controller"},
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
	{PCIC_CRYPTO,		PCIS_CRYPTO_ENTERTAIN,	"entertainment crypto"},
	{PCIC_DASP,		-1,			"dasp"},
	{PCIC_DASP,		PCIS_DASP_DPIO,		"DPIO module"},
	{0, 0,		NULL}
};

void
pci_probe_nomatch(device_t dev, device_t child)
{
	int i;
	const char *cp, *scp;
	char *device;

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
	pci_cfg_save(child, device_get_ivars(child), 1);
}

void
pci_child_detached(device_t dev, device_t child)
{
	struct pci_devinfo *dinfo;
	struct resource_list *rl;

	dinfo = device_get_ivars(child);
	rl = &dinfo->resources;

	/*
	 * Have to deallocate IRQs before releasing any MSI messages and
	 * have to release MSI messages before deallocating any memory
	 * BARs.
	 */
	if (resource_list_release_active(rl, dev, child, SYS_RES_IRQ) != 0)
		pci_printf(&dinfo->cfg, "Device leaked IRQ resources\n");
	if (dinfo->cfg.msi.msi_alloc != 0 || dinfo->cfg.msix.msix_alloc != 0) {
		pci_printf(&dinfo->cfg, "Device leaked MSI vectors\n");
		(void)pci_release_msi(child);
	}
	if (resource_list_release_active(rl, dev, child, SYS_RES_MEMORY) != 0)
		pci_printf(&dinfo->cfg, "Device leaked memory resources\n");
	if (resource_list_release_active(rl, dev, child, SYS_RES_IOPORT) != 0)
		pci_printf(&dinfo->cfg, "Device leaked I/O resources\n");

	pci_cfg_save(child, dinfo, 1);
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
	case PCI_IVAR_DOMAIN:
		*result = cfg->domain;
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
	case PCI_IVAR_CMDREG:
		*result = cfg->cmdreg;
		break;
	case PCI_IVAR_CACHELNSZ:
		*result = cfg->cachelnsz;
		break;
	case PCI_IVAR_MINGNT:
		*result = cfg->mingnt;
		break;
	case PCI_IVAR_MAXLAT:
		*result = cfg->maxlat;
		break;
	case PCI_IVAR_LATTIMER:
		*result = cfg->lattimer;
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
	case PCI_IVAR_DOMAIN:
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
	int i, error, none_count;

	none_count = 0;
	/* get the head of the device queue */
	devlist_head = &pci_devq;

	/*
	 * Go through the list of devices and print out devices
	 */
	for (error = 0, i = 0,
	     dinfo = STAILQ_FIRST(devlist_head);
	     (dinfo != NULL) && (error == 0) && (i < pci_numdevs) && !db_pager_quit;
	     dinfo = STAILQ_NEXT(dinfo, pci_links), i++) {

		/* Populate pd_name and pd_unit */
		name = NULL;
		if (dinfo->cfg.dev)
			name = device_get_name(dinfo->cfg.dev);

		p = &dinfo->conf;
		db_printf("%s%d@pci%d:%d:%d:%d:\tclass=0x%06x card=0x%08x "
			"chip=0x%08x rev=0x%02x hdr=0x%02x\n",
			(name && *name) ? name : "none",
			(name && *name) ? (int)device_get_unit(dinfo->cfg.dev) :
			none_count++,
			p->pc_sel.pc_domain, p->pc_sel.pc_bus, p->pc_sel.pc_dev,
			p->pc_sel.pc_func, (p->pc_class << 16) |
			(p->pc_subclass << 8) | p->pc_progif,
			(p->pc_subdevice << 16) | p->pc_subvendor,
			(p->pc_device << 16) | p->pc_vendor,
			p->pc_revid, p->pc_hdr);
	}
}
#endif /* DDB */

static struct resource *
pci_reserve_map(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->resources;
	struct resource_list_entry *rle;
	struct resource *res;
	struct pci_map *pm;
	pci_addr_t map, testval;
	int mapsize;

	res = NULL;
	pm = pci_find_bar(child, *rid);
	if (pm != NULL) {
		/* This is a BAR that we failed to allocate earlier. */
		mapsize = pm->pm_size;
		map = pm->pm_value;
	} else {
		/*
		 * Weed out the bogons, and figure out how large the
		 * BAR/map is.  BARs that read back 0 here are bogus
		 * and unimplemented.  Note: atapci in legacy mode are
		 * special and handled elsewhere in the code.  If you
		 * have a atapci device in legacy mode and it fails
		 * here, that other code is broken.
		 */
		pci_read_bar(child, *rid, &map, &testval);

		/*
		 * Determine the size of the BAR and ignore BARs with a size
		 * of 0.  Device ROM BARs use a different mask value.
		 */
		if (PCIR_IS_BIOS(&dinfo->cfg, *rid))
			mapsize = pci_romsize(testval);
		else
			mapsize = pci_mapsize(testval);
		if (mapsize == 0)
			goto out;
		pm = pci_add_bar(child, *rid, map, mapsize);
	}

	if (PCI_BAR_MEM(map) || PCIR_IS_BIOS(&dinfo->cfg, *rid)) {
		if (type != SYS_RES_MEMORY) {
			if (bootverbose)
				device_printf(dev,
				    "child %s requested type %d for rid %#x,"
				    " but the BAR says it is an memio\n",
				    device_get_nameunit(child), type, *rid);
			goto out;
		}
	} else {
		if (type != SYS_RES_IOPORT) {
			if (bootverbose)
				device_printf(dev,
				    "child %s requested type %d for rid %#x,"
				    " but the BAR says it is an ioport\n",
				    device_get_nameunit(child), type, *rid);
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
	count = (pci_addr_t)1 << mapsize;
	if (RF_ALIGNMENT(flags) < mapsize)
		flags = (flags & ~RF_ALIGNMENT_MASK) | RF_ALIGNMENT_LOG2(mapsize);
	if (PCI_BAR_MEM(map) && (map & PCIM_BAR_MEM_PREFETCH))
		flags |= RF_PREFETCHABLE;

	/*
	 * Allocate enough resource, and then write back the
	 * appropriate BAR for that resource.
	 */
	res = BUS_ALLOC_RESOURCE(device_get_parent(dev), child, type, rid,
	    start, end, count, flags & ~RF_ACTIVE);
	if (res == NULL) {
		device_printf(child,
		    "%#lx bytes of rid %#x res %d failed (%#lx, %#lx).\n",
		    count, *rid, type, start, end);
		goto out;
	}
	resource_list_add(rl, type, *rid, start, end, count);
	rle = resource_list_find(rl, type, *rid);
	if (rle == NULL)
		panic("pci_reserve_map: unexpectedly can't find resource.");
	rle->res = res;
	rle->start = rman_get_start(res);
	rle->end = rman_get_end(res);
	rle->count = count;
	rle->flags = RLE_RESERVED;
	if (bootverbose)
		device_printf(child,
		    "Lazy allocation of %#lx bytes rid %#x type %d at %#lx\n",
		    count, *rid, type, rman_get_start(res));
	map = rman_get_start(res);
	pci_write_bar(child, pm, map);
out:
	return (res);
}

struct resource *
pci_alloc_resource(device_t dev, device_t child, int type, int *rid,
		   u_long start, u_long end, u_long count, u_int flags)
{
	struct pci_devinfo *dinfo;
	struct resource_list *rl;
	struct resource_list_entry *rle;
	struct resource *res;
	pcicfgregs *cfg;

	if (device_get_parent(child) != dev)
		return (BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
		    type, rid, start, end, count, flags));

	/*
	 * Perform lazy resource allocation
	 */
	dinfo = device_get_ivars(child);
	rl = &dinfo->resources;
	cfg = &dinfo->cfg;
	switch (type) {
	case SYS_RES_IRQ:
		/*
		 * Can't alloc legacy interrupt once MSI messages have
		 * been allocated.
		 */
		if (*rid == 0 && (cfg->msi.msi_alloc > 0 ||
		    cfg->msix.msix_alloc > 0))
			return (NULL);

		/*
		 * If the child device doesn't have an interrupt
		 * routed and is deserving of an interrupt, try to
		 * assign it one.
		 */
		if (*rid == 0 && !PCI_INTERRUPT_VALID(cfg->intline) &&
		    (cfg->intpin != 0))
			pci_assign_interrupt(dev, child, 0);
		break;
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
#ifdef NEW_PCIB
		/*
		 * PCI-PCI bridge I/O window resources are not BARs.
		 * For those allocations just pass the request up the
		 * tree.
		 */
		if (cfg->hdrtype == PCIM_HDRTYPE_BRIDGE) {
			switch (*rid) {
			case PCIR_IOBASEL_1:
			case PCIR_MEMBASE_1:
			case PCIR_PMBASEL_1:
				/*
				 * XXX: Should we bother creating a resource
				 * list entry?
				 */
				return (bus_generic_alloc_resource(dev, child,
				    type, rid, start, end, count, flags));
			}
		}
#endif
		/* Reserve resources for this BAR if needed. */
		rle = resource_list_find(rl, type, *rid);
		if (rle == NULL) {
			res = pci_reserve_map(dev, child, type, rid, start, end,
			    count, flags);
			if (res == NULL)
				return (NULL);
		}
	}
	return (resource_list_alloc(rl, dev, child, type, rid,
	    start, end, count, flags));
}

int
pci_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct pci_devinfo *dinfo;
	struct resource_list *rl;
	pcicfgregs *cfg;

	if (device_get_parent(child) != dev)
		return (BUS_RELEASE_RESOURCE(device_get_parent(dev), child,
		    type, rid, r));

	dinfo = device_get_ivars(child);
	cfg = &dinfo->cfg;
#ifdef NEW_PCIB
	/*
	 * PCI-PCI bridge I/O window resources are not BARs.  For
	 * those allocations just pass the request up the tree.
	 */
	if (cfg->hdrtype == PCIM_HDRTYPE_BRIDGE &&
	    (type == SYS_RES_IOPORT || type == SYS_RES_MEMORY)) {
		switch (rid) {
		case PCIR_IOBASEL_1:
		case PCIR_MEMBASE_1:
		case PCIR_PMBASEL_1:
			return (bus_generic_release_resource(dev, child, type,
			    rid, r));
		}
	}
#endif

	rl = &dinfo->resources;
	return (resource_list_release(rl, dev, child, type, rid, r));
}

int
pci_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct pci_devinfo *dinfo;
	int error;

	error = bus_generic_activate_resource(dev, child, type, rid, r);
	if (error)
		return (error);

	/* Enable decoding in the command register when activating BARs. */
	if (device_get_parent(child) == dev) {
		/* Device ROMs need their decoding explicitly enabled. */
		dinfo = device_get_ivars(child);
		if (type == SYS_RES_MEMORY && PCIR_IS_BIOS(&dinfo->cfg, rid))
			pci_write_bar(child, pci_find_bar(child, rid),
			    rman_get_start(r) | PCIM_BIOS_ENABLE);
		switch (type) {
		case SYS_RES_IOPORT:
		case SYS_RES_MEMORY:
			error = PCI_ENABLE_IO(dev, child, type);
			break;
		}
	}
	return (error);
}

int
pci_deactivate_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	struct pci_devinfo *dinfo;
	int error;

	error = bus_generic_deactivate_resource(dev, child, type, rid, r);
	if (error)
		return (error);

	/* Disable decoding for device ROMs. */	
	if (device_get_parent(child) == dev) {
		dinfo = device_get_ivars(child);
		if (type == SYS_RES_MEMORY && PCIR_IS_BIOS(&dinfo->cfg, rid))
			pci_write_bar(child, pci_find_bar(child, rid),
			    rman_get_start(r));
	}
	return (0);
}

void
pci_delete_child(device_t dev, device_t child)
{
	struct resource_list_entry *rle;
	struct resource_list *rl;
	struct pci_devinfo *dinfo;

	dinfo = device_get_ivars(child);
	rl = &dinfo->resources;

	if (device_is_attached(child))
		device_detach(child);

	/* Turn off access to resources we're about to free */
	pci_write_config(child, PCIR_COMMAND, pci_read_config(child,
	    PCIR_COMMAND, 2) & ~(PCIM_CMD_MEMEN | PCIM_CMD_PORTEN), 2);

	/* Free all allocated resources */
	STAILQ_FOREACH(rle, rl, link) {
		if (rle->res) {
			if (rman_get_flags(rle->res) & RF_ACTIVE ||
			    resource_list_busy(rl, rle->type, rle->rid)) {
				pci_printf(&dinfo->cfg,
				    "Resource still owned, oops. "
				    "(type=%d, rid=%d, addr=%lx)\n",
				    rle->type, rle->rid,
				    rman_get_start(rle->res));
				bus_release_resource(child, rle->type, rle->rid,
				    rle->res);
			}
			resource_list_unreserve(rl, dev, child, rle->type,
			    rle->rid);
		}
	}
	resource_list_free(rl);

	device_delete_child(dev, child);
	pci_freecfg(dinfo);
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
	if (rle == NULL)
		return;

	if (rle->res) {
		if (rman_get_flags(rle->res) & RF_ACTIVE ||
		    resource_list_busy(rl, type, rid)) {
			device_printf(dev, "delete_resource: "
			    "Resource still owned by child, oops. "
			    "(type=%d, rid=%d, addr=%lx)\n",
			    type, rid, rman_get_start(rle->res));
			return;
		}
		resource_list_unreserve(rl, dev, child, type, rid);
	}
	resource_list_delete(rl, type, rid);
}

struct resource_list *
pci_get_resource_list (device_t dev, device_t child)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);

	return (&dinfo->resources);
}

bus_dma_tag_t
pci_get_dma_tag(device_t bus, device_t dev)
{
	struct pci_softc *sc = device_get_softc(bus);

	return (sc->sc_dma_tag);
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
pci_cfg_restore_pcie(device_t dev, struct pci_devinfo *dinfo)
{
#define	WREG(n, v)	pci_write_config(dev, pos + (n), (v), 2)
	struct pcicfg_pcie *cfg;
	int version, pos;

	cfg = &dinfo->cfg.pcie;
	pos = cfg->pcie_location;

	version = cfg->pcie_flags & PCIEM_FLAGS_VERSION;

	WREG(PCIER_DEVICE_CTL, cfg->pcie_device_ctl);

	if (version > 1 || cfg->pcie_type == PCIEM_TYPE_ROOT_PORT ||
	    cfg->pcie_type == PCIEM_TYPE_ENDPOINT ||
	    cfg->pcie_type == PCIEM_TYPE_LEGACY_ENDPOINT)
		WREG(PCIER_LINK_CTL, cfg->pcie_link_ctl);

	if (version > 1 || (cfg->pcie_type == PCIEM_TYPE_ROOT_PORT ||
	    (cfg->pcie_type == PCIEM_TYPE_DOWNSTREAM_PORT &&
	     (cfg->pcie_flags & PCIEM_FLAGS_SLOT))))
		WREG(PCIER_SLOT_CTL, cfg->pcie_slot_ctl);

	if (version > 1 || cfg->pcie_type == PCIEM_TYPE_ROOT_PORT ||
	    cfg->pcie_type == PCIEM_TYPE_ROOT_EC)
		WREG(PCIER_ROOT_CTL, cfg->pcie_root_ctl);

	if (version > 1) {
		WREG(PCIER_DEVICE_CTL2, cfg->pcie_device_ctl2);
		WREG(PCIER_LINK_CTL2, cfg->pcie_link_ctl2);
		WREG(PCIER_SLOT_CTL2, cfg->pcie_slot_ctl2);
	}
#undef WREG
}

static void
pci_cfg_restore_pcix(device_t dev, struct pci_devinfo *dinfo)
{
	pci_write_config(dev, dinfo->cfg.pcix.pcix_location + PCIXR_COMMAND,
	    dinfo->cfg.pcix.pcix_command,  2);
}

void
pci_cfg_restore(device_t dev, struct pci_devinfo *dinfo)
{

	/*
	 * Only do header type 0 devices.  Type 1 devices are bridges,
	 * which we know need special treatment.  Type 2 devices are
	 * cardbus bridges which also require special treatment.
	 * Other types are unknown, and we err on the side of safety
	 * by ignoring them.
	 */
	if ((dinfo->cfg.hdrtype & PCIM_HDRTYPE) != PCIM_HDRTYPE_NORMAL)
		return;

	/*
	 * Restore the device to full power mode.  We must do this
	 * before we restore the registers because moving from D3 to
	 * D0 will cause the chip's BARs and some other registers to
	 * be reset to some unknown power on reset values.  Cut down
	 * the noise on boot by doing nothing if we are already in
	 * state D0.
	 */
	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0)
		pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	pci_restore_bars(dev);
	pci_write_config(dev, PCIR_COMMAND, dinfo->cfg.cmdreg, 2);
	pci_write_config(dev, PCIR_INTLINE, dinfo->cfg.intline, 1);
	pci_write_config(dev, PCIR_INTPIN, dinfo->cfg.intpin, 1);
	pci_write_config(dev, PCIR_MINGNT, dinfo->cfg.mingnt, 1);
	pci_write_config(dev, PCIR_MAXLAT, dinfo->cfg.maxlat, 1);
	pci_write_config(dev, PCIR_CACHELNSZ, dinfo->cfg.cachelnsz, 1);
	pci_write_config(dev, PCIR_LATTIMER, dinfo->cfg.lattimer, 1);
	pci_write_config(dev, PCIR_PROGIF, dinfo->cfg.progif, 1);
	pci_write_config(dev, PCIR_REVID, dinfo->cfg.revid, 1);

	/*
	 * Restore extended capabilities for PCI-Express and PCI-X
	 */
	if (dinfo->cfg.pcie.pcie_location != 0)
		pci_cfg_restore_pcie(dev, dinfo);
	if (dinfo->cfg.pcix.pcix_location != 0)
		pci_cfg_restore_pcix(dev, dinfo);

	/* Restore MSI and MSI-X configurations if they are present. */
	if (dinfo->cfg.msi.msi_location != 0)
		pci_resume_msi(dev);
	if (dinfo->cfg.msix.msix_location != 0)
		pci_resume_msix(dev);
}

static void
pci_cfg_save_pcie(device_t dev, struct pci_devinfo *dinfo)
{
#define	RREG(n)	pci_read_config(dev, pos + (n), 2)
	struct pcicfg_pcie *cfg;
	int version, pos;

	cfg = &dinfo->cfg.pcie;
	pos = cfg->pcie_location;

	cfg->pcie_flags = RREG(PCIER_FLAGS);

	version = cfg->pcie_flags & PCIEM_FLAGS_VERSION;

	cfg->pcie_device_ctl = RREG(PCIER_DEVICE_CTL);

	if (version > 1 || cfg->pcie_type == PCIEM_TYPE_ROOT_PORT ||
	    cfg->pcie_type == PCIEM_TYPE_ENDPOINT ||
	    cfg->pcie_type == PCIEM_TYPE_LEGACY_ENDPOINT)
		cfg->pcie_link_ctl = RREG(PCIER_LINK_CTL);

	if (version > 1 || (cfg->pcie_type == PCIEM_TYPE_ROOT_PORT ||
	    (cfg->pcie_type == PCIEM_TYPE_DOWNSTREAM_PORT &&
	     (cfg->pcie_flags & PCIEM_FLAGS_SLOT))))
		cfg->pcie_slot_ctl = RREG(PCIER_SLOT_CTL);

	if (version > 1 || cfg->pcie_type == PCIEM_TYPE_ROOT_PORT ||
	    cfg->pcie_type == PCIEM_TYPE_ROOT_EC)
		cfg->pcie_root_ctl = RREG(PCIER_ROOT_CTL);

	if (version > 1) {
		cfg->pcie_device_ctl2 = RREG(PCIER_DEVICE_CTL2);
		cfg->pcie_link_ctl2 = RREG(PCIER_LINK_CTL2);
		cfg->pcie_slot_ctl2 = RREG(PCIER_SLOT_CTL2);
	}
#undef RREG
}

static void
pci_cfg_save_pcix(device_t dev, struct pci_devinfo *dinfo)
{
	dinfo->cfg.pcix.pcix_command = pci_read_config(dev,
	    dinfo->cfg.pcix.pcix_location + PCIXR_COMMAND, 2);
}

void
pci_cfg_save(device_t dev, struct pci_devinfo *dinfo, int setstate)
{
	uint32_t cls;
	int ps;

	/*
	 * Only do header type 0 devices.  Type 1 devices are bridges, which
	 * we know need special treatment.  Type 2 devices are cardbus bridges
	 * which also require special treatment.  Other types are unknown, and
	 * we err on the side of safety by ignoring them.  Powering down
	 * bridges should not be undertaken lightly.
	 */
	if ((dinfo->cfg.hdrtype & PCIM_HDRTYPE) != PCIM_HDRTYPE_NORMAL)
		return;

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

	if (dinfo->cfg.pcie.pcie_location != 0)
		pci_cfg_save_pcie(dev, dinfo);

	if (dinfo->cfg.pcix.pcix_location != 0)
		pci_cfg_save_pcix(dev, dinfo);

	/*
	 * don't set the state for display devices, base peripherals and
	 * memory devices since bad things happen when they are powered down.
	 * We should (a) have drivers that can easily detach and (b) use
	 * generic drivers for these devices so that some device actually
	 * attaches.  We need to make sure that when we implement (a) we don't
	 * power the device down on a reattach.
	 */
	cls = pci_get_class(dev);
	if (!setstate)
		return;
	switch (pci_do_power_nodriver)
	{
		case 0:		/* NO powerdown at all */
			return;
		case 1:		/* Conservative about what to power down */
			if (cls == PCIC_STORAGE)
				return;
			/*FALLTHROUGH*/
		case 2:		/* Agressive about what to power down */
			if (cls == PCIC_DISPLAY || cls == PCIC_MEMORY ||
			    cls == PCIC_BASEPERIPH)
				return;
			/*FALLTHROUGH*/
		case 3:		/* Power down everything */
			break;
	}
	/*
	 * PCI spec says we can only go into D3 state from D0 state.
	 * Transition from D[12] into D0 before going to D3 state.
	 */
	ps = pci_get_powerstate(dev);
	if (ps != PCI_POWERSTATE_D0 && ps != PCI_POWERSTATE_D3)
		pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D3)
		pci_set_powerstate(dev, PCI_POWERSTATE_D3);
}

/* Wrapper APIs suitable for device driver use. */
void
pci_save_state(device_t dev)
{
	struct pci_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	pci_cfg_save(dev, dinfo, 0);
}

void
pci_restore_state(device_t dev)
{
	struct pci_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	pci_cfg_restore(dev, dinfo);
}
