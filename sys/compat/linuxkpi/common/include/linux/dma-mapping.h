/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 * $FreeBSD$
 */
#ifndef	_LINUXKPI_LINUX_DMA_MAPPING_H_
#define _LINUXKPI_LINUX_DMA_MAPPING_H_

#include <linux/types.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/dma-attrs.h>
#include <linux/scatterlist.h>
#include <linux/mm.h>
#include <linux/page.h>
#include <linux/sizes.h>

#include <sys/systm.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>

#include <machine/bus.h>

enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3,
};

struct dma_map_ops {
	void* (*alloc_coherent)(struct device *dev, size_t size,
	    dma_addr_t *dma_handle, gfp_t gfp);
	void (*free_coherent)(struct device *dev, size_t size,
	    void *vaddr, dma_addr_t dma_handle);
	dma_addr_t (*map_page)(struct device *dev, struct page *page,
	    unsigned long offset, size_t size, enum dma_data_direction dir,
	    unsigned long attrs);
	void (*unmap_page)(struct device *dev, dma_addr_t dma_handle,
	    size_t size, enum dma_data_direction dir, unsigned long attrs);
	int (*map_sg)(struct device *dev, struct scatterlist *sg,
	    int nents, enum dma_data_direction dir, unsigned long attrs);
	void (*unmap_sg)(struct device *dev, struct scatterlist *sg, int nents,
	    enum dma_data_direction dir, unsigned long attrs);
	void (*sync_single_for_cpu)(struct device *dev, dma_addr_t dma_handle,
	    size_t size, enum dma_data_direction dir);
	void (*sync_single_for_device)(struct device *dev,
	    dma_addr_t dma_handle, size_t size, enum dma_data_direction dir);
	void (*sync_single_range_for_cpu)(struct device *dev,
	    dma_addr_t dma_handle, unsigned long offset, size_t size,
	    enum dma_data_direction dir);
	void (*sync_single_range_for_device)(struct device *dev,
	    dma_addr_t dma_handle, unsigned long offset, size_t size,
	    enum dma_data_direction dir);
	void (*sync_sg_for_cpu)(struct device *dev, struct scatterlist *sg,
	    int nents, enum dma_data_direction dir);
	void (*sync_sg_for_device)(struct device *dev, struct scatterlist *sg,
	    int nents, enum dma_data_direction dir);
	int (*mapping_error)(struct device *dev, dma_addr_t dma_addr);
	int (*dma_supported)(struct device *dev, u64 mask);
	int is_phys;
};

#define	DMA_BIT_MASK(n)	((2ULL << ((n) - 1)) - 1ULL)

int linux_dma_tag_init(struct device *, u64);
int linux_dma_tag_init_coherent(struct device *, u64);
void *linux_dma_alloc_coherent(struct device *dev, size_t size,
    dma_addr_t *dma_handle, gfp_t flag);
void *linuxkpi_dmam_alloc_coherent(struct device *dev, size_t size,
    dma_addr_t *dma_handle, gfp_t flag);
dma_addr_t linux_dma_map_phys(struct device *dev, vm_paddr_t phys, size_t len);
void linux_dma_unmap(struct device *dev, dma_addr_t dma_addr, size_t size);
int linux_dma_map_sg_attrs(struct device *dev, struct scatterlist *sgl,
    int nents, enum dma_data_direction dir __unused,
    unsigned long attrs __unused);
void linux_dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sg,
    int nents __unused, enum dma_data_direction dir __unused,
    unsigned long attrs __unused);
void linuxkpi_dma_sync(struct device *, dma_addr_t, size_t, bus_dmasync_op_t);

static inline int
dma_supported(struct device *dev, u64 dma_mask)
{

	/* XXX busdma takes care of this elsewhere. */
	return (1);
}

static inline int
dma_set_mask(struct device *dev, u64 dma_mask)
{

	if (!dev->dma_priv || !dma_supported(dev, dma_mask))
		return -EIO;

	return (linux_dma_tag_init(dev, dma_mask));
}

static inline int
dma_set_coherent_mask(struct device *dev, u64 dma_mask)
{

	if (!dev->dma_priv || !dma_supported(dev, dma_mask))
		return -EIO;

	return (linux_dma_tag_init_coherent(dev, dma_mask));
}

static inline int
dma_set_mask_and_coherent(struct device *dev, u64 dma_mask)
{
	int r;

	r = dma_set_mask(dev, dma_mask);
	if (r == 0)
		dma_set_coherent_mask(dev, dma_mask);
	return (r);
}

static inline void *
dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle,
    gfp_t flag)
{
	return (linux_dma_alloc_coherent(dev, size, dma_handle, flag));
}

static inline void *
dma_zalloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle,
    gfp_t flag)
{

	return (dma_alloc_coherent(dev, size, dma_handle, flag | __GFP_ZERO));
}

static inline void *
dmam_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle,
    gfp_t flag)
{

	return (linuxkpi_dmam_alloc_coherent(dev, size, dma_handle, flag));
}

static inline void
dma_free_coherent(struct device *dev, size_t size, void *cpu_addr,
    dma_addr_t dma_addr)
{

	linux_dma_unmap(dev, dma_addr, size);
	kmem_free(cpu_addr, size);
}

static inline dma_addr_t
dma_map_page_attrs(struct device *dev, struct page *page, size_t offset,
    size_t size, enum dma_data_direction dir, unsigned long attrs)
{

	return (linux_dma_map_phys(dev, VM_PAGE_TO_PHYS(page) + offset, size));
}

/* linux_dma_(un)map_sg_attrs does not support attrs yet */
#define	dma_map_sg_attrs(dev, sgl, nents, dir, attrs)	\
	linux_dma_map_sg_attrs(dev, sgl, nents, dir, 0)

