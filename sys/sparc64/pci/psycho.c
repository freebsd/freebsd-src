/*-
 * Copyright (c) 1999, 2000 Matthew R. Green
 * Copyright (c) 2001 - 2003 by Thomas Moestl <tmm@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: psycho.c,v 1.39 2001/10/07 20:30:41 eeh Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Support for `Hummingbird' (UltraSPARC IIe), `Psycho' and `Psycho+'
 * (UltraSPARC II) and `Sabre' (UltraSPARC IIi) UPA to PCI bridges.
 */

#include "opt_ofw_pci.h"
#include "opt_psycho.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/reboot.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/bus_private.h>
#include <machine/iommureg.h>
#include <machine/bus_common.h>
#include <machine/nexusvar.h>
#include <machine/ofw_bus.h>
#include <machine/ofw_upa.h>
#include <machine/resource.h>
#include <machine/ver.h>

#include <sys/rman.h>

#include <machine/iommuvar.h>

#include <dev/pci/pcivar.h>

#include <sparc64/pci/ofw_pci.h>
#include <sparc64/pci/psychoreg.h>
#include <sparc64/pci/psychovar.h>

#include "pcib_if.h"

static const struct psycho_desc *psycho_find_desc(const struct psycho_desc *,
    const char *);
static const struct psycho_desc *psycho_get_desc(phandle_t, const char *);
static void psycho_set_intr(struct psycho_softc *, int, device_t, bus_addr_t,
    int, driver_intr_t);
static int psycho_find_intrmap(struct psycho_softc *, int, bus_addr_t *,
    bus_addr_t *, u_long *);
static void psycho_intr_stub(void *);
static bus_space_tag_t psycho_alloc_bus_tag(struct psycho_softc *, int);

/* Interrupt handlers */
static void psycho_ue(void *);
static void psycho_ce(void *);
static void psycho_pci_bus(void *);
static void psycho_powerfail(void *);
static void psycho_overtemp(void *);
#ifdef PSYCHO_MAP_WAKEUP
static void psycho_wakeup(void *);
#endif

/* IOMMU support */
static void psycho_iommu_init(struct psycho_softc *, int, uint32_t);

/*
 * Methods
 */
static device_probe_t psycho_probe;
static device_attach_t psycho_attach;
static bus_read_ivar_t psycho_read_ivar;
static bus_setup_intr_t psycho_setup_intr;
static bus_teardown_intr_t psycho_teardown_intr;
static bus_alloc_resource_t psycho_alloc_resource;
static bus_activate_resource_t psycho_activate_resource;
static bus_deactivate_resource_t psycho_deactivate_resource;
static bus_release_resource_t psycho_release_resource;
static pcib_maxslots_t psycho_maxslots;
static pcib_read_config_t psycho_read_config;
static pcib_write_config_t psycho_write_config;
static pcib_route_interrupt_t psycho_route_interrupt;
static ofw_pci_intr_pending_t psycho_intr_pending;
static ofw_pci_get_bus_handle_t psycho_get_bus_handle;
static ofw_bus_get_node_t psycho_get_node;
static ofw_pci_adjust_busrange_t psycho_adjust_busrange;

static device_method_t psycho_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		psycho_probe),
	DEVMETHOD(device_attach,	psycho_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	psycho_read_ivar),
	DEVMETHOD(bus_setup_intr,	psycho_setup_intr),
	DEVMETHOD(bus_teardown_intr,	psycho_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	psycho_alloc_resource),
	DEVMETHOD(bus_activate_resource,	psycho_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	psycho_deactivate_resource),
	DEVMETHOD(bus_release_resource,	psycho_release_resource),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	psycho_maxslots),
	DEVMETHOD(pcib_read_config,	psycho_read_config),
	DEVMETHOD(pcib_write_config,	psycho_write_config),
	DEVMETHOD(pcib_route_interrupt,	psycho_route_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	psycho_get_node),

	/* ofw_pci interface */
	DEVMETHOD(ofw_pci_intr_pending,	psycho_intr_pending),
	DEVMETHOD(ofw_pci_get_bus_handle,	psycho_get_bus_handle),
	DEVMETHOD(ofw_pci_adjust_busrange,	psycho_adjust_busrange),

	{ 0, 0 }
};

static driver_t psycho_driver = {
	"pcib",
	psycho_methods,
	sizeof(struct psycho_softc),
};

static devclass_t psycho_devclass;

DRIVER_MODULE(psycho, nexus, psycho_driver, psycho_devclass, 0, 0);

SLIST_HEAD(, psycho_softc) psycho_softcs =
    SLIST_HEAD_INITIALIZER(psycho_softcs);

struct psycho_clr {
	struct psycho_softc	*pci_sc;
	bus_addr_t		pci_clr;	/* clear register */
	driver_intr_t		*pci_handler;	/* handler to call */
	void			*pci_arg;	/* argument for the handler */
	void			*pci_cookie;	/* parent bus int. cookie */
};

#define	PSYCHO_READ8(sc, off) \
	bus_space_read_8((sc)->sc_bustag, (sc)->sc_bushandle, (off))
#define	PSYCHO_WRITE8(sc, off, v) \
	bus_space_write_8((sc)->sc_bustag, (sc)->sc_bushandle, (off), (v))
#define	PCICTL_READ8(sc, off) \
	PSYCHO_READ8((sc), (sc)->sc_pcictl + (off))
#define	PCICTL_WRITE8(sc, off, v) \
	PSYCHO_WRITE8((sc), (sc)->sc_pcictl + (off), (v))

