/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Cisco Systems.  All rights reserved.
 * Copyright (c) 2005 Mellanox Technologies. All rights reserved.
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

#define	LINUXKPI_PARAM_PREFIX ibcore_

#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/dma-attrs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <sys/priv.h>
#include <sys/resourcevar.h>
#include <vm/vm_pageout.h>
#include <vm/vm_map.h>
#include "uverbs.h"

#define IB_UMEM_MAX_PAGE_CHUNK		(PAGE_SIZE / sizeof (struct page *))

static int allow_weak_ordering;
module_param_named(weak_ordering, allow_weak_ordering, int, 0444);
MODULE_PARM_DESC(weak_ordering,  "Allow weak ordering for data registered memory");

static struct ib_umem *peer_umem_get(struct ib_peer_memory_client *ib_peer_mem,
				       struct ib_umem *umem, unsigned long addr,
				       int dmasync, int invalidation_supported)
{
	int ret;
	const struct peer_memory_client *peer_mem = ib_peer_mem->peer_mem;
	struct invalidation_ctx *invalidation_ctx = NULL;

	umem->ib_peer_mem = ib_peer_mem;
	if (invalidation_supported) {
		invalidation_ctx = kzalloc(sizeof(*invalidation_ctx), GFP_KERNEL);
		if (!invalidation_ctx) {
			ret = -ENOMEM;
			goto out;
		}
		umem->invalidation_ctx = invalidation_ctx;
		invalidation_ctx->umem = umem;
		mutex_lock(&ib_peer_mem->lock);
		invalidation_ctx->context_ticket =
				ib_peer_insert_context(ib_peer_mem, invalidation_ctx);
		/* unlock before calling get pages to prevent a dead-lock from the callback */
		mutex_unlock(&ib_peer_mem->lock);
	}

	ret = peer_mem->get_pages(addr, umem->length, umem->writable, 1,
				&umem->sg_head, 
				umem->peer_mem_client_context,
				invalidation_ctx ?
				(void *)invalidation_ctx->context_ticket : NULL);

	if (invalidation_ctx) {
		/* taking the lock back, checking that wasn't invalidated at that time */
		mutex_lock(&ib_peer_mem->lock);
		if (invalidation_ctx->peer_invalidated) {
			printk(KERN_ERR "peer_umem_get: pages were invalidated by peer\n");
			ret = -EINVAL;
		}
	}

	if (ret)
		goto out;

	umem->page_size = peer_mem->get_page_size
					(umem->peer_mem_client_context);
	if (umem->page_size <= 0)
		goto put_pages;

	umem->offset = addr & ((unsigned long)umem->page_size - 1);
	ret = peer_mem->dma_map(&umem->sg_head,
					umem->peer_mem_client_context,
					umem->context->device->dma_device,
					dmasync,
					&umem->nmap);
	if (ret)
		goto put_pages;

	ib_peer_mem->stats.num_reg_pages +=
			umem->nmap * (umem->page_size >> PAGE_SHIFT);
	ib_peer_mem->stats.num_alloc_mrs += 1;
	return umem;

put_pages:

	peer_mem->put_pages(umem->peer_mem_client_context,
					&umem->sg_head);
out:
	if (invalidation_ctx) {
		ib_peer_remove_context(ib_peer_mem, invalidation_ctx->context_ticket);
		mutex_unlock(&umem->ib_peer_mem->lock);
		kfree(invalidation_ctx);
	}

	ib_put_peer_client(ib_peer_mem, umem->peer_mem_client_context,
				umem->peer_mem_srcu_key);
	kfree(umem);
	return ERR_PTR(ret);
}

