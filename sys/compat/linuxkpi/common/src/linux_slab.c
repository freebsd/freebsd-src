/*-
 * Copyright (c) 2017 Mellanox Technologies, Ltd.
 * All rights reserved.
 * Copyright (c) 2024-2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
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

#include <sys/cdefs.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <linux/kernel.h>
#include <linux/irq_work.h>
#include <linux/llist.h>

#include <sys/param.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>

struct linux_kmem_rcu {
	struct rcu_head rcu_head;
	struct linux_kmem_cache *cache;
};

struct linux_kmem_cache {
	uma_zone_t cache_zone;
	linux_kmem_ctor_t *cache_ctor;
	unsigned cache_flags;
	unsigned cache_size;
	struct llist_head cache_items;
	struct task cache_task;
};

#define	LINUX_KMEM_TO_RCU(c, m)					\
	((struct linux_kmem_rcu *)((char *)(m) +		\
	(c)->cache_size - sizeof(struct linux_kmem_rcu)))

#define	LINUX_RCU_TO_KMEM(r)					\
	((void *)((char *)(r) + sizeof(struct linux_kmem_rcu) - \
	(r)->cache->cache_size))

static LLIST_HEAD(linux_kfree_async_list);

static void	lkpi_kmem_cache_free_async_fn(void *, int);

void *
lkpi_kmem_cache_alloc(struct linux_kmem_cache *c, gfp_t flags)
{
	return (uma_zalloc_arg(c->cache_zone, c,
	    linux_check_m_flags(flags)));
}

void *
lkpi_kmem_cache_zalloc(struct linux_kmem_cache *c, gfp_t flags)
{
	return (uma_zalloc_arg(c->cache_zone, c,
	    linux_check_m_flags(flags | M_ZERO)));
}

static int
linux_kmem_ctor(void *mem, int size, void *arg, int flags)
{
	struct linux_kmem_cache *c = arg;

	if (unlikely(c->cache_flags & SLAB_TYPESAFE_BY_RCU)) {
		struct linux_kmem_rcu *rcu = LINUX_KMEM_TO_RCU(c, mem);

		/* duplicate cache pointer */
		rcu->cache = c;
	}

	/* check for constructor */
	if (likely(c->cache_ctor != NULL))
		c->cache_ctor(mem);

	return (0);
}

static void
linux_kmem_cache_free_rcu_callback(struct rcu_head *head)
{
	struct linux_kmem_rcu *rcu =
	    container_of(head, struct linux_kmem_rcu, rcu_head);

	uma_zfree(rcu->cache->cache_zone, LINUX_RCU_TO_KMEM(rcu));
}

struct linux_kmem_cache *
linux_kmem_cache_create(const char *name, size_t size, size_t align,
    unsigned flags, linux_kmem_ctor_t *ctor)
{
	struct linux_kmem_cache *c;

	c = malloc(sizeof(*c), M_KMALLOC, M_WAITOK);

	if (flags & SLAB_HWCACHE_ALIGN)
		align = UMA_ALIGN_CACHE;
	else if (align != 0)
		align--;

	if (flags & SLAB_TYPESAFE_BY_RCU) {
		/* make room for RCU structure */
		size = ALIGN(size, sizeof(void *));
		size += sizeof(struct linux_kmem_rcu);

		/* create cache_zone */
		c->cache_zone = uma_zcreate(name, size,
		    linux_kmem_ctor, NULL, NULL, NULL,
		    align, UMA_ZONE_ZINIT);
	} else {
		/* make room for async task list items */
		size = MAX(size, sizeof(struct llist_node));

		/* create cache_zone */
		c->cache_zone = uma_zcreate(name, size,
		    ctor ? linux_kmem_ctor : NULL, NULL,
		    NULL, NULL, align, 0);
	}

	c->cache_flags = flags;
	c->cache_ctor = ctor;
	c->cache_size = size;
	init_llist_head(&c->cache_items);
	TASK_INIT(&c->cache_task, 0, lkpi_kmem_cache_free_async_fn, c);
	return (c);
}

static inline void
lkpi_kmem_cache_free_rcu(struct linux_kmem_cache *c, void *m)
{
	struct linux_kmem_rcu *rcu = LINUX_KMEM_TO_RCU(c, m);

	call_rcu(&rcu->rcu_head, linux_kmem_cache_free_rcu_callback);
}

static inline void
lkpi_kmem_cache_free_sync(struct linux_kmem_cache *c, void *m)
{
	uma_zfree(c->cache_zone, m);
}

static void
lkpi_kmem_cache_free_async_fn(void *context, int pending)
{
	struct linux_kmem_cache *c = context;
	struct llist_node *freed, *next;

	llist_for_each_safe(freed, next, llist_del_all(&c->cache_items))
		lkpi_kmem_cache_free_sync(c, freed);
}

