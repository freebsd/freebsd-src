/*
 * Copyright (c) 1999, 2000 Matthew R. Green
 * All rights reserved.
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.  All rights reserved.
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
 *
 * $FreeBSD$
 */

/*
 * Support for `psycho' and `psycho+' UPA to PCI bridge and
 * UltraSPARC IIi and IIe `sabre' PCI controllers.
 */

#include "opt_psycho.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>

#include <ofw/openfirm.h>
#include <ofw/ofw_pci.h>

#include <machine/bus.h>
#include <machine/iommureg.h>
#include <machine/bus_common.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/nexusvar.h>
#include <machine/ofw_upa.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <machine/iommuvar.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <sparc64/pci/ofw_pci.h>
#include <sparc64/pci/psychoreg.h>
#include <sparc64/pci/psychovar.h>

#include "pcib_if.h"
#include "sparcbus_if.h"

static void psycho_get_ranges(phandle_t, struct upa_ranges **, int *);
static void psycho_set_intr(struct psycho_softc *, int, device_t, bus_addr_t,
    int, driver_intr_t);
static int psycho_find_intrmap(struct psycho_softc *, int, bus_addr_t *,
    bus_addr_t *, u_long *);
static void psycho_intr_stub(void *);
#ifdef PSYCHO_STRAY
static void psycho_intr_stray(void *);
#endif
static bus_space_tag_t psycho_alloc_bus_tag(struct psycho_softc *, int);


/* Interrupt handlers */
static void psycho_ue(void *);
static void psycho_ce(void *);
static void psycho_bus_a(void *);
static void psycho_bus_b(void *);
static void psycho_powerfail(void *);
#ifdef PSYCHO_MAP_WAKEUP
static void psycho_wakeup(void *);
#endif

/* IOMMU support */
static void psycho_iommu_init(struct psycho_softc *, int);
static ofw_pci_binit_t psycho_binit;

/*
 * bus space and bus dma support for UltraSPARC `psycho'.  note that most
 * of the bus dma support is provided by the iommu dvma controller.
 */
static int psycho_dmamap_create(bus_dma_tag_t, bus_dma_tag_t, int,
    bus_dmamap_t *);
static int psycho_dmamap_destroy(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t);
static int psycho_dmamap_load(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
    void *, bus_size_t, bus_dmamap_callback_t *, void *, int);
static int psycho_dmamap_load_mbuf(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
    struct mbuf *, bus_dmamap_callback2_t *, void *, int);
static int psycho_dmamap_load_uio(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
    struct uio *, bus_dmamap_callback2_t *, void *, int);
static void psycho_dmamap_unload(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t);
static void psycho_dmamap_sync(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
    bus_dmasync_op_t);
static int psycho_dmamem_alloc(bus_dma_tag_t, bus_dma_tag_t, void **, int,
    bus_dmamap_t *);
static void psycho_dmamem_free(bus_dma_tag_t, bus_dma_tag_t, void *,
    bus_dmamap_t);

/*
 * autoconfiguration
 */
static int psycho_probe(device_t);
static int psycho_attach(device_t);
static int psycho_read_ivar(device_t, device_t, int, u_long *);
static int psycho_setup_intr(device_t, device_t, struct resource *, int,
    driver_intr_t *, void *, void **);
static int psycho_teardown_intr(device_t, device_t, struct resource *, void *);
static struct resource *psycho_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
static int psycho_activate_resource(device_t, device_t, int, int,
    struct resource *);
static int psycho_deactivate_resource(device_t, device_t, int, int,
    struct resource *);
static int psycho_release_resource(device_t, device_t, int, int,
    struct resource *);
static int psycho_maxslots(device_t);
static u_int32_t psycho_read_config(device_t, u_int, u_int, u_int, u_int, int);
static void psycho_write_config(device_t, u_int, u_int, u_int, u_int, u_int32_t,
    int);
static int psycho_route_interrupt(device_t, device_t, int);
static int psycho_intr_pending(device_t, int);
static u_int32_t psycho_guess_ino(device_t, phandle_t, u_int, u_int);
static bus_space_handle_t psycho_get_bus_handle(device_t dev, enum sbbt_id id,
    bus_space_handle_t childhdl, bus_space_tag_t *tag);

static device_method_t psycho_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		psycho_probe),
	DEVMETHOD(device_attach,	psycho_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	psycho_read_ivar),
	DEVMETHOD(bus_setup_intr, 	psycho_setup_intr),
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

	/* sparcbus interface */
	DEVMETHOD(sparcbus_intr_pending,	psycho_intr_pending),
	DEVMETHOD(sparcbus_guess_ino,	psycho_guess_ino),
	DEVMETHOD(sparcbus_get_bus_handle,	psycho_get_bus_handle),

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
	bus_addr_t	pci_clr;	/* clear register */
	driver_intr_t	*pci_handler;	/* handler to call */
	void		*pci_arg;	/* argument for the handler */
	void		*pci_cookie;	/* interrupt cookie of parent bus */
};