/*
 * "Sabre" is the UltraSPARC IIi onboard UPA to PCI bridge.  It manages a
 * single PCI bus and does not have a streaming buffer.  It often has an APB
 * (advanced PCI bridge) connected to it, which was designed specifically for
 * the IIi.  The APB let's the IIi handle two independednt PCI buses, and
 * appears as two "Simba"'s underneath the Sabre.
 *
 * "Hummingbird" is the UltraSPARC IIe onboard UPA to PCI bridge. It's
 * basically the same as Sabre but without an APB underneath it.
 *
 * "Psycho" and "Psycho+" are dual UPA to PCI bridges.  They sit on the UPA bus
 * and manage two PCI buses.  "Psycho" has two 64-bit 33MHz buses, while
 * "Psycho+" controls both a 64-bit 33Mhz and a 64-bit 66Mhz PCI bus.  You
 * will usually find a "Psycho+" since I don't think the original "Psycho"
 * ever shipped, and if it did it would be in the U30.
 *
 * Each "Psycho" PCI bus appears as a separate OFW node, but since they are
 * both part of the same IC, they only have a single register space.  As such,
 * they need to be configured together, even though the autoconfiguration will
 * attach them separately.
 *
 * On UltraIIi machines, "Sabre" itself usually takes pci0, with "Simba" often
 * as pci1 and pci2, although they have been implemented with other PCI bus
 * numbers on some machines.
 *
 * On UltraII machines, there can be any number of "Psycho+" ICs, each
 * providing two PCI buses.
 */
#ifdef DEBUGGER_ON_POWERFAIL
#define	PSYCHO_PWRFAIL_INT_FLAGS	INTR_FAST
#else
#define	PSYCHO_PWRFAIL_INT_FLAGS	0
#endif

#define	OFW_PCI_TYPE		"pci"

struct psycho_desc {
	const char	*pd_string;
	int		pd_mode;
	const char	*pd_name;
};

static const struct psycho_desc psycho_compats[] = {
	{ "pci108e,8000", PSYCHO_MODE_PSYCHO,	"Psycho compatible" },
	{ "pci108e,a000", PSYCHO_MODE_SABRE,	"Sabre compatible" },
	{ "pci108e,a001", PSYCHO_MODE_SABRE,	"Hummingbird compatible" },
	{ NULL,		  0,			NULL }
};

static const struct psycho_desc psycho_models[] = {
	{ "SUNW,psycho",  PSYCHO_MODE_PSYCHO,	"Psycho" },
	{ "SUNW,sabre",   PSYCHO_MODE_SABRE,	"Sabre" },
	{ NULL,		  0,			NULL }
};

static const struct psycho_desc *
psycho_find_desc(const struct psycho_desc *table, const char *string)
{
	const struct psycho_desc *desc;

	for (desc = table; desc->pd_string != NULL; desc++) {
		if (strcmp(desc->pd_string, string) == 0)
			return (desc);
	}
	return (NULL);
}

static const struct psycho_desc *
psycho_get_desc(phandle_t node, const char *model)
{
	const struct psycho_desc *rv;
	char compat[32];

	rv = NULL;
	if (model != NULL)
		rv = psycho_find_desc(psycho_models, model);
	if (rv == NULL &&
	    OF_getprop(node, "compatible", compat, sizeof(compat)) != -1)
		rv = psycho_find_desc(psycho_compats, compat);
	return (rv);
}

static int
psycho_probe(device_t dev)
{
	const char *dtype;

	dtype = nexus_get_device_type(dev);
	if (nexus_get_reg(dev) != NULL && dtype != NULL &&
	    strcmp(dtype, OFW_PCI_TYPE) == 0 &&
	    psycho_get_desc(nexus_get_node(dev),
	    nexus_get_model(dev)) != NULL) {
		device_set_desc(dev, "U2P UPA-PCI bridge");
		return (0);
	}

	return (ENXIO);
}

