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
 *	from: NetBSD: psycho.c,v 1.35 2001/09/10 16:17:06 eeh Exp
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

#include <ofw/openfirm.h>
#include <ofw/ofw_pci.h>

#include <machine/bus.h>
#include <machine/cache.h>
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
static void psycho_set_intr(struct psycho_softc *, int, device_t, u_long *,
    int, driver_intr_t);
static int psycho_find_intrmap(struct psycho_softc *, int, u_long **,
    u_long **, u_long *);
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

/*
 * bus space and bus dma support for UltraSPARC `psycho'.  note that most
 * of the bus dma support is provided by the iommu dvma controller.
 */
static int psycho_dmamem_alloc(bus_dma_tag_t, void **, int, bus_dmamap_t *);
static void psycho_dmamem_free(bus_dma_tag_t, void *, bus_dmamap_t);
static int psycho_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
    bus_dmamap_callback_t *, void *, int);
static void psycho_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
static void psycho_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_dmasync_op_t);

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

static int psycho_ndevs;
static struct psycho_softc *psycho_softcs[4];

struct psycho_clr {
	u_long	*pci_clr;		/* clear register */
	driver_intr_t	*pci_handler;	/* handler to call */
	void	*pci_arg;		/* argument for the handler */
	void	*pci_cookie;		/* interrupt cookie of parent bus */
};

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
#define	OFW_PCI_TYPE		"pci"
#define OFW_SABRE_MODEL		"SUNW,sabre"
#define OFW_SABRE_COMPAT	"pci108e,a001"
#define OFW_SIMBA_MODEL		"SUNW,simba"
#define OFW_PSYCHO_MODEL	"SUNW,psycho"

