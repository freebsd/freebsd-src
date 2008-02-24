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
__FBSDID("$FreeBSD: src/sys/arm/xscale/i80321/i80321_aau.c,v 1.4 2006/03/02 14:06:38 cognet Exp $");

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

typedef struct i80321_aaudesc_s {
	vm_paddr_t next_desc;
	uint32_t sar[4];
	vm_paddr_t local_addr;
	vm_size_t count;
	uint32_t descr_ctrl;
} __packed	i80321_aaudesc_t;

typedef struct i80321_aauring_s {
	i80321_aaudesc_t *desc;
	vm_paddr_t phys_addr;
	bus_dmamap_t map;
} i80321_aauring_t;

#define AAU_RING_SIZE 64

struct i80321_aau_softc {
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_aau_sh;
	bus_dma_tag_t dmatag;
	i80321_aauring_t aauring[AAU_RING_SIZE];
	int flags;
#define BUSY	0x1
	int unit;
	struct mtx mtx;
};

static int
i80321_aau_probe(device_t dev)
{
	device_set_desc(dev, "I80321 AAU");
	return (0);
}

static struct i80321_aau_softc *aau_softc;

static void
i80321_mapphys(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	vm_paddr_t *addr = (vm_paddr_t *)arg;

	*addr = segs->ds_addr;
}

#define AAU_REG_WRITE(softc, reg, val) \
    bus_space_write_4((softc)->sc_st, (softc)->sc_aau_sh, \
	(reg), (val))
#define AAU_REG_READ(softc, reg) \
    bus_space_read_4((softc)->sc_st, (softc)->sc_aau_sh, \
	(reg))

static int aau_bzero(void *, int, int);

static int
i80321_aau_attach(device_t dev)
{
	struct i80321_aau_softc *softc = device_get_softc(dev);
	struct i80321_softc *sc = device_get_softc(device_get_parent(dev));
	struct i80321_aaudesc_s *aaudescs;

	mtx_init(&softc->mtx, "AAU mtx", NULL, MTX_SPIN);
	softc->sc_st = sc->sc_st;
	if (bus_space_subregion(softc->sc_st, sc->sc_sh, VERDE_AAU_BASE, 
	    VERDE_AAU_SIZE, &softc->sc_aau_sh) != 0)
		panic("%s: unable to subregion AAU registers",
		    device_get_name(dev));
	if (bus_dma_tag_create(NULL, sizeof(i80321_aaudesc_t), 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, 
	    AAU_RING_SIZE * sizeof(i80321_aaudesc_t),
	    1, sizeof(i80321_aaudesc_t), BUS_DMA_ALLOCNOW, busdma_lock_mutex, 
	    &Giant, &softc->dmatag))
		panic("Couldn't create a dma tag");
	if (bus_dmamem_alloc(softc->dmatag, (void **)&aaudescs,
    	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT, &softc->aauring[0].map))
		panic("Couldn't alloc dma memory");

	for (int i = 0; i < AAU_RING_SIZE; i++) {
		if (i > 0)
			if (bus_dmamap_create(softc->dmatag, 0,
			    &softc->aauring[i].map))
				panic("Couldn't create dma map");
		softc->aauring[i].desc = &aaudescs[i];
		bus_dmamap_load(softc->dmatag, softc->aauring[i].map,
		    softc->aauring[i].desc, sizeof(i80321_aaudesc_t),
		    i80321_mapphys, &softc->aauring[i].phys_addr, 0);
		bzero(softc->aauring[i].desc, sizeof(i80321_aaudesc_t));
	}
	aau_softc = softc;
	_arm_bzero = aau_bzero;
	_min_bzero_size = 1024;
	return (0);
}

static __inline void
test_virt_addr(void *addr, int len)
{
	int to_nextpage;

	while (len > 0) {
		*(char *)addr = 0;
		to_nextpage = ((vm_offset_t)addr & ~PAGE_MASK) +
		    PAGE_SIZE - (vm_offset_t)addr;
		if (to_nextpage >= len)
			break;
		len -= to_nextpage;
		addr = (void *)((vm_offset_t)addr + to_nextpage);
	}
}