static int
psycho_attach(device_t dev)
{
	struct psycho_softc *sc;
	struct psycho_softc *osc = NULL;
	struct psycho_softc *asc;
	struct ofw_pci_ranges *range;
	struct upa_regs *reg;
	const struct psycho_desc *desc;
	phandle_t node;
	uint64_t csr;
	uint32_t dvmabase;
	u_long mlen;
	int psycho_br[2];
	int n, i, nrange, nreg, rid;
#ifdef PSYCHO_DEBUG
	bus_addr_t map, clr;
	uint64_t mr;
#endif

	node = nexus_get_node(dev);
	sc = device_get_softc(dev);
	desc = psycho_get_desc(node, nexus_get_model(dev));

	sc->sc_node = node;
	sc->sc_dev = dev;
	sc->sc_mode = desc->pd_mode;

	/*
	 * The Psycho gets three register banks:
	 * (0) per-PBM configuration and status registers
	 * (1) per-PBM PCI configuration space, containing only the
	 *     PBM 256-byte PCI header
	 * (2) the shared Psycho configuration registers
	 */
	reg = nexus_get_reg(dev);
	nreg = nexus_get_nreg(dev);
	if (sc->sc_mode == PSYCHO_MODE_PSYCHO) {
		if (nreg <= 2)
			panic("%s: %d not enough registers", __func__, nreg);
		sc->sc_basepaddr = (vm_paddr_t)UPA_REG_PHYS(&reg[2]);
		mlen = UPA_REG_SIZE(&reg[2]);
		sc->sc_pcictl = UPA_REG_PHYS(&reg[0]) - sc->sc_basepaddr;
		switch (sc->sc_pcictl) {
		case PSR_PCICTL0:
			sc->sc_half = 0;
			break;
		case PSR_PCICTL1:
			sc->sc_half = 1;
			break;
		default:
			panic("%s: bogus PCI control register location",
			    __func__);
		}
	} else {
		if (nreg <= 0)
			panic("%s: %d not enough registers", __func__, nreg);
		sc->sc_basepaddr = (vm_paddr_t)UPA_REG_PHYS(&reg[0]);
		mlen = UPA_REG_SIZE(reg);
		sc->sc_pcictl = PSR_PCICTL0;
		sc->sc_half = 0;
	}

	/*
	 * Match other Psycho's that are already configured against
	 * the base physical address. This will be the same for a
	 * pair of devices that share register space.
	 */
	SLIST_FOREACH(asc, &psycho_softcs, sc_link) {
		if (asc->sc_basepaddr == sc->sc_basepaddr) {
			/* Found partner. */
			osc = asc;
			break;
		}
	}

	if (osc == NULL) {
		rid = 0;
		sc->sc_mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
		    sc->sc_basepaddr, sc->sc_basepaddr + mlen - 1, mlen,
		    RF_ACTIVE);
		if (sc->sc_mem_res == NULL ||
		    rman_get_start(sc->sc_mem_res) != sc->sc_basepaddr)
			panic("%s: could not allocate device memory", __func__);
		sc->sc_bustag = rman_get_bustag(sc->sc_mem_res);
		sc->sc_bushandle = rman_get_bushandle(sc->sc_mem_res);
	} else {
		/*
		 * There's another Psycho using the same register space.
		 * Copy the relevant stuff.
		 */
		sc->sc_mem_res = NULL;
		sc->sc_bustag = osc->sc_bustag;
		sc->sc_bushandle = osc->sc_bushandle;
	}
	csr = PSYCHO_READ8(sc, PSR_CS);
	sc->sc_ign = 0x7c0; /* Hummingbird/Sabre IGN is always 0x1f. */
	if (sc->sc_mode == PSYCHO_MODE_PSYCHO)
		sc->sc_ign = PSYCHO_GCSR_IGN(csr) << INTMAP_IGN_SHIFT;

	device_printf(dev, "%s, impl %d, version %d, ign %#x, bus %c\n",
	    desc->pd_name, (int)PSYCHO_GCSR_IMPL(csr),
	    (int)PSYCHO_GCSR_VERS(csr), sc->sc_ign, 'A' + sc->sc_half);

	/* Setup the PCI control register. */
	csr = PCICTL_READ8(sc, PCR_CS);
	csr |= PCICTL_MRLM | PCICTL_ARB_PARK | PCICTL_ERRINTEN | PCICTL_4ENABLE;
	csr &= ~(PCICTL_SERR | PCICTL_CPU_PRIO | PCICTL_ARB_PRIO |
	    PCICTL_RTRYWAIT);
	PCICTL_WRITE8(sc, PCR_CS, csr);

	if (sc->sc_mode == PSYCHO_MODE_SABRE) {
		/* Use the PROM preset for now. */
		csr = PCICTL_READ8(sc, PCR_TAS);
		if (csr == 0)
			panic("%s: Hummingbird/Sabre TAS not initialized.",
			    __func__);
		dvmabase = (ffs(csr) - 1) << PCITAS_ADDR_SHIFT;
	} else
		dvmabase = -1;

	/* Initialize memory and I/O rmans. */
	sc->sc_pci_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_pci_io_rman.rm_descr = "Psycho PCI I/O Ports";
	if (rman_init(&sc->sc_pci_io_rman) != 0 ||
	    rman_manage_region(&sc->sc_pci_io_rman, 0, PSYCHO_IO_SIZE) != 0)
		panic("%s: failed to set up I/O rman", __func__);
	sc->sc_pci_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_pci_mem_rman.rm_descr = "Psycho PCI Memory";
	if (rman_init(&sc->sc_pci_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_pci_mem_rman, 0, PSYCHO_MEM_SIZE) != 0)
		panic("%s: failed to set up memory rman", __func__);

	nrange = OF_getprop_alloc(node, "ranges", sizeof(*range),
	    (void **)&range);
	/*
	 * Make sure that the expected ranges are present. The OFW_PCI_CS_MEM64
	 * one is not currently used though.
	 */
	if (nrange != PSYCHO_NRANGE)
		panic("%s: unsupported number of ranges", __func__);
	/*
	 * Find the addresses of the various bus spaces.
	 * There should not be multiple ones of one kind.
	 * The physical start addresses of the ranges are the configuration,
	 * memory and I/O handles.
	 */
	for (n = 0; n < PSYCHO_NRANGE; n++) {
		i = OFW_PCI_RANGE_CS(&range[n]);
		if (sc->sc_pci_bh[i] != 0)
			panic("%s: duplicate range for space %d", __func__, i);
		sc->sc_pci_bh[i] = OFW_PCI_RANGE_PHYS(&range[n]);
	}
	free(range, M_OFWPROP);

	/* Register the softc, this is needed for paired Psychos. */
	SLIST_INSERT_HEAD(&psycho_softcs, sc, sc_link);

	/*
	 * Register a PCI bus error interrupt handler according to which
	 * half this is. Hummingbird/Sabre don't have a PCI bus B error
	 * interrupt but they are also only used for PCI bus A.
	 */
	psycho_set_intr(sc, 0, dev, sc->sc_half == 0 ? PSR_PCIAERR_INT_MAP :
	    PSR_PCIBERR_INT_MAP, INTR_FAST, psycho_pci_bus);

	/*
	 * If we're a Hummingbird/Sabre or the first of a pair of Psycho's to
	 * arrive here, start up the IOMMU.
	 */
	if (osc == NULL) {
		/*
		 * Establish handlers for interesting interrupts...
		 *
		 * XXX We need to remember these and remove this to support
		 * hotplug on the UPA/FHC bus.
		 *
		 * XXX Not all controllers have these, but installing them
		 * is better than trying to sort through this mess.
		 */
		psycho_set_intr(sc, 1, dev, PSR_UE_INT_MAP, INTR_FAST,
		    psycho_ue);
		psycho_set_intr(sc, 2, dev, PSR_CE_INT_MAP, 0, psycho_ce);
		psycho_set_intr(sc, 3, dev, PSR_POWER_INT_MAP,
		    PSYCHO_PWRFAIL_INT_FLAGS, psycho_powerfail);
		/* Psycho-specific initialization */
		if (sc->sc_mode == PSYCHO_MODE_PSYCHO) {
			/*
			 * Hummingbirds/Sabres do not have the following two
			 * interrupts.
			 */

			/*
			 * The spare hardware interrupt is used for the
			 * over-temperature interrupt.
			 */
			psycho_set_intr(sc, 4, dev, PSR_SPARE_INT_MAP,
			    INTR_FAST, psycho_overtemp);
#ifdef PSYCHO_MAP_WAKEUP
			/*
			 * psycho_wakeup() doesn't do anything useful right
			 * now.
			 */
			psycho_set_intr(sc, 5, dev, PSR_PWRMGT_INT_MAP, 0,
			    psycho_wakeup);
#endif /* PSYCHO_MAP_WAKEUP */

			/* Initialize the counter-timer. */
			sparc64_counter_init(sc->sc_bustag, sc->sc_bushandle,
			    PSR_TC0);
		}

		/*
		 * Setup IOMMU and PCI configuration if we're the first
		 * of a pair of Psycho's to arrive here.
		 *
		 * We should calculate a TSB size based on amount of RAM
		 * and number of bus controllers and number and type of
		 * child devices.
		 *
		 * For the moment, 32KB should be more than enough.
		 */
		sc->sc_is = malloc(sizeof(struct iommu_state), M_DEVBUF,
		    M_NOWAIT);
		if (sc->sc_is == NULL)
			panic("%s: malloc iommu_state failed", __func__);
		sc->sc_is->is_sb[0] = 0;
		sc->sc_is->is_sb[1] = 0;
		if (OF_getproplen(node, "no-streaming-cache") < 0)
			sc->sc_is->is_sb[0] = sc->sc_pcictl + PCR_STRBUF;
		psycho_iommu_init(sc, 3, dvmabase);
	} else {
		/* Just copy IOMMU state, config tag and address. */
		sc->sc_is = osc->sc_is;
		if (OF_getproplen(node, "no-streaming-cache") < 0)
			sc->sc_is->is_sb[1] = sc->sc_pcictl + PCR_STRBUF;
		iommu_reset(sc->sc_is);
	}

	/* Allocate our tags. */
	sc->sc_pci_memt = psycho_alloc_bus_tag(sc, PCI_MEMORY_BUS_SPACE);
	sc->sc_pci_iot = psycho_alloc_bus_tag(sc, PCI_IO_BUS_SPACE);
	sc->sc_pci_cfgt = psycho_alloc_bus_tag(sc, PCI_CONFIG_BUS_SPACE);
	if (bus_dma_tag_create(nexus_get_dmatag(dev), 8, 1, 0, 0x3ffffffff,
	    NULL, NULL, 0x3ffffffff, 0xff, 0xffffffff, 0, NULL, NULL,
	    &sc->sc_pci_dmat) != 0)
		panic("%s: bus_dma_tag_create failed", __func__);
	/* Customize the tag. */
	sc->sc_pci_dmat->dt_cookie = sc->sc_is;
	sc->sc_pci_dmat->dt_mt = &iommu_dma_methods;
	/* XXX: register as root DMA tag (kludge). */
	sparc64_root_dma_tag = sc->sc_pci_dmat;