static inline void
lkpi_kmem_cache_free_async(struct linux_kmem_cache *c, void *m)
{
	if (m == NULL)
		return;

	llist_add(m, &c->cache_items);
	taskqueue_enqueue(linux_irq_work_tq, &c->cache_task);
}

void
lkpi_kmem_cache_free(struct linux_kmem_cache *c, void *m)
{
	if (unlikely(c->cache_flags & SLAB_TYPESAFE_BY_RCU))
		lkpi_kmem_cache_free_rcu(c, m);
	else if (unlikely(curthread->td_critnest != 0))
		lkpi_kmem_cache_free_async(c, m);
	else
		lkpi_kmem_cache_free_sync(c, m);
}

void
linux_kmem_cache_destroy(struct linux_kmem_cache *c)
{
	if (c == NULL)
		return;

	if (unlikely(c->cache_flags & SLAB_TYPESAFE_BY_RCU)) {
		/* make sure all free callbacks have been called */
		rcu_barrier();
	}

	if (!llist_empty(&c->cache_items))
		taskqueue_enqueue(linux_irq_work_tq, &c->cache_task);
	taskqueue_drain(linux_irq_work_tq, &c->cache_task);
	uma_zdestroy(c->cache_zone);
	free(c, M_KMALLOC);
}

void *
lkpi___kmalloc_node(size_t size, gfp_t flags, int node)
{
	if (size <= PAGE_SIZE)
		return (malloc_domainset(size, M_KMALLOC,
		    linux_get_vm_domain_set(node), linux_check_m_flags(flags)));
	else
		return (contigmalloc_domainset(size, M_KMALLOC,
		    linux_get_vm_domain_set(node), linux_check_m_flags(flags),
		    0, -1UL, PAGE_SIZE, 0));
}

void *
lkpi___kmalloc(size_t size, gfp_t flags)
{
	size_t _s;

	/* sizeof(struct llist_node) is used for kfree_async(). */
	_s = MAX(size, sizeof(struct llist_node));

	if (_s <= PAGE_SIZE)
		return (malloc(_s, M_KMALLOC, linux_check_m_flags(flags)));
	else
		return (contigmalloc(_s, M_KMALLOC, linux_check_m_flags(flags),
		    0, -1UL, PAGE_SIZE, 0));
}

void *
lkpi_krealloc(void *ptr, size_t size, gfp_t flags)
{
	void *nptr;
	size_t osize;

	/*
	 * First handle invariants based on function arguments.
	 */
	if (ptr == NULL)
		return (kmalloc(size, flags));

	osize = ksize(ptr);
	if (size <= osize)
		return (ptr);

	/*
	 * We know the new size > original size.  realloc(9) does not (and cannot)
	 * know about our requirements for physically contiguous memory, so we can
	 * only call it for sizes up to and including PAGE_SIZE, and otherwise have
	 * to replicate its functionality using kmalloc to get the contigmalloc(9)
	 * backing.
	 */
	if (size <= PAGE_SIZE)
		return (realloc(ptr, size, M_KMALLOC, linux_check_m_flags(flags)));

	nptr = kmalloc(size, flags);
	if (nptr == NULL)
		return (NULL);

	memcpy(nptr, ptr, osize);
	kfree(ptr);
	return (nptr);
}

struct lkpi_kmalloc_ctx {
	size_t size;
	gfp_t flags;
	void *addr;
};

static void
lkpi_kmalloc_cb(void *ctx)
{
	struct lkpi_kmalloc_ctx *lmc = ctx;

	lmc->addr = __kmalloc(lmc->size, lmc->flags);
}

void *
lkpi_kmalloc(size_t size, gfp_t flags)
{
	struct lkpi_kmalloc_ctx lmc = { .size = size, .flags = flags };

	lkpi_fpu_safe_exec(&lkpi_kmalloc_cb, &lmc);
	return(lmc.addr);
}

static void
linux_kfree_async_fn(void *context, int pending)
{
	struct llist_node *freed;

	while((freed = llist_del_first(&linux_kfree_async_list)) != NULL)
		kfree(freed);
}
static struct task linux_kfree_async_task =
    TASK_INITIALIZER(0, linux_kfree_async_fn, &linux_kfree_async_task);

static void
linux_kfree_async(void *addr)
{
	if (addr == NULL)
		return;
	llist_add(addr, &linux_kfree_async_list);
	taskqueue_enqueue(linux_irq_work_tq, &linux_kfree_async_task);
}

void
lkpi_kfree(const void *ptr)
{
	if (ZERO_OR_NULL_PTR(ptr))
		return;

	if (curthread->td_critnest != 0)
		linux_kfree_async(__DECONST(void *, ptr));
	else
		free(__DECONST(void *, ptr), M_KMALLOC);
}

