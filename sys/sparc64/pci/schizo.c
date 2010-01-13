/*-
 * Copyright (c) 1999, 2000 Matthew R. Green
 * Copyright (c) 2001 - 2003 by Thomas Moestl <tmm@FreeBSD.org>
 * Copyright (c) 2005, 2007, 2008 by Marius Strobl <marius@FreeBSD.org>
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
 *	from: FreeBSD: psycho.c 183152 2008-09-18 19:45:22Z marius
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for `Schizo' Fireplane/Safari to PCI 2.1 and `Tomatillo' JBus to
 * PCI 2.2 bridges
 */

#include "opt_ofw_pci.h"
#include "opt_schizo.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/bus_common.h>
#include <machine/bus_private.h>
#include <machine/fsr.h>
#include <machine/iommureg.h>
#include <machine/iommuvar.h>
#include <machine/ofw_bus.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <sparc64/pci/ofw_pci.h>
#include <sparc64/pci/schizoreg.h>
#include <sparc64/pci/schizovar.h>

#include "pcib_if.h"

static const struct schizo_desc *schizo_get_desc(device_t);
static void schizo_set_intr(struct schizo_softc *, u_int, u_int,
    driver_filter_t);
static driver_filter_t schizo_dma_sync_stub;
static driver_filter_t ichip_dma_sync_stub;
static void schizo_intr_enable(void *);
static void schizo_intr_disable(void *);
static void schizo_intr_assign(void *);
static void schizo_intr_clear(void *);
static int schizo_intr_register(struct schizo_softc *sc, u_int ino);
static int schizo_get_intrmap(struct schizo_softc *, u_int,
    bus_addr_t *, bus_addr_t *);
static bus_space_tag_t schizo_alloc_bus_tag(struct schizo_softc *, int);
static timecounter_get_t schizo_get_timecount;

/* Interrupt handlers */
static driver_filter_t schizo_pci_bus;
static driver_filter_t schizo_ue;
static driver_filter_t schizo_ce;
static driver_filter_t schizo_host_bus;
static driver_filter_t schizo_cdma;

/* IOMMU support */
static void schizo_iommu_init(struct schizo_softc *, int, uint32_t);

/*
 * Methods
 */
static device_probe_t schizo_probe;
static device_attach_t schizo_attach;
static bus_read_ivar_t schizo_read_ivar;
static bus_setup_intr_t schizo_setup_intr;
static bus_teardown_intr_t schizo_teardown_intr;
static bus_alloc_resource_t schizo_alloc_resource;
static bus_activate_resource_t schizo_activate_resource;
static bus_deactivate_resource_t schizo_deactivate_resource;
static bus_release_resource_t schizo_release_resource;
static bus_get_dma_tag_t schizo_get_dma_tag;
static pcib_maxslots_t schizo_maxslots;
static pcib_read_config_t schizo_read_config;
static pcib_write_config_t schizo_write_config;
static pcib_route_interrupt_t schizo_route_interrupt;
static ofw_bus_get_node_t schizo_get_node;

static device_method_t schizo_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		schizo_probe),
	DEVMETHOD(device_attach,	schizo_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	schizo_read_ivar),
	DEVMETHOD(bus_setup_intr,	schizo_setup_intr),
	DEVMETHOD(bus_teardown_intr,	schizo_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	schizo_alloc_resource),
	DEVMETHOD(bus_activate_resource,	schizo_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	schizo_deactivate_resource),
	DEVMETHOD(bus_release_resource,	schizo_release_resource),
	DEVMETHOD(bus_get_dma_tag,	schizo_get_dma_tag),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	schizo_maxslots),
	DEVMETHOD(pcib_read_config,	schizo_read_config),
	DEVMETHOD(pcib_write_config,	schizo_write_config),
	DEVMETHOD(pcib_route_interrupt,	schizo_route_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	schizo_get_node),

	KOBJMETHOD_END
};

static devclass_t schizo_devclass;

DEFINE_CLASS_0(pcib, schizo_driver, schizo_methods,
    sizeof(struct schizo_softc));
DRIVER_MODULE(schizo, nexus, schizo_driver, schizo_devclass, 0, 0);

static SLIST_HEAD(, schizo_softc) schizo_softcs =
    SLIST_HEAD_INITIALIZER(schizo_softcs);

static const struct intr_controller schizo_ic = {
	schizo_intr_enable,
	schizo_intr_disable,
	schizo_intr_assign,
	schizo_intr_clear
};

struct schizo_icarg {
	struct schizo_softc	*sica_sc;
	bus_addr_t		sica_map;
	bus_addr_t		sica_clr;
};

struct schizo_dma_sync {
	struct schizo_softc	*sds_sc;
	driver_filter_t		*sds_handler;
	void			*sds_arg;
	void			*sds_cookie;
	uint64_t		sds_syncval;
	device_t		sds_ppb;	/* farest PCI-PCI bridge */
	uint8_t			sds_bus;	/* bus of farest PCI device */
	uint8_t			sds_slot;	/* slot of farest PCI device */
	uint8_t			sds_func;	/* func. of farest PCI device */
};

#define	SCHIZO_PERF_CNT_QLTY	100

#define	SCHIZO_SPC_READ_8(spc, sc, offs) \
	bus_read_8((sc)->sc_mem_res[(spc)], (offs))
#define	SCHIZO_SPC_WRITE_8(spc, sc, offs, v) \
	bus_write_8((sc)->sc_mem_res[(spc)], (offs), (v))

#define	SCHIZO_PCI_READ_8(sc, offs) \
	SCHIZO_SPC_READ_8(STX_PCI, (sc), (offs))
#define	SCHIZO_PCI_WRITE_8(sc, offs, v) \
	SCHIZO_SPC_WRITE_8(STX_PCI, (sc), (offs), (v))
#define	SCHIZO_CTRL_READ_8(sc, offs) \
	SCHIZO_SPC_READ_8(STX_CTRL, (sc), (offs))
#define	SCHIZO_CTRL_WRITE_8(sc, offs, v) \
	SCHIZO_SPC_WRITE_8(STX_CTRL, (sc), (offs), (v))
#define	SCHIZO_PCICFG_READ_8(sc, offs) \
	SCHIZO_SPC_READ_8(STX_PCICFG, (sc), (offs))
#define	SCHIZO_PCICFG_WRITE_8(sc, offs, v) \
	SCHIZO_SPC_WRITE_8(STX_PCICFG, (sc), (offs), (v))
#define	SCHIZO_ICON_READ_8(sc, offs) \
	SCHIZO_SPC_READ_8(STX_ICON, (sc), (offs))
