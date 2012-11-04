/*-
 * Copyright (c) 2005 Olivier Houchard.  All rights reserved.
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/md_var.h>

#include <arm/xscale/i80321/i80321reg.h>
#include <arm/xscale/i80321/i80321var.h>
#include <arm/xscale/i80321/iq80321reg.h>
#include <arm/xscale/i80321/iq80321var.h>
#include <arm/xscale/i80321/i80321_intr.h>

typedef struct i80321_dmadesc_s {
	vm_paddr_t next_desc;
	vm_paddr_t low_pciaddr;
	vm_paddr_t high_pciaddr;
	vm_paddr_t local_addr;
	vm_size_t count;
	uint32_t descr_ctrl;
	uint64_t unused;
} __packed	i80321_dmadesc_t;

typedef struct i80321_dmaring_s {
	i80321_dmadesc_t *desc;
	vm_paddr_t phys_addr;
	bus_dmamap_t map;
} i80321_dmaring_t;

#define DMA_RING_SIZE 64

struct i80321_dma_softc {
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_dma_sh;
	bus_dma_tag_t dmatag;
	i80321_dmaring_t dmaring[DMA_RING_SIZE];
	int flags;
#define BUSY	0x1
	int unit;
	struct mtx mtx;
};

static int
i80321_dma_probe(device_t dev)
{
	device_set_desc(dev, "I80321 DMA Unit");
	return (0);
}

static struct i80321_dma_softc *softcs[2]; /* XXX */

static void
i80321_mapphys(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	vm_paddr_t *addr = (vm_paddr_t *)arg;

	*addr = segs->ds_addr;
}

#define DMA_REG_WRITE(softc, reg, val) \
    bus_space_write_4((softc)->sc_st, (softc)->sc_dma_sh, \
	(reg), (val))
#define DMA_REG_READ(softc, reg) \
    bus_space_read_4((softc)->sc_st, (softc)->sc_dma_sh, \
	(reg))

#define DMA_CLEAN_MASK (0x2|0x4|0x8|0x20|0x100|0x200)
static int dma_memcpy(void *, void *, int, int);

static int
i80321_dma_attach(device_t dev)
{
	struct i80321_dma_softc *softc = device_get_softc(dev);
	struct i80321_softc *sc = device_get_softc(device_get_parent(dev));
	int unit = device_get_unit(dev);
	i80321_dmadesc_t *dmadescs;

	mtx_init(&softc->mtx, "DMA engine mtx", NULL, MTX_SPIN);
	softc->sc_st = sc->sc_st;
	if (bus_space_subregion(softc->sc_st, sc->sc_sh, unit == 0 ?
	    VERDE_DMA_BASE0 : VERDE_DMA_BASE1, VERDE_DMA_SIZE,
	    &softc->sc_dma_sh) != 0)
		panic("%s: unable to subregion DMA registers",
		    device_get_name(dev));
	if (bus_dma_tag_create(NULL, sizeof(i80321_dmadesc_t),
	    0, BUS_SPACE_MAXADDR,  BUS_SPACE_MAXADDR, NULL, NULL,
	    DMA_RING_SIZE * sizeof(i80321_dmadesc_t), 1,
	    sizeof(i80321_dmadesc_t), BUS_DMA_ALLOCNOW, busdma_lock_mutex,
	    &Giant, &softc->dmatag))
		panic("Couldn't create a dma tag");
	DMA_REG_WRITE(softc, 0, 0);
	if (bus_dmamem_alloc(softc->dmatag, (void **)&dmadescs,
    	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT, &softc->dmaring[0].map))
		panic("Couldn't alloc dma memory");
	for (int i = 0; i < DMA_RING_SIZE; i++) {
		if (i > 0)
			if (bus_dmamap_create(softc->dmatag, 0,
			    &softc->dmaring[i].map))
				panic("Couldn't alloc dmamap");
		softc->dmaring[i].desc = &dmadescs[i];	
		bus_dmamap_load(softc->dmatag, softc->dmaring[i].map,
		    softc->dmaring[i].desc, sizeof(i80321_dmadesc_t),
		    i80321_mapphys, &softc->dmaring[i].phys_addr, 0);
	}
	softc->unit = unit;
	softcs[unit] = softc;
	_arm_memcpy = dma_memcpy;
	_min_memcpy_size = 1024;
	return (0);
}

