/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2017 Mellanox Technologies, Ltd.
 * Copyright (c) 2015 Matthew Dillon <dillon@backplane.com>
 * Copyright (c) 2016 Matthew Macy
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
#ifndef	_LINUXKPI_LINUX_SCATTERLIST_H_
#define	_LINUXKPI_LINUX_SCATTERLIST_H_

#include <sys/types.h>
#include <sys/sf_buf.h>

#include <linux/page.h>
#include <linux/slab.h>
#include <linux/mm.h>

struct bus_dmamap;
struct scatterlist {
	unsigned long page_link;
#define	SG_PAGE_LINK_CHAIN	0x1UL
#define	SG_PAGE_LINK_LAST	0x2UL
#define	SG_PAGE_LINK_MASK	0x3UL
	unsigned int offset;
	unsigned int length;
	dma_addr_t dma_address;
	struct bus_dmamap *dma_map;	/* FreeBSD specific */
};

CTASSERT((sizeof(struct scatterlist) & SG_PAGE_LINK_MASK) == 0);

struct sg_table {
	struct scatterlist *sgl;
	unsigned int nents;
	unsigned int orig_nents;
};

struct sg_page_iter {
	struct scatterlist *sg;
	unsigned int sg_pgoffset;
	unsigned int maxents;
	struct {
		unsigned int nents;
		int	pg_advance;
	} internal;
};

struct sg_dma_page_iter {
	struct sg_page_iter base;
};

#define	SCATTERLIST_MAX_SEGMENT	(-1U & ~(PAGE_SIZE - 1))

#define	SG_MAX_SINGLE_ALLOC	(PAGE_SIZE / sizeof(struct scatterlist))

#define	SG_MAGIC		0x87654321UL
#define	SG_CHAIN		SG_PAGE_LINK_CHAIN
#define	SG_END			SG_PAGE_LINK_LAST

#define	sg_is_chain(sg)		((sg)->page_link & SG_PAGE_LINK_CHAIN)
#define	sg_is_last(sg)		((sg)->page_link & SG_PAGE_LINK_LAST)
#define	sg_chain_ptr(sg)	\
	((struct scatterlist *) ((sg)->page_link & ~SG_PAGE_LINK_MASK))

#define	sg_dma_address(sg)	(sg)->dma_address
#define	sg_dma_len(sg)		(sg)->length

#define	for_each_sg_page(sgl, iter, nents, pgoffset)			\
	for (_sg_iter_init(sgl, iter, nents, pgoffset);			\
	     (iter)->sg; _sg_iter_next(iter))
#define	for_each_sg_dma_page(sgl, iter, nents, pgoffset) 		\
	for_each_sg_page(sgl, &(iter)->base, nents, pgoffset)

#define	for_each_sg(sglist, sg, sgmax, iter)				\
	for (iter = 0, sg = (sglist); iter < (sgmax); iter++, sg = sg_next(sg))

typedef struct scatterlist *(sg_alloc_fn) (unsigned int, gfp_t);
typedef void (sg_free_fn) (struct scatterlist *, unsigned int);

static inline void
sg_assign_page(struct scatterlist *sg, struct page *page)
{
	unsigned long page_link = sg->page_link & SG_PAGE_LINK_MASK;

	sg->page_link = page_link | (unsigned long)page;
}

static inline void
sg_set_page(struct scatterlist *sg, struct page *page, unsigned int len,
    unsigned int offset)
{
	sg_assign_page(sg, page);
	sg->offset = offset;
	sg->length = len;
}

static inline struct page *
sg_page(struct scatterlist *sg)
{
	return ((struct page *)((sg)->page_link & ~SG_PAGE_LINK_MASK));
}

static inline void
sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen)
{
	sg_set_page(sg, virt_to_page(buf), buflen,
	    ((uintptr_t)buf) & (PAGE_SIZE - 1));
}

static inline struct scatterlist *
sg_next(struct scatterlist *sg)
{
	if (sg_is_last(sg))
		return (NULL);
	sg++;
	if (sg_is_chain(sg))
		sg = sg_chain_ptr(sg);
	return (sg);
}

static inline vm_paddr_t
sg_phys(struct scatterlist *sg)
{
	return (VM_PAGE_TO_PHYS(sg_page(sg)) + sg->offset);
}

