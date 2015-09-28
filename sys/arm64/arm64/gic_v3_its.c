/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitset.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/pciio.h>
#include <sys/pcpu.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/intr.h>

#include "gic_v3_reg.h"
#include "gic_v3_var.h"

#include "pic_if.h"

/* Device and PIC methods */
static int gic_v3_its_attach(device_t);

static device_method_t gic_v3_its_methods[] = {
	/* Device interface */
	DEVMETHOD(device_attach,	gic_v3_its_attach),
	/*
	 * PIC interface
	 */
	/* MSI-X */
	DEVMETHOD(pic_alloc_msix,	gic_v3_its_alloc_msix),
	DEVMETHOD(pic_map_msix,		gic_v3_its_map_msix),
	/* MSI */
	DEVMETHOD(pic_alloc_msi,	gic_v3_its_alloc_msi),
	DEVMETHOD(pic_map_msi,		gic_v3_its_map_msix),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_0(gic_v3_its, gic_v3_its_driver, gic_v3_its_methods,
    sizeof(struct gic_v3_its_softc));

MALLOC_DEFINE(M_GIC_V3_ITS, "GICv3 ITS", GIC_V3_ITS_DEVSTR);

static int its_alloc_tables(struct gic_v3_its_softc *);
static void its_free_tables(struct gic_v3_its_softc *);
static void its_init_commandq(struct gic_v3_its_softc *);
static void its_init_cpu_collection(struct gic_v3_its_softc *);
static uint32_t its_get_devid(device_t);

static int its_cmd_send(struct gic_v3_its_softc *, struct its_cmd_desc *);

static void its_cmd_mapc(struct gic_v3_its_softc *, struct its_col *, uint8_t);
static void its_cmd_mapvi(struct gic_v3_its_softc *, struct its_dev *, uint32_t,
    uint32_t);
static void its_cmd_mapi(struct gic_v3_its_softc *, struct its_dev *, uint32_t);
static void its_cmd_inv(struct gic_v3_its_softc *, struct its_dev *, uint32_t);
static void its_cmd_invall(struct gic_v3_its_softc *, struct its_col *);

static uint32_t its_get_devbits(device_t);

static void lpi_init_conftable(struct gic_v3_its_softc *);
static void lpi_bitmap_init(struct gic_v3_its_softc *);
static int lpi_config_cpu(struct gic_v3_its_softc *);
static void lpi_alloc_cpu_pendtables(struct gic_v3_its_softc *);

const char *its_ptab_cache[] = {
	[GITS_BASER_CACHE_NCNB] = "(NC,NB)",
	[GITS_BASER_CACHE_NC] = "(NC)",
	[GITS_BASER_CACHE_RAWT] = "(RA,WT)",
	[GITS_BASER_CACHE_RAWB] = "(RA,WB)",
	[GITS_BASER_CACHE_WAWT] = "(WA,WT)",
	[GITS_BASER_CACHE_WAWB] = "(WA,WB)",
	[GITS_BASER_CACHE_RAWAWT] = "(RAWA,WT)",
	[GITS_BASER_CACHE_RAWAWB] = "(RAWA,WB)",
};

const char *its_ptab_share[] = {
	[GITS_BASER_SHARE_NS] = "none",
	[GITS_BASER_SHARE_IS] = "inner",
	[GITS_BASER_SHARE_OS] = "outer",
	[GITS_BASER_SHARE_RES] = "none",
};

const char *its_ptab_type[] = {
	[GITS_BASER_TYPE_UNIMPL] = "Unimplemented",
	[GITS_BASER_TYPE_DEV] = "Devices",
	[GITS_BASER_TYPE_VP] = "Virtual Processors",
	[GITS_BASER_TYPE_PP] = "Physical Processors",
	[GITS_BASER_TYPE_IC] = "Interrupt Collections",
	[GITS_BASER_TYPE_RES5] = "Reserved (5)",
	[GITS_BASER_TYPE_RES6] = "Reserved (6)",
	[GITS_BASER_TYPE_RES7] = "Reserved (7)",
};

/*
 * Vendor specific quirks.
 * One needs to add appropriate entry to its_quirks[]
 * table if the imlementation varies from the generic ARM ITS.
 */

/* Cavium ThunderX PCI devid acquire function */
static uint32_t its_get_devbits_thunder(device_t);
static uint32_t its_get_devid_thunder(device_t);

static const struct its_quirks its_quirks[] = {
	{
		/*
		 * Hardware:		Cavium ThunderX
		 * Chip revision:	Pass 1.0, Pass 1.1
		 */
		.cpuid =	CPU_ID_RAW(CPU_IMPL_CAVIUM, CPU_PART_THUNDER, 0, 0),
		.cpuid_mask =	CPU_IMPL_MASK | CPU_PART_MASK,
		.devid_func =	its_get_devid_thunder,
		.devbits_func =	its_get_devbits_thunder,
	},
};

static struct gic_v3_its_softc *its_sc;

#define	gic_its_read(sc, len, reg)		\
    bus_read_##len(&sc->its_res[0], reg)

#define	gic_its_write(sc, len, reg, val)	\
    bus_write_##len(&sc->its_res[0], reg, val)

static int
gic_v3_its_attach(device_t dev)
{
	struct gic_v3_its_softc *sc;
	uint64_t gits_tmp;
	uint32_t gits_pidr2;
	int rid;
	int ret;

	sc = device_get_softc(dev);

	/*
	 * XXX ARM64TODO: Avoid configuration of more than one ITS
	 * device. To be removed when multi-PIC support is added
	 * to FreeBSD (or at least multi-ITS is implemented). Limit
	 * supported ITS sockets to '0' only.
	 */
	if (device_get_unit(dev) != 0) {
		device_printf(dev,
		    "Only single instance of ITS is supported, exitting...\n");
		return (ENXIO);
	}
	sc->its_socket = 0;

	/*
	 * Initialize sleep & spin mutex for ITS
	 */
	/* Protects ITS device list and assigned LPIs bitmaps. */
	mtx_init(&sc->its_mtx, "ITS sleep lock", NULL, MTX_DEF);
	/* Protects access to ITS command circular buffer. */
	mtx_init(&sc->its_spin_mtx, "ITS spin lock", NULL, MTX_SPIN);

	rid = 0;
	sc->its_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->its_res == NULL) {
		device_printf(dev, "Could not allocate memory\n");
		return (ENXIO);
	}

	sc->dev = dev;

	gits_pidr2 = gic_its_read(sc, 4, GITS_PIDR2);
	switch (gits_pidr2 & GITS_PIDR2_ARCH_MASK) {
	case GITS_PIDR2_ARCH_GICv3: /* fall through */
	case GITS_PIDR2_ARCH_GICv4:
		if (bootverbose) {
			device_printf(dev, "ITS found. Architecture rev. %u\n",
			    (u_int)(gits_pidr2 & GITS_PIDR2_ARCH_MASK) >> 4);
		}
		break;
	default:
		device_printf(dev, "No ITS found in the system\n");
		gic_v3_its_detach(dev);
		return (ENODEV);
	}

	/* 1. Initialize commands queue */
	its_init_commandq(sc);

	/* 2. Provide memory for any private ITS tables */
	ret = its_alloc_tables(sc);
	if (ret != 0) {
		gic_v3_its_detach(dev);
		return (ret);
	}

	/* 3. Allocate collections. One per-CPU */
	for (int cpu = 0; cpu < mp_ncpus; cpu++)
		if (CPU_ISSET(cpu, &all_cpus) != 0)
			sc->its_cols[cpu] = malloc(sizeof(*sc->its_cols[0]),
				M_GIC_V3_ITS, (M_WAITOK | M_ZERO));
		else
			sc->its_cols[cpu] = NULL;

	/* 4. Enable ITS in GITS_CTLR */
	gits_tmp = gic_its_read(sc, 4, GITS_CTLR);
	gic_its_write(sc, 4, GITS_CTLR, gits_tmp | GITS_CTLR_EN);

	/* 5. Initialize LPIs configuration table */
	lpi_init_conftable(sc);

	/* 6. LPIs bitmap init */
	lpi_bitmap_init(sc);

	/* 7. Allocate pending tables for all CPUs */
	lpi_alloc_cpu_pendtables(sc);

	/* 8. CPU init */
	(void)its_init_cpu(sc);

	/* 9. Init ITS devices list */
	TAILQ_INIT(&sc->its_dev_list);

	arm_register_msi_pic(dev);

	/*
	 * XXX ARM64TODO: We need to have ITS software context
	 * when being called by the interrupt code (mask/unmask).
	 * This may be used only when one ITS is present in
	 * the system and eventually should be removed.
	 */
	KASSERT(its_sc == NULL,
	    ("Trying to assign its_sc that is already set"));
	its_sc = sc;

	return (0);
}