#define	SCHIZO_ICON_WRITE_8(sc, offs, v) \
	SCHIZO_SPC_WRITE_8(STX_ICON, (sc), (offs), (v))

struct schizo_desc {
	const char	*sd_string;
	int		sd_mode;
	const char	*sd_name;
};

static const struct schizo_desc const schizo_compats[] = {
	{ "pci108e,8001",	SCHIZO_MODE_SCZ,	"Schizo" },
	{ "pci108e,a801",	SCHIZO_MODE_TOM,	"Tomatillo" },
	{ NULL,			0,			NULL }
};

static const struct schizo_desc *
schizo_get_desc(device_t dev)
{
	const struct schizo_desc *desc;
	const char *compat;

	compat = ofw_bus_get_compat(dev);
	if (compat == NULL)
		return (NULL);
	for (desc = schizo_compats; desc->sd_string != NULL; desc++)
		if (strcmp(desc->sd_string, compat) == 0)
			return (desc);
	return (NULL);
}

static int
schizo_probe(device_t dev)
{
	const char *dtype;

	dtype = ofw_bus_get_type(dev);
	if (dtype != NULL && strcmp(dtype, OFW_TYPE_PCI) == 0 &&
	    schizo_get_desc(dev) != NULL) {
		device_set_desc(dev, "Sun Host-PCI bridge");
		return (0);
	}
	return (ENXIO);
}