static int
psycho_probe(device_t dev)
{
	phandle_t node;
	char *dtype, *model;
	static char compat[32];

	node = nexus_get_node(dev);
	if (OF_getprop(node, "compatible", compat, sizeof(compat)) == -1)
		compat[0] = '\0';

	dtype = nexus_get_device_type(dev);
	model = nexus_get_model(dev);
	/* match on a type of "pci" and a sabre or a psycho */
	if (nexus_get_reg(dev) != NULL && dtype != NULL &&
	    strcmp(dtype, OFW_PCI_TYPE) == 0 &&
	    ((model != NULL && (strcmp(model, OFW_SABRE_MODEL) == 0 ||
	      strcmp(model, OFW_PSYCHO_MODEL) == 0)) ||
	      strcmp(compat, OFW_SABRE_COMPAT) == 0)) {
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
	char compat[32];
	char *model;
	phandle_t node;
	u_int64_t csr;
	u_long pci_ctl, mlen;
	int psycho_br[2];
	int n, i, nreg, rid;
#if defined(PSYCHO_DEBUG) || defined(PSYCHO_STRAY)
	u_long *map, *clr;
#endif

	bootverbose = 1;
	node = nexus_get_node(dev);
	sc = device_get_softc(dev);
	if (OF_getprop(node, "compatible", compat, sizeof(compat)) == -1)
		compat[0] = '\0';

	sc->sc_node = node;
	sc->sc_dev = dev;
	sc->sc_dmatag = nexus_get_dmatag(dev);

	/*
	 * call the model-specific initialisation routine.
	 */
	model = nexus_get_model(dev);
	if ((model != NULL &&
	     strcmp(model, OFW_SABRE_MODEL) == 0) ||
	    strcmp(compat, OFW_SABRE_COMPAT) == 0) {
		sc->sc_mode = PSYCHO_MODE_SABRE;
		if (model == NULL)
			model = "sabre";
	} else if (model != NULL &&
	    strcmp(model, OFW_PSYCHO_MODEL) == 0)
		sc->sc_mode = PSYCHO_MODE_PSYCHO;
	else
		panic("psycho_attach: unknown model!");

	/*
	 * The psycho gets three register banks:
	 * (0) per-PBM configuration and status registers
	 * (1) per-PBM PCI configuration space, containing only the
	 *     PBM 256-byte PCI header
	 * (2) the shared psycho configuration registers (struct psychoreg)
	 *
	 * XXX use the prom address for the psycho registers?  we do so far.
	 */
	reg = nexus_get_reg(dev);
	nreg = nexus_get_nreg(dev);
	/* Register layouts are different.  stuupid. */
	if (sc->sc_mode == PSYCHO_MODE_PSYCHO) {
		if (nreg <= 2)
			panic("psycho_attach: %d not enough registers", nreg);
		sc->sc_basepaddr = (vm_offset_t)UPA_REG_PHYS(&reg[2]);
		mlen = UPA_REG_SIZE(&reg[2]);
		pci_ctl = UPA_REG_PHYS(&reg[0]);
	} else {
		if (nreg <= 0)
			panic("psycho_attach: %d not enough registers", nreg);
		sc->sc_basepaddr = (vm_offset_t)UPA_REG_PHYS(&reg[0]);
		mlen = UPA_REG_SIZE(reg);
		pci_ctl = sc->sc_basepaddr +
		    offsetof(struct psychoreg, psy_pcictl[0]);
	}

	if (pci_ctl < sc->sc_basepaddr)
		panic("psycho_attach: bogus pci control register location");
	pci_ctl -= sc->sc_basepaddr;
	rid = 0;
	sc->sc_mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
	    sc->sc_basepaddr, sc->sc_basepaddr + mlen - 1, mlen, RF_ACTIVE);
	if (sc->sc_mem_res == NULL ||
	    rman_get_start(sc->sc_mem_res) != sc->sc_basepaddr)
		panic("psycho_attach: could not allocate device memory");
	sc->sc_bustag = rman_get_bustag(sc->sc_mem_res);
	sc->sc_bushandle = rman_get_bushandle(sc->sc_mem_res);
	if (sparc64_bus_mem_map(UPA_BUS_SPACE, sc->sc_basepaddr, mlen, 0, NULL,
	    (void **)&sc->sc_regs))
		panic("psycho_attach: cannot map regs");
	csr = sc->sc_regs->psy_csr;
	sc->sc_ign = 0x7c0; /* APB IGN is always 0x7c */
	if (sc->sc_mode == PSYCHO_MODE_PSYCHO)
		sc->sc_ign = PSYCHO_GCSR_IGN(csr) << 6;

	device_printf(dev, "%s: impl %d, version %d: ign %x ",
		model, PSYCHO_GCSR_IMPL(csr), PSYCHO_GCSR_VERS(csr),
		sc->sc_ign);

	/*
	 * Match other psycho's that are already configured against
	 * the base physical address. This will be the same for a
	 * pair of devices that share register space.
	 */
	for (n = 0; n < psycho_ndevs && n < sizeof(psycho_softcs) /
	     sizeof(psycho_softcs[0]); n++) {
		asc = (struct psycho_softc *)psycho_softcs[n];

		if (asc == NULL || asc == sc)
			/* This entry is not there or it is me */
			continue;

		if (asc->sc_basepaddr != sc->sc_basepaddr)
			/* This is an unrelated psycho */
			continue;

		/* Found partner */
		osc = asc;
		break;
	}

	/*
	 * Setup the PCI control register
	 */
	csr = bus_space_read_8(sc->sc_bustag, sc->sc_bushandle,
	    pci_ctl +  offsetof(struct pci_ctl, pci_csr));
	csr |= PCICTL_MRLM |
	    PCICTL_ARB_PARK |
	    PCICTL_ERRINTEN |
	    PCICTL_4ENABLE;
	csr &= ~(PCICTL_SERR |
	    PCICTL_CPU_PRIO |
	    PCICTL_ARB_PRIO |
	    PCICTL_RTRYWAIT);
	bus_space_write_8(sc->sc_bustag, sc->sc_bushandle,
	    pci_ctl +  offsetof(struct pci_ctl, pci_csr), csr);

	/* grab the psycho ranges */
	psycho_get_ranges(sc->sc_node, &sc->sc_range, &sc->sc_nrange);

	/* get the bus-range for the psycho */
	n = OF_getprop(node, "bus-range", (void *)psycho_br, sizeof(psycho_br));
	if (n == -1)
		panic("could not get psycho bus-range");
	if (n != sizeof(psycho_br))
		panic("broken psycho bus-range (%d)", n);

	printf("bus range %u to %u; PCI bus %d\n", psycho_br[0], psycho_br[1],
	    psycho_br[0]);

	sc->sc_busno = psycho_br[0];

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
		panic("bus_dma_tag_create failed");
	/* Customize the tag */
	sc->sc_dmat->cookie = sc;
	sc->sc_dmat->dmamem_alloc = psycho_dmamem_alloc;
	sc->sc_dmat->dmamem_free = psycho_dmamem_free;
	sc->sc_dmat->dmamap_load = psycho_dmamap_load;
	sc->sc_dmat->dmamap_unload = psycho_dmamap_unload;
	sc->sc_dmat->dmamap_sync = psycho_dmamap_sync;
	/* XXX: register as root dma tag (kluge). */
	sparc64_root_dma_tag = sc->sc_dmat;

	if ((sc->sc_nintrmap = OF_getprop_alloc(sc->sc_node, "interrupt-map",
	    sizeof(*sc->sc_intrmap), (void **)&sc->sc_intrmap)) == -1 ||
	    OF_getprop(sc->sc_node, "interrupt-map-mask", &sc->sc_intrmapmsk,
		sizeof(sc->sc_intrmapmsk)) == -1) {
		if (sc->sc_intrmap != NULL) {
			free(sc->sc_intrmap, M_OFWPROP);
			sc->sc_intrmap = NULL;
		}
	}

	/* Register the softc, this is needed for paired psychos. */
	if (psycho_ndevs < sizeof(psycho_softcs) / sizeof(psycho_softcs[0]))
		psycho_softcs[psycho_ndevs] = sc;
	else
		device_printf(dev, "XXX: bump the number of psycho_softcs");
	psycho_ndevs++;
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
		psycho_set_intr(sc, 0, dev, &sc->sc_regs->ue_int_map,
		    INTR_TYPE_MISC | INTR_FAST, psycho_ue);
		psycho_set_intr(sc, 1, dev, &sc->sc_regs->ce_int_map,
		    INTR_TYPE_MISC, psycho_ce);
		psycho_set_intr(sc, 2, dev,
		    &sc->sc_regs->pciaerr_int_map, INTR_TYPE_MISC | INTR_FAST,
		    psycho_bus_a);
		psycho_set_intr(sc, 3, dev,
		    &sc->sc_regs->pciberr_int_map, INTR_TYPE_MISC | INTR_FAST,
		    psycho_bus_b);
		psycho_set_intr(sc, 4, dev, &sc->sc_regs->power_int_map,
		    INTR_TYPE_MISC | INTR_FAST, psycho_powerfail);
#ifdef PSYCHO_MAP_WAKEUP
		/*
		 * On some models, this is mapped to the same interrupt as
		 * pciberr by default, so leave it alone for now since
		 * psycho_wakeup() doesn't do anything useful anyway.
		 */
		psycho_set_intr(sc, 5, dev, &sc->sc_regs->pwrmgt_int_map,
		    INTR_TYPE_MISC, psycho_wakeup);
#endif /* PSYCHO_MAP_WAKEUP */

		/*
		 * Setup IOMMU and PCI configuration if we're the first
		 * of a pair of psycho's to arrive here.
		 *
		 * We should calculate a TSB size based on amount of RAM
		 * and number of bus controllers and number an type of
		 * child devices.
		 *
		 * For the moment, 32KB should be more than enough.
		 */
		psycho_iommu_init(sc, 2);
	} else {
		/* Just copy IOMMU state, config tag and address */
		sc->sc_is = osc->sc_is;
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
	for (map = &sc->sc_regs->pcia0_int_map,
	     clr = &sc->sc_regs->pcia0_int_clr[0], n = 0;
	     map <= &sc->sc_regs->pcib3_int_map; map++, clr += 4, n++) {
#ifdef PSYCHO_DEBUG
		device_printf(dev, "intr map (pci) %d: %lx\n", n, *map);
#endif
		*map &= ~INTMAP_V;
		membar(StoreStore);
		for (i = 0; i < 4; i++)
			clr[i] = 0;
		membar(StoreStore);
		*map |= INTMAP_V;
	}
	for (map = &sc->sc_regs->scsi_int_map, n = 0,
	     clr = &sc->sc_regs->scsi_int_clr, n = 0;
	     map <= &sc->sc_regs->ffb1_int_map; map++, clr++, n++) {
#ifdef PSYCHO_DEBUG
		device_printf(dev, "intr map (obio) %d: %lx, clr: %p\n", n,
		    *map, clr);
#endif
		*map &= ~INTMAP_V;
		membar(StoreStore);
		*clr = 0;
#ifdef PSYCHO_STRAY
		/*
		 * This can cause interrupt storms, and is therefore disabled
		 * by default.
		 * XXX: use intr_setup() to not confuse higher level code
		 */
		if (INTVEC(*map) != 0x7e6 && INTVEC(*map) != 0x7e7 &&
		    INTVEC(*map) != 0)
		intr_setup(PIL_LOW, intr_dequeue, INTVEC(*map), psycho_intr_stray,
		    (void *)clr);
#endif
		membar(StoreStore);
		*map |= INTMAP_V;
	}
#endif

	/*
	 * Initialize the interrupt registers of all devices hanging from
	 * the host bridge directly or indirectly via PCI-PCI bridges.
	 * The MI code (and the PCI spec) assume that this is done during
	 * system initialization, however the firmware does not do this
	 * at least on some models, and we probably shouldn't trust that
	 * the firmware uses the same model as this driver if it does.
	 */
	ofw_pci_init_intr(dev, sc->sc_node, sc->sc_intrmap, sc->sc_nintrmap,
	    &sc->sc_intrmapmsk);

	device_add_child(dev, "pci", device_get_unit(dev));
	return (bus_generic_attach(dev));
}

