/*
 * Copyright (c) 1987, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_malloc.c	8.3 (Berkeley) 1/4/94
 * $FreeBSD$
 */

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/vmmeter.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#if defined(INVARIANTS) && defined(__i386__)
#include <machine/cpu.h>
#endif

/*
 * When realloc() is called, if the new size is sufficiently smaller than
 * the old size, realloc() will allocate a new, smaller block to avoid
 * wasting memory. 'Sufficiently smaller' is defined as: newsize <=
 * oldsize / 2^n, where REALLOC_FRACTION defines the value of 'n'.
 */
#ifndef REALLOC_FRACTION
#define	REALLOC_FRACTION	1	/* new block if <= half the size */
#endif

MALLOC_DEFINE(M_CACHE, "cache", "Various Dynamically allocated caches");
MALLOC_DEFINE(M_DEVBUF, "devbuf", "device driver memory");
MALLOC_DEFINE(M_TEMP, "temp", "misc temporary data buffers");

MALLOC_DEFINE(M_IP6OPT, "ip6opt", "IPv6 options");
MALLOC_DEFINE(M_IP6NDP, "ip6ndp", "IPv6 Neighbor Discovery");

static void kmeminit __P((void *));
SYSINIT(kmem, SI_SUB_KMEM, SI_ORDER_FIRST, kmeminit, NULL)

static MALLOC_DEFINE(M_FREE, "free", "should be on free list");

static struct malloc_type *kmemstatistics;
static char *kmembase;
static char *kmemlimit;

#define KMEM_ZSHIFT	4
#define KMEM_ZBASE	16
#define KMEM_ZMASK	(KMEM_ZBASE - 1)

#define KMEM_ZMAX	65536
#define KMEM_ZSIZE	(KMEM_ZMAX >> KMEM_ZSHIFT)
static uma_zone_t kmemzones[KMEM_ZSIZE + 1];


/* These won't be powers of two for long */
struct {
	int size;
	char *name;
} kmemsizes[] = {
	{16, "16"},
	{32, "32"},
	{64, "64"},
	{128, "128"},
	{256, "256"},
	{512, "512"},
	{1024, "1024"},
	{2048, "2048"},
	{4096, "4096"},
	{8192, "8192"},
	{16384, "16384"},
	{32768, "32768"},
	{65536, "65536"},
	{0, NULL},
};

static struct mtx malloc_mtx;

u_int vm_kmem_size;

/*
 *	malloc:
 *
 *	Allocate a block of memory.
 *
 *	If M_NOWAIT is set, this routine will not block and return NULL if
 *	the allocation fails.
 */
void *
malloc(size, type, flags)
	unsigned long size;
	struct malloc_type *type;
	int flags;
{
	int s;
	long indx;
	caddr_t va;
	uma_zone_t zone;
	register struct malloc_type *ksp = type;

#if defined(INVARIANTS)
	if (flags == M_WAITOK)
		KASSERT(curthread->td_intr_nesting_level == 0,
		   ("malloc(M_WAITOK) in interrupt context"));
#endif
	s = splmem();
	/* mtx_lock(&malloc_mtx); XXX */
	while (ksp->ks_memuse >= ksp->ks_limit) {
		if (flags & M_NOWAIT) {
			splx(s);
			/* mtx_unlock(&malloc_mtx); XXX */
			return ((void *) NULL);
		}
		if (ksp->ks_limblocks < 65535)
			ksp->ks_limblocks++;
		msleep((caddr_t)ksp, /* &malloc_mtx */ NULL, PSWP+2, type->ks_shortdesc,
		    0);
	}
	/* mtx_unlock(&malloc_mtx); XXX */

	if (size <= KMEM_ZMAX) {
		indx = size;
		if (indx & KMEM_ZMASK)
			indx = (indx & ~KMEM_ZMASK) + KMEM_ZBASE;
		zone = kmemzones[indx >> KMEM_ZSHIFT];
		indx = zone->uz_size;
		va = uma_zalloc(zone, flags);
		if (va == NULL) {
			/* mtx_lock(&malloc_mtx); XXX */
			goto out;
		}
		ksp->ks_size |= indx;
	} else {
		/* XXX This is not the next power of two so this will break ks_size */
		indx = roundup(size, PAGE_SIZE);
		zone = NULL;
		va = uma_large_malloc(size, flags);
		if (va == NULL) {
			/* mtx_lock(&malloc_mtx); XXX */
			goto out;
		}
	}
	/* mtx_lock(&malloc_mtx); XXX */
	ksp->ks_memuse += indx;
	ksp->ks_inuse++;
out:
	ksp->ks_calls++;
	if (ksp->ks_memuse > ksp->ks_maxused)
		ksp->ks_maxused = ksp->ks_memuse;
	splx(s);
	/* mtx_unlock(&malloc_mtx); XXX */
	/* XXX: Do idle pre-zeroing.  */
	if (va != NULL && (flags & M_ZERO))
		bzero(va, size);
	return ((void *) va);
}

