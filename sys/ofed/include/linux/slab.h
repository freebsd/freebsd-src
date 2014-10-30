/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
#ifndef	_LINUX_SLAB_H_
#define	_LINUX_SLAB_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <vm/uma.h>

#include <linux/types.h>
#include <linux/gfp.h>

MALLOC_DECLARE(M_KMALLOC);

#define	kmalloc(size, flags)		malloc((size), M_KMALLOC, (flags))
#define	kzalloc(size, flags)		kmalloc((size), (flags) | M_ZERO)
#define	kzalloc_node(size, flags, node)	kzalloc(size, flags)
#define	kfree(ptr)			free(__DECONST(void *, (ptr)), M_KMALLOC)
#define	krealloc(ptr, size, flags)	realloc((ptr), (size), M_KMALLOC, (flags))
#define	kcalloc(n, size, flags)	        kmalloc((n) * (size), flags | M_ZERO)
#define	vzalloc(size)			kzalloc(size, GFP_KERNEL | __GFP_NOWARN)
#define	vfree(arg)			kfree(arg)
#define	vmalloc(size)                   kmalloc(size, GFP_KERNEL)
#define	vmalloc_node(size, node)        kmalloc(size, GFP_KERNEL)

struct kmem_cache {
	uma_zone_t	cache_zone;
	void		(*cache_ctor)(void *);
};

#define	SLAB_HWCACHE_ALIGN	0x0001

static inline int
kmem_ctor(void *mem, int size, void *arg, int flags)
{
	void (*ctor)(void *);

	ctor = arg;
	ctor(mem);

	return (0);
}

static inline struct kmem_cache *
kmem_cache_create(char *name, size_t size, size_t align, u_long flags,
    void (*ctor)(void *))
{
	struct kmem_cache *c;

	c = malloc(sizeof(*c), M_KMALLOC, M_WAITOK);
	if (align)
		align--;
	if (flags & SLAB_HWCACHE_ALIGN)
		align = UMA_ALIGN_CACHE;
	c->cache_zone = uma_zcreate(name, size, ctor ? kmem_ctor : NULL,
	    NULL, NULL, NULL, align, 0);
	c->cache_ctor = ctor;

	return c;
}

static inline void *
kmem_cache_alloc(struct kmem_cache *c, int flags)
{
	return uma_zalloc_arg(c->cache_zone, c->cache_ctor, flags);
}

static inline void
kmem_cache_free(struct kmem_cache *c, void *m)
{
	uma_zfree(c->cache_zone, m);
}

static inline void
kmem_cache_destroy(struct kmem_cache *c)
{
	uma_zdestroy(c->cache_zone);
	free(c, M_KMALLOC);
}

#endif	/* _LINUX_SLAB_H_ */