#ifdef PSYCHO_DEBUG
	/*
	 * Enable all interrupts and clear all interrupt states.
	 * This aids the debugging of interrupt routing problems.
	 */
	for (map = PSR_PCIA0_INT_MAP, clr = PSR_PCIA0_INT_CLR, n = 0;
	     map <= PSR_PCIB3_INT_MAP; map += 8, clr += 32, n++) {
		mr = PSYCHO_READ8(sc, map);
		device_printf(dev, "intr map (pci) %d: %#lx\n", n, (u_long)mr);
		PSYCHO_WRITE8(sc, map, mr & ~INTMAP_V);
		for (i = 0; i < 4; i++)
			PCICTL_WRITE8(sc, clr + i * 8, 0);
		PSYCHO_WRITE8(sc, map, INTMAP_ENABLE(mr, PCPU_GET(mid)));
	}
	for (map = PSR_SCSI_INT_MAP, clr = PSR_SCSI_INT_CLR, n = 0;
	     map <= PSR_SERIAL_INT_MAP; map += 8, clr += 8, n++) {
		mr = PSYCHO_READ8(sc, map);
		device_printf(dev, "intr map (obio) %d: %#lx, clr: %#lx\n", n,
		    (u_long)mr, (u_long)clr);
		PSYCHO_WRITE8(sc, map, mr & ~INTMAP_V);
		PSYCHO_WRITE8(sc, clr, 0);
		PSYCHO_WRITE8(sc, map, INTMAP_ENABLE(mr, PCPU_GET(mid)));
	}