/* Will not detach but use it for convenience */
int
gic_v3_its_detach(device_t dev)
{
	device_t parent;
	struct gic_v3_softc *gic_sc;
	struct gic_v3_its_softc *sc;
	u_int cpuid;
	int rid = 0;

	sc = device_get_softc(dev);
	cpuid = PCPU_GET(cpuid);

	/* Release what's possible */

	/* Command queue */
	if ((void *)sc->its_cmdq_base != NULL) {
		contigfree((void *)sc->its_cmdq_base,
		    ITS_CMDQ_SIZE, M_GIC_V3_ITS);
	}
	/* ITTs */
	its_free_tables(sc);
	/* Collections */
	for (cpuid = 0; cpuid < mp_ncpus; cpuid++)
		free(sc->its_cols[cpuid], M_GIC_V3_ITS);
	/* LPI config table */
	parent = device_get_parent(sc->dev);
	gic_sc = device_get_softc(parent);
	if ((void *)gic_sc->gic_redists.lpis.conf_base != NULL) {
		contigfree((void *)gic_sc->gic_redists.lpis.conf_base,
		    LPI_CONFTAB_SIZE, M_GIC_V3_ITS);
	}
	for (cpuid = 0; cpuid < mp_ncpus; cpuid++)
		if ((void *)gic_sc->gic_redists.lpis.pend_base[cpuid] != NULL) {
			contigfree(
			    (void *)gic_sc->gic_redists.lpis.pend_base[cpuid],
			    roundup2(LPI_PENDTAB_SIZE, PAGE_SIZE_64K),
			    M_GIC_V3_ITS);
		}

	/* Resource... */
	bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->its_res);

	/* XXX ARM64TODO: Reset global pointer to ITS software context */
	its_sc = NULL;

	return (0);
}

static int
its_alloc_tables(struct gic_v3_its_softc *sc)
{
	uint64_t gits_baser, gits_tmp;
	uint64_t type, esize, cache, share, psz;
	size_t page_size, npages, nitspages, nidents, tn;
	size_t its_tbl_size;
	vm_offset_t ptab_vaddr;
	vm_paddr_t ptab_paddr;
	boolean_t first = TRUE;

	page_size = PAGE_SIZE_64K;

	for (tn = 0; tn < GITS_BASER_NUM; tn++) {
		gits_baser = gic_its_read(sc, 8, GITS_BASER(tn));
		type = GITS_BASER_TYPE(gits_baser);
		/* Get the Table Entry size */
		esize = GITS_BASER_ESIZE(gits_baser);

		switch (type) {
		case GITS_BASER_TYPE_UNIMPL: /* fall through */
		case GITS_BASER_TYPE_RES5:
		case GITS_BASER_TYPE_RES6:
		case GITS_BASER_TYPE_RES7:
			continue;
		case GITS_BASER_TYPE_DEV:
			nidents = (1 << its_get_devbits(sc->dev));
			its_tbl_size = esize * nidents;
			its_tbl_size = roundup2(its_tbl_size, page_size);
			npages = howmany(its_tbl_size, PAGE_SIZE);
			break;
		default:
			npages = howmany(page_size, PAGE_SIZE);
			break;
		}

		/* Allocate required space */
		ptab_vaddr = (vm_offset_t)contigmalloc(npages * PAGE_SIZE,
		    M_GIC_V3_ITS, (M_WAITOK | M_ZERO), 0, ~0UL, PAGE_SIZE, 0);

		sc->its_ptabs[tn].ptab_vaddr = ptab_vaddr;
		sc->its_ptabs[tn].ptab_pgsz = PAGE_SIZE;
		sc->its_ptabs[tn].ptab_npages = npages;

		ptab_paddr = vtophys(ptab_vaddr);
		KASSERT((ptab_paddr & GITS_BASER_PA_MASK) == ptab_paddr,
		    ("%s: Unaligned PA for Interrupt Translation Table",
		    device_get_name(sc->dev)));

		/* Set defaults: WAWB, IS */
		cache = GITS_BASER_CACHE_WAWB;
		share = GITS_BASER_SHARE_IS;

		for (;;) {
			nitspages = howmany(its_tbl_size, page_size);

			switch (page_size) {
			case PAGE_SIZE:		/* 4KB */
				psz = GITS_BASER_PSZ_4K;
				break;
			case PAGE_SIZE_16K:	/* 16KB */
				psz = GITS_BASER_PSZ_4K;
				break;
			case PAGE_SIZE_64K:	/* 64KB */
				psz = GITS_BASER_PSZ_64K;
				break;
			default:
				device_printf(sc->dev,
				    "Unsupported page size: %zuKB\n",
				    (page_size / 1024));
				its_free_tables(sc);
				return (ENXIO);
			}

			/* Clear fields under modification first */
			gits_baser &= ~(GITS_BASER_VALID |
			    GITS_BASER_CACHE_MASK | GITS_BASER_TYPE_MASK |
			    GITS_BASER_ESIZE_MASK | GITS_BASER_PA_MASK |
			    GITS_BASER_SHARE_MASK | GITS_BASER_PSZ_MASK |
			    GITS_BASER_SIZE_MASK);
			/* Construct register value */
			gits_baser |=
			    (type << GITS_BASER_TYPE_SHIFT) |
			    ((esize - 1) << GITS_BASER_ESIZE_SHIFT) |
			    (cache << GITS_BASER_CACHE_SHIFT) |
			    (share << GITS_BASER_SHARE_SHIFT) |
			    (psz << GITS_BASER_PSZ_SHIFT) |
			    ptab_paddr | (nitspages - 1) |
			    GITS_BASER_VALID;

			gic_its_write(sc, 8, GITS_BASER(tn), gits_baser);
			/*
			 * Verify.
			 * Depending on implementation we may encounter
			 * shareability and page size mismatch.
			 */
			gits_tmp = gic_its_read(sc, 8, GITS_BASER(tn));
			if (((gits_tmp ^ gits_baser) & GITS_BASER_SHARE_MASK) != 0) {
				share = gits_tmp & GITS_BASER_SHARE_MASK;
				share >>= GITS_BASER_SHARE_SHIFT;
				continue;
			}

			if (((gits_tmp ^ gits_baser) & GITS_BASER_PSZ_MASK) != 0) {
				switch (page_size) {
				case PAGE_SIZE_16K:
					/* Drop to 4KB page */
					page_size = PAGE_SIZE;
					continue;
				case PAGE_SIZE_64K:
					/* Drop to 16KB page */
					page_size = PAGE_SIZE_16K;
					continue;
				}
			}
			/*
			 * All possible adjustments should
			 * be applied by now so just break the loop.
			 */
			break;
		}
		/*
		 * Do not compare Cacheability field since
		 * it is implementation defined.
		 */
		gits_tmp &= ~GITS_BASER_CACHE_MASK;
		gits_baser &= ~GITS_BASER_CACHE_MASK;

		if (gits_tmp != gits_baser) {
			device_printf(sc->dev,
			    "Could not allocate ITS tables\n");
			its_free_tables(sc);
			return (ENXIO);
		}

		if (bootverbose) {
			if (first) {
				device_printf(sc->dev,
				    "Allocated ITS private tables:\n");
				first = FALSE;
			}
			device_printf(sc->dev,
			    "\tPTAB%zu for %s: PA 0x%lx,"
			    " %lu entries,"
			    " cache policy %s, %s shareable,"
			    " page size %zuKB\n",
			    tn, its_ptab_type[type], ptab_paddr,
			    (page_size * nitspages) / esize,
			    its_ptab_cache[cache], its_ptab_share[share],
			    page_size / 1024);
		}
	}

	return (0);
}