static int
aau_bzero(void *dst, int len, int flags)
{
	struct i80321_aau_softc *sc = aau_softc;
	i80321_aaudesc_t *desc;
	int ret;
	int csr;
	int descnb = 0;
	int tmplen = len;
	int to_nextpagedst;
	int min_hop;
	vm_paddr_t pa, tmppa;

	if (!sc)
		return (-1);
	mtx_lock_spin(&sc->mtx);
	if (sc->flags & BUSY) {
		mtx_unlock_spin(&sc->mtx);
		return (-1);
	}
	sc->flags |= BUSY;
	mtx_unlock_spin(&sc->mtx);
	desc = sc->aauring[0].desc;
	if (flags & IS_PHYSICAL) {
		desc->local_addr = (vm_paddr_t)dst;
		desc->next_desc = 0;
		desc->count = len;
		desc->descr_ctrl = 2 << 1 | 1 << 31; /* Fill, enable dest write */
		bus_dmamap_sync(sc->dmatag, sc->aauring[0].map, 
		    BUS_DMASYNC_PREWRITE);
	} else {
		test_virt_addr(dst, len);
		if ((vm_offset_t)dst & (31))
			cpu_dcache_wb_range((vm_offset_t)dst & ~31, 32);
		if (((vm_offset_t)dst + len) & 31)
			cpu_dcache_wb_range(((vm_offset_t)dst + len) & ~31,
			    32);
		cpu_dcache_inv_range((vm_offset_t)dst, len);
		while (tmplen > 0) {
			pa = vtophys(dst);
			to_nextpagedst = ((vm_offset_t)dst & ~PAGE_MASK) +
			    PAGE_SIZE - (vm_offset_t)dst;
			while (to_nextpagedst < tmplen) {
				tmppa = vtophys((vm_offset_t)dst +
				    to_nextpagedst);
				if (tmppa != pa + to_nextpagedst)
					break;
				to_nextpagedst += PAGE_SIZE;
			}
			min_hop = to_nextpagedst;
			if (min_hop < 64) {
				tmplen -= min_hop;
				bzero(dst, min_hop);
				cpu_dcache_wbinv_range((vm_offset_t)dst,
				    min_hop);

				dst = (void *)((vm_offset_t)dst + min_hop);
				if (tmplen <= 0 && descnb > 0) {
					sc->aauring[descnb - 1].desc->next_desc
					    = 0;
					bus_dmamap_sync(sc->dmatag, 
					    sc->aauring[descnb - 1].map, 
					    BUS_DMASYNC_PREWRITE);
				}
				continue;
			}
			desc->local_addr = pa;
			desc->count = tmplen > min_hop ? min_hop : tmplen;
			desc->descr_ctrl = 2 << 1 | 1 << 31; /* Fill, enable dest write */;
			if (min_hop < tmplen) {
				tmplen -= min_hop;
				dst = (void *)((vm_offset_t)dst + min_hop);
			} else
				tmplen = 0;
			if (descnb + 1 >= AAU_RING_SIZE) {
				mtx_lock_spin(&sc->mtx);
				sc->flags &= ~BUSY;
				mtx_unlock_spin(&sc->mtx);
				return (-1);
			}
			if (tmplen > 0) {
				desc->next_desc = sc->aauring[descnb + 1].
				    phys_addr;
				bus_dmamap_sync(sc->dmatag, 
				    sc->aauring[descnb].map, 
				    BUS_DMASYNC_PREWRITE);
				desc = sc->aauring[descnb + 1].desc;
				descnb++;
			} else {
				desc->next_desc = 0;
				bus_dmamap_sync(sc->dmatag, 
				    sc->aauring[descnb].map, 
				    BUS_DMASYNC_PREWRITE);
			}
									
		}

	}
	AAU_REG_WRITE(sc, 0x0c /* Descriptor addr */,
	    sc->aauring[0].phys_addr);
	AAU_REG_WRITE(sc, 0 /* Control register */, 1 << 0/* Start transfer */);
	while ((csr = AAU_REG_READ(sc, 0x4)) & (1 << 10));
	/* Wait until it's done. */
	if (csr & (1 << 5)) /* error */
		ret = -1;
	else
		ret = 0;
	/* Clear the interrupt. */
	AAU_REG_WRITE(sc, 0x4, csr);
	/* Stop the AAU. */
	AAU_REG_WRITE(sc, 0, 0);
	mtx_lock_spin(&sc->mtx);
	sc->flags &= ~BUSY;
	mtx_unlock_spin(&sc->mtx);
	return (ret);
}

static device_method_t i80321_aau_methods[] = {
	DEVMETHOD(device_probe, i80321_aau_probe),
	DEVMETHOD(device_attach, i80321_aau_attach),
	{0, 0},
};

static driver_t i80321_aau_driver = {
	"i80321_aau",
	i80321_aau_methods,
	sizeof(struct i80321_aau_softc),
};

static devclass_t i80321_aau_devclass;

DRIVER_MODULE(i80321_aau, iq, i80321_aau_driver, i80321_aau_devclass, 0, 0);
