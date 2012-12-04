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
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pl310.h>
#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

/**
 *	PL310 - L2 Cache Controller register offsets.
 *
 */
#define PL310_CACHE_ID              0x000
#define PL310_CACHE_TYPE            0x004
#define PL310_CTRL                  0x100
#define PL310_AUX_CTRL              0x104
#define PL310_EVENT_COUNTER_CTRL    0x200
#define PL310_EVENT_COUNTER1_CONF   0x204
#define PL310_EVENT_COUNTER0_CONF   0x208
#define PL310_EVENT_COUNTER1_VAL    0x20C
#define PL310_EVENT_COUNTER0_VAL    0x210
#define PL310_INTR_MASK             0x214
#define PL310_MASKED_INTR_STAT      0x218
#define PL310_RAW_INTR_STAT         0x21C
#define PL310_INTR_CLEAR            0x220
#define PL310_CACHE_SYNC            0x730
#define PL310_INV_LINE_PA           0x770
#define PL310_INV_WAY               0x77C
#define PL310_CLEAN_LINE_PA         0x7B0
#define PL310_CLEAN_LINE_IDX        0x7B8
#define PL310_CLEAN_WAY             0x7BC
#define PL310_CLEAN_INV_LINE_PA     0x7F0
#define PL310_CLEAN_INV_LINE_IDX    0x7F8
#define PL310_CLEAN_INV_WAY         0x7FC
#define PL310_LOCKDOWN_D_WAY(x)    (0x900 + ((x) * 8))
#define PL310_LOCKDOWN_I_WAY(x)    (0x904 + ((x) * 8))
#define PL310_LOCKDOWN_LINE_ENABLE  0x950
#define PL310_UNLOCK_ALL_LINES_WAY  0x954
#define PL310_ADDR_FILTER_START     0xC00
#define PL310_ADDR_FILTER_END       0xC04
#define PL310_DEBUG_CTRL            0xF40


#define PL310_AUX_CTRL_MASK                      0xc0000fff
#define PL310_AUX_CTRL_ASSOCIATIVITY_SHIFT       16
#define PL310_AUX_CTRL_WAY_SIZE_SHIFT            17
#define PL310_AUX_CTRL_WAY_SIZE_MASK             (0x7 << 17)
#define PL310_AUX_CTRL_SHARE_OVERRIDE_SHIFT      22
#define PL310_AUX_CTRL_NS_LOCKDOWN_SHIFT         26
#define PL310_AUX_CTRL_NS_INT_CTRL_SHIFT         27
#define PL310_AUX_CTRL_DATA_PREFETCH_SHIFT       28
#define PL310_AUX_CTRL_INSTR_PREFETCH_SHIFT      29
#define PL310_AUX_CTRL_EARLY_BRESP_SHIFT         30


void omap4_l2cache_wbinv_range(vm_paddr_t physaddr, vm_size_t size);
void omap4_l2cache_inv_range(vm_paddr_t physaddr, vm_size_t size);
void omap4_l2cache_wb_range(vm_paddr_t physaddr, vm_size_t size);
void omap4_l2cache_wbinv_all(void);
void omap4_l2cache_inv_all(void);
void omap4_l2cache_wb_all(void);

static uint32_t g_l2cache_way_mask;

static const uint32_t g_l2cache_line_size = 32;
static const uint32_t g_l2cache_align_mask = (32 - 1);

static uint32_t g_l2cache_size;

static struct pl310_softc *pl310_softc;

/**
 *	pl310_read4 - read a 32-bit value from the PL310 registers
 *	pl310_write4 - write a 32-bit value from the PL310 registers
 *	@off: byte offset within the register set to read from
 *	@val: the value to write into the register
 *	
 *
 *	LOCKING:
 *	None
 *
 *	RETURNS:
 *	nothing in case of write function, if read function returns the value read.
 */
static __inline uint32_t
pl310_read4(bus_size_t off)
{
	return bus_read_4(pl310_softc->sc_mem_res, off);
}
static __inline void
pl310_write4(bus_size_t off, uint32_t val)
{
	bus_write_4(pl310_softc->sc_mem_res, off, val);
}

static __inline void
pl310_wait_background_op(uint32_t off, uint32_t mask)
{
	while (pl310_read4(off) & mask);
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
	pl310_write4(PL310_CACHE_SYNC, 0);
}