static void peer_umem_release(struct ib_umem *umem)
{
	struct ib_peer_memory_client *ib_peer_mem = umem->ib_peer_mem;
	const struct peer_memory_client *peer_mem = ib_peer_mem->peer_mem;
	struct invalidation_ctx *invalidation_ctx = umem->invalidation_ctx;

	if (invalidation_ctx) {

		int peer_callback;
		int inflight_invalidation;
		/* If we are not under peer callback we must take the lock before removing
		  * core ticket from the tree and releasing its umem.
		  * It will let any inflight callbacks to be ended safely.
		  * If we are under peer callback or under error flow of reg_mr so that context
		  * wasn't activated yet lock was already taken.
		*/
		if (invalidation_ctx->func && !invalidation_ctx->peer_callback)
			mutex_lock(&ib_peer_mem->lock);
		ib_peer_remove_context(ib_peer_mem, invalidation_ctx->context_ticket);
		/* make sure to check inflight flag after took the lock and remove from tree.
		  * in addition, from that point using local variables for peer_callback and
		  * inflight_invalidation as after the complete invalidation_ctx can't be accessed
		  * any more as it may be freed by the callback.
		*/
		peer_callback = invalidation_ctx->peer_callback;
		inflight_invalidation = invalidation_ctx->inflight_invalidation;
		if (inflight_invalidation)
			complete(&invalidation_ctx->comp);
		/* On peer callback lock is handled externally */
		if (!peer_callback)
			/* unlocking before put_pages */
			mutex_unlock(&ib_peer_mem->lock);
		/* in case under callback context or callback is pending let it free the invalidation context */
		if (!peer_callback && !inflight_invalidation)
			kfree(invalidation_ctx);
	}

	peer_mem->dma_unmap(&umem->sg_head,
					umem->peer_mem_client_context,
					umem->context->device->dma_device);
	peer_mem->put_pages(&umem->sg_head,
					  umem->peer_mem_client_context);

	ib_peer_mem->stats.num_dereg_pages +=
			umem->nmap * (umem->page_size >> PAGE_SHIFT);
	ib_peer_mem->stats.num_dealloc_mrs += 1;
	ib_put_peer_client(ib_peer_mem, umem->peer_mem_client_context,
				umem->peer_mem_srcu_key);
	kfree(umem);

	return;

}

static void __ib_umem_release(struct ib_device *dev, struct ib_umem *umem, int dirty)
{

	vm_object_t object;
	struct scatterlist *sg;
	struct page *page;
	int i;

	object = NULL;
	if (umem->nmap > 0)
		ib_dma_unmap_sg(dev, umem->sg_head.sgl,
			umem->nmap,
			DMA_BIDIRECTIONAL);
	for_each_sg(umem->sg_head.sgl, sg, umem->npages, i) {
		page = sg_page(sg);
			if (umem->writable && dirty) {
				if (object && object != page->object)
					VM_OBJECT_WUNLOCK(object);
				if (object != page->object) {
					object = page->object;
					VM_OBJECT_WLOCK(object);
				}
				vm_page_dirty(page);
			}
		}
	sg_free_table(&umem->sg_head);
	if (object)
		VM_OBJECT_WUNLOCK(object);

}

void ib_umem_activate_invalidation_notifier(struct ib_umem *umem,
					       umem_invalidate_func_t func,
					       void *cookie)
{
	struct invalidation_ctx *invalidation_ctx = umem->invalidation_ctx;

	invalidation_ctx->func = func;
	invalidation_ctx->cookie = cookie;

	/* from that point any pending invalidations can be called */
	mutex_unlock(&umem->ib_peer_mem->lock);
	return;
}
EXPORT_SYMBOL(ib_umem_activate_invalidation_notifier);
/**
 * ib_umem_get - Pin and DMA map userspace memory.
 * @context: userspace context to pin memory for
 * @addr: userspace virtual address to start at
 * @size: length of region to pin
 * @access: IB_ACCESS_xxx flags for memory being pinned
 * @dmasync: flush in-flight DMA when the memory region is written
 */
