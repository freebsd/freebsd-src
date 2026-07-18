/*
 * Copyright (c) 2023-2025 Bjoern A. Zeeb
 * Copyright (c) 2025-2026 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_LINUXKPI_NET_PAGE_POOL_TYPES_H
#define	_LINUXKPI_NET_PAGE_POOL_TYPES_H

#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <net/netmem.h>

struct device;
struct napi_struct;

struct page_pool_params {
	struct device			*dev;
	uint32_t			flags;
	uint32_t			order;
	uint32_t			pool_size;
	uint32_t			max_len;
	uint32_t			offset;
	int				nid;		/* NUMA */
	enum dma_data_direction		dma_dir;
	struct napi_struct		*napi;
};

struct page_pool {
	struct page_pool_params		params;
};

#define	PP_FLAG_DMA_MAP		BIT(0)
#define	PP_FLAG_DMA_SYNC_DEV	BIT(1)
#define	PP_FLAG_PAGE_FRAG	BIT(2)
#define	PP_FLAGS_ALL		(PP_FLAG_DMA_MAP | PP_FLAG_DMA_SYNC_DEV | \
    PP_FLAG_PAGE_FRAG)

struct page *linuxkpi_page_pool_alloc_frag(struct page_pool *, uint32_t *,
    size_t, gfp_t);

static inline struct page *
page_pool_alloc_frag(struct page_pool *pp, uint32_t *offset,
    size_t size, gfp_t gfp)
{
	return (linuxkpi_page_pool_alloc_frag(pp, offset, size, gfp));
}

#endif	/* _LINUXKPI_NET_PAGE_POOL_TYPES_H */