static void
its_free_tables(struct gic_v3_its_softc *sc)
{
	vm_offset_t ptab_vaddr;
	size_t size;
	size_t tn;

	for (tn = 0; tn < GITS_BASER_NUM; tn++) {
		ptab_vaddr = sc->its_ptabs[tn].ptab_vaddr;
		if (ptab_vaddr == 0)
			continue;
		size = sc->its_ptabs[tn].ptab_pgsz;
		size *= sc->its_ptabs[tn].ptab_npages;

		if ((void *)ptab_vaddr != NULL)
			contigfree((void *)ptab_vaddr, size, M_GIC_V3_ITS);

		/* Clear the table description */
		memset(&sc->its_ptabs[tn], 0, sizeof(sc->its_ptabs[tn]));
	}
}

static void
its_init_commandq(struct gic_v3_its_softc *sc)
{
	uint64_t gits_cbaser, gits_tmp;
	uint64_t cache, share;
	vm_paddr_t cmdq_paddr;
	device_t dev;

	dev = sc->dev;
	/* Allocate memory for command queue */
	sc->its_cmdq_base = contigmalloc(ITS_CMDQ_SIZE, M_GIC_V3_ITS,
	    (M_WAITOK | M_ZERO), 0, ~0UL, ITS_CMDQ_SIZE, 0);
	/* Set command queue write pointer (command queue empty) */
	sc->its_cmdq_write = sc->its_cmdq_base;

	/* Save command queue pointer and attributes */
	cmdq_paddr = vtophys(sc->its_cmdq_base);

	/* Set defaults: Normal Inner WAWB, IS */
	cache = GITS_CBASER_CACHE_NIWAWB;
	share = GITS_CBASER_SHARE_IS;

	gits_cbaser = (cmdq_paddr |
	    (cache << GITS_CBASER_CACHE_SHIFT) |
	    (share << GITS_CBASER_SHARE_SHIFT) |
	    /* Number of 4KB pages - 1 */
	    ((ITS_CMDQ_SIZE / PAGE_SIZE) - 1) |
	    /* Valid bit */
	    GITS_CBASER_VALID);

	gic_its_write(sc, 8, GITS_CBASER, gits_cbaser);
	gits_tmp = gic_its_read(sc, 8, GITS_CBASER);

	if (((gits_tmp ^ gits_cbaser) & GITS_CBASER_SHARE_MASK) != 0) {
		if (bootverbose) {
			device_printf(dev,
			    "Will use cache flushing for commands queue\n");
		}
		/* Command queue needs cache flushing */
		sc->its_flags |= ITS_FLAGS_CMDQ_FLUSH;
	}

	gic_its_write(sc, 8, GITS_CWRITER, 0x0);
}

int
its_init_cpu(struct gic_v3_its_softc *sc)
{
	device_t parent;
	struct gic_v3_softc *gic_sc;

	/*
	 * NULL in place of the softc pointer means that
	 * this function was called during GICv3 secondary initialization.
	 */
	if (sc == NULL) {
		if (device_is_attached(its_sc->dev)) {
			/*
			 * XXX ARM64TODO: This is part of the workaround that
			 * saves ITS software context for further use in
			 * mask/unmask and here. This should be removed as soon
			 * as the upper layer is capable of passing the ITS
			 * context to this function.
			 */
			sc = its_sc;
		} else
			return (ENXIO);

		/* Skip if running secondary init on a wrong socket */
		if (sc->its_socket != CPU_CURRENT_SOCKET)
			return (ENXIO);
	}

	/*
	 * Check for LPIs support on this Re-Distributor.
	 */
	parent = device_get_parent(sc->dev);
	gic_sc = device_get_softc(parent);
	if ((gic_r_read(gic_sc, 4, GICR_TYPER) & GICR_TYPER_PLPIS) == 0) {
		if (bootverbose) {
			device_printf(sc->dev,
			    "LPIs not supported on CPU%u\n", PCPU_GET(cpuid));
		}
		return (ENXIO);
	}

	/* Configure LPIs for this CPU */
	lpi_config_cpu(sc);

	/* Initialize collections */
	its_init_cpu_collection(sc);

	return (0);
}

static void
its_init_cpu_collection(struct gic_v3_its_softc *sc)
{
	device_t parent;
	struct gic_v3_softc *gic_sc;
	uint64_t typer;
	uint64_t target;
	vm_offset_t redist_base;
	u_int cpuid;

	cpuid = PCPU_GET(cpuid);
	parent = device_get_parent(sc->dev);
	gic_sc = device_get_softc(parent);

	typer = gic_its_read(sc, 8, GITS_TYPER);
	if ((typer & GITS_TYPER_PTA) != 0) {
		redist_base =
		    rman_get_bushandle(gic_sc->gic_redists.pcpu[cpuid]);
		/*
		 * Target Address correspond to the base physical
		 * address of Re-Distributors.
		 */
		target = vtophys(redist_base);
	} else {
		/* Target Address correspond to unique processor numbers */
		typer = gic_r_read(gic_sc, 8, GICR_TYPER);
		target = GICR_TYPER_CPUNUM(typer);
	}

	sc->its_cols[cpuid]->col_target = target;
	sc->its_cols[cpuid]->col_id = cpuid;

	its_cmd_mapc(sc, sc->its_cols[cpuid], 1);
	its_cmd_invall(sc, sc->its_cols[cpuid]);

}