static void
psycho_set_intr(struct psycho_softc *sc, int index,
    device_t dev, u_long *map, int iflags, driver_intr_t handler)
{
	int rid;

	sc->sc_irq_res[index] = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
	    INTVEC(*map), INTVEC(*map), 1, RF_ACTIVE);
	if (sc->sc_irq_res[index] == NULL)
		panic("psycho_set_intr: failed to get interupt");
	bus_setup_intr(dev, sc->sc_irq_res[index], INTR_TYPE_MISC | iflags,
	    handler, sc, &sc->sc_ihand[index]);
	*map |= INTMAP_V;
}

static int
psycho_find_intrmap(struct psycho_softc *sc, int ino, u_long **intrmapptr,
    u_long **intrclrptr, u_long *intrdiagptr)
{
	u_long *intrmap, *intrclr, diag;
	int found;

	found = 0;
	/* Hunt thru obio first */
	diag = sc->sc_regs->obio_int_diag;
	for (intrmap = &sc->sc_regs->scsi_int_map,
		 intrclr = &sc->sc_regs->scsi_int_clr;
	     intrmap <= &sc->sc_regs->ffb1_int_map;
	     intrmap++, intrclr++, diag >>= 2) {
		if (INTINO(*intrmap) == ino) {
			diag &= 2;
			found = 1;
			break;
		}
	}

	if (!found) {
		diag = sc->sc_regs->pci_int_diag;
		/* Now do PCI interrupts */
		for (intrmap = &sc->sc_regs->pcia0_int_map,
			 intrclr = &sc->sc_regs->pcia0_int_clr[0];
		     intrmap <= &sc->sc_regs->pcib3_int_map;
		     intrmap++, intrclr += 4, diag >>= 8) {
			if (((*intrmap ^ ino) & 0x3c) == 0) {
				intrclr += ino & 3;
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
	struct psychoreg *regs = sc->sc_regs;

	sc->sc_regs->ue_int_clr = 0;
	/* It's uncorrectable.  Dump the regs and panic. */
	panic("%s: uncorrectable DMA error AFAR %llx AFSR %llx\n",
		device_get_name(sc->sc_dev),
		(long long)regs->psy_ue_afar, (long long)regs->psy_ue_afsr);
}

static void
psycho_ce(void *arg)
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;
	struct psychoreg *regs = sc->sc_regs;

	sc->sc_regs->ce_int_clr = 0;
	/* It's correctable.  Dump the regs and continue. */
	printf("%s: correctable DMA error AFAR %llx AFSR %llx\n",
		device_get_name(sc->sc_dev),
		(long long)regs->psy_ce_afar, (long long)regs->psy_ce_afsr);
}

static void
psycho_bus_a(void *arg)
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;
	struct psychoreg *regs = sc->sc_regs;

	sc->sc_regs->pciaerr_int_clr = 0;
	/* It's uncorrectable.  Dump the regs and panic. */
	panic("%s: PCI bus A error AFAR %lx AFSR %lx\n",
	    device_get_name(sc->sc_dev),
	    regs->psy_pcictl[0].pci_afar, regs->psy_pcictl[0].pci_afsr);
}

static void
psycho_bus_b(void *arg)
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;
	struct psychoreg *regs = sc->sc_regs;

	sc->sc_regs->pciberr_int_clr = 0;
	/* It's uncorrectable.  Dump the regs and panic. */
	panic("%s: PCI bus B error AFAR %lx AFSR %lx\n",
	    device_get_name(sc->sc_dev),
	    regs->psy_pcictl[1].pci_afar, regs->psy_pcictl[1].pci_afsr);
}

