/*-
 * Copyright (c) 2023 Bjoern A. Zeeb
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
 */

#ifndef	_LINUXKPI_NET_PAGE_POOL_H
#define	_LINUXKPI_NET_PAGE_POOL_H

#include <linux/kernel.h>	/* pr_debug */
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/netdevice.h>

struct device;

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

#endif	/* _LINUXKPI_NET_PAGE_POOL_H */