static void
lpi_init_conftable(struct gic_v3_its_softc *sc)
{
	device_t parent;
	struct gic_v3_softc *gic_sc;
	vm_offset_t conf_base;
	uint8_t prio_default;

	parent = device_get_parent(sc->dev);
	gic_sc = device_get_softc(parent);
	/*
	 * LPI Configuration Table settings.
	 * Notice that Configuration Table is shared among all
	 * Re-Distributors, so this is going to be created just once.
	 */
	conf_base = (vm_offset_t)contigmalloc(LPI_CONFTAB_SIZE,
	    M_GIC_V3_ITS, (M_WAITOK | M_ZERO), 0, ~0UL, PAGE_SIZE_64K, 0);

	if (bootverbose) {
		device_printf(sc->dev,
		    "LPI Configuration Table at PA: 0x%lx\n",
		    vtophys(conf_base));
	}

	/*
	 * Let the default priority be aligned with all other
	 * interrupts assuming that each interrupt is assigned
	 * MAX priority at startup. MAX priority on the other
	 * hand cannot be higher than 0xFC for LPIs.
	 */
	prio_default = GIC_PRIORITY_MAX;

	/* Write each settings byte to LPI configuration table */
	memset((void *)conf_base,
	    (prio_default & LPI_CONF_PRIO_MASK) | LPI_CONF_GROUP1,
	    LPI_CONFTAB_SIZE);

	cpu_dcache_wb_range((vm_offset_t)conf_base, roundup2(LPI_CONFTAB_SIZE,
	    PAGE_SIZE_64K));

	gic_sc->gic_redists.lpis.conf_base = conf_base;
}

static void
lpi_alloc_cpu_pendtables(struct gic_v3_its_softc *sc)
{
	device_t parent;
	struct gic_v3_softc *gic_sc;
	vm_offset_t pend_base;
	u_int cpuid;

	parent = device_get_parent(sc->dev);
	gic_sc = device_get_softc(parent);

	/*
	 * LPI Pending Table settings.
	 * This has to be done for each Re-Distributor, hence for each CPU.
	 */
	for (cpuid = 0; cpuid < mp_ncpus; cpuid++) {

		/* Limit allocation to active CPUs only */
		if (CPU_ISSET(cpuid, &all_cpus) == 0)
			continue;

		pend_base = (vm_offset_t)contigmalloc(
		    roundup2(LPI_PENDTAB_SIZE, PAGE_SIZE_64K), M_GIC_V3_ITS,
		    (M_WAITOK | M_ZERO), 0, ~0UL, PAGE_SIZE_64K, 0);

		/* Clean D-cache so that ITS can see zeroed pages */
		cpu_dcache_wb_range((vm_offset_t)pend_base,
		    roundup2(LPI_PENDTAB_SIZE, PAGE_SIZE_64K));

		if (bootverbose) {
			device_printf(sc->dev,
			    "LPI Pending Table for CPU%u at PA: 0x%lx\n",
			    cpuid, vtophys(pend_base));
		}

		gic_sc->gic_redists.lpis.pend_base[cpuid] = pend_base;
	}

	/* Ensure visibility of pend_base addresses on other CPUs */
	wmb();
}

static int
lpi_config_cpu(struct gic_v3_its_softc *sc)
{
	device_t parent;
	struct gic_v3_softc *gic_sc;
	vm_offset_t conf_base, pend_base;
	uint64_t gicr_xbaser, gicr_temp;
	uint64_t cache, share, idbits;
	uint32_t gicr_ctlr;
	u_int cpuid;

	parent = device_get_parent(sc->dev);
	gic_sc = device_get_softc(parent);
	cpuid = PCPU_GET(cpuid);

	/* Ensure data observability on a current CPU */
	rmb();

	conf_base = gic_sc->gic_redists.lpis.conf_base;
	pend_base = gic_sc->gic_redists.lpis.pend_base[cpuid];

	/* Disable LPIs */
	gicr_ctlr = gic_r_read(gic_sc, 4, GICR_CTLR);
	gicr_ctlr &= ~GICR_CTLR_LPI_ENABLE;
	gic_r_write(gic_sc, 4, GICR_CTLR, gicr_ctlr);
	/* Perform full system barrier */
	dsb(sy);

	/*
	 * Set GICR_PROPBASER
	 */

	/*
	 * Find out how many bits do we need for LPI identifiers.
	 * Remark 1.: Even though we have (LPI_CONFTAB_SIZE / 8) LPIs
	 *	      the notified LPI ID still starts from 8192
	 *	      (GIC_FIRST_LPI).
	 * Remark 2.: This could be done on compilation time but there
	 *	      seems to be no sufficient macro.
	 */
	idbits = flsl(LPI_CONFTAB_SIZE + GIC_FIRST_LPI) - 1;

	/* Set defaults: Normal Inner WAWB, IS */
	cache = GICR_PROPBASER_CACHE_NIWAWB;
	share = GICR_PROPBASER_SHARE_IS;

	gicr_xbaser = vtophys(conf_base) |
	    ((idbits - 1) & GICR_PROPBASER_IDBITS_MASK) |
	    (cache << GICR_PROPBASER_CACHE_SHIFT) |
	    (share << GICR_PROPBASER_SHARE_SHIFT);

	gic_r_write(gic_sc, 8, GICR_PROPBASER, gicr_xbaser);
	gicr_temp = gic_r_read(gic_sc, 8, GICR_PROPBASER);

	if (((gicr_xbaser ^ gicr_temp) & GICR_PROPBASER_SHARE_MASK) != 0) {
		if (bootverbose) {
			device_printf(sc->dev,
			    "Will use cache flushing for LPI "
			    "Configuration Table\n");
		}
		gic_sc->gic_redists.lpis.flags |= LPI_FLAGS_CONF_FLUSH;
	}

	/*
	 * Set GICR_PENDBASER
	 */

	/* Set defaults: Normal Inner WAWB, IS */
	cache = GICR_PENDBASER_CACHE_NIWAWB;
	share = GICR_PENDBASER_SHARE_IS;

	gicr_xbaser = vtophys(pend_base) |
	    (cache << GICR_PENDBASER_CACHE_SHIFT) |
	    (share << GICR_PENDBASER_SHARE_SHIFT);

	gic_r_write(gic_sc, 8, GICR_PENDBASER, gicr_xbaser);

	/* Enable LPIs */
	gicr_ctlr = gic_r_read(gic_sc, 4, GICR_CTLR);
	gicr_ctlr |= GICR_CTLR_LPI_ENABLE;
	gic_r_write(gic_sc, 4, GICR_CTLR, gicr_ctlr);

	dsb(sy);

	return (0);
}