static void
psycho_powerfail(void *arg)
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;

	sc->sc_regs->power_int_clr = 0;
	/* We lost power.  Try to shut down NOW. */
	printf("Power Failure Detected: Shutting down NOW.\n");
	shutdown_nice(0);
}

#ifdef PSYCHO_MAP_WAKEUP
static void
psycho_wakeup(void *arg)
{
	struct psycho_softc *sc = (struct psycho_softc *)arg;

	sc->sc_regs->pwrmgt_int_clr = 0;
	/* Gee, we don't really have a framework to deal with this properly. */
	printf("%s: power management wakeup\n",	device_get_name(sc->sc_dev));
}
#endif /* PSYCHO_MAP_WAKEUP */

/* initialise the IOMMU... */
void
psycho_iommu_init(struct psycho_softc *sc, int tsbsize)
{
	char *name;
	struct iommu_state *is;
	u_int32_t iobase = -1;
	int *vdma = NULL;
	int nitem;

	is = malloc(sizeof(struct iommu_state), M_DEVBUF, M_NOWAIT);
	if (is == NULL)
		panic("psycho_iommu_init: malloc failed");

	sc->sc_is = is;

	/* punch in our copies */
	is->is_bustag = sc->sc_bustag;
	is->is_iommu = &sc->sc_regs->psy_iommu;
	is->is_dtag = &sc->sc_regs->tlb_tag_diag[0];
	is->is_ddram = &sc->sc_regs->tlb_data_diag[0];
	is->is_dqueue = &sc->sc_regs->iommu_queue_diag[0];
	is->is_dva = &sc->sc_regs->iommu_svadiag;
	is->is_dtcmp = &sc->sc_regs->iommu_tlb_comp_diag;

	if (OF_getproplen(sc->sc_node, "no-streaming-cache") < 0)
		is->is_sb = 0;
	else
		is->is_sb = &sc->sc_regs->psy_iommu_strbuf;

	/*
	 * Separate the men from the boys.  Get the `virtual-dma'
	 * property for sabre and use that to make sure the damn
	 * iommu works.
	 *
	 * We could query the `#virtual-dma-size-cells' and
	 * `#virtual-dma-addr-cells' and DTRT, but I'm lazy.
	 */
	nitem = OF_getprop_alloc(sc->sc_node, "virtual-dma", sizeof(vdma),
	    (void **)&vdma);
	if (nitem > 0) {
		iobase = vdma[0];
		tsbsize = ffs(vdma[1]);
		if (tsbsize < 25 || tsbsize > 31 ||
		    (vdma[1] & ~(1 << (tsbsize - 1))) != 0) {
			printf("bogus tsb size %x, using 7\n", vdma[1]);
			tsbsize = 31;
		}
		tsbsize -= 24;
		free(vdma, M_OFWPROP);
	}

	/* give us a nice name.. */
	name = (char *)malloc(32, M_DEVBUF, M_NOWAIT);
	if (name == 0)
		panic("couldn't malloc iommu name");
	snprintf(name, 32, "%s dvma", device_get_name(sc->sc_dev));

	iommu_init(name, is, tsbsize, iobase);
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
	{ 0x1001108e, DQT_BAD_INTPIN },		/* Sun HME (PCIO func. 1) */
	{ 0x1101108e, DQT_BAD_INTPIN },		/* Sun GEM (PCIO2 func. 1) */
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
	 * Since we preinitialize all interrupt line registers, this should not
	 * happen for any built-in device.
	 * Devices on bridges that route interrupts cannot work now - the
	 * interrupt pin mappings are not known from the firmware...
	 */
	panic("psycho_route_interrupt");
}