static __inline int
virt_addr_is_valid(void *addr, int len, int write, int is_kernel)
{
	int to_nextpage;
	char tmp = 0;

	while (len > 0) {
		if (write) {
			if (is_kernel)
				*(char *)addr = 0;
			else if (subyte(addr, 0) != 0) {
				return (0);
			}
		} else {
			if (is_kernel)
				badaddr_read(addr, 1, &tmp);
			else if (fubyte(addr) == -1) {
				return (0);
			}
		}
		to_nextpage = ((vm_offset_t)addr & ~PAGE_MASK) +
		    PAGE_SIZE - (vm_offset_t)addr;
		if (to_nextpage >= len)
			break;
		len -= to_nextpage;
		addr = (void *)((vm_offset_t)addr + to_nextpage);
	}
	return (1);
}

static int
dma_memcpy(void *dst, void *src, int len, int flags)
{
	struct i80321_dma_softc *sc;
	i80321_dmadesc_t *desc;
	int ret;
	int csr;
	int descnb = 0;
	int tmplen = len;
	int to_nextpagesrc, to_nextpagedst;
	int min_hop;
	vm_paddr_t pa, pa2, tmppa;
	pmap_t pmap = vmspace_pmap(curthread->td_proc->p_vmspace);

	if (!softcs[0] || !softcs[1])
		return (-1);
	mtx_lock_spin(&softcs[0]->mtx);
	if (softcs[0]->flags & BUSY) {
		mtx_unlock_spin(&softcs[0]->mtx);
		mtx_lock_spin(&softcs[1]->mtx);
		if (softcs[1]->flags & BUSY) {
			mtx_unlock(&softcs[1]->mtx);
			return (-1);
		}
		sc = softcs[1];
	} else
		sc = softcs[0];
	sc->flags |= BUSY;
	mtx_unlock_spin(&sc->mtx);
	desc = sc->dmaring[0].desc;
	if (flags & IS_PHYSICAL) {
		desc->next_desc = 0;
		desc->low_pciaddr = (vm_paddr_t)src;
		desc->high_pciaddr = 0;
		desc->local_addr = (vm_paddr_t)dst;
		desc->count = len;
		desc->descr_ctrl = 1 << 6; /* Local memory to local memory. */
		bus_dmamap_sync(sc->dmatag,
		    sc->dmaring[0].map,
		    BUS_DMASYNC_PREWRITE);
	} else {
		if (!virt_addr_is_valid(dst, len, 1, !(flags & DST_IS_USER)) ||
		    !virt_addr_is_valid(src, len, 0, !(flags & SRC_IS_USER))) {
			mtx_lock_spin(&sc->mtx);
			sc->flags &= ~BUSY;
			mtx_unlock_spin(&sc->mtx);
			return (-1);
		}
		cpu_dcache_wb_range((vm_offset_t)src, len);
		if ((vm_offset_t)dst & (31))
			cpu_dcache_wb_range((vm_offset_t)dst & ~31, 32);
		if (((vm_offset_t)dst + len) & 31)
			cpu_dcache_wb_range(((vm_offset_t)dst + len) & ~31,
			    32);
		cpu_dcache_inv_range((vm_offset_t)dst, len);
		while (tmplen > 0) {
			pa = (flags & SRC_IS_USER) ?
			    pmap_extract(pmap, (vm_offset_t)src) :
				    vtophys(src);
			pa2 = (flags & DST_IS_USER) ?
			    pmap_extract(pmap, (vm_offset_t)dst) :
				    vtophys(dst);
			to_nextpagesrc = ((vm_offset_t)src & ~PAGE_MASK) +
			    PAGE_SIZE - (vm_offset_t)src;
			to_nextpagedst = ((vm_offset_t)dst & ~PAGE_MASK) +
			    PAGE_SIZE - (vm_offset_t)dst;
			while (to_nextpagesrc < tmplen) {
				tmppa = (flags & SRC_IS_USER) ?
				    pmap_extract(pmap, (vm_offset_t)src +
				    to_nextpagesrc) :
					    vtophys((vm_offset_t)src +
						to_nextpagesrc);
				if (tmppa != pa + to_nextpagesrc)
					break;
				to_nextpagesrc += PAGE_SIZE;
			}
			while (to_nextpagedst < tmplen) {
				tmppa = (flags & DST_IS_USER) ?
				    pmap_extract(pmap, (vm_offset_t)dst +
				    to_nextpagedst) :
					    vtophys((vm_offset_t)dst +
						to_nextpagedst);
				if (tmppa != pa2 + to_nextpagedst)
					break;
				to_nextpagedst += PAGE_SIZE;
			}
			min_hop = to_nextpagedst > to_nextpagesrc ?
			    to_nextpagesrc : to_nextpagedst;
			if (min_hop < 64) {
				tmplen -= min_hop;
				memcpy(dst, src, min_hop);
				cpu_dcache_wbinv_range((vm_offset_t)dst,
				    min_hop);

				src = (void *)((vm_offset_t)src + min_hop);
				dst = (void *)((vm_offset_t)dst + min_hop);
				if (tmplen <= 0 && descnb > 0) {
					sc->dmaring[descnb - 1].desc->next_desc
					    = 0;
					bus_dmamap_sync(sc->dmatag,
					    sc->dmaring[descnb - 1].map,
					    BUS_DMASYNC_PREWRITE);
				}
				continue;
			}
			desc->low_pciaddr = pa;
			desc->high_pciaddr = 0;
			desc->local_addr = pa2;
			desc->count = tmplen > min_hop ? min_hop : tmplen;
			desc->descr_ctrl = 1 << 6;
			if (min_hop < tmplen) {
				tmplen -= min_hop;
				src = (void *)((vm_offset_t)src + min_hop);
				dst = (void *)((vm_offset_t)dst + min_hop);
			} else
				tmplen = 0;
			if (descnb + 1 >= DMA_RING_SIZE) {
				mtx_lock_spin(&sc->mtx);
				sc->flags &= ~BUSY;
				mtx_unlock_spin(&sc->mtx);
				return (-1);
			}
			if (tmplen > 0) {
				desc->next_desc = sc->dmaring[descnb + 1].
				    phys_addr;
				bus_dmamap_sync(sc->dmatag,
				    sc->dmaring[descnb].map,
				    BUS_DMASYNC_PREWRITE);
				desc = sc->dmaring[descnb + 1].desc;
				descnb++;
			} else {
				desc->next_desc = 0;
				bus_dmamap_sync(sc->dmatag,
				    sc->dmaring[descnb].map,
				    BUS_DMASYNC_PREWRITE);
			}
									
		}

	}
	DMA_REG_WRITE(sc, 4 /* Status register */,
	    DMA_REG_READ(sc, 4) | DMA_CLEAN_MASK);
	DMA_REG_WRITE(sc, 0x10 /* Descriptor addr */,
	    sc->dmaring[0].phys_addr);
	DMA_REG_WRITE(sc, 0 /* Control register */, 1 | 2/* Start transfer */);
	while ((csr = DMA_REG_READ(sc, 0x4)) & (1 << 10));
	/* Wait until it's done. */
	if (csr & 0x2e) /* error */
		ret = -1;
	else
		ret = 0;
	DMA_REG_WRITE(sc, 0, 0);
	mtx_lock_spin(&sc->mtx);
	sc->flags &= ~BUSY;
	mtx_unlock_spin(&sc->mtx);
	return (ret);
}

static device_method_t i80321_dma_methods[] = {
	DEVMETHOD(device_probe, i80321_dma_probe),
	DEVMETHOD(device_attach, i80321_dma_attach),
	{0, 0},
};

static driver_t i80321_dma_driver = {
	"i80321_dma",
	i80321_dma_methods,
	sizeof(struct i80321_dma_softc),
};

static devclass_t i80321_dma_devclass;

DRIVER_MODULE(i80321_dma, iq, i80321_dma_driver, i80321_dma_devclass, 0, 0);