static void
lpi_bitmap_init(struct gic_v3_its_softc *sc)
{
	device_t parent;
	struct gic_v3_softc *gic_sc;
	uint32_t lpi_id_num;
	size_t lpi_chunks_num;
	size_t bits_in_chunk;

	parent = device_get_parent(sc->dev);
	gic_sc = device_get_softc(parent);

	lpi_id_num = (1 << gic_sc->gic_idbits) - 1;
	/* Substract IDs dedicated for SGIs, PPIs and SPIs */
	lpi_id_num -= GIC_FIRST_LPI;

	sc->its_lpi_maxid = lpi_id_num;

	bits_in_chunk = sizeof(*sc->its_lpi_bitmap) * NBBY;

	/*
	 * Round up to the number of bits in chunk.
	 * We will need to take care to avoid using invalid LPI IDs later.
	 */
	lpi_id_num = roundup2(lpi_id_num, bits_in_chunk);
	lpi_chunks_num = lpi_id_num / bits_in_chunk;

	sc->its_lpi_bitmap =
	    contigmalloc((lpi_chunks_num * sizeof(*sc->its_lpi_bitmap)),
	    M_GIC_V3_ITS, (M_WAITOK | M_ZERO), 0, ~0UL,
	    sizeof(*sc->its_lpi_bitmap), 0);
}

static int
lpi_alloc_chunk(struct gic_v3_its_softc *sc, struct lpi_chunk *lpic,
    u_int nvecs)
{
	int fclr; /* First cleared bit */
	uint8_t *bitmap;
	size_t nb, i;

	bitmap = (uint8_t *)sc->its_lpi_bitmap;

	fclr = 0;
retry:
	/* Check other bits - sloooow */
	for (i = 0, nb = fclr; i < nvecs; i++, nb++) {
		if (nb > sc->its_lpi_maxid)
			return (EINVAL);

		if (isset(bitmap, nb)) {
			/* To little free bits in this area. Move on. */
			fclr = nb + 1;
			goto retry;
		}
	}
	/* This area is free. Take it. */
	bit_nset(bitmap, fclr, fclr + nvecs - 1);
	lpic->lpi_base = fclr + GIC_FIRST_LPI;
	lpic->lpi_num = nvecs;
	lpic->lpi_free = lpic->lpi_num;

	return (0);
}

static void
lpi_free_chunk(struct gic_v3_its_softc *sc, struct lpi_chunk *lpic)
{
	int start, end;
	uint8_t *bitmap;

	bitmap = (uint8_t *)sc->its_lpi_bitmap;

	KASSERT((lpic->lpi_free == lpic->lpi_num),
	    ("Trying to free LPI chunk that is still in use.\n"));

	/* First bit of this chunk in a global bitmap */
	start = lpic->lpi_base - GIC_FIRST_LPI;
	/* and last bit of this chunk... */
	end = start + lpic->lpi_num - 1;

	/* Finally free this chunk */
	bit_nclear(bitmap, start, end);
}

static void
lpi_configure(struct gic_v3_its_softc *sc, struct its_dev *its_dev,
    uint32_t lpinum, boolean_t unmask)
{
	device_t parent;
	struct gic_v3_softc *gic_sc;
	uint8_t *conf_byte;

	parent = device_get_parent(sc->dev);
	gic_sc = device_get_softc(parent);

	conf_byte = (uint8_t *)gic_sc->gic_redists.lpis.conf_base;
	conf_byte += (lpinum - GIC_FIRST_LPI);

	if (unmask)
		*conf_byte |= LPI_CONF_ENABLE;
	else
		*conf_byte &= ~LPI_CONF_ENABLE;

	if ((gic_sc->gic_redists.lpis.flags & LPI_FLAGS_CONF_FLUSH) != 0) {
		/* Clean D-cache under configuration byte */
		cpu_dcache_wb_range((vm_offset_t)conf_byte, sizeof(*conf_byte));
	} else {
		/* DSB inner shareable, store */
		dsb(ishst);
	}

	its_cmd_inv(sc, its_dev, lpinum);
}

static void
lpi_map_to_device(struct gic_v3_its_softc *sc, struct its_dev *its_dev,
    uint32_t id, uint32_t pid)
{

	if ((pid < its_dev->lpis.lpi_base) ||
	    (pid >= (its_dev->lpis.lpi_base + its_dev->lpis.lpi_num)))
		panic("Trying to map ivalid LPI %u for the device\n", pid);

	its_cmd_mapvi(sc, its_dev, id, pid);
}

static void
lpi_xmask_irq(device_t parent, uint32_t irq, boolean_t unmask)
{
	struct its_dev *its_dev;

	TAILQ_FOREACH(its_dev, &its_sc->its_dev_list, entry) {
		if (irq >= its_dev->lpis.lpi_base &&
		    irq < (its_dev->lpis.lpi_base + its_dev->lpis.lpi_num)) {
			lpi_configure(its_sc, its_dev, irq, unmask);
			return;
		}
	}

	panic("Trying to %s not existing LPI: %u\n",
	    (unmask == TRUE) ? "unmask" : "mask", irq);
}

void
lpi_unmask_irq(device_t parent, uint32_t irq)
{

	lpi_xmask_irq(parent, irq, 1);
}

void
lpi_mask_irq(device_t parent, uint32_t irq)
{

	lpi_xmask_irq(parent, irq, 0);
}

/*
 * Commands handling.
 */

static __inline void
cmd_format_command(struct its_cmd *cmd, uint8_t cmd_type)
{
	/* Command field: DW0 [7:0] */
	cmd->cmd_dword[0] &= ~CMD_COMMAND_MASK;
	cmd->cmd_dword[0] |= cmd_type;
}

static __inline void
cmd_format_devid(struct its_cmd *cmd, uint32_t devid)
{
	/* Device ID field: DW0 [63:32] */
	cmd->cmd_dword[0] &= ~CMD_DEVID_MASK;
	cmd->cmd_dword[0] |= ((uint64_t)devid << CMD_DEVID_SHIFT);
}

static __inline void
cmd_format_size(struct its_cmd *cmd, uint16_t size)
{
	/* Size field: DW1 [4:0] */
	cmd->cmd_dword[1] &= ~CMD_SIZE_MASK;
	cmd->cmd_dword[1] |= (size & CMD_SIZE_MASK);
}

static __inline void
cmd_format_id(struct its_cmd *cmd, uint32_t id)
{
	/* ID field: DW1 [31:0] */
	cmd->cmd_dword[1] &= ~CMD_ID_MASK;
	cmd->cmd_dword[1] |= id;
}

static __inline void
cmd_format_pid(struct its_cmd *cmd, uint32_t pid)
{
	/* Physical ID field: DW1 [63:32] */
	cmd->cmd_dword[1] &= ~CMD_PID_MASK;
	cmd->cmd_dword[1] |= ((uint64_t)pid << CMD_PID_SHIFT);
}

static __inline void
cmd_format_col(struct its_cmd *cmd, uint16_t col_id)
{
	/* Collection field: DW2 [16:0] */
	cmd->cmd_dword[2] &= ~CMD_COL_MASK;
	cmd->cmd_dword[2] |= col_id;
}

static __inline void
cmd_format_target(struct its_cmd *cmd, uint64_t target)
{
	/* Target Address field: DW2 [47:16] */
	cmd->cmd_dword[2] &= ~CMD_TARGET_MASK;
	cmd->cmd_dword[2] |= (target & CMD_TARGET_MASK);
}

static __inline void
cmd_format_itt(struct its_cmd *cmd, uint64_t itt)
{
	/* ITT Address field: DW2 [47:8] */
	cmd->cmd_dword[2] &= ~CMD_ITT_MASK;
	cmd->cmd_dword[2] |= (itt & CMD_ITT_MASK);
}