static inline void *
sg_virt(struct scatterlist *sg)
{

	return ((void *)((unsigned long)page_address(sg_page(sg)) + sg->offset));
}

static inline void
sg_chain(struct scatterlist *prv, unsigned int prv_nents,
    struct scatterlist *sgl)
{
	struct scatterlist *sg = &prv[prv_nents - 1];

	sg->offset = 0;
	sg->length = 0;
	sg->page_link = ((unsigned long)sgl |
	    SG_PAGE_LINK_CHAIN) & ~SG_PAGE_LINK_LAST;
}

static inline void
sg_mark_end(struct scatterlist *sg)
{
	sg->page_link |= SG_PAGE_LINK_LAST;
	sg->page_link &= ~SG_PAGE_LINK_CHAIN;
}

static inline void
sg_init_table(struct scatterlist *sg, unsigned int nents)
{
	bzero(sg, sizeof(*sg) * nents);
	sg_mark_end(&sg[nents - 1]);
}

static struct scatterlist *
sg_kmalloc(unsigned int nents, gfp_t gfp_mask)
{
	if (nents == SG_MAX_SINGLE_ALLOC) {
		return ((void *)__get_free_page(gfp_mask));
	} else
		return (kmalloc(nents * sizeof(struct scatterlist), gfp_mask));
}

static inline void
sg_kfree(struct scatterlist *sg, unsigned int nents)
{
	if (nents == SG_MAX_SINGLE_ALLOC) {
		free_page((unsigned long)sg);
	} else
		kfree(sg);
}

static inline void
__sg_free_table(struct sg_table *table, unsigned int max_ents,
    bool skip_first_chunk, sg_free_fn * free_fn)
{
	struct scatterlist *sgl, *next;

	if (unlikely(!table->sgl))
		return;

	sgl = table->sgl;
	while (table->orig_nents) {
		unsigned int alloc_size = table->orig_nents;
		unsigned int sg_size;

		if (alloc_size > max_ents) {
			next = sg_chain_ptr(&sgl[max_ents - 1]);
			alloc_size = max_ents;
			sg_size = alloc_size - 1;
		} else {
			sg_size = alloc_size;
			next = NULL;
		}

		table->orig_nents -= sg_size;
		if (skip_first_chunk)
			skip_first_chunk = 0;
		else
			free_fn(sgl, alloc_size);
		sgl = next;
	}

	table->sgl = NULL;
}

static inline void
sg_free_table(struct sg_table *table)
{
	__sg_free_table(table, SG_MAX_SINGLE_ALLOC, 0, sg_kfree);
}

static inline int
__sg_alloc_table(struct sg_table *table, unsigned int nents,
    unsigned int max_ents, struct scatterlist *first_chunk,
    gfp_t gfp_mask, sg_alloc_fn *alloc_fn)
{
	struct scatterlist *sg, *prv;
	unsigned int left;

	memset(table, 0, sizeof(*table));

	if (nents == 0)
		return (-EINVAL);
	left = nents;
	prv = NULL;
	do {
		unsigned int sg_size;
		unsigned int alloc_size = left;

		if (alloc_size > max_ents) {
			alloc_size = max_ents;
			sg_size = alloc_size - 1;
		} else
			sg_size = alloc_size;

		left -= sg_size;

		if (first_chunk) {
			sg = first_chunk;
			first_chunk = NULL;
		} else {
			sg = alloc_fn(alloc_size, gfp_mask);
		}
		if (unlikely(!sg)) {
			if (prv)
				table->nents = ++table->orig_nents;

			return (-ENOMEM);
		}
		sg_init_table(sg, alloc_size);
		table->nents = table->orig_nents += sg_size;

		if (prv)
			sg_chain(prv, max_ents, sg);
		else
			table->sgl = sg;

		if (!left)
			sg_mark_end(&sg[sg_size - 1]);

		prv = sg;
	} while (left);

	return (0);
}

static inline int
sg_alloc_table(struct sg_table *table, unsigned int nents, gfp_t gfp_mask)
{
	int ret;

	ret = __sg_alloc_table(table, nents, SG_MAX_SINGLE_ALLOC,
	    NULL, gfp_mask, sg_kmalloc);
	if (unlikely(ret))
		__sg_free_table(table, SG_MAX_SINGLE_ALLOC, 0, sg_kfree);

	return (ret);
}