static int
schizo_attach(device_t dev)
{
	struct ofw_pci_ranges *range;
	const struct schizo_desc *desc;
	struct schizo_softc *asc, *sc, *osc;
	struct timecounter *tc;
	uint64_t ino_bitmap, reg;
	phandle_t node;
	uint32_t prop, prop_array[2];
	int i, mode, n, nrange, rid, tsbsize;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	desc = schizo_get_desc(dev);
	mode = desc->sd_mode;

	sc->sc_dev = dev;
	sc->sc_node = node;
	sc->sc_mode = mode;
	sc->sc_flags = 0;

	/*
	 * The Schizo has three register banks:
	 * (0) per-PBM PCI configuration and status registers, but for bus B
	 *     shared with the UPA64s interrupt mapping register banks
	 * (1) shared Schizo controller configuration and status registers
	 * (2) per-PBM PCI configuration space
	 *
	 * The Tomatillo has four register banks:
	 * (0) per-PBM PCI configuration and status registers
	 * (1) per-PBM Tomatillo controller configuration registers, but on
	 *     machines having the `jbusppm' device shared with its Estar
	 *     register bank for bus A
	 * (2) per-PBM PCI configuration space
	 * (3) per-PBM interrupt concentrator registers
	 */
	sc->sc_half = (bus_get_resource_start(dev, SYS_RES_MEMORY, STX_PCI) >>
	    20) & 1;
	for (n = 0; n < (mode == SCHIZO_MODE_SCZ ? SCZ_NREG : TOM_NREG);
	    n++) {
		rid = n;
		sc->sc_mem_res[n] = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &rid,
		    (((mode == SCHIZO_MODE_SCZ && ((sc->sc_half == 1 &&
		    n == STX_PCI) || n == STX_CTRL)) ||
		    (mode == SCHIZO_MODE_TOM && sc->sc_half == 0 &&
		    n == STX_CTRL)) ? RF_SHAREABLE : 0) | RF_ACTIVE);
		if (sc->sc_mem_res[n] == NULL)
			panic("%s: could not allocate register bank %d",
			    __func__, n);
	}

	/*
	 * Match other Schizos that are already configured against
	 * the controller base physical address.  This will be the
	 * same for a pair of devices that share register space.
	 */
	osc = NULL;
	SLIST_FOREACH(asc, &schizo_softcs, sc_link) {
		if (rman_get_start(asc->sc_mem_res[STX_CTRL]) ==
		    rman_get_start(sc->sc_mem_res[STX_CTRL])) {
			/* Found partner. */
			osc = asc;
			break;
		}
	}
	if (osc == NULL) {
		sc->sc_mtx = malloc(sizeof(*sc->sc_mtx), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (sc->sc_mtx == NULL)
			panic("%s: could not malloc mutex", __func__);
		mtx_init(sc->sc_mtx, "pcib_mtx", NULL, MTX_SPIN);
	} else {
		if (sc->sc_mode != SCHIZO_MODE_SCZ)
			panic("%s: no partner expected", __func__);
		if (mtx_initialized(osc->sc_mtx) == 0)
			panic("%s: mutex not initialized", __func__);
		sc->sc_mtx = osc->sc_mtx;
	}

	if (OF_getprop(node, "portid", &sc->sc_ign, sizeof(sc->sc_ign)) == -1)
		panic("%s: could not determine IGN", __func__);
	if (OF_getprop(node, "version#", &sc->sc_ver, sizeof(sc->sc_ver)) == -1)
		panic("%s: could not determine version", __func__);
	if (OF_getprop(node, "clock-frequency", &prop, sizeof(prop)) == -1)
		prop = 33000000;

	device_printf(dev, "%s, version %d, IGN %#x, bus %c, %dMHz\n",
	    desc->sd_name, sc->sc_ver, sc->sc_ign, 'A' + sc->sc_half,
	    prop / 1000 / 1000);

	/* Set up the PCI interrupt retry timer. */
#ifdef SCHIZO_DEBUG
	device_printf(dev, "PCI IRT 0x%016llx\n", (unsigned long long)
	    SCHIZO_PCI_READ_8(sc, STX_PCI_INTR_RETRY_TIM));
#endif
	SCHIZO_PCI_WRITE_8(sc, STX_PCI_INTR_RETRY_TIM, 5);

	/* Set up the PCI control register. */
	reg = SCHIZO_PCI_READ_8(sc, STX_PCI_CTRL);
	reg |= STX_PCI_CTRL_MMU_IEN | STX_PCI_CTRL_SBH_IEN |
	    STX_PCI_CTRL_ERR_IEN | STX_PCI_CTRL_ARB_MASK;
	reg &= ~(TOM_PCI_CTRL_DTO_IEN | STX_PCI_CTRL_ARB_PARK);
	if (OF_getproplen(node, "no-bus-parking") < 0)
		reg |= STX_PCI_CTRL_ARB_PARK;
	if (mode == SCHIZO_MODE_TOM) {
		reg |= TOM_PCI_CTRL_PRM | TOM_PCI_CTRL_PRO | TOM_PCI_CTRL_PRL;
		if (sc->sc_ver <= 1)	/* revision <= 2.0 */
			reg |= TOM_PCI_CTRL_DTO_IEN;
		else
			reg |= STX_PCI_CTRL_PTO;
	}
#ifdef SCHIZO_DEBUG
	device_printf(dev, "PCI CSR 0x%016llx -> 0x%016llx\n",
	    (unsigned long long)SCHIZO_PCI_READ_8(sc, STX_PCI_CTRL),
	    (unsigned long long)reg);
#endif
	SCHIZO_PCI_WRITE_8(sc, STX_PCI_CTRL, reg);

	/* Set up the PCI diagnostic register. */
	reg = SCHIZO_PCI_READ_8(sc, STX_PCI_DIAG);
	reg &= ~(SCZ_PCI_DIAG_RTRYARB_DIS | STX_PCI_DIAG_RETRY_DIS |
	    STX_PCI_DIAG_INTRSYNC_DIS);
#ifdef SCHIZO_DEBUG
	device_printf(dev, "PCI DR 0x%016llx -> 0x%016llx\n",
	    (unsigned long long)SCHIZO_PCI_READ_8(sc, STX_PCI_DIAG),
	    (unsigned long long)reg);
#endif
	SCHIZO_PCI_WRITE_8(sc, STX_PCI_DIAG, reg);

	/*
	 * On Tomatillo clear the I/O prefetch lengths (workaround for a
	 * Jalapeno bug).
	 */
	if (mode == SCHIZO_MODE_TOM)
		SCHIZO_PCI_WRITE_8(sc, TOM_PCI_IOC_CSR, TOM_PCI_IOC_PW |
		    (1 << TOM_PCI_IOC_PREF_OFF_SHIFT) | TOM_PCI_IOC_CPRM |
		    TOM_PCI_IOC_CPRO | TOM_PCI_IOC_CPRL);

	/*
	 * Hunt through all the interrupt mapping regs and register
	 * the interrupt controller for our interrupt vectors.  We do
	 * this early in order to be able to catch stray interrupts.
	 * This is complicated by the fact that a pair of Schizo PBMs
	 * shares one IGN.
	 */
	n = OF_getprop(node, "ino-bitmap", (void *)prop_array,
	    sizeof(prop_array));
	if (n == -1)
		panic("%s: could not get ino-bitmap", __func__);
	ino_bitmap = ((uint64_t)prop_array[1] << 32) | prop_array[0];
	for (n = 0; n <= STX_MAX_INO; n++) {
		if ((ino_bitmap & (1ULL << n)) == 0)
			continue;
		if (n == STX_FB0_INO || n == STX_FB1_INO)
			/* Leave for upa(4). */
			continue;
		i = schizo_intr_register(sc, n);
		if (i != 0)
			device_printf(dev, "could not register interrupt "
			    "controller for INO %d (%d)\n", n, i);
	}

	/*
	 * Setup Safari/JBus performance counter 0 in bus cycle counting
	 * mode as timecounter.  Unfortunately, this is broken with at
	 * least the version 4 Tomatillos found in Fire V120 and Blade
	 * 1500, which apparently actually count some different event at
	 * ~0.5 and 3MHz respectively instead (also when running in full
	 * power mode).  Besides, one counter seems to be shared by a
	 * "pair" of Tomatillos, too.
	 */
	if (sc->sc_half == 0) {
		SCHIZO_CTRL_WRITE_8(sc, STX_CTRL_PERF,
		    (STX_CTRL_PERF_DIS << STX_CTRL_PERF_CNT1_SHIFT) |
		    (STX_CTRL_PERF_BUSCYC << STX_CTRL_PERF_CNT0_SHIFT));
		tc = malloc(sizeof(*tc), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (tc == NULL)
			panic("%s: could not malloc timecounter", __func__);
		tc->tc_get_timecount = schizo_get_timecount;
		tc->tc_poll_pps = NULL;
		tc->tc_counter_mask = STX_CTRL_PERF_CNT_MASK;
		if (OF_getprop(OF_peer(0), "clock-frequency", &prop,
		    sizeof(prop)) == -1)
			panic("%s: could not determine clock frequency",
			    __func__);
		tc->tc_frequency = prop;
		tc->tc_name = strdup(device_get_nameunit(dev), M_DEVBUF);
		if (mode == SCHIZO_MODE_SCZ)
			tc->tc_quality = SCHIZO_PERF_CNT_QLTY;
		else
			tc->tc_quality = -SCHIZO_PERF_CNT_QLTY;
		tc->tc_priv = sc;
		tc_init(tc);
	}

	/*
	 * Set up the IOMMU.  Schizo, Tomatillo and XMITS all have
	 * one per PBM.  Schizo and XMITS additionally have a streaming
	 * buffer, in Schizo version < 5 (i.e. revision < 2.3) it's
	 * affected by several errata and basically unusable though.
	 */
	sc->sc_is.is_pmaxaddr = IOMMU_MAXADDR(STX_IOMMU_BITS);
	sc->sc_is.is_sb[0] = sc->sc_is.is_sb[1] = 0;
	if (OF_getproplen(node, "no-streaming-cache") < 0 &&
	    !(sc->sc_mode == SCHIZO_MODE_SCZ && sc->sc_ver < 5))
		sc->sc_is.is_sb[0] = STX_PCI_STRBUF;

#define	TSBCASE(x)							\
	case (IOTSB_BASESZ << (x)) << (IO_PAGE_SHIFT - IOTTE_SHIFT):	\
		tsbsize = (x);						\
		break;							\

	n = OF_getprop(node, "virtual-dma", (void *)prop_array,
	    sizeof(prop_array));
	if (n == -1 || n != sizeof(prop_array))
		schizo_iommu_init(sc, 7, -1);
	else {
		switch (prop_array[1]) {
		TSBCASE(1);
		TSBCASE(2);
		TSBCASE(3);
		TSBCASE(4);
		TSBCASE(5);
		TSBCASE(6);
		TSBCASE(7);
		TSBCASE(8);
		default:
			panic("%s: unsupported DVMA size 0x%x",
			    __func__, prop_array[1]);
			/* NOTREACHED */
		}
		schizo_iommu_init(sc, tsbsize, prop_array[0]);
	}

#undef TSBCASE

	/* Initialize memory and I/O rmans. */
	sc->sc_pci_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_pci_io_rman.rm_descr = "Schizo PCI I/O Ports";
	if (rman_init(&sc->sc_pci_io_rman) != 0 ||
	    rman_manage_region(&sc->sc_pci_io_rman, 0, STX_IO_SIZE) != 0)
		panic("%s: failed to set up I/O rman", __func__);
	sc->sc_pci_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_pci_mem_rman.rm_descr = "Schizo PCI Memory";
	if (rman_init(&sc->sc_pci_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_pci_mem_rman, 0, STX_MEM_SIZE) != 0)
		panic("%s: failed to set up memory rman", __func__);

	nrange = OF_getprop_alloc(node, "ranges", sizeof(*range),
	    (void **)&range);
	/*
	 * Make sure that the expected ranges are present.  The
	 * OFW_PCI_CS_MEM64 one is not currently used though.
	 */
	if (nrange != STX_NRANGE)
		panic("%s: unsupported number of ranges", __func__);
	/*
	 * Find the addresses of the various bus spaces.
	 * There should not be multiple ones of one kind.
	 * The physical start addresses of the ranges are the configuration,
	 * memory and I/O handles.
	 */
	for (n = 0; n < STX_NRANGE; n++) {
		i = OFW_PCI_RANGE_CS(&range[n]);
		if (sc->sc_pci_bh[i] != 0)
			panic("%s: duplicate range for space %d", __func__, i);
		sc->sc_pci_bh[i] = OFW_PCI_RANGE_PHYS(&range[n]);
	}
	free(range, M_OFWPROP);

	/* Register the softc, this is needed for paired Schizos. */
	SLIST_INSERT_HEAD(&schizo_softcs, sc, sc_link);

	/* Allocate our tags. */
	sc->sc_pci_memt = schizo_alloc_bus_tag(sc, PCI_MEMORY_BUS_SPACE);
	sc->sc_pci_iot = schizo_alloc_bus_tag(sc, PCI_IO_BUS_SPACE);
	sc->sc_pci_cfgt = schizo_alloc_bus_tag(sc, PCI_CONFIG_BUS_SPACE);
	if (bus_dma_tag_create(bus_get_dma_tag(dev), 8, 0,
	    sc->sc_is.is_pmaxaddr, ~0, NULL, NULL, sc->sc_is.is_pmaxaddr,
	    0xff, 0xffffffff, 0, NULL, NULL, &sc->sc_pci_dmat) != 0)
		panic("%s: bus_dma_tag_create failed", __func__);
	/* Customize the tag. */
	sc->sc_pci_dmat->dt_cookie = &sc->sc_is;
	sc->sc_pci_dmat->dt_mt = &iommu_dma_methods;

	/*
	 * Get the bus range from the firmware.
	 * NB: Tomatillos don't support PCI bus reenumeration.
	 */
	n = OF_getprop(node, "bus-range", (void *)prop_array,
	    sizeof(prop_array));
	if (n == -1)
		panic("%s: could not get bus-range", __func__);
	if (n != sizeof(prop_array))
		panic("%s: broken bus-range (%d)", __func__, n);
	if (bootverbose)
		device_printf(dev, "bus range %u to %u; PCI bus %d\n",
		    prop_array[0], prop_array[1], prop_array[0]);
	sc->sc_pci_secbus = prop_array[0];

	/* Clear any pending PCI error bits. */
	PCIB_WRITE_CONFIG(dev, sc->sc_pci_secbus, STX_CS_DEVICE, STX_CS_FUNC,
	    PCIR_STATUS, PCIB_READ_CONFIG(dev, sc->sc_pci_secbus,
	    STX_CS_DEVICE, STX_CS_FUNC, PCIR_STATUS, 2), 2);
	SCHIZO_PCI_WRITE_8(sc, STX_PCI_CTRL,
	    SCHIZO_PCI_READ_8(sc, STX_PCI_CTRL));
	SCHIZO_PCI_WRITE_8(sc, STX_PCI_AFSR,
	    SCHIZO_PCI_READ_8(sc, STX_PCI_AFSR));

	/*
	 * Establish handlers for interesting interrupts...
	 * Someone at Sun clearly was smoking crack; with Schizos PCI
	 * bus error interrupts for one PBM can be routed to the other
	 * PBM though we obviously need to use the softc of the former
	 * as the argument for the interrupt handler and the softc of
	 * the latter as the argument for the interrupt controller.
	 */
	if (sc->sc_half == 0) {
		if ((ino_bitmap & (1ULL << STX_PCIERR_A_INO)) != 0 ||
		    (osc != NULL && ((struct schizo_icarg *)intr_vectors[
		    INTMAP_VEC(sc->sc_ign, STX_PCIERR_A_INO)].iv_icarg)->
		    sica_sc == osc))
			/*
			 * We are the driver for PBM A and either also
			 * registered the interrupt controller for us or
			 * the driver for PBM B has probed first and
			 * registered it for us.
			 */
			schizo_set_intr(sc, 0, STX_PCIERR_A_INO,
			    schizo_pci_bus);
		if ((ino_bitmap & (1ULL << STX_PCIERR_B_INO)) != 0 &&
		    osc != NULL)
			/*
			 * We are the driver for PBM A but registered
			 * the interrupt controller for PBM B, i.e. the
			 * driver for PBM B attached first but couldn't
			 * set up a handler for PBM B.
			 */
			schizo_set_intr(osc, 0, STX_PCIERR_B_INO,
			    schizo_pci_bus);
	} else {
		if ((ino_bitmap & (1ULL << STX_PCIERR_B_INO)) != 0 ||
		    (osc != NULL && ((struct schizo_icarg *)intr_vectors[
		    INTMAP_VEC(sc->sc_ign, STX_PCIERR_B_INO)].iv_icarg)->
		    sica_sc == osc))
			/*
			 * We are the driver for PBM B and either also
			 * registered the interrupt controller for us or
			 * the driver for PBM A has probed first and
			 * registered it for us.
			 */
			schizo_set_intr(sc, 0, STX_PCIERR_B_INO,
			    schizo_pci_bus);
		if ((ino_bitmap & (1ULL << STX_PCIERR_A_INO)) != 0 &&
		    osc != NULL)
			/*
			 * We are the driver for PBM B but registered
			 * the interrupt controller for PBM A, i.e. the
			 * driver for PBM A attached first but couldn't
			 * set up a handler for PBM A.
			 */
			schizo_set_intr(osc, 0, STX_PCIERR_A_INO,
			    schizo_pci_bus);
	}
	if ((ino_bitmap & (1ULL << STX_UE_INO)) != 0)
		schizo_set_intr(sc, 1, STX_UE_INO, schizo_ue);
	if ((ino_bitmap & (1ULL << STX_CE_INO)) != 0)
		schizo_set_intr(sc, 2, STX_CE_INO, schizo_ce);
	if ((ino_bitmap & (1ULL << STX_BUS_INO)) != 0)
		schizo_set_intr(sc, 3, STX_BUS_INO, schizo_host_bus);

	/*
	 * According to the Schizo Errata I-13, consistent DMA flushing/
	 * syncing is FUBAR in version < 5 (i.e. revision < 2.3) bridges,
	 * so we can't use it and need to live with the consequences.
	 * With Schizo version >= 5, CDMA flushing/syncing is usable
	 * but requires the the workaround described in Schizo Errata
	 * I-23.  With Tomatillo and XMITS, CDMA flushing/syncing works
	 * as expected, Tomatillo version <= 4 (i.e. revision <= 2.3)
	 * bridges additionally require a block store after a write to
	 * TOMXMS_PCI_DMA_SYNC_PEND though.
	 */
	if ((sc->sc_mode == SCHIZO_MODE_SCZ && sc->sc_ver >= 5) ||
	    sc->sc_mode == SCHIZO_MODE_TOM || sc->sc_mode == SCHIZO_MODE_XMS) {
		sc->sc_flags |= SCHIZO_FLAGS_CDMA;
		if (sc->sc_mode == SCHIZO_MODE_SCZ) {
			n = STX_CDMA_A_INO + sc->sc_half;
			if (bus_set_resource(dev, SYS_RES_IRQ, 5,
			    INTMAP_VEC(sc->sc_ign, n), 1) != 0)
				panic("%s: failed to add CDMA interrupt",
				    __func__);
			i = schizo_intr_register(sc, n);
			if (i != 0)
				panic("%s: could not register interrupt "
				    "controller for CDMA (%d)", __func__, i);
			(void)schizo_get_intrmap(sc, n, NULL,
			   &sc->sc_cdma_clr);
			sc->sc_cdma_state = SCHIZO_CDMA_STATE_DONE;
			schizo_set_intr(sc, 5, n, schizo_cdma);
		}
		if (sc->sc_mode == SCHIZO_MODE_TOM && sc->sc_ver <= 4)
			sc->sc_flags |= SCHIZO_FLAGS_BSWAR;
	}

	/*
	 * Set the latency timer register as this isn't always done by the
	 * firmware.
	 */
	PCIB_WRITE_CONFIG(dev, sc->sc_pci_secbus, STX_CS_DEVICE, STX_CS_FUNC,
	    PCIR_LATTIMER, OFW_PCI_LATENCY, 1);

	ofw_bus_setup_iinfo(node, &sc->sc_pci_iinfo, sizeof(ofw_pci_intr_t));

	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));
}