static __inline void
cmd_format_valid(struct its_cmd *cmd, uint8_t valid)
{
	/* Valid field: DW2 [63] */
	cmd->cmd_dword[2] &= ~CMD_VALID_MASK;
	cmd->cmd_dword[2] |= ((uint64_t)valid << CMD_VALID_SHIFT);
}

static __inline void
cmd_fix_endian(struct its_cmd *cmd)
{
	size_t i;

	for (i = 0; i < nitems(cmd->cmd_dword); i++)
		cmd->cmd_dword[i] = htole64(cmd->cmd_dword[i]);
}

static void
its_cmd_mapc(struct gic_v3_its_softc *sc, struct its_col *col, uint8_t valid)
{
	struct its_cmd_desc desc;

	desc.cmd_type = ITS_CMD_MAPC;
	desc.cmd_desc_mapc.col = col;
	/*
	 * Valid bit set - map the collection.
	 * Valid bit cleared - unmap the collection.
	 */
	desc.cmd_desc_mapc.valid = valid;

	its_cmd_send(sc, &desc);
}

static void
its_cmd_mapvi(struct gic_v3_its_softc *sc, struct its_dev *its_dev,
    uint32_t id, uint32_t pid)
{
	struct its_cmd_desc desc;

	desc.cmd_type = ITS_CMD_MAPVI;
	desc.cmd_desc_mapvi.its_dev = its_dev;
	desc.cmd_desc_mapvi.id = id;
	desc.cmd_desc_mapvi.pid = pid;

	its_cmd_send(sc, &desc);
}

static void __unused
its_cmd_mapi(struct gic_v3_its_softc *sc, struct its_dev *its_dev,
    uint32_t lpinum)
{
	struct its_cmd_desc desc;

	desc.cmd_type = ITS_CMD_MAPI;
	desc.cmd_desc_mapi.its_dev = its_dev;
	desc.cmd_desc_mapi.lpinum = lpinum;

	its_cmd_send(sc, &desc);
}

static void
its_cmd_mapd(struct gic_v3_its_softc *sc, struct its_dev *its_dev,
    uint8_t valid)
{
	struct its_cmd_desc desc;

	desc.cmd_type = ITS_CMD_MAPD;
	desc.cmd_desc_mapd.its_dev = its_dev;
	desc.cmd_desc_mapd.valid = valid;

	its_cmd_send(sc, &desc);
}

static void
its_cmd_inv(struct gic_v3_its_softc *sc, struct its_dev *its_dev,
    uint32_t lpinum)
{
	struct its_cmd_desc desc;

	desc.cmd_type = ITS_CMD_INV;
	desc.cmd_desc_inv.lpinum = lpinum - its_dev->lpis.lpi_base;
	desc.cmd_desc_inv.its_dev = its_dev;

	its_cmd_send(sc, &desc);
}

static void
its_cmd_invall(struct gic_v3_its_softc *sc, struct its_col *col)
{
	struct its_cmd_desc desc;

	desc.cmd_type = ITS_CMD_INVALL;
	desc.cmd_desc_invall.col = col;

	its_cmd_send(sc, &desc);
}

/*
 * Helper routines for commands processing.
 */
static __inline boolean_t
its_cmd_queue_full(struct gic_v3_its_softc *sc)
{
	size_t read_idx, write_idx;

	write_idx = (size_t)(sc->its_cmdq_write - sc->its_cmdq_base);
	read_idx = gic_its_read(sc, 4, GITS_CREADR) / sizeof(struct its_cmd);

	/*
	 * The queue is full when the write offset points
	 * at the command before the current read offset.
	 */
	if (((write_idx + 1) % ITS_CMDQ_NENTRIES) == read_idx)
		return (TRUE);

	return (FALSE);
}

static __inline void
its_cmd_sync(struct gic_v3_its_softc *sc, struct its_cmd *cmd)
{

	if ((sc->its_flags & ITS_FLAGS_CMDQ_FLUSH) != 0) {
		/* Clean D-cache under command. */
		cpu_dcache_wb_range((vm_offset_t)cmd, sizeof(*cmd));
	} else {
		/* DSB inner shareable, store */
		dsb(ishst);
	}

}

static struct its_cmd *
its_cmd_alloc_locked(struct gic_v3_its_softc *sc)
{
	struct its_cmd *cmd;
	size_t us_left;

	/*
	 * XXX ARM64TODO: This is obviously a significant delay.
	 * The reason for that is that currently the time frames for
	 * the command to complete (and therefore free the descriptor)
	 * are not known.
	 */
	us_left = 1000000;

	mtx_assert(&sc->its_spin_mtx, MA_OWNED);
	while (its_cmd_queue_full(sc)) {
		if (us_left-- == 0) {
			/* Timeout while waiting for free command */
			device_printf(sc->dev,
			    "Timeout while waiting for free command\n");
			return (NULL);
		}
		DELAY(1);
	}

	cmd = sc->its_cmdq_write;
	sc->its_cmdq_write++;

	if (sc->its_cmdq_write == (sc->its_cmdq_base + ITS_CMDQ_NENTRIES)) {
		/* Wrap the queue */
		sc->its_cmdq_write = sc->its_cmdq_base;
	}

	return (cmd);
}

static uint64_t
its_cmd_prepare(struct its_cmd *cmd, struct its_cmd_desc *desc)
{
	uint64_t target;
	uint8_t cmd_type;
	u_int size;
	boolean_t error;

	error = FALSE;
	cmd_type = desc->cmd_type;
	target = ITS_TARGET_NONE;

	switch (cmd_type) {
	case ITS_CMD_SYNC:	/* Wait for previous commands completion */
		target = desc->cmd_desc_sync.col->col_target;
		cmd_format_command(cmd, ITS_CMD_SYNC);
		cmd_format_target(cmd, target);
		break;
	case ITS_CMD_MAPD:	/* Assign ITT to device */
		target = desc->cmd_desc_mapd.its_dev->col->col_target;
		cmd_format_command(cmd, ITS_CMD_MAPD);
		cmd_format_itt(cmd, vtophys(desc->cmd_desc_mapd.its_dev->itt));
		/*
		 * Size describes number of bits to encode interrupt IDs
		 * supported by the device minus one.
		 * When V (valid) bit is zero, this field should be written
		 * as zero.
		 */
		if (desc->cmd_desc_mapd.valid != 0) {
			size = fls(desc->cmd_desc_mapd.its_dev->lpis.lpi_num);
			size = MAX(1, size) - 1;
		} else
			size = 0;

		cmd_format_size(cmd, size);
		cmd_format_devid(cmd, desc->cmd_desc_mapd.its_dev->devid);
		cmd_format_valid(cmd, desc->cmd_desc_mapd.valid);
		break;
	case ITS_CMD_MAPC:	/* Map collection to Re-Distributor */
		target = desc->cmd_desc_mapc.col->col_target;
		cmd_format_command(cmd, ITS_CMD_MAPC);
		cmd_format_col(cmd, desc->cmd_desc_mapc.col->col_id);
		cmd_format_valid(cmd, desc->cmd_desc_mapc.valid);
		cmd_format_target(cmd, target);
		break;
	case ITS_CMD_MAPVI:
		target = desc->cmd_desc_mapvi.its_dev->col->col_target;
		cmd_format_command(cmd, ITS_CMD_MAPVI);
		cmd_format_devid(cmd, desc->cmd_desc_mapvi.its_dev->devid);
		cmd_format_id(cmd, desc->cmd_desc_mapvi.id);
		cmd_format_pid(cmd, desc->cmd_desc_mapvi.pid);
		cmd_format_col(cmd, desc->cmd_desc_mapvi.its_dev->col->col_id);
		break;
	case ITS_CMD_MAPI:
		target = desc->cmd_desc_mapi.its_dev->col->col_target;
		cmd_format_command(cmd, ITS_CMD_MAPI);
		cmd_format_devid(cmd, desc->cmd_desc_mapi.its_dev->devid);
		cmd_format_id(cmd, desc->cmd_desc_mapi.lpinum);
		cmd_format_col(cmd, desc->cmd_desc_mapi.its_dev->col->col_id);
		break;
	case ITS_CMD_INV:
		target = desc->cmd_desc_inv.its_dev->col->col_target;
		cmd_format_command(cmd, ITS_CMD_INV);
		cmd_format_devid(cmd, desc->cmd_desc_inv.its_dev->devid);
		cmd_format_id(cmd, desc->cmd_desc_inv.lpinum);
		break;
	case ITS_CMD_INVALL:
		cmd_format_command(cmd, ITS_CMD_INVALL);
		cmd_format_col(cmd, desc->cmd_desc_invall.col->col_id);
		break;
	default:
		error = TRUE;
		break;
	}

	if (!error)
		cmd_fix_endian(cmd);

	return (target);
}