struct psycho_strayclr {
	struct psycho_softc	*psc_sc;
	bus_addr_t	psc_clr;	/* clear register */
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
 * "sabre" is the UltraSPARC IIi onboard UPA to PCI bridge.  It manages a
 * single PCI bus and does not have a streaming buffer.  It often has an APB
 * (advanced PCI bridge) connected to it, which was designed specifically for
 * the IIi.  The APB let's the IIi handle two independednt PCI buses, and
 * appears as two "simba"'s underneath the sabre.
 *
 * "psycho" and "psycho+" is a dual UPA to PCI bridge.  It sits on the UPA bus
 * and manages two PCI buses.  "psycho" has two 64-bit 33MHz buses, while
 * "psycho+" controls both a 64-bit 33Mhz and a 64-bit 66Mhz PCI bus.  You
 * will usually find a "psycho+" since I don't think the original "psycho"
 * ever shipped, and if it did it would be in the U30.
 *
 * Each "psycho" PCI bus appears as a separate OFW node, but since they are
 * both part of the same IC, they only have a single register space.  As such,
 * they need to be configured together, even though the autoconfiguration will
 * attach them separately.
 *
 * On UltraIIi machines, "sabre" itself usually takes pci0, with "simba" often
 * as pci1 and pci2, although they have been implemented with other PCI bus
 * numbers on some machines.
 *
 * On UltraII machines, there can be any number of "psycho+" ICs, each
 * providing two PCI buses.
 *
 *
 * XXXX The psycho/sabre node has an `interrupts' attribute.  They contain
 * the values of the following interrupts in this order:
 *
 * PCI Bus Error	(30)
 * DMA UE		(2e)
 * DMA CE		(2f)
 * Power Fail		(25)
 *
 * We really should attach handlers for each.
 */
#ifdef DEBUGGER_ON_POWERFAIL
#define	PSYCHO_PWRFAIL_INT_FLAGS	INTR_FAST
#else
#define	PSYCHO_PWRFAIL_INT_FLAGS	0
#endif

#define	OFW_PCI_TYPE		"pci"

struct psycho_desc {
	char	*pd_string;
	int	pd_mode;
	char	*pd_name;
};

static struct psycho_desc psycho_compats[] = {
	{ "pci108e,8000", PSYCHO_MODE_PSYCHO,	"Psycho compatible" },
	{ "pci108e,a000", PSYCHO_MODE_SABRE,	"Sabre (US-IIi) compatible" },
	{ "pci108e,a001", PSYCHO_MODE_SABRE,	"Sabre (US-IIe) compatible" },
	{ NULL,		  0,			NULL }
};

static struct psycho_desc psycho_models[] = {
	{ "SUNW,psycho",  PSYCHO_MODE_PSYCHO,	"Psycho" },
	{ "SUNW,sabre",   PSYCHO_MODE_SABRE,	"Sabre" },
	{ NULL,		  0,			NULL }
};

static struct psycho_desc *
psycho_find_desc(struct psycho_desc *table, char *string)
{
	struct psycho_desc *desc;

	for (desc = table; desc->pd_string != NULL; desc++) {
		if (strcmp(desc->pd_string, string) == 0)
			return (desc);
	}
	return (NULL);
}

static struct psycho_desc *
psycho_get_desc(phandle_t node, char *model)
{
	struct psycho_desc *rv;
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
	phandle_t node;
	char *dtype;

	node = nexus_get_node(dev);
	dtype = nexus_get_device_type(dev);
	if (nexus_get_reg(dev) != NULL && dtype != NULL &&
	    strcmp(dtype, OFW_PCI_TYPE) == 0 &&
	    psycho_get_desc(node, nexus_get_model(dev)) != NULL) {
		device_set_desc(dev, "U2P UPA-PCI bridge");
		return (0);
	}

	return (ENXIO);
}

/*
 * SUNW,psycho initialisation ..
 *	- find the per-psycho registers
 *	- figure out the IGN.
 *	- find our partner psycho
 *	- configure ourselves
 *	- bus range, bus,
 *	- interrupt map,
 *	- setup the chipsets.
 *	- if we're the first of the pair, initialise the IOMMU, otherwise
 *	  just copy it's tags and addresses.
 */
static int
psycho_attach(device_t dev)
{
	struct psycho_softc *sc;
	struct psycho_softc *osc = NULL;
	struct psycho_softc *asc;
	struct upa_regs *reg;
	struct ofw_pci_bdesc obd;
	struct psycho_desc *desc;
	phandle_t node;
	u_int64_t csr;
	u_long mlen;
	int psycho_br[2];
	int n, i, nreg, rid;
#if defined(PSYCHO_DEBUG) || defined(PSYCHO_STRAY)
	bus_addr_t map, clr;
	u_int64_t mr;
#endif
#ifdef PSYCHO_STRAY
	struct psycho_strayclr *sclr;
#endif

	node = nexus_get_node(dev);
	sc = device_get_softc(dev);
	desc = psycho_get_desc(node, nexus_get_model(dev));

	sc->sc_node = node;
	sc->sc_dev = dev;
	sc->sc_dmatag = nexus_get_dmatag(dev);
	sc->sc_mode = desc->pd_mode;

	/*
	 * The psycho gets three register banks:
	 * (0) per-PBM configuration and status registers
	 * (1) per-PBM PCI configuration space, containing only the
	 *     PBM 256-byte PCI header
	 * (2) the shared psycho configuration registers (struct psychoreg)
	 */
	reg = nexus_get_reg(dev);
	nreg = nexus_get_nreg(dev);
	/* Register layouts are different.  stuupid. */
	if (sc->sc_mode == PSYCHO_MODE_PSYCHO) {
		if (nreg <= 2)
			panic("psycho_attach: %d not enough registers", nreg);
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
			panic("psycho_attach: bogus pci control register "
			    "location");
		}
	} else {
		if (nreg <= 0)
			panic("psycho_attach: %d not enough registers", nreg);
		sc->sc_basepaddr = (vm_paddr_t)UPA_REG_PHYS(&reg[0]);
		mlen = UPA_REG_SIZE(reg);
		sc->sc_pcictl = PSR_PCICTL0;
		sc->sc_half = 0;
	}

	/*
	 * Match other psycho's that are already configured against
	 * the base physical address. This will be the same for a
	 * pair of devices that share register space.
	 */
	SLIST_FOREACH(asc, &psycho_softcs, sc_link) {
		if (asc->sc_basepaddr == sc->sc_basepaddr) {
			/* Found partner */
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
			panic("psycho_attach: can't allocate device memory");
		sc->sc_bustag = rman_get_bustag(sc->sc_mem_res);
		sc->sc_bushandle = rman_get_bushandle(sc->sc_mem_res);
	} else {
		/*
		 * There's another psycho using the same register space. Copy the
		 * relevant stuff.
		 */
		sc->sc_mem_res = NULL;
		sc->sc_bustag = osc->sc_bustag;
		sc->sc_bushandle = osc->sc_bushandle;
	}
	csr = PSYCHO_READ8(sc, PSR_CS);
	sc->sc_ign = 0x7c0; /* APB IGN is always 0x7c */
	if (sc->sc_mode == PSYCHO_MODE_PSYCHO)
		sc->sc_ign = PSYCHO_GCSR_IGN(csr) << 6;

	device_printf(dev, "%s, impl %d, version %d, ign %#x, bus %c\n",
	    desc->pd_name, (int)PSYCHO_GCSR_IMPL(csr),
	    (int)PSYCHO_GCSR_VERS(csr), sc->sc_ign, 'A' + sc->sc_half);

	/*
	 * Setup the PCI control register
	 */
	csr = PCICTL_READ8(sc, PCR_CS);
	csr |= PCICTL_MRLM | PCICTL_ARB_PARK | PCICTL_ERRINTEN | PCICTL_4ENABLE;
	csr &= ~(PCICTL_SERR | PCICTL_CPU_PRIO | PCICTL_ARB_PRIO |
	    PCICTL_RTRYWAIT);
	PCICTL_WRITE8(sc, PCR_CS, csr);

	if (sc->sc_mode == PSYCHO_MODE_SABRE) {
		/*
		 * Use the PROM preset for now.
		 */
		csr = PCICTL_READ8(sc, PCR_TAS);
		if (csr == 0)
			panic("psycho_attach: sabre TAS not initialized.");
		sc->sc_dvmabase = (ffs(csr) - 1) << PCITAS_ADDR_SHIFT;
	} else
		sc->sc_dvmabase = -1;

	/* Grab the psycho ranges */
	psycho_get_ranges(sc->sc_node, &sc->sc_range, &sc->sc_nrange);

	/* Initialize memory and i/o rmans */
	sc->sc_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_io_rman.rm_descr = "Psycho PCI I/O Ports";
	if (rman_init(&sc->sc_io_rman) != 0 ||
	    rman_manage_region(&sc->sc_io_rman, 0, PSYCHO_IO_SIZE) != 0)
		panic("psycho_probe: failed to set up i/o rman");
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "Psycho PCI Memory";
	if (rman_init(&sc->sc_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_mem_rman, 0, PSYCHO_MEM_SIZE) != 0)
		panic("psycho_probe: failed to set up memory rman");
	/*
	 * Find the addresses of the various bus spaces.
	 * There should not be multiple ones of one kind.
	 * The physical start addresses of the ranges are the configuration,
	 * memory and IO handles.
	 */
	for (n = 0; n < sc->sc_nrange; n++) {
		i = UPA_RANGE_CS(&sc->sc_range[n]);
		if (sc->sc_bh[i] != 0)
			panic("psycho_attach: duplicate range for space %d", i);
		sc->sc_bh[i] = UPA_RANGE_PHYS(&sc->sc_range[n]);
	}
	/*
	 * Check that all needed handles are present. The PCI_CS_MEM64 one is
	 * not currently used.
	 */
	for (n = 0; n < 3; n++) {
		if (sc->sc_bh[n] == 0)
			panic("psycho_attach: range %d missing", n);
	}

	/* allocate our tags */
	sc->sc_memt = psycho_alloc_bus_tag(sc, PCI_MEMORY_BUS_SPACE);
	sc->sc_iot = psycho_alloc_bus_tag(sc, PCI_IO_BUS_SPACE);
	sc->sc_cfgt = psycho_alloc_bus_tag(sc, PCI_CONFIG_BUS_SPACE);
	if (bus_dma_tag_create(sc->sc_dmatag, 8, 1, 0, 0x3ffffffff, NULL, NULL,
	    0x3ffffffff, 0xff, 0xffffffff, 0, &sc->sc_dmat) != 0)
		panic("psycho_attach: bus_dma_tag_create failed");
	/* Customize the tag */
	sc->sc_dmat->dt_cookie = sc;
	sc->sc_dmat->dt_dmamap_create = psycho_dmamap_create;
	sc->sc_dmat->dt_dmamap_destroy = psycho_dmamap_destroy;
	sc->sc_dmat->dt_dmamap_load = psycho_dmamap_load;
	sc->sc_dmat->dt_dmamap_load_mbuf = psycho_dmamap_load_mbuf;
	sc->sc_dmat->dt_dmamap_load_uio = psycho_dmamap_load_uio;
	sc->sc_dmat->dt_dmamap_unload = psycho_dmamap_unload;
	sc->sc_dmat->dt_dmamap_sync = psycho_dmamap_sync;
	sc->sc_dmat->dt_dmamem_alloc = psycho_dmamem_alloc;
	sc->sc_dmat->dt_dmamem_free = psycho_dmamem_free;
	/* XXX: register as root dma tag (kluge). */
	sparc64_root_dma_tag = sc->sc_dmat;

	/* Register the softc, this is needed for paired psychos. */
	SLIST_INSERT_HEAD(&psycho_softcs, sc, sc_link);

	/*
	 * And finally, if we're a sabre or the first of a pair of psycho's to
	 * arrive here, start up the IOMMU and get a config space tag.
	 */
	if (osc == NULL) {
		/*
		 * Establish handlers for interesting interrupts....
		 *
		 * XXX We need to remember these and remove this to support
		 * hotplug on the UPA/FHC bus.
		 *
		 * XXX Not all controllers have these, but installing them
		 * is better than trying to sort through this mess.
		 */
		psycho_set_intr(sc, 0, dev, PSR_UE_INT_MAP, INTR_FAST,
		    psycho_ue);
		psycho_set_intr(sc, 1, dev, PSR_CE_INT_MAP, 0, psycho_ce);
		psycho_set_intr(sc, 2, dev, PSR_PCIAERR_INT_MAP, INTR_FAST,
		    psycho_bus_a);
		psycho_set_intr(sc, 4, dev, PSR_POWER_INT_MAP,
		    PSYCHO_PWRFAIL_INT_FLAGS, psycho_powerfail);
		/* Psycho-specific initialization. */
		if (sc->sc_mode == PSYCHO_MODE_PSYCHO) {
			/*
			 * Sabres do not have the following two interrupts.
			 */
			psycho_set_intr(sc, 3, dev, PSR_PCIBERR_INT_MAP,
			    INTR_FAST, psycho_bus_b);
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
		 * of a pair of psycho's to arrive here.
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
			panic("psycho_attach: malloc iommu_state failed");
		sc->sc_is->is_sb[0] = 0;
		sc->sc_is->is_sb[1] = 0;
		if (OF_getproplen(sc->sc_node, "no-streaming-cache") < 0)
			sc->sc_is->is_sb[0] = sc->sc_pcictl + PCR_STRBUF;
		psycho_iommu_init(sc, 3);
	} else {
		/* Just copy IOMMU state, config tag and address */
		sc->sc_is = osc->sc_is;
		if (OF_getproplen(sc->sc_node, "no-streaming-cache") < 0)
			sc->sc_is->is_sb[1] = sc->sc_pcictl + PCR_STRBUF;
		iommu_reset(sc->sc_is);
	}

	/*
	 * Enable all interrupts, clear all interrupt states, and install an
	 * interrupt handler for OBIO interrupts, which can be ISA ones
	 * (to frob the interrupt clear registers).
	 * This aids the debugging of interrupt routing problems, and is needed
	 * for isa drivers that use isa_irq_pending (otherwise the registers
	 * will never be cleared).
	 */
#if defined(PSYCHO_DEBUG) || defined(PSYCHO_STRAY)
	for (map = PSR_PCIA0_INT_MAP, clr = PSR_PCIA0_INT_CLR, n = 0;
	     map <= PSR_PCIB3_INT_MAP; map += 8, clr += 32, n++) {
		mr = PSYCHO_READ8(sc, map);
#ifdef PSYCHO_DEBUG
		device_printf(dev, "intr map (pci) %d: %#lx\n", n, (u_long)mr);
#endif
		PSYCHO_WRITE8(sc, map, mr & ~INTMAP_V);
		for (i = 0; i < 4; i++)
			PCICTL_WRITE8(sc, clr + i * 8, 0);
		PSYCHO_WRITE8(sc, map, INTMAP_ENABLE(mr, PCPU_GET(mid)));
	}
	for (map = PSR_SCSI_INT_MAP, clr = PSR_SCSI_INT_CLR, n = 0;
	     map <= PSR_SERIAL_INT_MAP; map += 8, clr += 8, n++) {
		mr = PSYCHO_READ8(sc, map);
#ifdef PSYCHO_DEBUG
		device_printf(dev, "intr map (obio) %d: %#lx, clr: %#lx\n", n,
		    (u_long)mr, (u_long)clr);
#endif
		PSYCHO_WRITE8(sc, map, mr & ~INTMAP_V);
		PSYCHO_WRITE8(sc, clr, 0);
#ifdef PSYCHO_STRAY
		/*
		 * This can cause interrupt storms, and is therefore disabled
		 * by default.
		 * XXX: use intr_setup() to not confuse higher level code
		 */
		if (INTVEC(mr) != 0x7e6 && INTVEC(mr) != 0x7e7 &&
		    INTVEC(mr) != 0) {
			sclr = malloc(sizeof(*sclr), M_DEVBUF, M_WAITOK);
			sclr->psc_sc = sc;
			sclr->psc_clr = clr;
			intr_setup(PIL_LOW, intr_fast, INTVEC(mr),
			    psycho_intr_stray, sclr);
		}
#endif
		PSYCHO_WRITE8(sc, map, INTMAP_ENABLE(mr, PCPU_GET(mid)));
	}
#endif

	/*
	 * Get the bus range from the firmware; it is used solely for obtaining
	 * the inital bus number, and cannot be trusted on all machines.
	 */
	n = OF_getprop(node, "bus-range", (void *)psycho_br, sizeof(psycho_br));
	if (n == -1)
		panic("could not get psycho bus-range");
	if (n != sizeof(psycho_br))
		panic("broken psycho bus-range (%d)", n);

	sc->sc_busno = ofw_pci_alloc_busno(sc->sc_node);
	obd.obd_bus = psycho_br[0];
	obd.obd_secbus = obd.obd_subbus = sc->sc_busno;
	obd.obd_slot = PCS_DEVICE;
	obd.obd_func = PCS_FUNC;
	obd.obd_init = psycho_binit;
	obd.obd_super = NULL;
	/* Initial setup. */
	psycho_binit(dev, &obd);
	/* Update the bus number to what was just programmed. */
	obd.obd_bus = obd.obd_secbus;
	/*
	 * Initialize the interrupt registers of all devices hanging from
	 * the host bridge directly or indirectly via PCI-PCI bridges.
	 * The MI code (and the PCI spec) assume that this is done during
	 * system initialization, however the firmware does not do this
	 * at least on some models, and we probably shouldn't trust that
	 * the firmware uses the same model as this driver if it does.
	 * Additionally, set up the bus numbers and ranges.
	 */
	ofw_pci_init(dev, sc->sc_node, sc->sc_ign, &obd);

	device_add_child(dev, "pci", device_get_unit(dev));
	return (bus_generic_attach(dev));
}

static void
psycho_set_intr(struct psycho_softc *sc, int index,
    device_t dev, bus_addr_t map, int iflags, driver_intr_t handler)
{
	int rid, vec;
	u_int64_t mr;

	mr = PSYCHO_READ8(sc, map);
	vec = INTVEC(mr);
	sc->sc_irq_res[index] = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
	    vec, vec, 1, RF_ACTIVE);
	if (sc->sc_irq_res[index] == NULL)
		panic("psycho_set_intr: failed to get interrupt");
	bus_setup_intr(dev, sc->sc_irq_res[index], INTR_TYPE_MISC | iflags,
	    handler, sc, &sc->sc_ihand[index]);
	PSYCHO_WRITE8(sc, map, INTMAP_ENABLE(mr, PCPU_GET(mid)));
}

static int
psycho_find_intrmap(struct psycho_softc *sc, int ino, bus_addr_t *intrmapptr,
    bus_addr_t *intrclrptr, bus_addr_t *intrdiagptr)
{
	bus_addr_t intrmap, intrclr;
	u_int64_t im;
	u_long diag;
	int found;

	found = 0;
	/* Hunt thru obio first */
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
		/* Now do PCI interrupts */
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

/* grovel the OBP for various psycho properties */
static void
psycho_get_ranges(phandle_t node, struct upa_ranges **rp, int *np)
{

	*np = OF_getprop_alloc(node, "ranges", sizeof(**rp), (void **)rp);
	if (*np == -1)
		panic("could not get psycho ranges");
}

/*
 * Interrupt handlers.
 */
static void
psycho_ue(void *arg)
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;
	u_int64_t afar, afsr;

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
	/* It's uncorrectable.  Dump the regs and panic. */
	panic("%s: uncorrectable DMA error AFAR %#lx AFSR %#lx",
	    device_get_name(sc->sc_dev), (u_long)afar, (u_long)afsr);
}

static void
psycho_ce(void *arg)
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;
	u_int64_t afar, afsr;

	PSYCHO_WRITE8(sc, PSR_CE_INT_CLR, 0);
	afar = PSYCHO_READ8(sc, PSR_CE_AFA);
	afsr = PSYCHO_READ8(sc, PSR_CE_AFS);
	/* It's correctable.  Dump the regs and continue. */
	printf("%s: correctable DMA error AFAR %#lx AFSR %#lx\n",
	    device_get_name(sc->sc_dev), (u_long)afar, (u_long)afsr);
}

static void
psycho_bus_a(void *arg)
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;
	u_int64_t afar, afsr;

	afar = PSYCHO_READ8(sc, PSR_PCICTL0 + PCR_AFA);
	afsr = PSYCHO_READ8(sc, PSR_PCICTL0 + PCR_AFS);
	/* It's uncorrectable.  Dump the regs and panic. */
	panic("%s: PCI bus A error AFAR %#lx AFSR %#lx",
	    device_get_name(sc->sc_dev), (u_long)afar, (u_long)afsr);
}

static void
psycho_bus_b(void *arg)
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;
	u_int64_t afar, afsr;

	afar = PSYCHO_READ8(sc, PSR_PCICTL1 + PCR_AFA);
	afsr = PSYCHO_READ8(sc, PSR_PCICTL1 + PCR_AFS);
	/* It's uncorrectable.  Dump the regs and panic. */
	panic("%s: PCI bus B error AFAR %#lx AFSR %#lx",
	    device_get_name(sc->sc_dev), (u_long)afar, (u_long)afsr);
}

static void
psycho_powerfail(void *arg)
{

	/* We lost power.  Try to shut down NOW. */
#ifdef DEBUGGER_ON_POWERFAIL
	struct psycho_softc *sc = (struct psycho_softc *)arg;

	Debugger("powerfail");
	PSYCHO_WRITE8(sc, PSR_POWER_INT_CLR, 0);
#else
	printf("Power Failure Detected: Shutting down NOW.\n");
	shutdown_nice(0);
#endif
}

#ifdef PSYCHO_MAP_WAKEUP
static void
psycho_wakeup(void *arg)
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;

	PSYCHO_WRITE8(sc, PSR_PWRMGT_INT_CLR, 0);
	/* Gee, we don't really have a framework to deal with this properly. */
	printf("%s: power management wakeup\n",	device_get_name(sc->sc_dev));
}
#endif /* PSYCHO_MAP_WAKEUP */

/* initialise the IOMMU... */
void
psycho_iommu_init(struct psycho_softc *sc, int tsbsize)
{
	char *name;
	struct iommu_state *is = sc->sc_is;

	/* punch in our copies */
	is->is_bustag = sc->sc_bustag;
	is->is_bushandle = sc->sc_bushandle;
	is->is_iommu = PSR_IOMMU;
	is->is_dtag = PSR_IOMMU_TLB_TAG_DIAG;
	is->is_ddram = PSR_IOMMU_TLB_DATA_DIAG;
	is->is_dqueue = PSR_IOMMU_QUEUE_DIAG;
	is->is_dva = PSR_IOMMU_SVADIAG;
	is->is_dtcmp = PSR_IOMMU_TLB_CMP_DIAG;

	/* give us a nice name.. */
	name = (char *)malloc(32, M_DEVBUF, M_NOWAIT);
	if (name == 0)
		panic("couldn't malloc iommu name");
	snprintf(name, 32, "%s dvma", device_get_name(sc->sc_dev));

	iommu_init(name, is, tsbsize, sc->sc_dvmabase, 0);
}

static void
psycho_binit(device_t busdev, struct ofw_pci_bdesc *obd)
{

#ifdef PSYCHO_DEBUG
	printf("psycho at %u/%u/%u: setting bus #s to %u/%u/%u\n",
	    obd->obd_bus, obd->obd_slot, obd->obd_func, obd->obd_bus,
	    obd->obd_secbus, obd->obd_subbus);
#endif /* PSYCHO_DEBUG */
	/*
	 * NOTE: this must be kept in this order, since the last write will
	 * change the config space address of the psycho.
	 */
	PCIB_WRITE_CONFIG(busdev, obd->obd_bus, obd->obd_slot, obd->obd_func,
	    PCSR_SUBBUS, obd->obd_subbus, 1);
	PCIB_WRITE_CONFIG(busdev, obd->obd_bus, obd->obd_slot, obd->obd_func,
	    PCSR_SECBUS, obd->obd_secbus, 1);
}

static int
psycho_maxslots(device_t dev)
{

	/*
	 * XXX: is this correct? At any rate, a number that is too high
	 * shouldn't do any harm, if only because of the way things are
	 * handled in psycho_read_config.
	 */
	return (31);
}

/*
 * Keep a table of quirky PCI devices that need fixups before the MI PCI code
 * creates the resource lists. This needs to be moved around once other bus
 * drivers are added. Moving it to the MI code should maybe be reconsidered
 * if one of these devices appear in non-sparc64 boxen. It's likely that not
 * all BIOSes/firmwares can deal with them.
 */
struct psycho_dquirk {
	u_int32_t	dq_devid;
	int		dq_quirk;
};

/* Quirk types. May be or'ed together. */
#define	DQT_BAD_INTPIN	1	/* Intpin reg 0, but intpin used */

static struct psycho_dquirk dquirks[] = {
	{ 0x1001108e, DQT_BAD_INTPIN },	/* Sun HME (PCIO func. 1) */
	{ 0x1101108e, DQT_BAD_INTPIN },	/* Sun GEM (PCIO2 func. 1) */
	{ 0x1102108e, DQT_BAD_INTPIN },	/* Sun FireWire ctl. (PCIO2 func. 2) */
	{ 0x1103108e, DQT_BAD_INTPIN },	/* Sun USB ctl. (PCIO2 func. 3) */
};

#define	NDQUIRKS	(sizeof(dquirks) / sizeof(dquirks[0]))

static u_int32_t
psycho_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
	int width)
{
	struct psycho_softc *sc;
	bus_space_handle_t bh;
	u_long offset = 0;
	u_int32_t r, devid;
	int i;

	/*
	 * The psycho bridge does not tolerate accesses to unconfigured PCI
	 * devices' or function's config space, so look up the device in the
	 * firmware device tree first, and if it is not present, return a value
	 * that will make the detection code think that there is no device here.
	 * This is ugly...
	 */
	if (reg == 0 && ofw_pci_find_node(bus, slot, func) == 0)
		return (0xffffffff);
	sc = (struct psycho_softc *)device_get_softc(dev);
	offset = PSYCHO_CONF_OFF(bus, slot, func, reg);
	bh = sc->sc_bh[PCI_CS_CONFIG];
	switch (width) {
	case 1:
		r = bus_space_read_1(sc->sc_cfgt, bh, offset);
		break;
	case 2:
		r = bus_space_read_2(sc->sc_cfgt, bh, offset);
		break;
	case 4:
		r = bus_space_read_4(sc->sc_cfgt, bh, offset);
		break;
	default:
		panic("psycho_read_config: bad width");
	}
	if (reg == PCIR_INTPIN && r == 0) {
		/* Check for DQT_BAD_INTPIN quirk. */
		devid = psycho_read_config(dev, bus, slot, func,
		    PCIR_DEVVENDOR, 4);
		for (i = 0; i < NDQUIRKS; i++) {
			if (dquirks[i].dq_devid == devid) {
				/*
				 * Need to set the intpin to a value != 0 so
				 * that the MI code will think that this device
				 * has an interrupt.
				 * Just use 1 (intpin a) for now. This is, of
				 * course, bogus, but since interrupts are
				 * routed in advance, this does not really
				 * matter.
				 */
				if ((dquirks[i].dq_quirk & DQT_BAD_INTPIN) != 0)
					r = 1;
				break;
			}
		}
	}
	return (r);
}

static void
psycho_write_config(device_t dev, u_int bus, u_int slot, u_int func,
	u_int reg, u_int32_t val, int width)
{
	struct psycho_softc *sc;
	bus_space_handle_t bh;
	u_long offset = 0;

	sc = (struct psycho_softc *)device_get_softc(dev);
	offset = PSYCHO_CONF_OFF(bus, slot, func, reg);
	bh = sc->sc_bh[PCI_CS_CONFIG];
	switch (width) {
	case 1:
		bus_space_write_1(sc->sc_cfgt, bh, offset, val);
		break;
	case 2:
		bus_space_write_2(sc->sc_cfgt, bh, offset, val);
		break;
	case 4:
		bus_space_write_4(sc->sc_cfgt, bh, offset, val);
		break;
	default:
		panic("psycho_write_config: bad width");
	}
}

static int
psycho_route_interrupt(device_t bus, device_t dev, int pin)
{

	/*
	 * XXX: ugly loathsome hack:
	 * We can't use ofw_pci_route_intr() here; the device passed may be
	 * the one of a bridge, so the original device can't be recovered.
	 *
	 * We need to use the firmware to route interrupts, however it has
	 * no interface which could be used to interpret intpins; instead,
	 * all assignments are done by device.
	 *
	 * The MI pci code will try to reroute interrupts of 0, although they
	 * are correct; all other interrupts are preinitialized, so if we
	 * get here, the intline is either 0 (so return 0), or we hit a
	 * device which was not preinitialized (e.g. hotplugged stuff), in
	 * which case we are lost.
	 */
	return (0);
}

static int
psycho_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)device_get_softc(dev);
	switch (which) {
	case PCIB_IVAR_BUS:
		*result = sc->sc_busno;
		return (0);
	}
	return (ENOENT);
}

/* Write to the correct clr register, and call the actual handler. */
static void
psycho_intr_stub(void *arg)
{
	struct psycho_clr *pc;

	pc = (struct psycho_clr *)arg;
	pc->pci_handler(pc->pci_arg);
	PSYCHO_WRITE8(pc->pci_sc, pc->pci_clr, 0);
}

#ifdef PSYCHO_STRAY
/*
 * Write to the correct clr register and return. arg is the address of the clear
 * register to be used.
 * XXX: print a message?
 */
static void
psycho_intr_stray(void *arg)
{
	struct psycho_strayclr *sclr = arg;

	PSYCHO_WRITE8(sclr->psc_sc, sclr->psc_clr, 0);
}
#endif

static int
psycho_setup_intr(device_t dev, device_t child,
    struct resource *ires,  int flags, driver_intr_t *intr, void *arg,
    void **cookiep)
{
	struct psycho_softc *sc;
	struct psycho_clr *pc;
	bus_addr_t intrmapptr, intrclrptr;
	long vec = rman_get_start(ires);
	u_int64_t mr;
	int ino, error;

	sc = (struct psycho_softc *)device_get_softc(dev);
	pc = (struct psycho_clr *)malloc(sizeof(*pc), M_DEVBUF, M_NOWAIT);
	if (pc == NULL)
		return (NULL);

	/*
	 * Hunt through all the interrupt mapping regs to look for our
	 * interrupt vector.
	 *
	 * XXX We only compare INOs rather than IGNs since the firmware may
	 * not provide the IGN and the IGN is constant for all device on that
	 * PCI controller.  This could cause problems for the FFB/external
	 * interrupt which has a full vector that can be set arbitrarily.
	 */
	ino = INTINO(vec);

	if (!psycho_find_intrmap(sc, ino, &intrmapptr, &intrclrptr, NULL)) {
		printf("Cannot find interrupt vector %lx\n", vec);
		free(pc, M_DEVBUF);
		return (NULL);
	}

#ifdef PSYCHO_DEBUG
	device_printf(dev, "psycho_setup_intr: INO %d, map %#lx, clr %#lx\n",
	    ino, (u_long)intrmapptr, (u_long)intrclrptr);
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
psycho_teardown_intr(device_t dev, device_t child,
    struct resource *vec, void *cookie)
{
	struct psycho_clr *pc;
	int error;

	pc = (struct psycho_clr *)cookie;
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

	sc = (struct psycho_softc *)device_get_softc(bus);
	if (type == SYS_RES_IRQ) {
		/*
		 * XXX: Don't accept blank ranges for now, only single
		 * interrupts. The other case should not happen with the MI pci
		 * code...
		 * XXX: This may return a resource that is out of the range
		 * that was specified. Is this correct...?
		 */
		if (start != end)
			panic("psycho_alloc_resource: XXX: interrupt range");
		start = end |= sc->sc_ign;
		return (bus_alloc_resource(bus, type, rid, start, end,
		    count, flags));
	}
	switch (type) {
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		bt = sc->sc_memt;
		bh = sc->sc_bh[PCI_CS_MEM32];
		break;
	case SYS_RES_IOPORT:
		rm = &sc->sc_io_rman;
		bt = sc->sc_iot;
		bh = sc->sc_bh[PCI_CS_IO];
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
		return (bus_activate_resource(bus, type, rid, r));
	if (type == SYS_RES_MEMORY) {
		/*
		 * Need to memory-map the device space, as some drivers depend
		 * on the virtual address being set and useable.
		 */
		error = sparc64_bus_mem_map(rman_get_bustag(r),
		    rman_get_bushandle(r), rman_get_size(r), 0, NULL, &p);
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
		return (bus_deactivate_resource(bus, type, rid, r));
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
		return (bus_release_resource(bus, type, rid, r));
	if (rman_get_flags(r) & RF_ACTIVE) {
		error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return error;
	}
	return (rman_release_resource(r));
}

static int
psycho_intr_pending(device_t dev, int intr)
{
	struct psycho_softc *sc;
	u_long diag;

	sc = (struct psycho_softc *)device_get_softc(dev);
	if (!psycho_find_intrmap(sc, intr, NULL, NULL, &diag)) {
		printf("psycho_intr_pending: mapping not found for %d\n", intr);
		return (0);
	}
	return (diag != 0);
}

static u_int32_t
psycho_guess_ino(device_t dev, phandle_t node, u_int slot, u_int pin)
{
	struct psycho_softc *sc = (struct psycho_softc *)device_get_softc(dev);
	bus_addr_t intrmap;

	/*
	 * If this is not for one of our direct children (i.e. we are mapping
	 * at our node), tell the interrupt mapper to go on - we need the
	 * slot number of the device or it's topmost parent bridge to guess
	 * the INO.
	 */
	if (node != sc->sc_node)
		return (255);
	/*
	 * Actually guess the INO. We always assume that this is a non-OBIO
	 * device, and use from the slot number to determine it.
	 * We only need to do this on e450s, it seems; here, the slot numbers
	 * for bus A are one-based, while those for bus B seemingly have an
	 * offset of 2 (hence the factor of 3 below).
	 */
	intrmap = PSR_PCIA0_INT_MAP + 8 * (slot - 1 + 3 * sc->sc_half);
	return (INTINO(PSYCHO_READ8(sc, intrmap)) + pin - 1);
}

static bus_space_handle_t
psycho_get_bus_handle(device_t dev, enum sbbt_id id,
    bus_space_handle_t childhdl, bus_space_tag_t *tag)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)device_get_softc(dev);
	switch(id) {
	case SBBT_IO:
		*tag = sc->sc_iot;
		return (sc->sc_bh[PCI_CS_IO] + childhdl);
	case SBBT_MEM:
		*tag = sc->sc_memt;
		return (sc->sc_bh[PCI_CS_MEM32] + childhdl);
	default:
		panic("psycho_get_bus_handle: illegal space\n");
	}
}

/*
 * below here is bus space and bus dma support
 */
static bus_space_tag_t
psycho_alloc_bus_tag(struct psycho_softc *sc, int type)
{
	bus_space_tag_t bt;

	bt = (bus_space_tag_t)malloc(sizeof(struct bus_space_tag), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (bt == NULL)
		panic("psycho_alloc_bus_tag: out of memory");

	bzero(bt, sizeof *bt);
	bt->bst_cookie = sc;
	bt->bst_parent = sc->sc_bustag;
	bt->bst_type = type;
	return (bt);
}

/*
 * hooks into the iommu dvma calls.
 */
static int
psycho_dmamem_alloc(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, void **vaddr,
    int flags, bus_dmamap_t *mapp)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)pdmat->dt_cookie;
	return (iommu_dvmamem_alloc(pdmat, ddmat, sc->sc_is, vaddr, flags,
	    mapp));
}

static void
psycho_dmamem_free(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, void *vaddr,
    bus_dmamap_t map)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)pdmat->dt_cookie;
	iommu_dvmamem_free(pdmat, ddmat, sc->sc_is, vaddr, map);
}

static int
psycho_dmamap_create(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, int flags,
    bus_dmamap_t *mapp)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)pdmat->dt_cookie;
	return (iommu_dvmamap_create(pdmat, ddmat, sc->sc_is, flags, mapp));

}

static int
psycho_dmamap_destroy(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat,
    bus_dmamap_t map)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)pdmat->dt_cookie;
	return (iommu_dvmamap_destroy(pdmat, ddmat, sc->sc_is, map));
}

static int
psycho_dmamap_load(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, bus_dmamap_t map,
    void *buf, bus_size_t buflen, bus_dmamap_callback_t *callback,
    void *callback_arg, int flags)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)pdmat->dt_cookie;
	return (iommu_dvmamap_load(pdmat, ddmat, sc->sc_is, map, buf, buflen,
	    callback, callback_arg, flags));
}

static int
psycho_dmamap_load_mbuf(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat,
    bus_dmamap_t map, struct mbuf *m, bus_dmamap_callback2_t *callback,
    void *callback_arg, int flags)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)pdmat->dt_cookie;
	return (iommu_dvmamap_load_mbuf(pdmat, ddmat, sc->sc_is, map, m,
	    callback, callback_arg, flags));
}

static int
psycho_dmamap_load_uio(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat,
    bus_dmamap_t map, struct uio *uio, bus_dmamap_callback2_t *callback,
    void *callback_arg, int flags)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)pdmat->dt_cookie;
	return (iommu_dvmamap_load_uio(pdmat, ddmat, sc->sc_is, map, uio,
	    callback, callback_arg, flags));
}

static void
psycho_dmamap_unload(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, bus_dmamap_t map)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)pdmat->dt_cookie;
	iommu_dvmamap_unload(pdmat, ddmat, sc->sc_is, map);
}

static void
psycho_dmamap_sync(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, bus_dmamap_t map,
    bus_dmasync_op_t op)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)pdmat->dt_cookie;
	iommu_dvmamap_sync(pdmat, ddmat, sc->sc_is, map, op);
}