static void
schizo_set_intr(struct schizo_softc *sc, u_int index, u_int ino,
    driver_filter_t handler)
{
	u_long vec;
	int rid;

	rid = index;
	sc->sc_irq_res[index] = bus_alloc_resource_any(sc->sc_dev, SYS_RES_IRQ,
	    &rid, RF_ACTIVE);
	if (sc->sc_irq_res[index] == NULL ||
	    INTIGN(vec = rman_get_start(sc->sc_irq_res[index])) != sc->sc_ign ||
	    INTINO(vec) != ino ||
	    intr_vectors[vec].iv_ic != &schizo_ic ||
	    bus_setup_intr(sc->sc_dev, sc->sc_irq_res[index],
	    INTR_TYPE_MISC | INTR_FAST, handler, NULL, sc,
	    &sc->sc_ihand[index]) != 0)
		panic("%s: failed to set up interrupt %d", __func__, index);
}

static int
schizo_intr_register(struct schizo_softc *sc, u_int ino)
{
	struct schizo_icarg *sica;
	bus_addr_t intrclr, intrmap;
	int error;

	if (schizo_get_intrmap(sc, ino, &intrmap, &intrclr) == 0)
		return (ENXIO);
	sica = malloc(sizeof(*sica), M_DEVBUF, M_NOWAIT);
	if (sica == NULL)
		return (ENOMEM);
	sica->sica_sc = sc;
	sica->sica_map = intrmap;
	sica->sica_clr = intrclr;
#ifdef SCHIZO_DEBUG
	device_printf(sc->sc_dev, "intr map (INO %d) %#lx: %#lx, clr: %#lx\n",
	    ino, (u_long)intrmap, (u_long)SCHIZO_PCI_READ_8(sc, intrmap),
	    (u_long)intrclr);
#endif
	error = (intr_controller_register(INTMAP_VEC(sc->sc_ign, ino),
	    &schizo_ic, sica));
	if (error != 0)
		free(sica, M_DEVBUF);
	return (error);
}