static __inline uint64_t
its_cmd_cwriter_offset(struct gic_v3_its_softc *sc, struct its_cmd *cmd)
{
	uint64_t off;

	off = (cmd - sc->its_cmdq_base) * sizeof(*cmd);

	return (off);
}

static void
its_cmd_wait_completion(struct gic_v3_its_softc *sc, struct its_cmd *cmd_first,
    struct its_cmd *cmd_last)
{
	uint64_t first, last, read;
	size_t us_left;

	/*
	 * XXX ARM64TODO: This is obviously a significant delay.
	 * The reason for that is that currently the time frames for
	 * the command to complete are not known.
	 */
	us_left = 1000000;

	first = its_cmd_cwriter_offset(sc, cmd_first);
	last = its_cmd_cwriter_offset(sc, cmd_last);

	for (;;) {
		read = gic_its_read(sc, 8, GITS_CREADR);
		if (read < first || read >= last)
			break;

		if (us_left-- == 0) {
			/* This means timeout */
			device_printf(sc->dev,
			    "Timeout while waiting for CMD completion.\n");
			return;
		}
		DELAY(1);
	}
}

static int
its_cmd_send(struct gic_v3_its_softc *sc, struct its_cmd_desc *desc)
{
	struct its_cmd *cmd, *cmd_sync, *cmd_write;
	struct its_col col_sync;
	struct its_cmd_desc desc_sync;
	uint64_t target, cwriter;

	mtx_lock_spin(&sc->its_spin_mtx);
	cmd = its_cmd_alloc_locked(sc);
	if (cmd == NULL) {
		device_printf(sc->dev, "could not allocate ITS command\n");
		mtx_unlock_spin(&sc->its_spin_mtx);
		return (EBUSY);
	}

	target = its_cmd_prepare(cmd, desc);
	its_cmd_sync(sc, cmd);

	if (target != ITS_TARGET_NONE) {
		cmd_sync = its_cmd_alloc_locked(sc);
		if (cmd_sync == NULL)
			goto end;
		desc_sync.cmd_type = ITS_CMD_SYNC;
		col_sync.col_target = target;
		desc_sync.cmd_desc_sync.col = &col_sync;
		its_cmd_prepare(cmd_sync, &desc_sync);
		its_cmd_sync(sc, cmd_sync);
	}
end:
	/* Update GITS_CWRITER */
	cwriter = its_cmd_cwriter_offset(sc, sc->its_cmdq_write);
	gic_its_write(sc, 8, GITS_CWRITER, cwriter);
	cmd_write = sc->its_cmdq_write;
	mtx_unlock_spin(&sc->its_spin_mtx);

	its_cmd_wait_completion(sc, cmd, cmd_write);

	return (0);
}

static struct its_dev *
its_device_find_locked(struct gic_v3_its_softc *sc, device_t pci_dev)
{
	struct its_dev *its_dev;

	mtx_assert(&sc->its_mtx, MA_OWNED);
	/* Find existing device if any */
	TAILQ_FOREACH(its_dev, &sc->its_dev_list, entry) {
		if (its_dev->pci_dev == pci_dev)
			return (its_dev);
	}

	return (NULL);
}

static struct its_dev *
its_device_alloc_locked(struct gic_v3_its_softc *sc, device_t pci_dev,
    u_int nvecs)
{
	struct its_dev *newdev;
	uint64_t typer;
	uint32_t devid;
	u_int cpuid;
	size_t esize;

	mtx_assert(&sc->its_mtx, MA_OWNED);
	/* Find existing device if any */
	newdev = its_device_find_locked(sc, pci_dev);
	if (newdev != NULL)
		return (newdev);

	devid = its_get_devid(pci_dev);

	/* There was no previously created device. Create one now */
	newdev = malloc(sizeof(*newdev), M_GIC_V3_ITS, (M_NOWAIT | M_ZERO));
	if (newdev == NULL)
		return (NULL);

	newdev->pci_dev = pci_dev;
	newdev->devid = devid;

	if (lpi_alloc_chunk(sc, &newdev->lpis, nvecs) != 0) {
		free(newdev, M_GIC_V3_ITS);
		return (NULL);
	}

	/* Get ITT entry size */
	typer = gic_its_read(sc, 8, GITS_TYPER);
	esize = GITS_TYPER_ITTES(typer);
	/*
	 * Allocate ITT for this device.
	 * PA has to be 256 B aligned. At least two entries for device.
	 */
	newdev->itt = (vm_offset_t)contigmalloc(
	    roundup2(roundup2(nvecs, 2) * esize, 0x100), M_GIC_V3_ITS,
	    (M_NOWAIT | M_ZERO), 0, ~0UL, 0x100, 0);
	if (newdev->itt == 0) {
		lpi_free_chunk(sc, &newdev->lpis);
		free(newdev, M_GIC_V3_ITS);
		return (NULL);
	}

	/*
	 * XXX ARM64TODO: Currently all interrupts are going
	 * to be bound to the CPU that performs the configuration.
	 */
	cpuid = PCPU_GET(cpuid);
	newdev->col = sc->its_cols[cpuid];

	TAILQ_INSERT_TAIL(&sc->its_dev_list, newdev, entry);

	/* Map device to its ITT */
	its_cmd_mapd(sc, newdev, 1);

	return (newdev);
}

static __inline void
its_device_asign_lpi_locked(struct gic_v3_its_softc *sc,
    struct its_dev *its_dev, u_int *irq)
{

