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
#include <linux/completion.h>
#include <rdma/ib_peer_mem.h>

struct ib_ucontext;
struct ib_umem;

typedef void (*umem_invalidate_func_t)(void *invalidation_cookie,
					    struct ib_umem *umem,
					    unsigned long addr, size_t size);

struct invalidation_ctx {
	struct ib_umem *umem;
	umem_invalidate_func_t func;
	void *cookie;
	unsigned long context_ticket;
	int peer_callback;
	int inflight_invalidation;
	int peer_invalidated;
	struct completion comp;
};

struct ib_umem {
	struct ib_ucontext     *context;
	size_t			length;
	int			offset;
	int			page_size;
	int                     writable;
	int                     hugetlb;
	struct work_struct	work;
	unsigned long		diff;
	unsigned long           start;
	struct sg_table sg_head;
	int                     nmap;
	int             npages;
	/* peer memory that manages this umem*/
	struct ib_peer_memory_client *ib_peer_mem;
	struct invalidation_ctx *invalidation_ctx;
	int peer_mem_srcu_key;
	/* peer memory private context */
	void *peer_mem_client_context;
};

struct ib_umem *ib_umem_get(struct ib_ucontext *context, unsigned long addr,
			    size_t size, int access, int dmasync);
struct ib_umem *ib_umem_get_ex(struct ib_ucontext *context, unsigned long addr,
			    size_t size, int access, int dmasync,
			    int invalidation_supported);
void  ib_umem_activate_invalidation_notifier(struct ib_umem *umem,
					       umem_invalidate_func_t func,
					       void *cookie);
void ib_umem_release(struct ib_umem *umem);
int ib_umem_page_count(struct ib_umem *umem);

#endif /* IB_UMEM_H */