static int
schizo_get_intrmap(struct schizo_softc *sc, u_int ino, bus_addr_t *intrmapptr,
    bus_addr_t *intrclrptr)
{
	bus_addr_t intrclr, intrmap;
	uint64_t mr;

	/*
	 * XXX we only look for INOs rather than INRs since the firmware
	 * may not provide the IGN and the IGN is constant for all devices
	 * on that PCI controller.
	 */

	if (ino > STX_MAX_INO) {
		device_printf(sc->sc_dev, "out of range INO %d requested\n",
		    ino);
		return (0);
	}

	intrmap = STX_PCI_IMAP_BASE + (ino << 3);
	intrclr = STX_PCI_ICLR_BASE + (ino << 3);
	mr = SCHIZO_PCI_READ_8(sc, intrmap);
	if (INTINO(mr) != ino) {
		device_printf(sc->sc_dev,
		    "interrupt map entry does not match INO (%d != %d)\n",
		    (int)INTINO(mr), ino);
		return (0);
	}

	if (intrmapptr != NULL)
		*intrmapptr = intrmap;
	if (intrclrptr != NULL)
		*intrclrptr = intrclr;
	return (1);
}

/*
 * Interrupt handlers
 */
static int
schizo_pci_bus(void *arg)
{
	struct schizo_softc *sc = arg;
	uint64_t afar, afsr, csr, iommu;
	uint32_t status;

	afar = SCHIZO_PCI_READ_8(sc, STX_PCI_AFAR);
	afsr = SCHIZO_PCI_READ_8(sc, STX_PCI_AFSR);
	csr = SCHIZO_PCI_READ_8(sc, STX_PCI_CTRL);
	iommu = SCHIZO_PCI_READ_8(sc, STX_PCI_IOMMU);
	status = PCIB_READ_CONFIG(sc->sc_dev, sc->sc_pci_secbus,
	    STX_CS_DEVICE, STX_CS_FUNC, PCIR_STATUS, 2);
	if ((csr & STX_PCI_CTRL_MMU_ERR) != 0) {
		if ((iommu & TOM_PCI_IOMMU_ERR) == 0)
			goto clear_error;

		/* These are non-fatal if target abort was signaled. */
		if ((status & PCIM_STATUS_STABORT) != 0 &&
		    ((iommu & TOM_PCI_IOMMU_ERRMASK) ==
		    TOM_PCI_IOMMU_INVALID_ERR ||
		    (iommu & TOM_PCI_IOMMU_ERR_ILLTSBTBW) != 0 ||
		    (iommu & TOM_PCI_IOMMU_ERR_BAD_VA) != 0)) {
			SCHIZO_PCI_WRITE_8(sc, STX_PCI_IOMMU, iommu);
			goto clear_error;
		}
	}

	panic("%s: PCI bus %c error AFAR %#llx AFSR %#llx PCI CSR %#llx "
	    "IOMMU %#llx STATUS %#llx", device_get_name(sc->sc_dev),
	    'A' + sc->sc_half, (unsigned long long)afar,
	    (unsigned long long)afsr, (unsigned long long)csr,
	    (unsigned long long)iommu, (unsigned long long)status);

 clear_error:
	if (bootverbose)
		device_printf(sc->sc_dev,
		    "PCI bus %c error AFAR %#llx AFSR %#llx PCI CSR %#llx "
		    "STATUS %#llx", 'A' + sc->sc_half,
		    (unsigned long long)afar, (unsigned long long)afsr,
		    (unsigned long long)csr, (unsigned long long)status);
	/* Clear the error bits that we caught. */
	PCIB_WRITE_CONFIG(sc->sc_dev, sc->sc_pci_secbus, STX_CS_DEVICE,
	    STX_CS_FUNC, PCIR_STATUS, status, 2);
	SCHIZO_PCI_WRITE_8(sc, STX_PCI_CTRL, csr);
	SCHIZO_PCI_WRITE_8(sc, STX_PCI_AFSR, afsr);
	return (FILTER_HANDLED);
}

