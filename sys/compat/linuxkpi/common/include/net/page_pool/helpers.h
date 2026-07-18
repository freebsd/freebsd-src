/*
 * Copyright (c) 2023-2025 Bjoern A. Zeeb
 * Copyright (c) 2025-2026 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_LINUXKPI_NET_PAGE_POOL_HELPERS_H
#define	_LINUXKPI_NET_PAGE_POOL_HELPERS_H

#include <linux/kernel.h>	/* pr_debug */
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <net/page_pool/types.h>

struct page_pool *linuxkpi_page_pool_create(const struct page_pool_params *);
void linuxkpi_page_pool_destroy(struct page_pool *);

struct page *linuxkpi_page_pool_dev_alloc_frag(struct page_pool *,
    uint32_t *, size_t);
void linuxkpi_page_pool_put_full_page(struct page_pool *, struct page *, bool);
dma_addr_t linuxkpi_page_pool_get_dma_addr(const struct page *);


static inline struct page_pool *
page_pool_create(const struct page_pool_params *ppparams)
{
	return (linuxkpi_page_pool_create(ppparams));
}

static inline void
page_pool_destroy(struct page_pool *pp)
{
	linuxkpi_page_pool_destroy(pp);
}


static inline struct page *
page_pool_dev_alloc_frag(struct page_pool *pp, uint32_t *offset,
    size_t size)
{
	return (linuxkpi_page_pool_dev_alloc_frag(pp, offset, size));
}

static inline void
page_pool_put_full_page(struct page_pool *pp, struct page *page,
    bool allow_direct)
{
	linuxkpi_page_pool_put_full_page(pp, page, allow_direct);
}

static inline dma_addr_t
page_pool_get_dma_addr(const struct page *page)
{
	return (linuxkpi_page_pool_get_dma_addr(page));
}

static inline enum dma_data_direction
page_pool_get_dma_dir(const struct page_pool *pp)
{
	return (pp->params.dma_dir);
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