#endif /* PSYCHO_DEBUG */

	/*
	 * Get the bus range from the firmware; it is used solely for obtaining
	 * the inital bus number, and cannot be trusted on all machines.
	 */
	n = OF_getprop(node, "bus-range", (void *)psycho_br, sizeof(psycho_br));
	if (n == -1)
		panic("%s: could not get Psycho bus-range", __func__);
	if (n != sizeof(psycho_br))
		panic("%s: broken Psycho bus-range (%d)", __func__, n);

	sc->sc_pci_secbus = sc->sc_pci_subbus = ofw_pci_alloc_busno(node);
	/*
	 * Program the bus range registers.
	 * NOTE: for the Psycho, the second write changes the bus number the
	 * Psycho itself uses for it's configuration space, so these
	 * writes must be kept in this order!
	 * The Hummingbird/Sabre always uses bus 0, but there only can be one
	 * Hummingbird/Sabre per machine.
	 */
	PCIB_WRITE_CONFIG(dev, psycho_br[0], PCS_DEVICE, PCS_FUNC, PCSR_SUBBUS,
	    sc->sc_pci_subbus, 1);
	PCIB_WRITE_CONFIG(dev, psycho_br[0], PCS_DEVICE, PCS_FUNC, PCSR_SECBUS,
	    sc->sc_pci_secbus, 1);

	ofw_bus_setup_iinfo(node, &sc->sc_pci_iinfo, sizeof(ofw_pci_intr_t));
	/*
	 * On E250 the interrupt map entry for the EBus bridge is wrong,
	 * causing incorrect interrupts to be assigned to some devices on
	 * the EBus. Work around it by changing our copy of the interrupt
	 * map mask to do perform a full comparison of the INO. That way
	 * the interrupt map entry for the EBus bridge won't match at all
	 * and the INOs specified in the "interrupts" properties of the
	 * EBus devices will be used directly instead.
	 */
	if (strcmp(sparc64_model, "SUNW,Ultra-250") == 0 &&
	    sc->sc_pci_iinfo.opi_imapmsk != NULL)
		*(ofw_pci_intr_t *)(&sc->sc_pci_iinfo.opi_imapmsk[
		    sc->sc_pci_iinfo.opi_addrc]) = INTMAP_INO_MASK;

	device_add_child(dev, "pci", sc->sc_pci_secbus);
	return (bus_generic_attach(dev));
}

static void
psycho_set_intr(struct psycho_softc *sc, int index, device_t dev,
    bus_addr_t map, int iflags, driver_intr_t handler)
{
	int rid, vec;
	uint64_t mr;

	rid = index;
	mr = PSYCHO_READ8(sc, map);
	vec = INTVEC(mr);
	sc->sc_irq_res[index] = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
	    vec, vec, 1, RF_ACTIVE);
	if (sc->sc_irq_res[index] == NULL)
		panic("%s: failed to get interrupt", __func__);
	bus_setup_intr(dev, sc->sc_irq_res[index], INTR_TYPE_MISC | iflags,
	    handler, sc, &sc->sc_ihand[index]);
	PSYCHO_WRITE8(sc, map, INTMAP_ENABLE(mr, PCPU_GET(mid)));
}

static int
psycho_find_intrmap(struct psycho_softc *sc, int ino, bus_addr_t *intrmapptr,
    bus_addr_t *intrclrptr, bus_addr_t *intrdiagptr)
{
	bus_addr_t intrmap, intrclr;
	uint64_t im;
	u_long diag;
	int found;

	found = 0;
	/* Hunt thru OBIO first. */
	diag = PSYCHO_READ8(sc, PSR_OBIO_INT_DIAG);
	for (intrmap = PSR_SCSI_INT_MAP, intrclr = PSR_SCSI_INT_CLR;
	     intrmap <= PSR_SERIAL_INT_MAP; intrmap += 8, intrclr += 8,
	     diag >>= 2) {
		im = PSYCHO_READ8(sc, intrmap);
		if (INTINO(im) == ino) {
			diag &= 2;
			found = 1;
			break;
		}
	}

	if (!found) {
		diag = PSYCHO_READ8(sc, PSR_PCI_INT_DIAG);
		/* Now do PCI interrupts. */
		for (intrmap = PSR_PCIA0_INT_MAP, intrclr = PSR_PCIA0_INT_CLR;
		     intrmap <= PSR_PCIB3_INT_MAP; intrmap += 8, intrclr += 32,
		     diag >>= 8) {
			if (sc->sc_mode == PSYCHO_MODE_PSYCHO &&
			    (intrmap == PSR_PCIA2_INT_MAP ||
			     intrmap ==  PSR_PCIA3_INT_MAP))
				continue;
			im = PSYCHO_READ8(sc, intrmap);
			if (((im ^ ino) & 0x3c) == 0) {
				intrclr += 8 * (ino & 3);
				diag = (diag >> ((ino & 3) * 2)) & 2;
				found = 1;
				break;
			}
		}
	}
	if (intrmapptr != NULL)
		*intrmapptr = intrmap;
	if (intrclrptr != NULL)
		*intrclrptr = intrclr;
	if (intrdiagptr != NULL)
		*intrdiagptr = diag;
	return (found);
}

/*
 * Interrupt handlers
 */