static int
schizo_ue(void *arg)
{
	struct schizo_softc *sc = arg;
	uint64_t afar, afsr;
	int i;

	mtx_lock_spin(sc->sc_mtx);
	afar = SCHIZO_CTRL_READ_8(sc, STX_CTRL_UE_AFAR);
	for (i = 0; i < 1000; i++)
		if (((afsr = SCHIZO_CTRL_READ_8(sc, STX_CTRL_UE_AFSR)) &
		    STX_CTRL_CE_AFSR_ERRPNDG) == 0)
			break;
	mtx_unlock_spin(sc->sc_mtx);
	panic("%s: uncorrectable DMA error AFAR %#llx AFSR %#llx",
	    device_get_name(sc->sc_dev), (unsigned long long)afar,
	    (unsigned long long)afsr);
	return (FILTER_HANDLED);
}

static int
schizo_ce(void *arg)
{
	struct schizo_softc *sc = arg;
	uint64_t afar, afsr;
	int i;

	mtx_lock_spin(sc->sc_mtx);
	afar = SCHIZO_CTRL_READ_8(sc, STX_CTRL_CE_AFAR);
	for (i = 0; i < 1000; i++)
		if (((afsr = SCHIZO_CTRL_READ_8(sc, STX_CTRL_UE_AFSR)) &
		    STX_CTRL_CE_AFSR_ERRPNDG) == 0)
			break;
	device_printf(sc->sc_dev,
	    "correctable DMA error AFAR %#llx AFSR %#llx\n",
	    (unsigned long long)afar, (unsigned long long)afsr);
	/* Clear the error bits that we caught. */
	SCHIZO_CTRL_WRITE_8(sc, STX_CTRL_UE_AFSR, afsr);
	mtx_unlock_spin(sc->sc_mtx);
	return (FILTER_HANDLED);
}

static int
schizo_host_bus(void *arg)
{
	struct schizo_softc *sc = arg;
	uint64_t errlog;

	errlog = SCHIZO_CTRL_READ_8(sc, STX_CTRL_BUS_ERRLOG);
	panic("%s: %s error %#llx", device_get_name(sc->sc_dev),
	    sc->sc_mode == SCHIZO_MODE_TOM ? "JBus" : "Safari",
	    (unsigned long long)errlog);
	return (FILTER_HANDLED);
}

static int
schizo_cdma(void *arg)
{
	struct schizo_softc *sc = arg;

	atomic_store_rel_32(&sc->sc_cdma_state, SCHIZO_CDMA_STATE_DONE);
	return (FILTER_HANDLED);
}

static void
schizo_iommu_init(struct schizo_softc *sc, int tsbsize, uint32_t dvmabase)
{

	/* Punch in our copies. */
	sc->sc_is.is_bustag = rman_get_bustag(sc->sc_mem_res[STX_PCI]);
	sc->sc_is.is_bushandle = rman_get_bushandle(sc->sc_mem_res[STX_PCI]);
	sc->sc_is.is_iommu = STX_PCI_IOMMU;
	sc->sc_is.is_dtag = STX_PCI_IOMMU_TLB_TAG_DIAG;
	sc->sc_is.is_ddram = STX_PCI_IOMMU_TLB_DATA_DIAG;
	sc->sc_is.is_dqueue = STX_PCI_IOMMU_QUEUE_DIAG;
	sc->sc_is.is_dva = STX_PCI_IOMMU_SVADIAG;
	sc->sc_is.is_dtcmp = STX_PCI_IOMMU_TLB_CMP_DIAG;

	iommu_init(device_get_nameunit(sc->sc_dev), &sc->sc_is, tsbsize,
	    dvmabase, 0);
}

static int
schizo_maxslots(device_t dev)
{
	struct schizo_softc *sc;

	sc = device_get_softc(dev);
	if (sc->sc_mode == SCHIZO_MODE_SCZ)
		return (sc->sc_half == 0 ? 4 : 6);

	/* XXX: is this correct? */
	return (PCI_SLOTMAX);
}

static uint32_t
schizo_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int width)
{
	struct schizo_softc *sc;
	bus_space_handle_t bh;
	u_long offset = 0;
	uint32_t r, wrd;
	int i;
	uint16_t shrt;
	uint8_t byte;

	sc = device_get_softc(dev);

	/*
	 * The Schizo bridges contain a dupe of their header at 0x80.
	 */
	if (sc->sc_mode == SCHIZO_MODE_SCZ && bus == sc->sc_pci_secbus &&
	    slot == STX_CS_DEVICE && func == STX_CS_FUNC &&
	    reg + width > 0x80)
		return (0);

	offset = STX_CONF_OFF(bus, slot, func, reg);
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
		/* NOTREACHED */
	}

	if (i) {
#ifdef SCHIZO_DEBUG
		printf("%s: read data error reading: %d.%d.%d: 0x%x\n",
		    __func__, bus, slot, func, reg);
#endif
		r = -1;
	}
	return (r);
}

