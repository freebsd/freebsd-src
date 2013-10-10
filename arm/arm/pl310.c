/*-
 * Copyright (c) 2012 Olivier Houchard <cognet@FreeBSD.org>
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
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
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BEN GRAY ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BEN GRAY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/intr.h>

#include <machine/bus.h>
#include <machine/pl310.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

/*
 * Define this if you need to disable PL310 for debugging purpose
 * Spec: 
 * http://infocenter.arm.com/help/topic/com.arm.doc.ddi0246e/DDI0246E_l2c310_r3p1_trm.pdf
 */

/* 
 * Hardcode errata for now
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0246b/pr01s02s02.html
 */
#define	PL310_ERRATA_588369
#define	PL310_ERRATA_753970
#define	PL310_ERRATA_727915

#define	PL310_LOCK(sc) do {		\
	mtx_lock_spin(&(sc)->sc_mtx);	\
} while(0);

#define	PL310_UNLOCK(sc) do {		\
	mtx_unlock_spin(&(sc)->sc_mtx);	\
} while(0);

static int pl310_enabled = 1;
TUNABLE_INT("pl310.enabled", &pl310_enabled);

static uint32_t g_l2cache_way_mask;

static const uint32_t g_l2cache_line_size = 32;
static const uint32_t g_l2cache_align_mask = (32 - 1);

static uint32_t g_l2cache_size;
static uint32_t g_way_size;
static uint32_t g_ways_assoc;

static struct pl310_softc *pl310_softc;

static int
pl310_filter(void *arg)
{
	struct pl310_softc *sc = arg;
	uint32_t intr;

	intr = pl310_read4(sc, PL310_INTR_MASK);

	if (!sc->sc_enabled && (intr & INTR_MASK_ECNTR)) {
		/*
		 * This is for debug purpose, so be blunt about it
		 * We disable PL310 only when something fishy is going
		 * on and we need to make sure L2 cache is 100% disabled
		 */
		panic("pl310: caches disabled but cache event detected\n");
	}

	return (FILTER_HANDLED);
}

static __inline void
pl310_wait_background_op(uint32_t off, uint32_t mask)
{

	while (pl310_read4(pl310_softc, off) & mask);
}


/**
 *	pl310_cache_sync - performs a cache sync operation
 * 
 *	According to the TRM:
 *
 *  "Before writing to any other register you must perform an explicit
 *   Cache Sync operation. This is particularly important when the cache is
 *   enabled and changes to how the cache allocates new lines are to be made."
 *
 *
 */
static __inline void
pl310_cache_sync(void)
{
	if ((pl310_softc == NULL) || !pl310_softc->sc_enabled)
		return;

#ifdef PL310_ERRATA_753970
	if (pl310_softc->sc_rtl_revision == CACHE_ID_RELEASE_r3p0)
		/* Write uncached PL310 register */
		pl310_write4(pl310_softc, 0x740, 0xffffffff);
	else
#endif
		pl310_write4(pl310_softc, PL310_CACHE_SYNC, 0xffffffff);
}


static void
pl310_wbinv_all(void)
{

	if ((pl310_softc == NULL) || !pl310_softc->sc_enabled)
		return;

	PL310_LOCK(pl310_softc);
#ifdef PL310_ERRATA_727915
	if (pl310_softc->sc_rtl_revision == CACHE_ID_RELEASE_r2p0) {
		int i, j;

		for (i = 0; i < g_ways_assoc; i++) {
			for (j = 0; j < g_way_size / g_l2cache_line_size; j++) {
				pl310_write4(pl310_softc, 
				    PL310_CLEAN_INV_LINE_IDX,
				    (i << 28 | j << 5));
			}
		}
		pl310_cache_sync();
		PL310_UNLOCK(pl310_softc);
		return;

	}
	if (pl310_softc->sc_rtl_revision == CACHE_ID_RELEASE_r3p0)
		platform_pl310_write_debug(pl310_softc, 3);
#endif
	pl310_write4(pl310_softc, PL310_CLEAN_INV_WAY, g_l2cache_way_mask);
	pl310_wait_background_op(PL310_CLEAN_INV_WAY, g_l2cache_way_mask);
	pl310_cache_sync();
#ifdef PL310_ERRATA_727915
	if (pl310_softc->sc_rtl_revision == CACHE_ID_RELEASE_r3p0)
		platform_pl310_write_debug(pl310_softc, 0);
#endif
	PL310_UNLOCK(pl310_softc);
}