static void
psycho_ue(void *arg)
{
	struct psycho_softc *sc = arg;
	uint64_t afar, afsr;

	afar = PSYCHO_READ8(sc, PSR_UE_AFA);
	afsr = PSYCHO_READ8(sc, PSR_UE_AFS);
	/*
	 * On the UltraSPARC-IIi/IIe, IOMMU misses/protection faults cause
	 * the AFAR to be set to the physical address of the TTE entry that
	 * was invalid/write protected. Call into the iommu code to have
	 * them decoded to virtual IO addresses.
	 */
	if ((afsr & UEAFSR_P_DTE) != 0)
		iommu_decode_fault(sc->sc_is, afar);
	panic("%s: uncorrectable DMA error AFAR %#lx AFSR %#lx",
	    device_get_name(sc->sc_dev), (u_long)afar, (u_long)afsr);
}

static void
psycho_ce(void *arg)
{
	struct psycho_softc *sc = arg;
	uint64_t afar, afsr;

	afar = PSYCHO_READ8(sc, PSR_CE_AFA);
	afsr = PSYCHO_READ8(sc, PSR_CE_AFS);
	device_printf(sc->sc_dev, "correctable DMA error AFAR %#lx "
	    "AFSR %#lx\n", (u_long)afar, (u_long)afsr);
	/* Clear the error bits that we caught. */
	PSYCHO_WRITE8(sc, PSR_CE_AFS, afsr & CEAFSR_ERRMASK);
	PSYCHO_WRITE8(sc, PSR_CE_INT_CLR, 0);
}

static void
psycho_pci_bus(void *arg)
{
	struct psycho_softc *sc = arg;
	uint64_t afar, afsr;

	afar = PCICTL_READ8(sc, PCR_AFA);
	afsr = PCICTL_READ8(sc, PCR_AFS);
	panic("%s: PCI bus %c error AFAR %#lx AFSR %#lx",
	    device_get_name(sc->sc_dev), 'A' + sc->sc_half, (u_long)afar,
	    (u_long)afsr);
}

static void
psycho_powerfail(void *arg)
{

#ifdef DEBUGGER_ON_POWERFAIL
	struct psycho_softc *sc = arg;

	kdb_enter("powerfail");
	PSYCHO_WRITE8(sc, PSR_POWER_INT_CLR, 0);
#else
	printf("Power Failure Detected: Shutting down NOW.\n");
	shutdown_nice(0);
#endif
}

static void
psycho_overtemp(void *arg)
{

	printf("DANGER: OVER TEMPERATURE detected.\nShutting down NOW.\n");
	shutdown_nice(RB_POWEROFF);
}

#ifdef PSYCHO_MAP_WAKEUP
static void
psycho_wakeup(void *arg)
{
	struct psycho_softc *sc = arg;

	PSYCHO_WRITE8(sc, PSR_PWRMGT_INT_CLR, 0);
	/* Gee, we don't really have a framework to deal with this properly. */
	device_printf(sc->sc_dev, "power management wakeup\n");
}
#endif /* PSYCHO_MAP_WAKEUP */

static void
psycho_iommu_init(struct psycho_softc *sc, int tsbsize, uint32_t dvmabase)
{
	char *name;
	struct iommu_state *is = sc->sc_is;

	/* Punch in our copies. */
	is->is_bustag = sc->sc_bustag;
	is->is_bushandle = sc->sc_bushandle;
	is->is_iommu = PSR_IOMMU;
	is->is_dtag = PSR_IOMMU_TLB_TAG_DIAG;
	is->is_ddram = PSR_IOMMU_TLB_DATA_DIAG;
	is->is_dqueue = PSR_IOMMU_QUEUE_DIAG;
	is->is_dva = PSR_IOMMU_SVADIAG;
	is->is_dtcmp = PSR_IOMMU_TLB_CMP_DIAG;

	/* Give us a nice name... */
	name = (char *)malloc(32, M_DEVBUF, M_NOWAIT);
	if (name == 0)
		panic("%s: could not malloc iommu name", __func__);
	snprintf(name, 32, "%s dvma", device_get_nameunit(sc->sc_dev));

	iommu_init(name, is, tsbsize, dvmabase, 0);
}

static int
psycho_maxslots(device_t dev)
{

	/* XXX: is this correct? */
	return (PCI_SLOTMAX);
}

static uint32_t
psycho_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int width)
{
	struct psycho_softc *sc;
	bus_space_handle_t bh;
	u_long offset = 0;
	uint8_t byte;
	uint16_t shrt;
	uint32_t wrd;
	uint32_t r;
	int i;

	sc = device_get_softc(dev);
	offset = PSYCHO_CONF_OFF(bus, slot, func, reg);
	bh = sc->sc_pci_bh[OFW_PCI_CS_CONFIG];
	switch (width) {
	case 1:
		i = bus_space_peek_1(sc->sc_pci_cfgt, bh, offset, &byte);
		r = byte;
		break;
	case 2:
		i = bus_space_peek_2(sc->sc_pci_cfgt, bh, offset, &shrt);
		r = shrt;
		break;
	case 4:
		i = bus_space_peek_4(sc->sc_pci_cfgt, bh, offset, &wrd);
		r = wrd;
		break;
	default:
		panic("%s: bad width", __func__);
	}

	if (i) {
#ifdef PSYCHO_DEBUG
		printf("Psycho read data error reading: %d.%d.%d: 0x%x\n",
		    bus, slot, func, reg);
#endif
		r = -1;
	}
	return (r);
}