static int
psycho_read_ivar(device_t dev, device_t child, int which, u_long *result)
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
	*pc->pci_clr = 0;
	pc->pci_handler(pc->pci_arg);
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

	*((u_long *)arg) = 0;
}
#endif

static int
psycho_setup_intr(device_t dev, device_t child,
    struct resource *ires,  int flags, driver_intr_t *intr, void *arg,
    void **cookiep)
{
	struct psycho_softc *sc;
	struct psycho_clr *pc;
	u_long *intrmapptr, *intrclrptr;
	int ino;
	int error;
	long vec = rman_get_start(ires);

	sc = (struct psycho_softc *)device_get_softc(dev);
	pc = (struct psycho_clr *)
		malloc(sizeof(*pc), M_DEVBUF, M_NOWAIT);
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
	device_printf(dev, "psycho_setup_intr: INO %d, map %p, clr %p\n", ino,
	    intrmapptr, intrclrptr);
#endif
	pc->pci_arg = arg;
	pc->pci_handler = intr;
	pc->pci_clr = intrclrptr;
	/* Disable the interrupt while we fiddle with it */
	*intrmapptr &= ~INTMAP_V;
	membar(Sync);
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
	*intrclrptr = 0;
	membar(StoreStore);
	/*
	 * Enable the interrupt now we have the handler installed.
	 * Read the current value as we can't change it besides the
	 * valid bit so so make sure only this bit is changed.
	 */
	*intrmapptr |= INTMAP_V;
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
		/* XXX: probably should use ranges property here. */
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

	if (type == SYS_RES_IRQ)
		return (bus_activate_resource(bus, type, rid, r));
	return (rman_activate_resource(r));
}

