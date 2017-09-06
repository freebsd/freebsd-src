/*-
 * Copyright (c) 2013-2015, Mellanox Technologies, Ltd.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <linux/module.h>
#include <rdma/ib_umem.h>
#include "mlx5_ib.h"

CTASSERT(sizeof(uintptr_t) == sizeof(unsigned long));

/* @umem: umem object to scan
 * @addr: ib virtual address requested by the user
 * @count: number of PAGE_SIZE pages covered by umem
 * @shift: page shift for the compound pages found in the region
 * @ncont: number of compund pages
 * @order: log2 of the number of compound pages
 */

void mlx5_ib_cont_pages(struct ib_umem *umem, u64 addr, int *count, int *shift,
			int *ncont, int *order)
{
	struct ib_umem_chunk *chunk;
	unsigned long tmp;
	unsigned long m;
	int i, j, k;
	u64 base = 0;
	int p = 0;
	int skip;
	int mask;
	u64 len;
	u64 pfn;
	struct scatterlist *sg;
	unsigned long page_shift = ilog2(umem->page_size);

	addr = addr >> page_shift;
	tmp = (uintptr_t)addr;
	m = find_first_bit(&tmp, 8 * sizeof(tmp));
	skip = 1 << m;
	mask = skip - 1;
	i = 0;

	list_for_each_entry(chunk, &umem->chunk_list, list) {
	    for (j = 0; j < chunk->nmap; j++) {
		sg = chunk->page_list + j;
		len = sg_dma_len(sg) >> page_shift;
		pfn = sg_dma_address(sg) >> page_shift;
		for (k = 0; k < len; k++) {
			if (!(i & mask)) {
				tmp = (uintptr_t)pfn;
				m = min_t(unsigned long, m,
					  find_first_bit(&tmp, 8 * sizeof(tmp)));
				skip = 1 << m;
				mask = skip - 1;
				base = pfn;
				p = 0;
			} else {
				if (base + p != pfn) {
					tmp = (uintptr_t)p;
					m = find_first_bit(&tmp, 8 * sizeof(tmp));
					skip = 1 << m;
					mask = skip - 1;
					base = pfn;
					p = 0;
				}
			}
			p++;
			i++;
		}
	    }
	}

	if (i) {
		m = min_t(unsigned long, ilog2(roundup_pow_of_two(i)), m);

		if (order)
			*order = ilog2(roundup_pow_of_two(i) >> m);

		*ncont = DIV_ROUND_UP(i, (1 << m));
	} else {
		m  = 0;

		if (order)
			*order = 0;

		*ncont = 0;
	}
	*shift = page_shift + m;
	*count = i;
}

/*
 * Populate the given array with bus addresses from the umem.
 *
 * dev - mlx5_ib device
 * umem - umem to use to fill the pages
 * page_shift - determines the page size used in the resulting array
 * offset - offset into the umem to start from,
 *          only implemented for ODP umems
 * num_pages - total number of pages to fill
 * pas - bus addresses array to fill
 * access_flags - access flags to set on all present pages.
		  use enum mlx5_ib_mtt_access_flags for this.
 */
static void __mlx5_ib_populate_pas(struct mlx5_ib_dev *dev, struct ib_umem *umem,
			    int page_shift, size_t offset,
			    __be64 *pas, int access_flags)
{
	unsigned long umem_page_shift = ilog2(umem->page_size);
	struct ib_umem_chunk *chunk;
	int shift = page_shift - umem_page_shift;
	int mask = (1 << shift) - 1;
	int i, j, k;
	u64 cur = 0;
	u64 base;
	int len;
	struct scatterlist *sg;

	i = 0;
	list_for_each_entry(chunk, &umem->chunk_list, list) {
	    for (j = 0; j < chunk->nmap; j++) {
		sg = chunk->page_list + j;
		len = sg_dma_len(sg) >> umem_page_shift;
		base = sg_dma_address(sg);
		for (k = 0; k < len; k++) {
			if (!(i & mask)) {
				cur = base + (k << umem_page_shift);
				cur |= access_flags;

				pas[i >> shift] = cpu_to_be64(cur);
				mlx5_ib_dbg(dev, "pas[%d] 0x%llx\n",
					    i >> shift, (unsigned long long)
					    be64_to_cpu(pas[i >> shift]));
			}  else
				mlx5_ib_dbg(dev, "=====> 0x%llx\n",
					    (unsigned long long)
					    (base + (k << umem_page_shift)));
			i++;
		}
	    }
	}
}

void mlx5_ib_populate_pas(struct mlx5_ib_dev *dev, struct ib_umem *umem,
			  int page_shift, __be64 *pas, int access_flags)
{
	return __mlx5_ib_populate_pas(dev, umem, page_shift, 0,
				      pas,
				      access_flags);
}

int mlx5_ib_get_buf_offset(u64 addr, int page_shift, u32 *offset)
{
	u64 page_size;
	u64 page_mask;
	u64 off_size;
	u64 off_mask;
	u64 buf_off;

	page_size = (u64)1 << page_shift;
	page_mask = page_size - 1;
	buf_off = addr & page_mask;
	off_size = page_size >> 6;
	off_mask = off_size - 1;

	if (buf_off & off_mask)
		return -EINVAL;

	*offset = (u32)(buf_off >> ilog2(off_size));
	return 0;
}
