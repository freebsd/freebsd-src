/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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

/*
 * SG table design.
 *
 * If flags bit 0 is set, then the sg field contains a pointer to the next sg
 * table list. Otherwise the next entry is at sg + 1, can be determined using
 * the sg_is_chain() function.
 *
 * If flags bit 1 is set, then this sg entry is the last element in a list,
 * can be determined using the sg_is_last() function.
 *
 * See sg_next().
 *
 */

struct scatterlist {
	union {
		struct page		*page;
		struct scatterlist	*sg;
	} sl_un;
	dma_addr_t	address;
	unsigned long	offset;
	uint32_t	length;
	uint32_t	flags;
};

struct sg_table {
	struct scatterlist *sgl;        /* the list */
	unsigned int nents;             /* number of mapped entries */
	unsigned int orig_nents;        /* original size of list */
};

struct sg_page_iter {
	struct scatterlist	*sg;
	unsigned int		sg_pgoffset;	/* page index */
	unsigned int		maxents;
};

/*
 * Maximum number of entries that will be allocated in one piece, if
 * a list larger than this is required then chaining will be utilized.
 */
#define SG_MAX_SINGLE_ALLOC             (PAGE_SIZE / sizeof(struct scatterlist))

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

/**
 * sg_chain - Chain two sglists together
 * @prv:        First scatterlist
 * @prv_nents:  Number of entries in prv
 * @sgl:        Second scatterlist
 *
 * Description:
 *   Links @prv@ and @sgl@ together, to form a longer scatterlist.
 *
 **/
static inline void
sg_chain(struct scatterlist *prv, unsigned int prv_nents,
					struct scatterlist *sgl)
{
/*
 * offset and length are unused for chain entry.  Clear them.
 */
	struct scatterlist *sg = &prv[prv_nents - 1];

	sg->offset = 0;
	sg->length = 0;

	/*
	 * Indicate a link pointer, and set the link to the second list.
	 */
	sg->flags = SG_CHAIN;
	sg->sl_un.sg = sgl;
}

/**
 * sg_mark_end - Mark the end of the scatterlist
 * @sg:          SG entryScatterlist
 *
 * Description:
 *   Marks the passed in sg entry as the termination point for the sg
 *   table. A call to sg_next() on this entry will return NULL.
 *
 **/
static inline void sg_mark_end(struct scatterlist *sg)
{
        sg->flags = SG_END;
}

/**
 * __sg_free_table - Free a previously mapped sg table
 * @table:      The sg table header to use
 * @max_ents:   The maximum number of entries per single scatterlist
 *
 *  Description:
 *    Free an sg table previously allocated and setup with
 *    __sg_alloc_table().  The @max_ents value must be identical to
 *    that previously used with __sg_alloc_table().
 *
 **/
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

		/*
		 * If we have more than max_ents segments left,
		 * then assign 'next' to the sg table after the current one.
		 * sg_size is then one less than alloc size, since the last
		 * element is the chain pointer.
		 */
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

/**
 * sg_free_table - Free a previously allocated sg table
 * @table:      The mapped sg table header
 *
 **/
static inline void
sg_free_table(struct sg_table *table)
{
	__sg_free_table(table, SG_MAX_SINGLE_ALLOC);
}

/**
 * __sg_alloc_table - Allocate and initialize an sg table with given allocator
 * @table:      The sg table header to use
 * @nents:      Number of entries in sg list
 * @max_ents:   The maximum number of entries the allocator returns per call
 * @gfp_mask:   GFP allocation mask
 *
 * Description:
 *   This function returns a @table @nents long. The allocator is
 *   defined to return scatterlist chunks of maximum size @max_ents.
 *   Thus if @nents is bigger than @max_ents, the scatterlists will be
 *   chained in units of @max_ents.
 *
 * Notes:
 *   If this function returns non-0 (eg failure), the caller must call
 *   __sg_free_table() to cleanup any leftover allocations.
 *
 **/
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
		unsigned int sg_size, alloc_size = left;

		if (alloc_size > max_ents) {
			alloc_size = max_ents;
			sg_size = alloc_size - 1;
		} else
			sg_size = alloc_size;

		left -= sg_size;

		sg = kmalloc(alloc_size * sizeof(struct scatterlist), gfp_mask);
		if (unlikely(!sg)) {
		/*
		 * Adjust entry count to reflect that the last
		 * entry of the previous table won't be used for
		 * linkage.  Without this, sg_kfree() may get
		 * confused.
		 */
			if (prv)
				table->nents = ++table->orig_nents;

			return -ENOMEM;
		}

		sg_init_table(sg, alloc_size);
		table->nents = table->orig_nents += sg_size;

		/*
		 * If this is the first mapping, assign the sg table header.
		 * If this is not the first mapping, chain previous part.
		 */
		if (prv)
			sg_chain(prv, max_ents, sg);
		else
			table->sgl = sg;

		/*
		* If no more entries after this one, mark the end
		*/
		if (!left)
			sg_mark_end(&sg[sg_size - 1]);

		prv = sg;
	} while (left);

	return 0;
}

/**
 * sg_alloc_table - Allocate and initialize an sg table
 * @table:      The sg table header to use
 * @nents:      Number of entries in sg list
 * @gfp_mask:   GFP allocation mask
 *
 *  Description:
 *    Allocate and initialize an sg table. If @nents@ is larger than
 *    SG_MAX_SINGLE_ALLOC a chained sg table will be setup.
 *
 **/

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

/*
 * Iterate pages in sg list.
 */
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

/*
 * NOTE: pgoffset is really a page index, not a byte offset.
 */
static inline void
_sg_iter_init(struct scatterlist *sgl, struct sg_page_iter *iter,
	      unsigned int nents, unsigned long pgoffset)
{
	if (nents) {
		/*
		 * Nominal case.  Note subtract 1 from starting page index
		 * for initial _sg_iter_next() call.
		 */
		iter->sg = sgl;
		iter->sg_pgoffset = pgoffset - 1;
		iter->maxents = nents;
		_sg_iter_next(iter);
	} else {
		/*
		 * Degenerate case
		 */
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

#endif	/* _LINUX_SCATTERLIST_H_ */