static int
psycho_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	if (type == SYS_RES_IRQ)
		return (bus_deactivate_resource(bus, type, rid, r));
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

	bt = (bus_space_tag_t)
		malloc(sizeof(struct bus_space_tag), M_DEVBUF, M_NOWAIT);
	if (bt == NULL)
		panic("could not allocate psycho bus tag");

	bzero(bt, sizeof *bt);
	bt->cookie = sc;
	bt->parent = sc->sc_bustag;
	bt->type = type;
	return (bt);
}

/*
 * hooks into the iommu dvma calls.
 */
static int
psycho_dmamem_alloc(bus_dma_tag_t dmat, void **vaddr, int flags, bus_dmamap_t *mapp)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)dmat->cookie;
	return (iommu_dvmamem_alloc(dmat, sc->sc_is, vaddr, flags, mapp));
}

static void
psycho_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)dmat->cookie;
	return (iommu_dvmamem_free(dmat, sc->sc_is, vaddr, map));
}

static int
psycho_dmamap_load(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
    bus_size_t buflen, bus_dmamap_callback_t *callback, void *callback_arg,
    int flags)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)dmat->cookie;
	return (iommu_dvmamap_load(dmat, sc->sc_is, map, buf, buflen, callback,
	    callback_arg, flags));
}

static void
psycho_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)dmat->cookie;
	iommu_dvmamap_unload(dmat, sc->sc_is, map);
}

static void
psycho_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map,
    bus_dmasync_op_t op)
{
	struct psycho_softc *sc;

	sc = (struct psycho_softc *)dmat->cookie;
	iommu_dvmamap_sync(dmat, sc->sc_is, map, op);
}