struct ib_umem *ib_umem_get_ex(struct ib_ucontext *context, unsigned long addr,
			    size_t size, int access, int dmasync,
			    int invalidation_supported)
{

	struct ib_umem *umem;
        struct proc *proc;
	pmap_t pmap;
        vm_offset_t end, last, start;
        vm_size_t npages;
        int error;
	int ret;
	int ents;
	int i;
	DEFINE_DMA_ATTRS(attrs);
	struct scatterlist *sg, *sg_list_start;
	int need_release = 0;

	error = priv_check(curthread, PRIV_VM_MLOCK);
	if (error)
		return ERR_PTR(-error);

	last = addr + size;
	start = addr & PAGE_MASK; /* Use the linux PAGE_MASK definition. */
	end = roundup2(last, PAGE_SIZE); /* Use PAGE_MASK safe operation. */
	if (last < addr || end < addr)
		return ERR_PTR(-EINVAL);
	npages = atop(end - start);
	if (npages > vm_page_max_wired)
		return ERR_PTR(-ENOMEM);
	umem = kzalloc(sizeof *umem, GFP_KERNEL);
	if (!umem)
		return ERR_PTR(-ENOMEM);
	proc = curthread->td_proc;
	PROC_LOCK(proc);
	if (ptoa(npages +
	    pmap_wired_count(vm_map_pmap(&proc->p_vmspace->vm_map))) >
	    lim_cur_proc(proc, RLIMIT_MEMLOCK)) {
		PROC_UNLOCK(proc);
		kfree(umem);
		return ERR_PTR(-ENOMEM);
	}
        PROC_UNLOCK(proc);
	if (npages + vm_cnt.v_wire_count > vm_page_max_wired) {
		kfree(umem);
		return ERR_PTR(-EAGAIN);
	}
	error = vm_map_wire(&proc->p_vmspace->vm_map, start, end,
	    VM_MAP_WIRE_USER | VM_MAP_WIRE_NOHOLES |
	    (umem->writable ? VM_MAP_WIRE_WRITE : 0));
	if (error != KERN_SUCCESS) {
		kfree(umem);
		return ERR_PTR(-ENOMEM);
	}

	umem->context   = context;
	umem->length    = size;
	umem->offset    = addr & ~PAGE_MASK;
	umem->page_size = PAGE_SIZE;
	umem->start	= addr;
	/*
	 * We ask for writable memory if any access flags other than
	 * "remote read" are set.  "Local write" and "remote write"
	 * obviously require write access.  "Remote atomic" can do
	 * things like fetch and add, which will modify memory, and
	 * "MW bind" can change permissions by binding a window.
	 */
	umem->writable  = !!(access & ~IB_ACCESS_REMOTE_READ);

	if (invalidation_supported || context->peer_mem_private_data) {

		struct ib_peer_memory_client *peer_mem_client;

		peer_mem_client =  ib_get_peer_client(context, addr, size,
			&umem->peer_mem_client_context,
				&umem->peer_mem_srcu_key);
		if (peer_mem_client)
			return peer_umem_get(peer_mem_client, umem, addr,
				dmasync, invalidation_supported);
	}

	umem->hugetlb = 0;

	pmap = vm_map_pmap(&proc->p_vmspace->vm_map);

	if (npages == 0) {
		ret = -EINVAL;
			goto out;
		}

	ret = sg_alloc_table(&umem->sg_head, npages, GFP_KERNEL);
	if (ret)
		goto out;

	need_release = 1;
	sg_list_start = umem->sg_head.sgl;

	while (npages) {

		ents = min_t(int, npages, IB_UMEM_MAX_PAGE_CHUNK);
		umem->npages += ents;

		for_each_sg(sg_list_start, sg, ents, i) {
			vm_paddr_t pa;

			pa = pmap_extract(pmap, start);
			if (pa == 0) {
				ret = -ENOMEM;
				goto out;
			}
			sg_set_page(sg, PHYS_TO_VM_PAGE(pa),
			    PAGE_SIZE, 0);
			npages--;
			start += PAGE_SIZE;
		}

		/* preparing for next loop */
		sg_list_start = sg;
	}

	umem->nmap = ib_dma_map_sg_attrs(context->device,
					umem->sg_head.sgl,
					umem->npages,
						  DMA_BIDIRECTIONAL,
						  &attrs);
	if (umem->nmap != umem->npages) {
			ret = -ENOMEM;
			goto out;
		}

out:
	if (ret < 0) {
		if (need_release)
		__ib_umem_release(context->device, umem, 0);
		kfree(umem);
	}

	return ret < 0 ? ERR_PTR(ret) : umem;
}
EXPORT_SYMBOL(ib_umem_get_ex);

struct ib_umem *ib_umem_get(struct ib_ucontext *context, unsigned long addr,
			    size_t size, int access, int dmasync)
{
	return ib_umem_get_ex(context, addr,
			    size, access, dmasync, 0);
}
EXPORT_SYMBOL(ib_umem_get);

/**
 * ib_umem_release - release memory pinned with ib_umem_get
 * @umem: umem struct to release
 */
void ib_umem_release(struct ib_umem *umem)
{

	vm_offset_t addr, end, last, start;
	vm_size_t size;
	int error;

	if (umem->ib_peer_mem) {
		peer_umem_release(umem);
		return;
	}

	__ib_umem_release(umem->context->device, umem, 1);

	if (umem->context->closing) {
		kfree(umem);
		return;
	}

	error = priv_check(curthread, PRIV_VM_MUNLOCK);

	if (error)
		return;

	addr = umem->start;
	size = umem->length;
	last = addr + size;
        start = addr & PAGE_MASK; /* Use the linux PAGE_MASK definition. */
	end = roundup2(last, PAGE_SIZE); /* Use PAGE_MASK safe operation. */
	vm_map_unwire(&curthread->td_proc->p_vmspace->vm_map, start, end,
	    VM_MAP_WIRE_USER | VM_MAP_WIRE_NOHOLES);
	kfree(umem);

}
EXPORT_SYMBOL(ib_umem_release);

int ib_umem_page_count(struct ib_umem *umem)
{
	int shift;
	int i;
	int n;
	struct scatterlist *sg;

	shift = ilog2(umem->page_size);

	n = 0;
	for_each_sg(umem->sg_head.sgl, sg, umem->nmap, i)
		n += sg_dma_len(sg) >> shift;

	return n;
}
EXPORT_SYMBOL(ib_umem_page_count);