static void
schizo_write_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    uint32_t val, int width)
{
	struct schizo_softc *sc;
	bus_space_handle_t bh;
	u_long offset = 0;

	sc = device_get_softc(dev);
	offset = STX_CONF_OFF(bus, slot, func, reg);
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
		/* NOTREACHED */
	}
}

static int
schizo_route_interrupt(device_t bridge, device_t dev, int pin)
{
	struct schizo_softc *sc;
	struct ofw_pci_register reg;
	ofw_pci_intr_t pintr, mintr;
	uint8_t maskbuf[sizeof(reg) + sizeof(pintr)];

	sc = device_get_softc(bridge);
	pintr = pin;
	if (ofw_bus_lookup_imap(ofw_bus_get_node(dev), &sc->sc_pci_iinfo, &reg,
	    sizeof(reg), &pintr, sizeof(pintr), &mintr, sizeof(mintr), maskbuf))
		return (mintr);

	device_printf(bridge, "could not route pin %d for device %d.%d\n",
	    pin, pci_get_slot(dev), pci_get_function(dev));
	return (PCI_INVALID_IRQ);
}

static int
schizo_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct schizo_softc *sc;

	sc = device_get_softc(dev);
	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = device_get_unit(dev);
		return (0);
	case PCIB_IVAR_BUS:
		*result = sc->sc_pci_secbus;
		return (0);
	}
	return (ENOENT);
}

static int
schizo_dma_sync_stub(void *arg)
{
	struct timeval cur, end;
	struct schizo_dma_sync *sds = arg;
	struct schizo_softc *sc = sds->sds_sc;
	uint32_t state;

	(void)PCIB_READ_CONFIG(sds->sds_ppb, sds->sds_bus, sds->sds_slot,
	    sds->sds_func, PCIR_VENDOR, 2);
	for (; atomic_cmpset_acq_32(&sc->sc_cdma_state, SCHIZO_CDMA_STATE_DONE,
	    SCHIZO_CDMA_STATE_PENDING) == 0;)
		;
	SCHIZO_PCI_WRITE_8(sc, sc->sc_cdma_clr, 1);
	microuptime(&cur);
	end.tv_sec = 1;
	end.tv_usec = 0;
	timevaladd(&end, &cur);
	for (; (state = atomic_load_32(&sc->sc_cdma_state)) !=
	    SCHIZO_CDMA_STATE_DONE && timevalcmp(&cur, &end, <=);)
		microuptime(&cur);
	if (state != SCHIZO_CDMA_STATE_DONE)
		panic("%s: DMA does not sync", __func__);
	return (sds->sds_handler(sds->sds_arg));
}

#define	VIS_BLOCKSIZE	64

static int
ichip_dma_sync_stub(void *arg)
{
	static u_char buf[VIS_BLOCKSIZE] __aligned(VIS_BLOCKSIZE);
	struct timeval cur, end;
	struct schizo_dma_sync *sds = arg;
	struct schizo_softc *sc = sds->sds_sc;
	register_t reg, s;

	(void)PCIB_READ_CONFIG(sds->sds_ppb, sds->sds_bus, sds->sds_slot,
	    sds->sds_func, PCIR_VENDOR, 2);
	SCHIZO_PCI_WRITE_8(sc, TOMXMS_PCI_DMA_SYNC_PEND, sds->sds_syncval);
	microuptime(&cur);
	end.tv_sec = 1;
	end.tv_usec = 0;
	timevaladd(&end, &cur);
	for (; ((reg = SCHIZO_PCI_READ_8(sc, TOMXMS_PCI_DMA_SYNC_PEND)) &
	    sds->sds_syncval) != 0 && timevalcmp(&cur, &end, <=);)
		microuptime(&cur);
	if ((reg & sds->sds_syncval) != 0)
		panic("%s: DMA does not sync", __func__);

	if ((sc->sc_flags & SCHIZO_FLAGS_BSWAR) != 0) {
		s = intr_disable();
		reg = rd(fprs);
		wr(fprs, reg | FPRS_FEF, 0);
		__asm __volatile("stda %%f0, [%0] %1"
		    : : "r" (buf), "n" (ASI_BLK_COMMIT_S));
		membar(Sync);
		wr(fprs, reg, 0);
		intr_restore(s);
	}
	return (sds->sds_handler(sds->sds_arg));
}

static void
schizo_intr_enable(void *arg)
{
	struct intr_vector *iv = arg;
	struct schizo_icarg *sica = iv->iv_icarg;

	SCHIZO_PCI_WRITE_8(sica->sica_sc, sica->sica_map,
	    INTMAP_ENABLE(iv->iv_vec, iv->iv_mid));
}

static void
schizo_intr_disable(void *arg)
{
	struct intr_vector *iv = arg;
	struct schizo_icarg *sica = iv->iv_icarg;

	SCHIZO_PCI_WRITE_8(sica->sica_sc, sica->sica_map, iv->iv_vec);
}

static void
schizo_intr_assign(void *arg)
{
	struct intr_vector *iv = arg;
	struct schizo_icarg *sica = iv->iv_icarg;

	SCHIZO_PCI_WRITE_8(sica->sica_sc, sica->sica_map, INTMAP_TID(
	    SCHIZO_PCI_READ_8(sica->sica_sc, sica->sica_map), iv->iv_mid));
}

static void
schizo_intr_clear(void *arg)
{
	struct intr_vector *iv = arg;
	struct schizo_icarg *sica = iv->iv_icarg;

	SCHIZO_PCI_WRITE_8(sica->sica_sc, sica->sica_clr, 0);
}