	mtx_assert(&sc->its_mtx, MA_OWNED);
	if (its_dev->lpis.lpi_free == 0) {
		panic("Requesting more LPIs than allocated for this device. "
		    "LPI num: %u, free %u", its_dev->lpis.lpi_num,
		    its_dev->lpis.lpi_free);
	}
	*irq = its_dev->lpis.lpi_base + (its_dev->lpis.lpi_num -
	    its_dev->lpis.lpi_free);
	its_dev->lpis.lpi_free--;
}

/*
 * ITS quirks.
 * Add vendor specific PCI devid function here.
 */
static uint32_t
its_get_devid_thunder(device_t pci_dev)
{
	int bsf;
	int pem;
	uint32_t bus;

	bus = pci_get_bus(pci_dev);

	bsf = PCI_RID(pci_get_bus(pci_dev), pci_get_slot(pci_dev),
	    pci_get_function(pci_dev));

	/* ECAM is on bus=0 */
	if (bus == 0) {
		return ((pci_get_domain(pci_dev) << PCI_RID_DOMAIN_SHIFT) |
		    bsf);
	/* PEM otherwise */
	} else {
		/* PEM (PCIe MAC/root complex) number is equal to domain */
		pem = pci_get_domain(pci_dev);

		/*
		 * Set appropriate device ID (passed by the HW along with
		 * the transaction to memory) for different root complex
		 * numbers using hard-coded domain portion for each group.
		 */
		if (pem < 3)
			return ((0x1 << PCI_RID_DOMAIN_SHIFT) | bsf);

		if (pem < 6)
			return ((0x3 << PCI_RID_DOMAIN_SHIFT) | bsf);

		if (pem < 9)
			return ((0x9 << PCI_RID_DOMAIN_SHIFT) | bsf);

		if (pem < 12)
			return ((0xB << PCI_RID_DOMAIN_SHIFT) | bsf);
	}

	return (0);
}

static uint32_t
its_get_devbits_thunder(device_t dev)
{
	uint32_t devid_bits;

	/*
	 * GITS_TYPER[17:13] of ThunderX reports that device IDs
	 * are to be 21 bits in length.
	 * The entry size of the ITS table can be read from GITS_BASERn[52:48]
	 * and on ThunderX is supposed to be 8 bytes in length (for device
	 * table). Finally the page size that is to be used by ITS to access
	 * this table will be set to 64KB.
	 *
	 * This gives 0x200000 entries of size 0x8 bytes covered by 256 pages
	 * each of which 64KB in size. The number of pages (minus 1) should
	 * then be written to GITS_BASERn[7:0]. In that case this value would
	 * be 0xFF but on ThunderX the maximum value that HW accepts is 0xFD.
	 *
	 * Set arbitrary number of device ID bits to 20 in order to limit
	 * the number of entries in ITS device table to 0x100000 and hence
	 * the table size to 8MB.
	 */
	devid_bits = 20;
	if (bootverbose) {
		device_printf(dev,
		    "Limiting number of Device ID bits implemented to %d\n",
		    devid_bits);
	}

	return (devid_bits);
}

static __inline uint32_t
its_get_devbits_default(device_t dev)
{
	uint64_t gits_typer;
	struct gic_v3_its_softc *sc;

	sc = device_get_softc(dev);

	gits_typer = gic_its_read(sc, 8, GITS_TYPER);

	return (GITS_TYPER_DEVB(gits_typer));
}

static uint32_t
its_get_devbits(device_t dev)
{
	const struct its_quirks *quirk;
	size_t i;

	for (i = 0; i < nitems(its_quirks); i++) {
		quirk = &its_quirks[i];
		if (CPU_MATCH_RAW(quirk->cpuid_mask, quirk->cpuid)) {
			if (quirk->devbits_func != NULL)
				return ((*quirk->devbits_func)(dev));
		}
	}

	return (its_get_devbits_default(dev));
}

static __inline uint32_t
its_get_devid_default(device_t pci_dev)
{

	return (PCI_DEVID_GENERIC(pci_dev));
}

static uint32_t
its_get_devid(device_t pci_dev)
{
	const struct its_quirks *quirk;
	size_t i;

	for (i = 0; i < nitems(its_quirks); i++) {
		quirk = &its_quirks[i];
		if (CPU_MATCH_RAW(quirk->cpuid_mask, quirk->cpuid)) {
			if (quirk->devid_func != NULL)
				return ((*quirk->devid_func)(pci_dev));
		}
	}

	return (its_get_devid_default(pci_dev));
}

/*
 * Message signalled interrupts handling.
 */

/*
 * XXX ARM64TODO: Watch out for "irq" type.
 *
 * In theory GIC can handle up to (2^32 - 1) interrupt IDs whereas
 * we pass "irq" pointer of type integer. This is obviously wrong but
 * is determined by the way as PCI layer wants it to be done.
 */
int
gic_v3_its_alloc_msix(device_t dev, device_t pci_dev, int *irq)
{
	struct gic_v3_its_softc *sc;
	struct its_dev *its_dev;
	u_int nvecs;

	sc = device_get_softc(dev);

	mtx_lock(&sc->its_mtx);
	nvecs = PCI_MSIX_NUM(pci_dev);

	/*
	 * Allocate device as seen by ITS if not already available.
	 * Notice that MSI-X interrupts are allocated on one-by-one basis.
	 */
	its_dev = its_device_alloc_locked(sc, pci_dev, nvecs);
	if (its_dev == NULL) {
		mtx_unlock(&sc->its_mtx);
		return (ENOMEM);
	}

	its_device_asign_lpi_locked(sc, its_dev, irq);
	mtx_unlock(&sc->its_mtx);

	return (0);
}

int
gic_v3_its_alloc_msi(device_t dev, device_t pci_dev, int count, int *irqs)
{
	struct gic_v3_its_softc *sc;
	struct its_dev *its_dev;

	sc = device_get_softc(dev);

	/* Allocate device as seen by ITS if not already available. */
	mtx_lock(&sc->its_mtx);
	its_dev = its_device_alloc_locked(sc, pci_dev, count);
	if (its_dev == NULL) {
		mtx_unlock(&sc->its_mtx);
		return (ENOMEM);
	}

	for (; count > 0; count--) {
		its_device_asign_lpi_locked(sc, its_dev, irqs);
		irqs++;
	}
	mtx_unlock(&sc->its_mtx);

	return (0);
}

int
gic_v3_its_map_msix(device_t dev, device_t pci_dev, int irq, uint64_t *addr,
    uint32_t *data)
{
	struct gic_v3_its_softc *sc;
	bus_space_handle_t its_bsh;
	struct its_dev *its_dev;
	uint64_t its_pa;
	uint32_t id;

	sc = device_get_softc(dev);
	/* Verify that this device is allocated and owns this LPI */
	mtx_lock(&sc->its_mtx);
	its_dev = its_device_find_locked(sc, pci_dev);
	mtx_unlock(&sc->its_mtx);
	if (its_dev == NULL)
		return (EINVAL);

	id = irq - its_dev->lpis.lpi_base;
	lpi_map_to_device(sc, its_dev, id, irq);

	its_bsh = rman_get_bushandle(&sc->its_res[0]);
	its_pa = vtophys(its_bsh);

	*addr = (its_pa + GITS_TRANSLATER);
	*data = id;

	return (0);
}