static void
pl310_wbinv_all(void)
{
#if 1
	pl310_write4(PL310_DEBUG_CTRL, 3);
#endif
	pl310_write4(PL310_CLEAN_INV_WAY, g_l2cache_way_mask);
	pl310_wait_background_op(PL310_CLEAN_INV_WAY, g_l2cache_way_mask);
	pl310_cache_sync();
#if 1
	pl310_write4(PL310_DEBUG_CTRL, 0);
#endif
		
}

static void
pl310_wbinv_range(vm_paddr_t start, vm_size_t size)
{
	
	if (start & g_l2cache_align_mask) {
		size += start & g_l2cache_align_mask;
		start &= ~g_l2cache_align_mask;
	}
	if (size & g_l2cache_align_mask) {
		size &= ~g_l2cache_align_mask;
	   	size += g_l2cache_line_size;
	}
#if 1

	pl310_write4(PL310_DEBUG_CTRL, 3);
#endif
	while (size > 0) {
#if 1
		/* 
		 * Errata 588369 says that clean + inv may keep the 
		 * cache line if it was clean, the recommanded workaround
		 * is to clean then invalidate the cache line, with
		 * write-back and cache linefill disabled
		 */
		   
		pl310_write4(PL310_CLEAN_LINE_PA, start);
		pl310_write4(PL310_INV_LINE_PA, start);
#else
		pl310_write4(PL310_CLEAN_INV_LINE_PA, start);
#endif
		start += g_l2cache_line_size;
		size -= g_l2cache_line_size;
	}
#if 1
	pl310_write4(PL310_DEBUG_CTRL, 0);
#endif
	pl310_wait_background_op(PL310_CLEAN_INV_LINE_PA, 1);
	pl310_cache_sync();
		
}

static void
pl310_wb_range(vm_paddr_t start, vm_size_t size)
{
	
	if (start & g_l2cache_align_mask) {
		size += start & g_l2cache_align_mask;
		start &= ~g_l2cache_align_mask;
	}
	if (size & g_l2cache_align_mask) {
		size &= ~g_l2cache_align_mask;
		size += g_l2cache_line_size;
	}
	while (size > 0) {
		pl310_write4(PL310_CLEAN_LINE_PA, start);
		start += g_l2cache_line_size;
		size -= g_l2cache_line_size;
	}
	pl310_cache_sync();
	pl310_wait_background_op(PL310_CLEAN_LINE_PA, 1);

}

static void
pl310_inv_range(vm_paddr_t start, vm_size_t size)
{

	if (start & g_l2cache_align_mask) {
		size += start & g_l2cache_align_mask;
		start &= ~g_l2cache_align_mask;
	}
	if (size & g_l2cache_align_mask) {
		size &= ~g_l2cache_align_mask;
		size += g_l2cache_line_size;
	}
	while (size > 0) {
		pl310_write4(PL310_INV_LINE_PA, start);
		start += g_l2cache_line_size;
		size -= g_l2cache_line_size;
	}
	pl310_cache_sync();
	pl310_wait_background_op(PL310_INV_LINE_PA, 1);

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
	uint32_t way_size;
	uint32_t ways_assoc;
	uint32_t ctrl_value;

	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 
	    RF_ACTIVE);
	if (sc->sc_mem_res == NULL)
		panic("%s: Cannot map registers", device_get_name(dev));
	pl310_softc = sc;

	platform_init_pl310(sc);
	aux_value = pl310_read4(PL310_AUX_CTRL);
	way_size = (aux_value & PL310_AUX_CTRL_WAY_SIZE_MASK) >>
	    PL310_AUX_CTRL_WAY_SIZE_SHIFT;
	way_size = 1 << (way_size + 13);
	if (aux_value & (1 << PL310_AUX_CTRL_ASSOCIATIVITY_SHIFT))
		ways_assoc = 16;
	else
		ways_assoc = 8;
	g_l2cache_way_mask = (1 << ways_assoc) - 1;
	g_l2cache_size = way_size * ways_assoc;
	/* Print the information */
	printf("  L2 Cache: %uKB/%dB %d ways\n", (g_l2cache_size / 1024),
	       g_l2cache_line_size, ways_assoc);
	ctrl_value = pl310_read4(PL310_CTRL);
	if (!(ctrl_value & 0x1)) {
		/* Enable the L2 cache if disabled */
		pl310_write4(PL310_CTRL, ctrl_value & 0x1);
	}
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