/*
 *	free:
 *
 *	Free a block of memory allocated by malloc.
 *
 *	This routine may not block.
 */
void
free(addr, type)
	void *addr;
	struct malloc_type *type;
{
	uma_slab_t slab;
	void *mem;
	u_long size;
	int s;
	register struct malloc_type *ksp = type;

	/* free(NULL, ...) does nothing */
	if (addr == NULL)
		return;

	size = 0;
	s = splmem();

	mem = (void *)((u_long)addr & (~UMA_SLAB_MASK));
	slab = hash_sfind(mallochash, mem);

	if (slab == NULL)
		panic("free: address %p(%p) has not been allocated.\n", addr, mem);

	if (!(slab->us_flags & UMA_SLAB_MALLOC)) {
		size = slab->us_zone->uz_size;
		uma_zfree_arg(slab->us_zone, addr, slab);
	} else {
		size = slab->us_size;
		uma_large_free(slab);
	}
	/* mtx_lock(&malloc_mtx); XXX */

	ksp->ks_memuse -= size;
	if (ksp->ks_memuse + size >= ksp->ks_limit &&
	    ksp->ks_memuse < ksp->ks_limit)
		wakeup((caddr_t)ksp);
	ksp->ks_inuse--;
	splx(s);
	/* mtx_unlock(&malloc_mtx); XXX */
}

/*
 *	realloc: change the size of a memory block
 */
void *
realloc(addr, size, type, flags)
	void *addr;
	unsigned long size;
	struct malloc_type *type;
	int flags;
{
	uma_slab_t slab;
	unsigned long alloc;
	void *newaddr;

	/* realloc(NULL, ...) is equivalent to malloc(...) */
	if (addr == NULL)
		return (malloc(size, type, flags));

	slab = hash_sfind(mallochash,
	    (void *)((u_long)addr & ~(UMA_SLAB_MASK)));

	/* Sanity check */
	KASSERT(slab != NULL,
	    ("realloc: address %p out of range", (void *)addr));

	/* Get the size of the original block */
	if (slab->us_zone)
		alloc = slab->us_zone->uz_size;
	else
		alloc = slab->us_size;

	/* Reuse the original block if appropriate */
	if (size <= alloc
	    && (size > (alloc >> REALLOC_FRACTION) || alloc == MINALLOCSIZE))
		return (addr);

	/* Allocate a new, bigger (or smaller) block */
	if ((newaddr = malloc(size, type, flags)) == NULL)
		return (NULL);

	/* Copy over original contents */
	bcopy(addr, newaddr, min(size, alloc));
	free(addr, type);
	return (newaddr);
}

/*
 *	reallocf: same as realloc() but free memory on failure.
 */
void *
reallocf(addr, size, type, flags)
	void *addr;
	unsigned long size;
	struct malloc_type *type;
	int flags;
{
	void *mem;

	if ((mem = realloc(addr, size, type, flags)) == NULL)
		free(addr, type);
	return (mem);
}

/*
 * Initialize the kernel memory allocator
 */