#define	dma_unmap_sg_attrs(dev, sg, nents, dir, attrs)	\
	linux_dma_unmap_sg_attrs(dev, sg, nents, dir, 0)

static inline dma_addr_t
dma_map_page(struct device *dev, struct page *page,
    unsigned long offset, size_t size, enum dma_data_direction direction)
{

	return (linux_dma_map_phys(dev, VM_PAGE_TO_PHYS(page) + offset, size));
}

static inline void
dma_unmap_page(struct device *dev, dma_addr_t dma_address, size_t size,
    enum dma_data_direction direction)
{

	linux_dma_unmap(dev, dma_address, size);
}

static inline void
dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma, size_t size,
    enum dma_data_direction direction)
{
	bus_dmasync_op_t op;

	switch (direction) {
	case DMA_BIDIRECTIONAL:
		op = BUS_DMASYNC_POSTREAD;
		linuxkpi_dma_sync(dev, dma, size, op);
		op = BUS_DMASYNC_PREREAD;
		break;
	case DMA_TO_DEVICE:
		op = BUS_DMASYNC_POSTWRITE;
		break;
	case DMA_FROM_DEVICE:
		op = BUS_DMASYNC_POSTREAD;
		break;
	default:
		return;
	}

	linuxkpi_dma_sync(dev, dma, size, op);
}

static inline void
dma_sync_single(struct device *dev, dma_addr_t addr, size_t size,
    enum dma_data_direction dir)
{
	dma_sync_single_for_cpu(dev, addr, size, dir);
}

static inline void
dma_sync_single_for_device(struct device *dev, dma_addr_t dma,
    size_t size, enum dma_data_direction direction)
{
	bus_dmasync_op_t op;

	switch (direction) {
	case DMA_BIDIRECTIONAL:
		op = BUS_DMASYNC_PREWRITE;
		break;
	case DMA_TO_DEVICE:
		op = BUS_DMASYNC_PREREAD;
		break;
	case DMA_FROM_DEVICE:
		op = BUS_DMASYNC_PREWRITE;
		break;
	default:
		return;
	}

	linuxkpi_dma_sync(dev, dma, size, op);
}

static inline void
dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg, int nelems,
    enum dma_data_direction direction)
{
}

static inline void
dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg, int nelems,
    enum dma_data_direction direction)
{
}

static inline void
dma_sync_single_range_for_cpu(struct device *dev, dma_addr_t dma_handle,
    unsigned long offset, size_t size, int direction)
{
}

static inline void
dma_sync_single_range_for_device(struct device *dev, dma_addr_t dma_handle,
    unsigned long offset, size_t size, int direction)
{
}

#define	DMA_MAPPING_ERROR	(~(dma_addr_t)0)

static inline int
dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{

	if (dma_addr == 0 || dma_addr == DMA_MAPPING_ERROR)
		return (-ENOMEM);
	return (0);
}

static inline unsigned int dma_set_max_seg_size(struct device *dev,
    unsigned int size)
{
	return (0);
}

static inline dma_addr_t
_dma_map_single_attrs(struct device *dev, void *ptr, size_t size,
    enum dma_data_direction direction, unsigned long attrs __unused)
{
	dma_addr_t dma;

	dma = linux_dma_map_phys(dev, vtophys(ptr), size);
	if (!dma_mapping_error(dev, dma))
		dma_sync_single_for_device(dev, dma, size, direction);

	return (dma);
}

static inline void
_dma_unmap_single_attrs(struct device *dev, dma_addr_t dma, size_t size,
    enum dma_data_direction direction, unsigned long attrs __unused)
{

	dma_sync_single_for_cpu(dev, dma, size, direction);
	linux_dma_unmap(dev, dma, size);
}

static inline size_t
dma_max_mapping_size(struct device *dev)
{

	return (SCATTERLIST_MAX_SEGMENT);
}

#define	dma_map_single_attrs(dev, ptr, size, dir, attrs)	\
	_dma_map_single_attrs(dev, ptr, size, dir, 0)

#define	dma_unmap_single_attrs(dev, dma_addr, size, dir, attrs)	\
	_dma_unmap_single_attrs(dev, dma_addr, size, dir, 0)

#define dma_map_single(d, a, s, r) dma_map_single_attrs(d, a, s, r, 0)
#define dma_unmap_single(d, a, s, r) dma_unmap_single_attrs(d, a, s, r, 0)
#define dma_map_sg(d, s, n, r) dma_map_sg_attrs(d, s, n, r, 0)
#define dma_unmap_sg(d, s, n, r) dma_unmap_sg_attrs(d, s, n, r, 0)

#define	DEFINE_DMA_UNMAP_ADDR(name)		dma_addr_t name
#define	DEFINE_DMA_UNMAP_LEN(name)		__u32 name
#define	dma_unmap_addr(p, name)			((p)->name)
#define	dma_unmap_addr_set(p, name, v)		(((p)->name) = (v))
#define	dma_unmap_len(p, name)			((p)->name)
#define	dma_unmap_len_set(p, name, v)		(((p)->name) = (v))

extern int uma_align_cache;
#define	dma_get_cache_alignment()	uma_align_cache


static inline int
dma_map_sgtable(struct device *dev, struct sg_table *sgt,
    enum dma_data_direction dir,
    unsigned long attrs)
{

	return (dma_map_sg_attrs(dev, sgt->sgl, sgt->nents, dir, attrs));
}

static inline void
dma_unmap_sgtable(struct device *dev, struct sg_table *sgt,
    enum dma_data_direction dir,
    unsigned long attrs)
{

	dma_unmap_sg_attrs(dev, sgt->sgl, sgt->nents, dir, attrs);
}


#endif	/* _LINUXKPI_LINUX_DMA_MAPPING_H_ */
