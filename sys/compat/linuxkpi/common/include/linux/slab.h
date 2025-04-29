/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2021 Mellanox Technologies, Ltd.
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
#ifndef	_LINUXKPI_LINUX_SLAB_H_
#define	_LINUXKPI_LINUX_SLAB_H_

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/limits.h>

#include <linux/compat.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/llist.h>
#include <linux/overflow.h>

MALLOC_DECLARE(M_KMALLOC);

#define	kvzalloc(size, flags)		kmalloc(size, (flags) | __GFP_ZERO)
#define	kvcalloc(n, size, flags)	kvmalloc_array(n, size, (flags) | __GFP_ZERO)
#define	kzalloc(size, flags)		kmalloc(size, (flags) | __GFP_ZERO)
#define	kzalloc_node(size, flags, node)	kmalloc_node(size, (flags) | __GFP_ZERO, node)
#define	kfree_const(ptr)		kfree(ptr)
#define kfree_async(ptr)		kfree(ptr)		/* drm-kmod 5.4 compat */
#define	vzalloc(size)			__vmalloc(size, GFP_KERNEL | __GFP_NOWARN | __GFP_ZERO, 0)
#define	vfree(arg)			kfree(arg)
#define	kvfree(arg)			kfree(arg)
#define	vmalloc_node(size, node)	__vmalloc_node(size, GFP_KERNEL, node)
#define	vmalloc_user(size)		__vmalloc(size, GFP_KERNEL | __GFP_ZERO, 0)
#define	vmalloc(size)			__vmalloc(size, GFP_KERNEL, 0)

/*
 * Prefix some functions with linux_ to avoid namespace conflict
 * with the OpenSolaris code in the kernel.
 */
#define	kmem_cache		linux_kmem_cache
#define	kmem_cache_create(...)	linux_kmem_cache_create(__VA_ARGS__)
#define	kmem_cache_alloc(...)	lkpi_kmem_cache_alloc(__VA_ARGS__)
#define	kmem_cache_zalloc(...)	lkpi_kmem_cache_zalloc(__VA_ARGS__)
#define	kmem_cache_free(...)	lkpi_kmem_cache_free(__VA_ARGS__)
#define	kmem_cache_destroy(...) linux_kmem_cache_destroy(__VA_ARGS__)
#define	kmem_cache_shrink(x)	(0)

#define	KMEM_CACHE(__struct, flags)					\
	linux_kmem_cache_create(#__struct, sizeof(struct __struct),	\
	__alignof(struct __struct), (flags), NULL)

typedef void linux_kmem_ctor_t (void *);

struct linux_kmem_cache;

#define	SLAB_HWCACHE_ALIGN	(1 << 0)
#define	SLAB_TYPESAFE_BY_RCU	(1 << 1)
#define	SLAB_RECLAIM_ACCOUNT	(1 << 2)

#define	SLAB_DESTROY_BY_RCU \
	SLAB_TYPESAFE_BY_RCU

#define	ARCH_KMALLOC_MINALIGN \
	__alignof(unsigned long long)

#define	ZERO_SIZE_PTR		((void *)16)
#define ZERO_OR_NULL_PTR(x)	((x) == NULL || (x) == ZERO_SIZE_PTR)

struct linux_kmem_cache *linux_kmem_cache_create(const char *name,
    size_t size, size_t align, unsigned flags, linux_kmem_ctor_t *ctor);
void *lkpi_kmem_cache_alloc(struct linux_kmem_cache *, gfp_t);
void *lkpi_kmem_cache_zalloc(struct linux_kmem_cache *, gfp_t);
void lkpi_kmem_cache_free(struct linux_kmem_cache *, void *);
void linux_kmem_cache_destroy(struct linux_kmem_cache *);

void *lkpi_kmalloc(size_t, gfp_t);
void *lkpi___kmalloc(size_t, gfp_t);
void *lkpi___kmalloc_node(size_t, gfp_t, int);
void *lkpi_krealloc(void *, size_t, gfp_t);
void lkpi_kfree(const void *);

static inline gfp_t
linux_check_m_flags(gfp_t flags)
{
	const gfp_t m = M_NOWAIT | M_WAITOK;

	/* make sure either M_NOWAIT or M_WAITOK is set */
	if ((flags & m) == 0)
		flags |= M_NOWAIT;
	else if ((flags & m) == m)
		flags &= ~M_WAITOK;

	/* mask away LinuxKPI specific flags */
	return (flags & GFP_NATIVE_MASK);
}

