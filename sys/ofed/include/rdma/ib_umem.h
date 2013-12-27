/*
 * Copyright (c) 2007 Cisco Systems.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef IB_UMEM_H
#define IB_UMEM_H

#include <linux/list.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>
#include <linux/dma-attrs.h>

struct ib_ucontext;

struct ib_umem {
	struct ib_ucontext     *context;
	size_t			length;
	int			offset;
	int			page_size;
	int                     writable;
	int                     hugetlb;
	struct list_head	chunk_list;
#ifdef __linux__
	struct work_struct	work;
	struct mm_struct       *mm;
#else
	unsigned long		start;
#endif
	unsigned long		diff;
};

struct ib_cmem {

        struct ib_ucontext     *context;
        size_t                  length;
        /* Link list of contiguous blocks being part of that cmem  */
        struct list_head ib_cmem_block;

        /* Order of cmem block,  2^ block_order will equal number
             of physical pages per block
        */
        unsigned long    block_order;
        /* Refernce counter for that memory area
          - When value became 0 pages will be returned to the kernel.
        */
        struct kref refcount;
};


struct ib_umem_chunk {
	struct list_head	list;
	int                     nents;
	int                     nmap;
	struct dma_attrs	attrs;
	struct scatterlist      page_list[0];
};

struct ib_umem *ib_umem_get(struct ib_ucontext *context, unsigned long addr,
			    size_t size, int access, int dmasync);
void ib_umem_release(struct ib_umem *umem);
int ib_umem_page_count(struct ib_umem *umem);

int ib_cmem_map_contiguous_pages_to_vma(struct ib_cmem *ib_cmem,
        struct vm_area_struct *vma);
struct ib_cmem *ib_cmem_alloc_contiguous_pages(struct ib_ucontext *context,
                                unsigned long total_size,
                                unsigned long page_size_order);
void ib_cmem_release_contiguous_pages(struct ib_cmem *cmem);
int ib_umem_map_to_vma(struct ib_umem *umem,
                                struct vm_area_struct *vma);


#endif /* IB_UMEM_H */
