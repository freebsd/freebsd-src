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
#ifndef	_LINUX_SCATTERLIST_H_
#define	_LINUX_SCATTERLIST_H_

#include <linux/string.h>
#include <linux/page.h>

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
	    ((uintptr_t)buf) & ~PAGE_MASK);
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

#define	for_each_sg(sglist, sg, sgmax, _itr)				\
	for (_itr = 0, sg = (sglist); _itr < (sgmax); _itr++, sg = sg_next(sg))

#endif	/* _LINUX_SCATTERLIST_H_ */
