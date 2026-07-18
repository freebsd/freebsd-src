/*
 * Copyright (c) 2025-2026 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <net/page_pool/helpers.h>

struct page_pool *
linuxkpi_page_pool_create(const struct page_pool_params *ppparams)
{
	struct page_pool *pp;
	int error;

#ifndef PAGE_IS_LKPI_PAGE
	WARN_ONCE(1, "\n!!! LinuxKPI page pool not supported yet!!!\n\n");
#endif

	pp = kzalloc_node(sizeof(*pp), GFP_KERNEL, ppparams->nid);
	if (pp == NULL)
		return (ERR_PTR(-ENOMEM));

	pp->params = *ppparams;

	if ((pp->params.flags & PP_FLAG_DMA_MAP) != 0) {
		if (pp->params.dma_dir != DMA_FROM_DEVICE &&
		    pp->params.dma_dir != DMA_BIDIRECTIONAL) {
			pr_warn_once("%s: invalid dma_dir %#04x\n",
			    __func__, pp->params.dma_dir);
			error = -EINVAL;
			goto free_pp;
		}
	}

	if ((pp->params.flags & PP_FLAG_DMA_SYNC_DEV) != 0) {
		if ((pp->params.flags & PP_FLAG_DMA_MAP) == 0) {
			pr_warn_once("%s: DMA_SYNC_DEV but no DMA_MAP\n",
			    __func__);
			error = -EINVAL;
			goto free_pp;
		}

		if (pp->params.max_len == 0) {
			pr_warn_once("%s: max_len unset\n", __func__);
			error = -EINVAL;
			goto free_pp;
		}
	}

	return (pp);

free_pp:
	kfree(pp);
	return (ERR_PTR(error));
}

void
linuxkpi_page_pool_destroy(struct page_pool *pp)
{
	kfree(pp);
}

/* ---------------------------------------------------------------------------- */

struct page *
linuxkpi_page_pool_alloc_frag(struct page_pool *pp, uint32_t *offset,
    size_t size, gfp_t gfp)
{
#ifdef PAGE_IS_LKPI_PAGE
	struct page *p;
	dma_addr_t dma;

	if (size > PAGE_SIZE) {
		pr_warn_once("%s: size %zu > PAGE_SIZE\n", __func__, size);
		return (NULL);
	}

	/*
	 * We do not try to be smart on the first cut until 'struct page' work
	 * is sorted and settled.  Everyone gets a page for now.
	 * XXX in the future deal with page fragments, pool limits, etc.
	 */
	p = alloc_page(gfp);
	if (p == NULL)
		return (NULL);

	if (offset != NULL)
		*offset = 0;

	if ((pp->params.flags & PP_FLAG_DMA_MAP) != 0) {
		dma = dma_map_page_attrs(pp->params.dev, p, 0, PAGE_SIZE,
		    pp->params.dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
		if (dma_mapping_error(pp->params.dev, dma)) {
			put_page(p);
			return (NULL);
		}

		/* We store the dma address in struct page as well. */
		p->dma_addr = dma;
	}

	if ((pp->params.flags & PP_FLAG_DMA_SYNC_DEV) != 0)
		dma_sync_single_for_device(pp->params.dev, dma, PAGE_SIZE,
		    pp->params.dma_dir);

	/*
	 * We may not yet need to track the page pool on the page in order
	 * to know where to return it but prepare at least for that.
	 */
	p->pp = pp;

	return (p);
#else
	return (NULL);
#endif /* PAGE_IS_LKPI_PAGE */
}

struct page *
linuxkpi_page_pool_dev_alloc_frag(struct page_pool *pp, uint32_t *offset,
    size_t size)
{
	gfp_t gfp;

	gfp = (GFP_ATOMIC | __GFP_NOWARN);
	return (page_pool_alloc_frag(pp, offset, size, gfp));
}

void
linuxkpi_page_pool_put_full_page(struct page_pool *pp, struct page *page,
    bool allow_direct)
{
	/* We just free the full page we handed out for now. */
#ifdef PAGE_IS_LKPI_PAGE
	page->pp = NULL;
	page->dma_addr = 0;
#endif
	put_page(page);
}

dma_addr_t
linuxkpi_page_pool_get_dma_addr(const struct page *page)
{

	KASSERT(page != NULL, ("%s: page %p\n", __func__, page));

#ifdef PAGE_IS_LKPI_PAGE
	return (page->dma_addr);
#else
	return (0);
#endif
}