static void
psycho_write_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
     uint32_t val, int width)
{
	struct psycho_softc *sc;
	bus_space_handle_t bh;
	u_long offset = 0;

	sc = device_get_softc(dev);
	offset = PSYCHO_CONF_OFF(bus, slot, func, reg);
	bh = sc->sc_pci_bh[OFW_PCI_CS_CONFIG];
	switch (width) {
	case 1:
		bus_space_write_1(sc->sc_pci_cfgt, bh, offset, val);
		break;
	case 2:
		bus_space_write_2(sc->sc_pci_cfgt, bh, offset, val);
		break;
	case 4:
		bus_space_write_4(sc->sc_pci_cfgt, bh, offset, val);
		break;
	default:
		panic("%s: bad width", __func__);
	}
}

static int
psycho_route_interrupt(device_t bridge, device_t dev, int pin)
{
	struct psycho_softc *sc;
	struct ofw_pci_register reg;
	bus_addr_t intrmap;
	phandle_t node = ofw_bus_get_node(dev);
	ofw_pci_intr_t pintr, mintr;
	uint8_t maskbuf[sizeof(reg) + sizeof(pintr)];

	sc = device_get_softc(bridge);
	pintr = pin;
	if (ofw_bus_lookup_imap(node, &sc->sc_pci_iinfo, &reg, sizeof(reg),
	    &pintr, sizeof(pintr), &mintr, sizeof(mintr), maskbuf))
		return (mintr);
	/*
	 * If this is outside of the range for an intpin, it's likely a full
	 * INO, and no mapping is required at all; this happens on the U30,
 	 * where there's no interrupt map at the Psycho node. Fortunately,
	 * there seem to be no INOs in the intpin range on this boxen, so
	 * this easy heuristics will do.
	 */
	if (pin > 4)
		return (pin);
	/*
	 * Guess the INO; we always assume that this is a non-OBIO
	 * device, and that pin is a "real" intpin number. Determine
	 * the mapping register to be used by the slot number.
	 * We only need to do this on E450s, it seems; here, the slot numbers
	 * for bus A are one-based, while those for bus B seemingly have an
	 * offset of 2 (hence the factor of 3 below).
	 */
	intrmap = PSR_PCIA0_INT_MAP +
	    8 * (pci_get_slot(dev) - 1 + 3 * sc->sc_half);
	mintr = INTINO(PSYCHO_READ8(sc, intrmap)) + pin - 1;
	device_printf(bridge, "guessing interrupt %d for device %d/%d pin %d\n",
	    (int)mintr, pci_get_slot(dev), pci_get_function(dev), pin);
	return (mintr);
}

static int
psycho_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct psycho_softc *sc;

	sc = device_get_softc(dev);
	switch (which) {
	case PCIB_IVAR_BUS:
		*result = sc->sc_pci_secbus;
		return (0);
	}
	return (ENOENT);
}

/* Write to the correct clr register, and call the actual handler. */
static void
psycho_intr_stub(void *arg)
{
	struct psycho_clr *pc = arg;

	pc->pci_handler(pc->pci_arg);
	PSYCHO_WRITE8(pc->pci_sc, pc->pci_clr, 0);
}

static int
psycho_setup_intr(device_t dev, device_t child, struct resource *ires,
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct psycho_softc *sc;
	struct psycho_clr *pc;
	bus_addr_t intrmapptr, intrclrptr;
	long vec = rman_get_start(ires);
	uint64_t mr;
	int ino, error;

	sc = device_get_softc(dev);
	pc = (struct psycho_clr *)malloc(sizeof(*pc), M_DEVBUF, M_NOWAIT);
	if (pc == NULL)
		return (0);

	/*
	 * Hunt through all the interrupt mapping regs to look for our
	 * interrupt vector.
	 *
	 * XXX We only compare INOs rather than IGNs since the firmware may
	 * not provide the IGN and the IGN is constant for all devices on that
	 * PCI controller.  This could cause problems for the FFB/external
	 * interrupt which has a full vector that can be set arbitrarily.
	 */
	ino = INTINO(vec);

	if (!psycho_find_intrmap(sc, ino, &intrmapptr, &intrclrptr, NULL)) {
		device_printf(dev, "Cannot find interrupt vector %lx\n", vec);
		free(pc, M_DEVBUF);
		return (0);
	}

#ifdef PSYCHO_DEBUG
	device_printf(dev, "%s: INO %d, map %#lx, clr %#lx\n", __func__, ino,
	    (u_long)intrmapptr, (u_long)intrclrptr);
#endif
	pc->pci_sc = sc;
	pc->pci_arg = arg;
	pc->pci_handler = intr;
	pc->pci_clr = intrclrptr;
	/* Disable the interrupt while we fiddle with it */
	mr = PSYCHO_READ8(sc, intrmapptr);
	PSYCHO_WRITE8(sc, intrmapptr, mr & ~INTMAP_V);
	error = BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags,
	    psycho_intr_stub, pc, cookiep);
	if (error != 0) {
		free(pc, M_DEVBUF);
		return (error);
	}
	pc->pci_cookie = *cookiep;
	*cookiep = pc;

	/*
	 * Clear the interrupt, it might have been triggered before it was
	 * set up.
	 */
	PSYCHO_WRITE8(sc, intrclrptr, 0);
	/*
	 * Enable the interrupt and program the target module now we have the
	 * handler installed.
	 */
	PSYCHO_WRITE8(sc, intrmapptr, INTMAP_ENABLE(mr, PCPU_GET(mid)));
	return (error);
}

