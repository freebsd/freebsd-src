/*-
 * Copyright (c) 2023-2025 Bjoern A. Zeeb
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
};

#define	PP_FLAG_DMA_MAP		BIT(0)
#define	PP_FLAG_DMA_SYNC_DEV	BIT(1)
#define	PP_FLAG_PAGE_FRAG	BIT(2)

#endif	/* _LINUXKPI_NET_PAGE_POOL_TYPES_H */