static void
pl310_wbinv_range(vm_paddr_t start, vm_size_t size)
{

	if ((pl310_softc == NULL) || !pl310_softc->sc_enabled)
		return;

	PL310_LOCK(pl310_softc);
	if (start & g_l2cache_align_mask) {
		size += start & g_l2cache_align_mask;
		start &= ~g_l2cache_align_mask;
	}
	if (size & g_l2cache_align_mask) {
		size &= ~g_l2cache_align_mask;
	   	size += g_l2cache_line_size;
	}


#ifdef PL310_ERRATA_727915
	platform_pl310_write_debug(pl310_softc, 3);
#endif
	while (size > 0) {
#ifdef PL310_ERRATA_588369
		if (pl310_softc->sc_rtl_revision <= CACHE_ID_RELEASE_r1p0) {
			/* 
			 * Errata 588369 says that clean + inv may keep the 
			 * cache line if it was clean, the recommanded
			 * workaround is to clean then invalidate the cache
			 * line, with write-back and cache linefill disabled.
			 */
			pl310_write4(pl310_softc, PL310_CLEAN_LINE_PA, start);
			pl310_write4(pl310_softc, PL310_INV_LINE_PA, start);
		} else
#endif
			pl310_write4(pl310_softc, PL310_CLEAN_INV_LINE_PA,
			    start);
		start += g_l2cache_line_size;
		size -= g_l2cache_line_size;
	}
#ifdef PL310_ERRATA_727915
	platform_pl310_write_debug(pl310_softc, 0);
#endif

	pl310_cache_sync();
	PL310_UNLOCK(pl310_softc);
}

static void
pl310_wb_range(vm_paddr_t start, vm_size_t size)
{

	if ((pl310_softc == NULL) || !pl310_softc->sc_enabled)
		return;

	PL310_LOCK(pl310_softc);
	if (start & g_l2cache_align_mask) {
		size += start & g_l2cache_align_mask;
		start &= ~g_l2cache_align_mask;
	}

	if (size & g_l2cache_align_mask) {
		size &= ~g_l2cache_align_mask;
		size += g_l2cache_line_size;
	}

	while (size > 0) {
		pl310_write4(pl310_softc, PL310_CLEAN_LINE_PA, start);
		start += g_l2cache_line_size;
		size -= g_l2cache_line_size;
	}

	pl310_cache_sync();
	PL310_UNLOCK(pl310_softc);
}

static void
pl310_inv_range(vm_paddr_t start, vm_size_t size)
{

	if ((pl310_softc == NULL) || !pl310_softc->sc_enabled)
		return;

	PL310_LOCK(pl310_softc);
	if (start & g_l2cache_align_mask) {
		size += start & g_l2cache_align_mask;
		start &= ~g_l2cache_align_mask;
	}
	if (size & g_l2cache_align_mask) {
		size &= ~g_l2cache_align_mask;
		size += g_l2cache_line_size;
	}
	while (size > 0) {
		pl310_write4(pl310_softc, PL310_INV_LINE_PA, start);
		start += g_l2cache_line_size;
		size -= g_l2cache_line_size;
	}

	pl310_cache_sync();
	PL310_UNLOCK(pl310_softc);
}

static int
pl310_probe(device_t dev)
{
	
	if (!ofw_bus_is_compatible(dev, "arm,pl310"))
		return (ENXIO);
	device_set_desc(dev, "PL310 L2 cache controller");
	return (0);
}

