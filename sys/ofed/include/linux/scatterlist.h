/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2015 Mellanox Technologies, Ltd.
 * Copyright (c) 2015 Matthew Dillon <dillon@backplane.com>
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
#ifndef	_LINUX_SCATTERLIST_H_
#define	_LINUX_SCATTERLIST_H_

#include <linux/page.h>
#include <linux/slab.h>

struct scatterlist {
	union {
		struct page *page;
		struct scatterlist *sg;
	}	sl_un;
	dma_addr_t address;
	unsigned long offset;
	uint32_t length;
	uint32_t flags;
};

struct sg_table {
	struct scatterlist *sgl;
	unsigned int nents;
	unsigned int orig_nents;
};

struct sg_page_iter {
	struct scatterlist *sg;
	unsigned int sg_pgoffset;
	unsigned int maxents;
};

#define	SG_MAX_SINGLE_ALLOC	(PAGE_SIZE / sizeof(struct scatterlist))

#define	sg_dma_address(sg)	(sg)->address
#define	sg_dma_len(sg)		(sg)->length
#define	sg_page(sg)		(sg)->sl_un.page
#define	sg_scatternext(sg)	(sg)->sl_un.sg

#define	SG_END		0x01
#define	SG_CHAIN	0x02

static inline void
sg_set_page(struct scatterlist *sg, struct page *page, unsigned int len,
    unsigned int offset)
{
	sg_page(sg) = page;
	sg_dma_len(sg) = len;
	sg->offset = offset;
	if (offset > PAGE_SIZE)
		panic("sg_set_page: Invalid offset %d\n", offset);
}

static inline void
sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen)
{
	sg_set_page(sg, virt_to_page(buf), buflen,
	    ((uintptr_t)buf) & (PAGE_SIZE - 1));
}

static inline void
sg_init_table(struct scatterlist *sg, unsigned int nents)
{
	bzero(sg, sizeof(*sg) * nents);
	sg[nents - 1].flags = SG_END;
}

static inline struct scatterlist *
sg_next(struct scatterlist *sg)
{
	if (sg->flags & SG_END)
		return (NULL);
	sg++;
	if (sg->flags & SG_CHAIN)
		sg = sg_scatternext(sg);
	return (sg);
}

static inline vm_paddr_t
sg_phys(struct scatterlist *sg)
{
	return sg_page(sg)->phys_addr + sg->offset;
}

static inline void
sg_chain(struct scatterlist *prv, unsigned int prv_nents,
    struct scatterlist *sgl)
{
	struct scatterlist *sg = &prv[prv_nents - 1];

	sg->offset = 0;
	sg->length = 0;
	sg->flags = SG_CHAIN;
	sg->sl_un.sg = sgl;
}

static inline void 
sg_mark_end(struct scatterlist *sg)
{
	sg->flags = SG_END;
}

static inline void
__sg_free_table(struct sg_table *table, unsigned int max_ents)
{
	struct scatterlist *sgl, *next;

	if (unlikely(!table->sgl))
		return;

	sgl = table->sgl;
	while (table->orig_nents) {
		unsigned int alloc_size = table->orig_nents;
		unsigned int sg_size;

		if (alloc_size > max_ents) {
			next = sgl[max_ents - 1].sl_un.sg;
			alloc_size = max_ents;
			sg_size = alloc_size - 1;
		} else {
			sg_size = alloc_size;
			next = NULL;
		}

		table->orig_nents -= sg_size;
		kfree(sgl);
		sgl = next;
	}

	table->sgl = NULL;
}

static inline void
sg_free_table(struct sg_table *table)
{
	__sg_free_table(table, SG_MAX_SINGLE_ALLOC);
}

static inline int
__sg_alloc_table(struct sg_table *table, unsigned int nents,
    unsigned int max_ents, gfp_t gfp_mask)
{
	struct scatterlist *sg, *prv;
	unsigned int left;

	memset(table, 0, sizeof(*table));

	if (nents == 0)
		return -EINVAL;
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

		sg = kmalloc(alloc_size * sizeof(struct scatterlist), gfp_mask);
		if (unlikely(!sg)) {
			if (prv)
				table->nents = ++table->orig_nents;

			return -ENOMEM;
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

	return 0;
}

static inline int
sg_alloc_table(struct sg_table *table, unsigned int nents, gfp_t gfp_mask)
{
	int ret;

	ret = __sg_alloc_table(table, nents, SG_MAX_SINGLE_ALLOC,
	    gfp_mask);
	if (unlikely(ret))
		__sg_free_table(table, SG_MAX_SINGLE_ALLOC);

	return ret;
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

static inline dma_addr_t
sg_page_iter_dma_address(struct sg_page_iter *spi)
{
	return spi->sg->address + (spi->sg_pgoffset << PAGE_SHIFT);
}

#define	for_each_sg_page(sgl, iter, nents, pgoffset)			\
	for (_sg_iter_init(sgl, iter, nents, pgoffset);			\
	     (iter)->sg; _sg_iter_next(iter))

#define	for_each_sg(sglist, sg, sgmax, _itr)				\
	for (_itr = 0, sg = (sglist); _itr < (sgmax); _itr++, sg = sg_next(sg))

#endif					/* _LINUX_SCATTERLIST_H_ */
