/*-
 * Copyright (c) 1998 Doug Rabson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <machine/bus.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <machine/sgmap.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

MALLOC_DEFINE(M_SGMAP, "sgmap", "Scatter Gather mapping");

struct sgmap {
	struct rman	rm;	/* manage range of bus addresses */
	sgmap_map_callback *map; /* map one page in the sgmap */
	void		*arg;	/* argument to map function */
	bus_addr_t	sba;
	bus_addr_t	eba;
};

void *overflow_page = 0;
vm_offset_t overflow_page_pa;

vm_offset_t
sgmap_overflow_page(void)
{
	/*
	 * Allocate the overflow page if necessary.
	 */
	if (!overflow_page) {
		overflow_page = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
		if (!overflow_page)
			panic("sgmap_alloc_region: can't allocate overflow page");
		overflow_page_pa = pmap_kextract((vm_offset_t) overflow_page);
	}

	return overflow_page_pa;
}

/*
 * Create an sgmap to manage a range of bus addresses which map
 * physical memory using a scatter-gather map.
 */
struct sgmap *
sgmap_map_create(bus_addr_t sba, bus_addr_t eba,
		 sgmap_map_callback *map, void *arg)
{
	struct sgmap *sgmap;
    
	sgmap = malloc(sizeof *sgmap, M_SGMAP, M_NOWAIT);
	if (!sgmap)
		return 0;


	sgmap->rm.rm_start = sba;
	sgmap->rm.rm_end = eba;
	sgmap->rm.rm_type = RMAN_ARRAY;
	sgmap->rm.rm_descr = "Scatter Gather Bus Addresses";
	rman_init(&sgmap->rm);
	rman_manage_region(&sgmap->rm, sba, eba);
	sgmap->map = map;
	sgmap->arg = arg;
	sgmap->sba = sba;
	sgmap->eba = eba;

	return sgmap;
}

/*
 * Destroy an sgmap created with sgmap_map_create().
 */
void
sgmap_map_destroy(struct sgmap *sgmap)
{
	rman_fini(&sgmap->rm);
	free(sgmap, M_SGMAP);
}

/*
 * Map a range of virtual addresses using the sgmap and return the bus 
 * address of the mapped region. An opaque handle for the mapped
 * region is also returned in *mhp. This handle should be passed to
 * sgmap_free_region() when the mapping is no longer required.
 */
bus_addr_t
sgmap_alloc_region(struct sgmap *sgmap,
		   bus_size_t size,
		   bus_size_t boundary,
		   void **mhp)
{
	struct resource *res;
	bus_addr_t ba, nba;

	/*
	 * This ensures allocations are on page boundaries. The extra
	 * page is used as a guard page since dma prefetching can
	 * generate accesses to addresses outside the transfer range.
	 */
	size = round_page(size);

	/*
	 * Attempt to allocate within each boundary delimited region.
	 */
	res = 0;
	for (ba = sgmap->sba; ba < sgmap->eba; ba = nba) {
		nba = (ba + boundary) & ~(boundary - 1);
		res = rman_reserve_resource(&sgmap->rm,
					    ba, nba - 1, size + PAGE_SIZE,
					    RF_ACTIVE, 0);
		if (res)
			break;
	}

	if (res == 0)
		return 0;

	*mhp = (void *) res;
	return rman_get_start(res);
}

void
sgmap_load_region(struct sgmap *sgmap,
		  bus_addr_t sba,
		  vm_offset_t va,
		  bus_size_t size)
{
	bus_addr_t ba, eba;

	/*
	 * Call the chipset to map each page in the mapped range to
	 * the correct physical page.
	 */
	for (ba = sba, eba = sba + size; ba < eba;
	     ba += PAGE_SIZE, va += PAGE_SIZE) {
		vm_offset_t pa = pmap_kextract(va);
		sgmap->map(sgmap->arg, ba, pa);
	}
	sgmap->map(sgmap->arg, ba, overflow_page_pa);
}

void
sgmap_unload_region(struct sgmap *sgmap,
		    bus_addr_t sba,
		    bus_size_t size)
{
	bus_addr_t ba, eba;

	/*
	 * Call the chipset to unmap each page.
	 */
	for (ba = sba, eba = sba + size; ba < eba; ba += PAGE_SIZE) {
		sgmap->map(sgmap->arg, ba, 0);
	}
	sgmap->map(sgmap->arg, ba, 0);
}

/*
 * Free a region allocated using sgmap_alloc_region().
 */
void
sgmap_free_region(struct sgmap *sgmap, void *mh)
{
	struct resource *res = mh;
	rman_release_resource(res);
}