static int
pl310_attach(device_t dev)
{
	struct pl310_softc *sc = device_get_softc(dev);
	int rid = 0;
	uint32_t aux_value;
	uint32_t ctrl_value;
	uint32_t cache_id;

	sc->sc_dev = dev;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 
	    RF_ACTIVE);
	if (sc->sc_mem_res == NULL)
		panic("%s: Cannot map registers", device_get_name(dev));

	/* Allocate an IRQ resource */
	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	                                        RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_irq_res == NULL) {
		panic("Cannot allocate IRQ\n");
	}

	pl310_softc = sc;
	mtx_init(&sc->sc_mtx, "pl310lock", NULL, MTX_SPIN);
	sc->sc_enabled = pl310_enabled;

	/* activate the interrupt */
	bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
				pl310_filter, NULL, sc, &sc->sc_irq_h);

	cache_id = pl310_read4(sc, PL310_CACHE_ID);
	sc->sc_rtl_revision = (cache_id >> CACHE_ID_RELEASE_SHIFT) &
	    CACHE_ID_RELEASE_MASK;
	device_printf(dev, "Part number: 0x%x, release: 0x%x\n",
	    (cache_id >> CACHE_ID_PARTNUM_SHIFT) & CACHE_ID_PARTNUM_MASK,
	    (cache_id >> CACHE_ID_RELEASE_SHIFT) & CACHE_ID_RELEASE_MASK);
	aux_value = pl310_read4(sc, PL310_AUX_CTRL);
	g_way_size = (aux_value & AUX_CTRL_WAY_SIZE_MASK) >>
	    AUX_CTRL_WAY_SIZE_SHIFT;
	g_way_size = 1 << (g_way_size + 13);
	if (aux_value & (1 << AUX_CTRL_ASSOCIATIVITY_SHIFT))
		g_ways_assoc = 16;
	else
		g_ways_assoc = 8;
	g_l2cache_way_mask = (1 << g_ways_assoc) - 1;
	g_l2cache_size = g_way_size * g_ways_assoc;
	/* Print the information */
	device_printf(dev, "L2 Cache: %uKB/%dB %d ways\n", (g_l2cache_size / 1024),
	       g_l2cache_line_size, g_ways_assoc);

	ctrl_value = pl310_read4(sc, PL310_CTRL);

	if (sc->sc_enabled && !(ctrl_value & CTRL_ENABLED)) {
		/* Enable the L2 cache if disabled */
		platform_pl310_write_ctrl(sc, CTRL_ENABLED);
	} 

	if (!sc->sc_enabled && (ctrl_value & CTRL_ENABLED)) {
		/*
		 * Set counters so when cache event happens
		 * we'll get interrupt and be warned that something 
		 * is off
		 */

		/* Cache Line Eviction for Counter 0 */
		pl310_write4(sc, PL310_EVENT_COUNTER0_CONF, 
		    EVENT_COUNTER_CONF_INCR | EVENT_COUNTER_CONF_CO);
		/* Data Read Request for Counter 1 */
		pl310_write4(sc, PL310_EVENT_COUNTER1_CONF, 
		    EVENT_COUNTER_CONF_INCR | EVENT_COUNTER_CONF_DRREQ);

		/* Temporary switch on for final flush*/
		sc->sc_enabled = 1;
		pl310_wbinv_all();
		sc->sc_enabled = 0;
		platform_pl310_write_ctrl(sc, CTRL_DISABLED);

		/* Enable and clear pending interrupts */
		pl310_write4(sc, PL310_INTR_CLEAR, INTR_MASK_ECNTR);
		pl310_write4(sc, PL310_INTR_MASK, INTR_MASK_ALL);

		/* Enable counters and reset C0 and C1 */
		pl310_write4(sc, PL310_EVENT_COUNTER_CTRL, 
		    EVENT_COUNTER_CTRL_ENABLED | 
		    EVENT_COUNTER_CTRL_C0_RESET | 
		    EVENT_COUNTER_CTRL_C1_RESET);

	}

	if (sc->sc_enabled)
		platform_pl310_init(sc);

	pl310_wbinv_all();

	/* Set the l2 functions in the set of cpufuncs */
	cpufuncs.cf_l2cache_wbinv_all = pl310_wbinv_all;
	cpufuncs.cf_l2cache_wbinv_range = pl310_wbinv_range;
	cpufuncs.cf_l2cache_inv_range = pl310_inv_range;
	cpufuncs.cf_l2cache_wb_range = pl310_wb_range;

	return (0);
}

static device_method_t pl310_methods[] = {
	DEVMETHOD(device_probe, pl310_probe),
	DEVMETHOD(device_attach, pl310_attach),
	{0, 0},
};

static driver_t pl310_driver = {
        "l2cache",
        pl310_methods,
        sizeof(struct pl310_softc),
};
static devclass_t pl310_devclass;

DRIVER_MODULE(pl310, simplebus, pl310_driver, pl310_devclass, 0, 0);