static int
schizo_setup_intr(device_t dev, device_t child, struct resource *ires,
    int flags, driver_filter_t *filt, driver_intr_t *intr, void *arg,
    void **cookiep)
{
	devclass_t pci_devclass;
	device_t cdev, pdev, pcidev;
	struct schizo_dma_sync *sds;
	struct schizo_softc *sc;
	u_long vec;
	int error, found;

	sc = device_get_softc(dev);
	/*
	 * Make sure the vector is fully specified.
	 */
	vec = rman_get_start(ires);
	if (INTIGN(vec) != sc->sc_ign) {
		device_printf(dev, "invalid interrupt vector 0x%lx\n", vec);
		return (EINVAL);
	}

	if (intr_vectors[vec].iv_ic == &schizo_ic) {
		/*
		 * Ensure we use the right softc in case the interrupt
		 * is routed to our companion PBM for some odd reason.
		 */
		sc = ((struct schizo_icarg *)intr_vectors[vec].iv_icarg)->
		    sica_sc;
	} else if (intr_vectors[vec].iv_ic == NULL) {
		/*
		 * Work around broken firmware which misses entries in
		 * the ino-bitmap.
		 */
		error = schizo_intr_register(sc, INTINO(vec));
		if (error != 0) {
			device_printf(dev, "could not register interrupt "
			    "controller for vector 0x%lx (%d)\n", vec, error);
			return (error);
		}
		if (bootverbose)
			device_printf(dev, "belatedly registered as "
			    "interrupt controller for vector 0x%lx\n", vec);
	} else {
		device_printf(dev,
		    "invalid interrupt controller for vector 0x%lx\n", vec);
		return (EINVAL);
	}

	/*
	 * Install a a wrapper for CDMA flushing/syncing for devices
	 * behind PCI-PCI bridges if possible.
	 */
	pcidev = NULL;
	found = 0;
	pci_devclass = devclass_find("pci");
	for (cdev = child; cdev != dev; cdev = pdev) {
		pdev = device_get_parent(cdev);
		if (pcidev == NULL) {
			if (device_get_devclass(pdev) != pci_devclass)
				continue;
			pcidev = cdev;
			continue;
		}
		if (pci_get_class(cdev) == PCIC_BRIDGE &&
		    pci_get_subclass(cdev) == PCIS_BRIDGE_PCI)
			found = 1;
	}
	if ((sc->sc_flags & SCHIZO_FLAGS_CDMA) != 0) {
		sds = malloc(sizeof(*sds), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (sds == NULL)
			return (ENOMEM);
		if (found != 0 && pcidev != NULL) {
			sds->sds_sc = sc;
			sds->sds_arg = arg;
			sds->sds_ppb =
			    device_get_parent(device_get_parent(pcidev));
			sds->sds_bus = pci_get_bus(pcidev);
			sds->sds_slot = pci_get_slot(pcidev);
			sds->sds_func = pci_get_function(pcidev);
			sds->sds_syncval = 1ULL << INTINO(vec);
			if (bootverbose)
				device_printf(dev, "installed DMA sync "
				    "wrapper for device %d.%d on bus %d\n",
				    sds->sds_slot, sds->sds_func,
				    sds->sds_bus);

#define	DMA_SYNC_STUB							\
	(sc->sc_mode == SCHIZO_MODE_SCZ ? schizo_dma_sync_stub :	\
	ichip_dma_sync_stub)

			if (intr == NULL) {
				sds->sds_handler = filt;
				error = bus_generic_setup_intr(dev, child,
				    ires, flags, DMA_SYNC_STUB, intr, sds,
				    cookiep);
			} else {
				sds->sds_handler = (driver_filter_t *)intr;
				error = bus_generic_setup_intr(dev, child,
				    ires, flags, filt, (driver_intr_t *)
				    DMA_SYNC_STUB, sds, cookiep);
			}

#undef DMA_SYNC_STUB

		} else
			error = bus_generic_setup_intr(dev, child, ires,
			    flags, filt, intr, arg, cookiep);
		if (error != 0) {
			free(sds, M_DEVBUF);
			return (error);
		}
		sds->sds_cookie = *cookiep;
		*cookiep = sds;
		return (error);
	} else if (found != 0)
		device_printf(dev, "WARNING: using devices behind PCI-PCI "
		    "bridges may cause data corruption\n");
	return (bus_generic_setup_intr(dev, child, ires, flags, filt, intr,
	    arg, cookiep));
}

static int
schizo_teardown_intr(device_t dev, device_t child, struct resource *vec,
    void *cookie)
{
	struct schizo_dma_sync *sds;
	struct schizo_softc *sc;
	int error;

	sc = device_get_softc(dev);
	if ((sc->sc_flags & SCHIZO_FLAGS_CDMA) != 0) {
		sds = cookie;
		error = bus_generic_teardown_intr(dev, child, vec,
		    sds->sds_cookie);
		if (error == 0)
			free(sds, M_DEVBUF);
		return (error);
	}
	return (bus_generic_teardown_intr(dev, child, vec, cookie));
}

static struct resource *
schizo_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct schizo_softc *sc;
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
		 * interrupts.  The other case should not happen with
		 * the MI PCI code...
		 * XXX: This may return a resource that is out of the
		 * range that was specified.  Is this correct...?
		 */
		if (start != end)
			panic("%s: XXX: interrupt range", __func__);
		start = end = INTMAP_VEC(sc->sc_ign, end);
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
		/* NOTREACHED */
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL)
		return (NULL);
	rman_set_rid(rv, *rid);
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
schizo_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	void *p;
	int error;

	if (type == SYS_RES_IRQ)
		return (BUS_ACTIVATE_RESOURCE(device_get_parent(bus), child,
		    type, rid, r));
	if (type == SYS_RES_MEMORY) {
		/*
		 * Need to memory-map the device space, as some drivers
		 * depend on the virtual address being set and usable.
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
schizo_deactivate_resource(device_t bus, device_t child, int type, int rid,
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
schizo_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	int error;

	if (type == SYS_RES_IRQ)
		return (BUS_RELEASE_RESOURCE(device_get_parent(bus), child,
		    type, rid, r));
	if (rman_get_flags(r) & RF_ACTIVE) {
		error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return (error);
	}
	return (rman_release_resource(r));
}

static bus_dma_tag_t
schizo_get_dma_tag(device_t bus, device_t child)
{
	struct schizo_softc *sc;

	sc = device_get_softc(bus);
	return (sc->sc_pci_dmat);
}

static phandle_t
schizo_get_node(device_t bus, device_t dev)
{
	struct schizo_softc *sc;

	sc = device_get_softc(bus);
	/* We only have one child, the PCI bus, which needs our own node. */
	return (sc->sc_node);
}

static bus_space_tag_t
schizo_alloc_bus_tag(struct schizo_softc *sc, int type)
{
	bus_space_tag_t bt;

	bt = (bus_space_tag_t)malloc(sizeof(struct bus_space_tag), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (bt == NULL)
		panic("%s: out of memory", __func__);

	bt->bst_cookie = sc;
	bt->bst_parent = rman_get_bustag(sc->sc_mem_res[STX_PCI]);
	bt->bst_type = type;
	return (bt);
}

static u_int
schizo_get_timecount(struct timecounter *tc)
{
	struct schizo_softc *sc;

	sc = tc->tc_priv;
	return (SCHIZO_CTRL_READ_8(sc, STX_CTRL_PERF_CNT) &
	    (STX_CTRL_PERF_CNT_MASK << STX_CTRL_PERF_CNT_CNT0_SHIFT));
}