/* ARGSUSED*/
static void
kmeminit(dummy)
	void *dummy;
{
	register long indx;
	u_long npg;
	u_long mem_size;
	void *hashmem;
	u_long hashsize;
	int highbit;
	int bits;
	int i;

	mtx_init(&malloc_mtx, "malloc", MTX_DEF);

	/*
	 * Try to auto-tune the kernel memory size, so that it is
	 * more applicable for a wider range of machine sizes.
	 * On an X86, a VM_KMEM_SIZE_SCALE value of 4 is good, while
	 * a VM_KMEM_SIZE of 12MB is a fair compromise.  The
	 * VM_KMEM_SIZE_MAX is dependent on the maximum KVA space
	 * available, and on an X86 with a total KVA space of 256MB,
	 * try to keep VM_KMEM_SIZE_MAX at 80MB or below.
	 *
	 * Note that the kmem_map is also used by the zone allocator,
	 * so make sure that there is enough space.
	 */
	vm_kmem_size = VM_KMEM_SIZE;
	mem_size = cnt.v_page_count * PAGE_SIZE;

#if defined(VM_KMEM_SIZE_SCALE)
	if ((mem_size / VM_KMEM_SIZE_SCALE) > vm_kmem_size)
		vm_kmem_size = mem_size / VM_KMEM_SIZE_SCALE;
#endif

#if defined(VM_KMEM_SIZE_MAX)
	if (vm_kmem_size >= VM_KMEM_SIZE_MAX)
		vm_kmem_size = VM_KMEM_SIZE_MAX;
#endif

	/* Allow final override from the kernel environment */
	TUNABLE_INT_FETCH("kern.vm.kmem.size", &vm_kmem_size);

	/*
	 * Limit kmem virtual size to twice the physical memory.
	 * This allows for kmem map sparseness, but limits the size
	 * to something sane. Be careful to not overflow the 32bit
	 * ints while doing the check.
	 */
	if ((vm_kmem_size / 2) > (cnt.v_page_count * PAGE_SIZE))
		vm_kmem_size = 2 * cnt.v_page_count * PAGE_SIZE;

	/*
	 * In mbuf_init(), we set up submaps for mbufs and clusters, in which
	 * case we rounddown() (nmbufs * MSIZE) and (nmbclusters * MCLBYTES),
	 * respectively. Mathematically, this means that what we do here may
	 * amount to slightly more address space than we need for the submaps,
	 * but it never hurts to have an extra page in kmem_map.
	 */
	npg = (nmbufs * MSIZE + nmbclusters * MCLBYTES + nmbcnt *
	    sizeof(u_int) + vm_kmem_size) / PAGE_SIZE;

	kmem_map = kmem_suballoc(kernel_map, (vm_offset_t *)&kmembase,
		(vm_offset_t *)&kmemlimit, (vm_size_t)(npg * PAGE_SIZE));
	kmem_map->system_map = 1;

	hashsize = npg * sizeof(void *);

	highbit = 0;
	bits = 0;
	/* The hash size must be a power of two */
	for (i = 0; i < 8 * sizeof(hashsize); i++)
		if (hashsize & (1 << i)) {
			highbit = i;
			bits++;
		}
	if (bits > 1) 
		hashsize = 1 << (highbit);

	hashmem = (void *)kmem_alloc(kernel_map, (vm_size_t)hashsize);
	uma_startup2(hashmem, hashsize / sizeof(void *));

	for (i = 0, indx = 0; kmemsizes[indx].size != 0; indx++) {
		uma_zone_t zone;
		int size = kmemsizes[indx].size;
		char *name = kmemsizes[indx].name;

		zone = uma_zcreate(name, size, NULL, NULL, NULL, NULL,
		    UMA_ALIGN_PTR, UMA_ZONE_MALLOC);
		for (;i <= size; i+= KMEM_ZBASE)
			kmemzones[i >> KMEM_ZSHIFT] = zone;
		
	}
}

void
malloc_init(data)
	void *data;
{
	struct malloc_type *type = (struct malloc_type *)data;

	if (type->ks_magic != M_MAGIC)
		panic("malloc type lacks magic");

	if (type->ks_limit != 0)
		return;

	if (cnt.v_page_count == 0)
		panic("malloc_init not allowed before vm init");

	/*
	 * The default limits for each malloc region is 1/2 of the
	 * malloc portion of the kmem map size.
	 */
	type->ks_limit = vm_kmem_size / 2;
	type->ks_next = kmemstatistics;	
	kmemstatistics = type;
}

void
malloc_uninit(data)
	void *data;
{
	struct malloc_type *type = (struct malloc_type *)data;
	struct malloc_type *t;

	if (type->ks_magic != M_MAGIC)
		panic("malloc type lacks magic");

	if (cnt.v_page_count == 0)
		panic("malloc_uninit not allowed before vm init");

	if (type->ks_limit == 0)
		panic("malloc_uninit on uninitialized type");

	if (type == kmemstatistics)
		kmemstatistics = type->ks_next;
	else {
		for (t = kmemstatistics; t->ks_next != NULL; t = t->ks_next) {
			if (t->ks_next == type) {
				t->ks_next = type->ks_next;
				break;
			}
		}
	}
	type->ks_next = NULL;
	type->ks_limit = 0;
}