static int
psycho_teardown_intr(device_t dev, device_t child, struct resource *vec,
     void *cookie)
{
	struct psycho_clr *pc = cookie;
	int error;

	error = BUS_TEARDOWN_INTR(device_get_parent(dev), child, vec,
	    pc->pci_cookie);
	/*
	 * Don't disable the interrupt for now, so that stray interupts get
	 * detected...
	 */
	if (error != 0)
		free(pc, M_DEVBUF);
	return (error);
}

static struct resource *
psycho_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct psycho_softc *sc;
	struct resource *rv;
	struct rman *rm;
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int needactivate = flags & RF_ACTIVE;

	flags &= ~RF_ACTIVE;

	sc = device_get_softc(bus);
	if (type == SYS_RES_IRQ) {
		/*
		 * XXX: Don't accept blank ranges for now, only single
		 * interrupts. The other case should not happen with the
		 * MI PCI code...
		 * XXX: This may return a resource that is out of the
		 * range that was specified. Is this correct...?
		 */
		if (start != end)
			panic("%s: XXX: interrupt range", __func__);
		start = end |= sc->sc_ign;
		return (BUS_ALLOC_RESOURCE(device_get_parent(bus), child, type,
		    rid, start, end, count, flags));
	}
	switch (type) {
	case SYS_RES_MEMORY:
		rm = &sc->sc_pci_mem_rman;
		bt = sc->sc_pci_memt;
		bh = sc->sc_pci_bh[OFW_PCI_CS_MEM32];
		break;
	case SYS_RES_IOPORT:
		rm = &sc->sc_pci_io_rman;
		bt = sc->sc_pci_iot;
		bh = sc->sc_pci_bh[OFW_PCI_CS_IO];
		break;
	default:
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL)
		return (NULL);

	bh += rman_get_start(rv);
	rman_set_bustag(rv, bt);
	rman_set_bushandle(rv, bh);

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
psycho_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	void *p;
	int error;

	if (type == SYS_RES_IRQ)
		return (BUS_ACTIVATE_RESOURCE(device_get_parent(bus), child,
		    type, rid, r));
	if (type == SYS_RES_MEMORY) {
		/*
		 * Need to memory-map the device space, as some drivers depend
		 * on the virtual address being set and useable.
		 */
		error = sparc64_bus_mem_map(rman_get_bustag(r),
		    rman_get_bushandle(r), rman_get_size(r), 0, 0, &p);
		if (error != 0)
			return (error);
		rman_set_virtual(r, p);
	}
	return (rman_activate_resource(r));
}

static int
psycho_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	if (type == SYS_RES_IRQ)
		return (BUS_DEACTIVATE_RESOURCE(device_get_parent(bus), child,
		    type, rid, r));
	if (type == SYS_RES_MEMORY) {
		sparc64_bus_mem_unmap(rman_get_virtual(r), rman_get_size(r));
		rman_set_virtual(r, NULL);
	}
	return (rman_deactivate_resource(r));
}

static int
psycho_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	int error;

	if (type == SYS_RES_IRQ)
		return (BUS_RELEASE_RESOURCE(device_get_parent(bus), child,
		    type, rid, r));
	if (rman_get_flags(r) & RF_ACTIVE) {
		error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return error;
	}
	return (rman_release_resource(r));
}

static int
psycho_intr_pending(device_t dev, ofw_pci_intr_t intr)
{
	struct psycho_softc *sc;
	u_long diag;

	sc = device_get_softc(dev);
	if (!psycho_find_intrmap(sc, intr, NULL, NULL, &diag)) {
		device_printf(dev, "%s: mapping not found for %d\n", __func__,
		    intr);
		return (0);
	}
	return (diag != 0);
}

static bus_space_handle_t
psycho_get_bus_handle(device_t dev, int type, bus_space_handle_t childhdl,
    bus_space_tag_t *tag)
{
	struct psycho_softc *sc;

	sc = device_get_softc(dev);
	switch (type) {
	case SYS_RES_IOPORT:
		*tag = sc->sc_pci_iot;
		return (sc->sc_pci_bh[OFW_PCI_CS_IO] + childhdl);
	case SYS_RES_MEMORY:
		*tag = sc->sc_pci_memt;
		return (sc->sc_pci_bh[OFW_PCI_CS_MEM32] + childhdl);
	default:
		panic("%s: illegal space (%d)\n", __func__, type);
	}
}

static phandle_t
psycho_get_node(device_t bus, device_t dev)
{
	struct psycho_softc *sc;

	sc = device_get_softc(bus);
	/* We only have one child, the PCI bus, which needs our own node. */
	return (sc->sc_node);
}

static void
psycho_adjust_busrange(device_t dev, u_int subbus)
{
	struct psycho_softc *sc;

	sc = device_get_softc(dev);
	/* If necessary, adjust the subordinate bus number register. */
	if (subbus > sc->sc_pci_subbus) {
#ifdef PSYCHO_DEBUG
		device_printf(dev,
		    "adjusting secondary bus number from %d to %d\n",
		    sc->sc_pci_subbus, subbus);
#endif
		sc->sc_pci_subbus = subbus;
		PCIB_WRITE_CONFIG(dev, sc->sc_pci_secbus, PCS_DEVICE, PCS_FUNC,
		    PCSR_SUBBUS, subbus, 1);
	}
}

static bus_space_tag_t
psycho_alloc_bus_tag(struct psycho_softc *sc, int type)
{
	bus_space_tag_t bt;

	bt = (bus_space_tag_t)malloc(sizeof(struct bus_space_tag), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (bt == NULL)
		panic("%s: out of memory", __func__);

	bt->bst_cookie = sc;
	bt->bst_parent = sc->sc_bustag;
	bt->bst_type = type;
	return (bt);
}