/*
 * Base functions with a native implementation.
 */
static inline void *
kmalloc(size_t size, gfp_t flags)
{
	return (lkpi_kmalloc(size, flags));
}

static inline void *
__kmalloc(size_t size, gfp_t flags)
{
	return (lkpi___kmalloc(size, flags));
}

static inline void *
kmalloc_node(size_t size, gfp_t flags, int node)
{
	return (lkpi___kmalloc_node(size, flags, node));
}

static inline void *
krealloc(void *ptr, size_t size, gfp_t flags)
{
	return (lkpi_krealloc(ptr, size, flags));
}

static inline void
kfree(const void *ptr)
{
	lkpi_kfree(ptr);
}

/*
 * Other k*alloc() funtions using the above as underlying allocator.
 */
/* kmalloc */
static inline void *
kmalloc_array(size_t n, size_t size, gfp_t flags)
{
	if (WOULD_OVERFLOW(n, size))
		panic("%s: %zu * %zu overflowed", __func__, n, size);

	return (kmalloc(size * n, flags));
}

static inline void *
kcalloc(size_t n, size_t size, gfp_t flags)
{
	flags |= __GFP_ZERO;
	return (kmalloc_array(n, size, flags));
}

/* kmalloc_node */
static inline void *
kmalloc_array_node(size_t n, size_t size, gfp_t flags, int node)
{
	if (WOULD_OVERFLOW(n, size))
		panic("%s: %zu * %zu overflowed", __func__, n, size);

	return (kmalloc_node(size * n, flags, node));
}

static inline void *
kcalloc_node(size_t n, size_t size, gfp_t flags, int node)
{
	flags |= __GFP_ZERO;
	return (kmalloc_array_node(n, size, flags, node));
}

/* krealloc */
static inline void *
krealloc_array(void *ptr, size_t n, size_t size, gfp_t flags)
{
	if (WOULD_OVERFLOW(n, size))
		return NULL;

	return (krealloc(ptr, n * size, flags));
}

/*
 * vmalloc/kvalloc functions.
 */
static inline void *
__vmalloc(size_t size, gfp_t flags, int other)
{
	return (malloc(size, M_KMALLOC, linux_check_m_flags(flags)));
}

static inline void *
__vmalloc_node(size_t size, gfp_t flags, int node)
{
	return (malloc_domainset(size, M_KMALLOC,
	    linux_get_vm_domain_set(node), linux_check_m_flags(flags)));
}

static inline void *
vmalloc_32(size_t size)
{
	return (contigmalloc(size, M_KMALLOC, M_WAITOK, 0, UINT_MAX, 1, 1));
}

/* May return non-contiguous memory. */
static inline void *
kvmalloc(size_t size, gfp_t flags)
{
	return (malloc(size, M_KMALLOC, linux_check_m_flags(flags)));
}

static inline void *
kvmalloc_array(size_t n, size_t size, gfp_t flags)
{
	if (WOULD_OVERFLOW(n, size))
		panic("%s: %zu * %zu overflowed", __func__, n, size);

	return (kvmalloc(size * n, flags));
}

static inline void *
kvrealloc(const void *ptr, size_t oldsize, size_t newsize, gfp_t flags)
{
	void *newptr;

	if (newsize <= oldsize)
		return (__DECONST(void *, ptr));

	newptr = kvmalloc(newsize, flags);
	if (newptr != NULL) {
		memcpy(newptr, ptr, oldsize);
		kvfree(ptr);
	}

	return (newptr);
}

/*
 * Misc.
 */

static __inline void
kfree_sensitive(const void *ptr)
{
	if (ZERO_OR_NULL_PTR(ptr))
		return;

	zfree(__DECONST(void *, ptr), M_KMALLOC);
}

static inline size_t
ksize(const void *ptr)
{
	return (malloc_usable_size(ptr));
}

static inline size_t
kmalloc_size_roundup(size_t size)
{
	if (unlikely(size == 0 || size == SIZE_MAX))
		return (size);
	return (malloc_size(size));
}

#endif					/* _LINUXKPI_LINUX_SLAB_H_ */