static inline int
__sg_alloc_table_from_pages(struct sg_table *sgt,
    struct page **pages, unsigned int count,
    unsigned long off, unsigned long size,
    unsigned int max_segment, gfp_t gfp_mask)
{
	unsigned int i, segs, cur, len;
	int rc;
	struct scatterlist *s;

	if (__predict_false(!max_segment || offset_in_page(max_segment)))
		return (-EINVAL);

	len = 0;
	for (segs = i = 1; i < count; ++i) {
		len += PAGE_SIZE;
		if (len >= max_segment ||
		    page_to_pfn(pages[i]) != page_to_pfn(pages[i - 1]) + 1) {
			++segs;
			len = 0;
		}
	}
	if (__predict_false((rc = sg_alloc_table(sgt, segs, gfp_mask))))
		return (rc);

	cur = 0;
	for_each_sg(sgt->sgl, s, sgt->orig_nents, i) {
		unsigned long seg_size;
		unsigned int j;

		len = 0;
		for (j = cur + 1; j < count; ++j) {
			len += PAGE_SIZE;
			if (len >= max_segment || page_to_pfn(pages[j]) !=
			    page_to_pfn(pages[j - 1]) + 1)
				break;
		}

		seg_size = ((j - cur) << PAGE_SHIFT) - off;
		sg_set_page(s, pages[cur], MIN(size, seg_size), off);
		size -= seg_size;
		off = 0;
		cur = j;
	}
	return (0);
}

static inline int
sg_alloc_table_from_pages(struct sg_table *sgt,
    struct page **pages, unsigned int count,
    unsigned long off, unsigned long size,
    gfp_t gfp_mask)
{

	return (__sg_alloc_table_from_pages(sgt, pages, count, off, size,
	    SCATTERLIST_MAX_SEGMENT, gfp_mask));
}

static inline int
sg_nents(struct scatterlist *sg)
{
	int nents;

	for (nents = 0; sg; sg = sg_next(sg))
		nents++;
	return (nents);
}

static inline void
__sg_page_iter_start(struct sg_page_iter *piter,
    struct scatterlist *sglist, unsigned int nents,
    unsigned long pgoffset)
{
	piter->internal.pg_advance = 0;
	piter->internal.nents = nents;

	piter->sg = sglist;
	piter->sg_pgoffset = pgoffset;
}

static inline void
_sg_iter_next(struct sg_page_iter *iter)
{
	struct scatterlist *sg;
	unsigned int pgcount;

	sg = iter->sg;
	pgcount = (sg->offset + sg->length + PAGE_SIZE - 1) >> PAGE_SHIFT;

	++iter->sg_pgoffset;
	while (iter->sg_pgoffset >= pgcount) {
		iter->sg_pgoffset -= pgcount;
		sg = sg_next(sg);
		--iter->maxents;
		if (sg == NULL || iter->maxents == 0)
			break;
		pgcount = (sg->offset + sg->length + PAGE_SIZE - 1) >> PAGE_SHIFT;
	}
	iter->sg = sg;
}

static inline int
sg_page_count(struct scatterlist *sg)
{
	return (PAGE_ALIGN(sg->offset + sg->length) >> PAGE_SHIFT);
}
#define	sg_dma_page_count(sg) \
	sg_page_count(sg)

static inline bool
__sg_page_iter_next(struct sg_page_iter *piter)
{
	unsigned int pgcount;

	if (piter->internal.nents == 0)
		return (0);
	if (piter->sg == NULL)
		return (0);

	piter->sg_pgoffset += piter->internal.pg_advance;
	piter->internal.pg_advance = 1;

	while (1) {
		pgcount = sg_page_count(piter->sg);
		if (likely(piter->sg_pgoffset < pgcount))
			break;
		piter->sg_pgoffset -= pgcount;
		piter->sg = sg_next(piter->sg);
		if (--piter->internal.nents == 0)
			return (0);
		if (piter->sg == NULL)
			return (0);
	}
	return (1);
}
#define	__sg_page_iter_dma_next(itr) \
	__sg_page_iter_next(&(itr)->base)

