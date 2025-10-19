/*-
 * Copyright (c) 2023-2025 Bjoern A. Zeeb
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_LINUXKPI_NET_PAGE_POOL_HELPERS_H
#define	_LINUXKPI_NET_PAGE_POOL_HELPERS_H

#include <linux/kernel.h>	/* pr_debug */
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <net/page_pool/types.h>

static inline struct page_pool *
page_pool_create(const struct page_pool_params *ppparams)
{

	pr_debug("%s: TODO\n", __func__);
	return (NULL);
}

static inline void
page_pool_destroy(struct page_pool *ppool)
{

	pr_debug("%s: TODO\n", __func__);
}

static inline struct page *
page_pool_dev_alloc_frag(struct page_pool *ppool, uint32_t *offset,
    size_t size)
{

	pr_debug("%s: TODO\n", __func__);
	return (NULL);
}

static inline dma_addr_t
page_pool_get_dma_addr(struct page *page)
{

	pr_debug("%s: TODO\n", __func__);
	return (0);
}

static inline enum dma_data_direction
page_pool_get_dma_dir(const struct page_pool *ppool)
{

	pr_debug("%s: TODO\n", __func__);
	return (DMA_BIDIRECTIONAL);
}

static inline void
page_pool_put_full_page(struct page_pool *ppool, struct page *page,
    bool allow_direct)
{

	pr_debug("%s: TODO\n", __func__);
}

static inline int
page_pool_ethtool_stats_get_count(void)
{

	pr_debug("%s: TODO\n", __func__);
	return (0);
}

static inline uint8_t *
page_pool_ethtool_stats_get_strings(uint8_t *x)
{

	pr_debug("%s: TODO\n", __func__);
	return (x);
}

#endif	/* _LINUXKPI_NET_PAGE_POOL_HELPERS_H */
