/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
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
 */
#ifndef	_LINUX_DMA_MAPPING_H_
#define _LINUX_DMA_MAPPING_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/systm.h>

#include <linux/device.h>
#include <linux/err.h>
#include <linux/dma-attrs.h>
#include <linux/scatterlist.h>
#include <linux/page.h>

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
	    struct dma_attrs *attrs);
	void (*unmap_page)(struct device *dev, dma_addr_t dma_handle,
	    size_t size, enum dma_data_direction dir, struct dma_attrs *attrs);
	int (*map_sg)(struct device *dev, struct scatterlist *sg,
	    int nents, enum dma_data_direction dir, struct dma_attrs *attrs);
	void (*unmap_sg)(struct device *dev, struct scatterlist *sg, int nents,
	    enum dma_data_direction dir, struct dma_attrs *attrs);
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

static inline int
dma_supported(struct device *dev, u64 mask)
{
	KASSERT(0, ("%s", __FUNCTION__));
}
 
static inline int
dma_set_mask(struct device *dev, u64 dma_mask)
{
	KASSERT(0, ("%s", __FUNCTION__));
}

static inline void *
dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle,
    gfp_t flag)
{
	KASSERT(0, ("%s", __FUNCTION__));
}
                       
static inline void
dma_free_coherent(struct device *dev, size_t size, void *cpu_addr,
    dma_addr_t dma_handle)
{
	KASSERT(0, ("%s", __FUNCTION__));
}
 
static inline dma_addr_t
dma_map_single(struct device *dev, void *cpu_addr, size_t size,
    enum dma_data_direction direction)
{
	KASSERT(0, ("%s", __FUNCTION__));
}
 
static inline void
dma_unmap_single(struct device *dev, dma_addr_t dma_addr, size_t size,
    enum dma_data_direction direction)
{
	KASSERT(0, ("%s", __FUNCTION__));
}
 
static inline dma_addr_t
dma_map_page(struct device *dev, struct page *page,
    unsigned long offset, size_t size, enum dma_data_direction direction)
{
	KASSERT(0, ("%s", __FUNCTION__));
;
}

static inline void
dma_unmap_page(struct device *dev, dma_addr_t dma_address, size_t size,
    enum dma_data_direction direction)
{
	KASSERT(0, ("%s", __FUNCTION__));
}
 
static inline int
dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
    enum dma_data_direction direction)
{
	KASSERT(0, ("%s", __FUNCTION__));
}

static inline void
dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nhwentries,
    enum dma_data_direction direction)
{  
	KASSERT(0, ("%s", __FUNCTION__));
}

static inline void
dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma_handle, size_t size,
    enum dma_data_direction direction)
{
	KASSERT(0, ("%s", __FUNCTION__));
}

static inline void
dma_sync_single_for_device(struct device *dev, dma_addr_t dma_handle,
    size_t size, enum dma_data_direction direction)
{
	KASSERT(0, ("%s", __FUNCTION__));
}

static inline void
dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg, int nelems,
    enum dma_data_direction direction)
{
	KASSERT(0, ("%s", __FUNCTION__));
}

static inline void
dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg, int nelems,
    enum dma_data_direction direction)
{
	KASSERT(0, ("%s", __FUNCTION__));
}

static inline int
dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	KASSERT(0, ("%s", __FUNCTION__));
}

static inline dma_addr_t dma_map_single_attrs(struct device *dev, void *ptr,
    size_t size, enum dma_data_direction dir, struct dma_attrs *attrs)
{
	KASSERT(0, ("%s", __FUNCTION__));
}

static inline void dma_unmap_single_attrs(struct device *dev, dma_addr_t addr,
    size_t size, enum dma_data_direction dir, struct dma_attrs *attrs)
{
	KASSERT(0, ("%s", __FUNCTION__));
}

static inline int dma_map_sg_attrs(struct device *dev, struct scatterlist *sg,
    int nents, enum dma_data_direction dir, struct dma_attrs *attrs)
{
	KASSERT(0, ("%s", __FUNCTION__));
}

static inline void dma_unmap_sg_attrs(struct device *dev,
    struct scatterlist *sg, int nents, enum dma_data_direction dir,
    struct dma_attrs *attrs)
{
	KASSERT(0, ("%s", __FUNCTION__));
}

#define dma_map_single(d, a, s, r) dma_map_single_attrs(d, a, s, r, NULL)
#define dma_unmap_single(d, a, s, r) dma_unmap_single_attrs(d, a, s, r, NULL)
#define dma_map_sg(d, s, n, r) dma_map_sg_attrs(d, s, n, r, NULL)
#define dma_unmap_sg(d, s, n, r) dma_unmap_sg_attrs(d, s, n, r, NULL)

#endif	/* _LINUX_DMA_MAPPING_H_ */