static inline void
_sg_iter_init(struct scatterlist *sgl, struct sg_page_iter *iter,
    unsigned int nents, unsigned long pgoffset)
{
	if (nents) {
		iter->sg = sgl;
		iter->sg_pgoffset = pgoffset - 1;
		iter->maxents = nents;
		_sg_iter_next(iter);
	} else {
		iter->sg = NULL;
		iter->sg_pgoffset = 0;
		iter->maxents = 0;
	}
}

/*
 * sg_page_iter_dma_address() is implemented as a macro because it
 * needs to accept two different and identical structure types. This
 * allows both old and new code to co-exist. The compile time assert
 * adds some safety, that the structure sizes match.
 */
#define	sg_page_iter_dma_address(spi) ({		\
	struct sg_page_iter *__spi = (void *)(spi);	\
	dma_addr_t __dma_address;			\
	CTASSERT(sizeof(*(spi)) == sizeof(*__spi));	\
	__dma_address = __spi->sg->dma_address +	\
	    (__spi->sg_pgoffset << PAGE_SHIFT);		\
	__dma_address;					\
})

static inline struct page *
sg_page_iter_page(struct sg_page_iter *piter)
{
	return (nth_page(sg_page(piter->sg), piter->sg_pgoffset));
}

static __inline size_t
sg_pcopy_from_buffer(struct scatterlist *sgl, unsigned int nents,
    const void *buf, size_t buflen, off_t skip)
{
	struct sg_page_iter piter;
	struct page *page;
	struct sf_buf *sf;
	size_t len, copied;
	char *p, *b;

	if (buflen == 0)
		return (0);

	b = __DECONST(char *, buf);
	copied = 0;
	sched_pin();
	for_each_sg_page(sgl, &piter, nents, 0) {

		/* Skip to the start. */
		if (piter.sg->length <= skip) {
			skip -= piter.sg->length;
			continue;
		}

		/* See how much to copy. */
		KASSERT(((piter.sg->length - skip) != 0 && (buflen != 0)),
		    ("%s: sg len %u - skip %ju || buflen %zu is 0\n",
		    __func__, piter.sg->length, (uintmax_t)skip, buflen));
		len = min(piter.sg->length - skip, buflen);

		page = sg_page_iter_page(&piter);
		sf = sf_buf_alloc(page, SFB_CPUPRIVATE | SFB_NOWAIT);
		if (sf == NULL)
			break;
		p = (char *)sf_buf_kva(sf) + piter.sg_pgoffset + skip;
		memcpy(p, b, len);
		sf_buf_free(sf);

		/* We copied so nothing more to skip. */
		skip = 0;
		copied += len;
		/* Either we exactly filled the page, or we are done. */
		buflen -= len;
		if (buflen == 0)
			break;
		b += len;
	}
	sched_unpin();

	return (copied);
}

static inline size_t
sg_copy_from_buffer(struct scatterlist *sgl, unsigned int nents,
    const void *buf, size_t buflen)
{
	return (sg_pcopy_from_buffer(sgl, nents, buf, buflen, 0));
}

static inline size_t
sg_pcopy_to_buffer(struct scatterlist *sgl, unsigned int nents,
    void *buf, size_t buflen, off_t offset)
{
	struct sg_page_iter iter;
	struct scatterlist *sg;
	struct page *page;
	struct sf_buf *sf;
	char *vaddr;
	size_t total = 0;
	size_t len;

	if (!PMAP_HAS_DMAP)
		sched_pin();
	for_each_sg_page(sgl, &iter, nents, 0) {
		sg = iter.sg;

		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		}
		len = ulmin(buflen, sg->length - offset);
		if (len == 0)
			break;

		page = sg_page_iter_page(&iter);
		if (!PMAP_HAS_DMAP) {
			sf = sf_buf_alloc(page, SFB_CPUPRIVATE | SFB_NOWAIT);
			if (sf == NULL)
				break;
			vaddr = (char *)sf_buf_kva(sf);
		} else
			vaddr = (char *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(page));
		memcpy(buf, vaddr + sg->offset + offset, len);
		if (!PMAP_HAS_DMAP)
			sf_buf_free(sf);

		/* start at beginning of next page */
		offset = 0;

		/* advance buffer */
		buf = (char *)buf + len;
		buflen -= len;
		total += len;
	}
	if (!PMAP_HAS_DMAP)
		sched_unpin();
	return (total);
}

#endif					/* _LINUXKPI_LINUX_SCATTERLIST_H_ */
