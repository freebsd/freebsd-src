/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/vmmeter.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <vm/uma_dbg.h>

#ifdef DEBUG_MEMGUARD
#include <vm/memguard.h>
#endif

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

static void kmeminit(void *);
SYSINIT(kmem, SI_SUB_KMEM, SI_ORDER_FIRST, kmeminit, NULL)

static MALLOC_DEFINE(M_FREE, "free", "should be on free list");

static struct malloc_type *kmemstatistics;
static char *kmembase;
static char *kmemlimit;

#define KMEM_ZSHIFT	4
#define KMEM_ZBASE	16
#define KMEM_ZMASK	(KMEM_ZBASE - 1)

#define KMEM_ZMAX	PAGE_SIZE
#define KMEM_ZSIZE	(KMEM_ZMAX >> KMEM_ZSHIFT)
static u_int8_t kmemsize[KMEM_ZSIZE + 1];

/* These won't be powers of two for long */
struct {
	int kz_size;
	char *kz_name;
	uma_zone_t kz_zone;
} kmemzones[] = {
	{16, "16", NULL},
	{32, "32", NULL},
	{64, "64", NULL},
	{128, "128", NULL},
	{256, "256", NULL},
	{512, "512", NULL},
	{1024, "1024", NULL},
	{2048, "2048", NULL},
	{4096, "4096", NULL},
#if PAGE_SIZE > 4096
	{8192, "8192", NULL},
#if PAGE_SIZE > 8192
	{16384, "16384", NULL},
#if PAGE_SIZE > 16384
	{32768, "32768", NULL},
#if PAGE_SIZE > 32768
	{65536, "65536", NULL},
#if PAGE_SIZE > 65536
#error	"Unsupported PAGE_SIZE"
#endif	/* 65536 */
#endif	/* 32768 */
#endif	/* 16384 */
#endif	/* 8192 */
#endif	/* 4096 */
	{0, NULL},
};

#ifdef DEBUG_MEMGUARD
u_int vm_memguard_divisor;
SYSCTL_UINT(_vm, OID_AUTO, memguard_divisor, CTLFLAG_RD, &vm_memguard_divisor,
    0, "(kmem_size/memguard_divisor) == memguard submap size");
#endif

u_int vm_kmem_size;
SYSCTL_UINT(_vm, OID_AUTO, kmem_size, CTLFLAG_RD, &vm_kmem_size, 0,
    "Size of kernel memory");

u_int vm_kmem_size_max;
SYSCTL_UINT(_vm, OID_AUTO, kmem_size_max, CTLFLAG_RD, &vm_kmem_size_max, 0,
    "Maximum size of kernel memory");

u_int vm_kmem_size_scale;
SYSCTL_UINT(_vm, OID_AUTO, kmem_size_scale, CTLFLAG_RD, &vm_kmem_size_scale, 0,
    "Scale factor for kernel memory size");

/*
 * The malloc_mtx protects the kmemstatistics linked list.
 */

struct mtx malloc_mtx;

#ifdef MALLOC_PROFILE
uint64_t krequests[KMEM_ZSIZE + 1];

static int sysctl_kern_mprof(SYSCTL_HANDLER_ARGS);
#endif

static int sysctl_kern_malloc(SYSCTL_HANDLER_ARGS);

/* time_uptime of last malloc(9) failure */
static time_t t_malloc_fail;

#ifdef MALLOC_MAKE_FAILURES
/*
 * Causes malloc failures every (n) mallocs with M_NOWAIT.  If set to 0,
 * doesn't cause failures.
 */
SYSCTL_NODE(_debug, OID_AUTO, malloc, CTLFLAG_RD, 0,
    "Kernel malloc debugging options");

static int malloc_failure_rate;
static int malloc_nowait_count;
static int malloc_failure_count;
SYSCTL_INT(_debug_malloc, OID_AUTO, failure_rate, CTLFLAG_RW,
    &malloc_failure_rate, 0, "Every (n) mallocs with M_NOWAIT will fail");
TUNABLE_INT("debug.malloc.failure_rate", &malloc_failure_rate);
SYSCTL_INT(_debug_malloc, OID_AUTO, failure_count, CTLFLAG_RD,
    &malloc_failure_count, 0, "Number of imposed M_NOWAIT malloc failures");
#endif

int
malloc_last_fail(void)
{

	return (time_uptime - t_malloc_fail);
}

/*
 * Add this to the informational malloc_type bucket.
 */
static void
malloc_type_zone_allocated(struct malloc_type *ksp, unsigned long size,
    int zindx)
{
	mtx_lock(&ksp->ks_mtx);
	ksp->ks_calls++;
	if (zindx != -1)
		ksp->ks_size |= 1 << zindx;
	if (size != 0) {
		ksp->ks_memuse += size;
		ksp->ks_inuse++;
		if (ksp->ks_memuse > ksp->ks_maxused)
			ksp->ks_maxused = ksp->ks_memuse;
	}
	mtx_unlock(&ksp->ks_mtx);
}

void
malloc_type_allocated(struct malloc_type *ksp, unsigned long size)
{
	malloc_type_zone_allocated(ksp, size, -1);
}

/*
 * Remove this allocation from the informational malloc_type bucket.
 */
void
malloc_type_freed(struct malloc_type *ksp, unsigned long size)
{
	mtx_lock(&ksp->ks_mtx);
	KASSERT(size <= ksp->ks_memuse,
		("malloc(9)/free(9) confusion.\n%s",
		 "Probably freeing with wrong type, but maybe not here."));
	ksp->ks_memuse -= size;
	ksp->ks_inuse--;
	mtx_unlock(&ksp->ks_mtx);
}

/*
 *	malloc:
 *
 *	Allocate a block of memory.
 *
 *	If M_NOWAIT is set, this routine will not block and return NULL if
 *	the allocation fails.
 */
void *
malloc(unsigned long size, struct malloc_type *type, int flags)
{
	int indx;
	caddr_t va;
	uma_zone_t zone;
	uma_keg_t keg;
#ifdef DIAGNOSTIC
	unsigned long osize = size;
#endif

#ifdef INVARIANTS
	/*
	 * To make sure that WAITOK or NOWAIT is set, but not more than
	 * one, and check against the API botches that are common.
	 */
	indx = flags & (M_WAITOK | M_NOWAIT | M_DONTWAIT | M_TRYWAIT);
	if (indx != M_NOWAIT && indx != M_WAITOK) {
		static	struct timeval lasterr;
		static	int curerr, once;
		if (once == 0 && ppsratecheck(&lasterr, &curerr, 1)) {
			printf("Bad malloc flags: %x\n", indx);
			kdb_backtrace();
			flags |= M_WAITOK;
			once++;
		}
	}
#endif
#if 0
	if (size == 0)
		kdb_enter("zero size malloc");
#endif
#ifdef MALLOC_MAKE_FAILURES
	if ((flags & M_NOWAIT) && (malloc_failure_rate != 0)) {
		atomic_add_int(&malloc_nowait_count, 1);
		if ((malloc_nowait_count % malloc_failure_rate) == 0) {
			atomic_add_int(&malloc_failure_count, 1);
			t_malloc_fail = time_uptime;
			return (NULL);
		}
	}
#endif
	if (flags & M_WAITOK)
		KASSERT(curthread->td_intr_nesting_level == 0,
		   ("malloc(M_WAITOK) in interrupt context"));

#ifdef DEBUG_MEMGUARD
	/* XXX CHANGEME! */
	if (type == M_SUBPROC)
		return memguard_alloc(size, flags);
#endif

	if (size <= KMEM_ZMAX) {
		if (size & KMEM_ZMASK)
			size = (size & ~KMEM_ZMASK) + KMEM_ZBASE;
		indx = kmemsize[size >> KMEM_ZSHIFT];
		zone = kmemzones[indx].kz_zone;
		keg = zone->uz_keg;
#ifdef MALLOC_PROFILE
		krequests[size >> KMEM_ZSHIFT]++;
#endif
		va = uma_zalloc(zone, flags);
		if (va != NULL)
			size = keg->uk_size;
		malloc_type_zone_allocated(type, va == NULL ? 0 : size, indx);
	} else {
		size = roundup(size, PAGE_SIZE);
		zone = NULL;
		keg = NULL;
		va = uma_large_malloc(size, flags);
		malloc_type_allocated(type, va == NULL ? 0 : size);
	}
	if (flags & M_WAITOK)
		KASSERT(va != NULL, ("malloc(M_WAITOK) returned NULL"));
	else if (va == NULL)
		t_malloc_fail = time_uptime;
#ifdef DIAGNOSTIC
	if (va != NULL && !(flags & M_ZERO)) {
		memset(va, 0x70, osize);
	}
#endif
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
free(void *addr, struct malloc_type *type)
{
	uma_slab_t slab;
	u_long size;

	/* free(NULL, ...) does nothing */
	if (addr == NULL)
		return;

#ifdef DEBUG_MEMGUARD
	/* XXX CHANGEME! */
	if (type == M_SUBPROC) {
		memguard_free(addr);
		return;
	}
#endif

	KASSERT(type->ks_memuse > 0,
		("malloc(9)/free(9) confusion.\n%s",
		 "Probably freeing with wrong type, but maybe not here."));
	size = 0;

	slab = vtoslab((vm_offset_t)addr & (~UMA_SLAB_MASK));

	if (slab == NULL)
		panic("free: address %p(%p) has not been allocated.\n",
		    addr, (void *)((u_long)addr & (~UMA_SLAB_MASK)));


	if (!(slab->us_flags & UMA_SLAB_MALLOC)) {
#ifdef INVARIANTS
		struct malloc_type **mtp = addr;
#endif
		size = slab->us_keg->uk_size;
#ifdef INVARIANTS
		/*
		 * Cache a pointer to the malloc_type that most recently freed
		 * this memory here.  This way we know who is most likely to
		 * have stepped on it later.
		 *
		 * This code assumes that size is a multiple of 8 bytes for
		 * 64 bit machines
		 */
		mtp = (struct malloc_type **)
		    ((unsigned long)mtp & ~UMA_ALIGN_PTR);
		mtp += (size - sizeof(struct malloc_type *)) /
		    sizeof(struct malloc_type *);
		*mtp = type;
#endif
		uma_zfree_arg(LIST_FIRST(&slab->us_keg->uk_zones), addr, slab);
	} else {
		size = slab->us_size;
		uma_large_free(slab);
	}
	malloc_type_freed(type, size);
}

/*
 *	realloc: change the size of a memory block
 */
void *
realloc(void *addr, unsigned long size, struct malloc_type *type, int flags)
{
	uma_slab_t slab;
	unsigned long alloc;
	void *newaddr;

	/* realloc(NULL, ...) is equivalent to malloc(...) */
	if (addr == NULL)
		return (malloc(size, type, flags));

#ifdef DEBUG_MEMGUARD
/* XXX: CHANGEME! */
if (type == M_SUBPROC) {
	slab = NULL;
	alloc = size;
} else {
#endif

	slab = vtoslab((vm_offset_t)addr & ~(UMA_SLAB_MASK));

	/* Sanity check */
	KASSERT(slab != NULL,
	    ("realloc: address %p out of range", (void *)addr));

	/* Get the size of the original block */
	if (slab->us_keg)
		alloc = slab->us_keg->uk_size;
	else
		alloc = slab->us_size;

	/* Reuse the original block if appropriate */
	if (size <= alloc
	    && (size > (alloc >> REALLOC_FRACTION) || alloc == MINALLOCSIZE))
		return (addr);

#ifdef DEBUG_MEMGUARD
}
#endif

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
reallocf(void *addr, unsigned long size, struct malloc_type *type, int flags)
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
kmeminit(void *dummy)
{
	u_int8_t indx;
	u_long mem_size;
	int i;
 
	mtx_init(&malloc_mtx, "malloc", NULL, MTX_DEF);

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
	vm_kmem_size = VM_KMEM_SIZE + nmbclusters * PAGE_SIZE;
	mem_size = cnt.v_page_count;

#if defined(VM_KMEM_SIZE_SCALE)
	vm_kmem_size_scale = VM_KMEM_SIZE_SCALE;
#endif
	TUNABLE_INT_FETCH("vm.kmem_size_scale", &vm_kmem_size_scale);
	if (vm_kmem_size_scale > 0 &&
	    (mem_size / vm_kmem_size_scale) > (vm_kmem_size / PAGE_SIZE))
		vm_kmem_size = (mem_size / vm_kmem_size_scale) * PAGE_SIZE;

#if defined(VM_KMEM_SIZE_MAX)
	vm_kmem_size_max = VM_KMEM_SIZE_MAX;
#endif
	TUNABLE_INT_FETCH("vm.kmem_size_max", &vm_kmem_size_max);
	if (vm_kmem_size_max > 0 && vm_kmem_size >= vm_kmem_size_max)
		vm_kmem_size = vm_kmem_size_max;

	/* Allow final override from the kernel environment */
#ifndef BURN_BRIDGES
	if (TUNABLE_INT_FETCH("kern.vm.kmem.size", &vm_kmem_size) != 0)
		printf("kern.vm.kmem.size is now called vm.kmem_size!\n");
#endif
	TUNABLE_INT_FETCH("vm.kmem_size", &vm_kmem_size);

	/*
	 * Limit kmem virtual size to twice the physical memory.
	 * This allows for kmem map sparseness, but limits the size
	 * to something sane. Be careful to not overflow the 32bit
	 * ints while doing the check.
	 */
	if (((vm_kmem_size / 2) / PAGE_SIZE) > cnt.v_page_count)
		vm_kmem_size = 2 * cnt.v_page_count * PAGE_SIZE;

	/*
	 * Tune settings based on the kernel map's size at this time.
	 */
	init_param3(vm_kmem_size / PAGE_SIZE);

	kmem_map = kmem_suballoc(kernel_map, (vm_offset_t *)&kmembase,
		(vm_offset_t *)&kmemlimit, vm_kmem_size);
	kmem_map->system_map = 1;

#ifdef DEBUG_MEMGUARD
	/*
	 * Initialize MemGuard if support compiled in.  MemGuard is a
	 * replacement allocator used for detecting tamper-after-free
	 * scenarios as they occur.  It is only used for debugging.
	 */
	vm_memguard_divisor = 10;
	TUNABLE_INT_FETCH("vm.memguard_divisor", &vm_memguard_divisor);

	/* Pick a conservative value if provided value sucks. */
	if ((vm_memguard_divisor <= 0) ||
	    ((vm_kmem_size / vm_memguard_divisor) == 0))
		vm_memguard_divisor = 10;
	memguard_init(kmem_map, vm_kmem_size / vm_memguard_divisor);
#endif

	uma_startup2();

	for (i = 0, indx = 0; kmemzones[indx].kz_size != 0; indx++) {
		int size = kmemzones[indx].kz_size;
		char *name = kmemzones[indx].kz_name;

		kmemzones[indx].kz_zone = uma_zcreate(name, size,
#ifdef INVARIANTS
		    mtrash_ctor, mtrash_dtor, mtrash_init, mtrash_fini,
#else
		    NULL, NULL, NULL, NULL,
#endif
		    UMA_ALIGN_PTR, UMA_ZONE_MALLOC);
		    
		for (;i <= size; i+= KMEM_ZBASE)
			kmemsize[i >> KMEM_ZSHIFT] = indx;
		
	}
}

void
malloc_init(void *data)
{
	struct malloc_type *type = (struct malloc_type *)data;

	mtx_lock(&malloc_mtx);
	if (type->ks_magic != M_MAGIC)
		panic("malloc type lacks magic");

	if (cnt.v_page_count == 0)
		panic("malloc_init not allowed before vm init");

	if (type->ks_next != NULL)
		return;

	type->ks_next = kmemstatistics;	
	kmemstatistics = type;
	mtx_init(&type->ks_mtx, type->ks_shortdesc, "Malloc Stats", MTX_DEF);
	mtx_unlock(&malloc_mtx);
}

void
malloc_uninit(void *data)
{
	struct malloc_type *type = (struct malloc_type *)data;
	struct malloc_type *t;

	mtx_lock(&malloc_mtx);
	mtx_lock(&type->ks_mtx);
	if (type->ks_magic != M_MAGIC)
		panic("malloc type lacks magic");

	if (cnt.v_page_count == 0)
		panic("malloc_uninit not allowed before vm init");

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
	mtx_destroy(&type->ks_mtx);
	mtx_unlock(&malloc_mtx);
}

static int
sysctl_kern_malloc(SYSCTL_HANDLER_ARGS)
{
	struct malloc_type *type;
	int linesize = 128;
	int curline;
	int bufsize;
	int first;
	int error;
	char *buf;
	char *p;
	int cnt;
	int len;
	int i;

	cnt = 0;

	mtx_lock(&malloc_mtx);
	for (type = kmemstatistics; type != NULL; type = type->ks_next)
		cnt++;

	mtx_unlock(&malloc_mtx);
	bufsize = linesize * (cnt + 1);
	p = buf = (char *)malloc(bufsize, M_TEMP, M_WAITOK|M_ZERO);
	mtx_lock(&malloc_mtx);

	len = snprintf(p, linesize,
	    "\n        Type  InUse MemUse HighUse Requests  Size(s)\n");
	p += len;

	for (type = kmemstatistics; cnt != 0 && type != NULL;
	    type = type->ks_next, cnt--) {
		if (type->ks_calls == 0)
			continue;

		curline = linesize - 2;	/* Leave room for the \n */
		len = snprintf(p, curline, "%13s%6lu%6luK%7luK%9llu",
			type->ks_shortdesc,
			type->ks_inuse,
			(type->ks_memuse + 1023) / 1024,
			(type->ks_maxused + 1023) / 1024,
			(long long unsigned)type->ks_calls);
		curline -= len;
		p += len;

		first = 1;
		for (i = 0; i < sizeof(kmemzones) / sizeof(kmemzones[0]) - 1;
		    i++) {
			if (type->ks_size & (1 << i)) {
				if (first)
					len = snprintf(p, curline, "  ");
				else
					len = snprintf(p, curline, ",");
				curline -= len;
				p += len;

				len = snprintf(p, curline,
				    "%s", kmemzones[i].kz_name);
				curline -= len;
				p += len;

				first = 0;
			}
		}

		len = snprintf(p, 2, "\n");
		p += len;
	}

	mtx_unlock(&malloc_mtx);
	error = SYSCTL_OUT(req, buf, p - buf);

	free(buf, M_TEMP);
	return (error);
}

SYSCTL_OID(_kern, OID_AUTO, malloc, CTLTYPE_STRING|CTLFLAG_RD,
    NULL, 0, sysctl_kern_malloc, "A", "Malloc Stats");

#ifdef MALLOC_PROFILE

static int
sysctl_kern_mprof(SYSCTL_HANDLER_ARGS)
{
	int linesize = 64;
	uint64_t count;
	uint64_t waste;
	uint64_t mem;
	int bufsize;
	int error;
	char *buf;
	int rsize;
	int size;
	char *p;
	int len;
	int i;

	bufsize = linesize * (KMEM_ZSIZE + 1);
	bufsize += 128; 	/* For the stats line */
	bufsize += 128; 	/* For the banner line */
	waste = 0;
	mem = 0;

	p = buf = (char *)malloc(bufsize, M_TEMP, M_WAITOK|M_ZERO);
	len = snprintf(p, bufsize,
	    "\n  Size                    Requests  Real Size\n");
	bufsize -= len;
	p += len;

	for (i = 0; i < KMEM_ZSIZE; i++) {
		size = i << KMEM_ZSHIFT;
		rsize = kmemzones[kmemsize[i]].kz_size;
		count = (long long unsigned)krequests[i];

		len = snprintf(p, bufsize, "%6d%28llu%11d\n",
		    size, (unsigned long long)count, rsize);
		bufsize -= len;
		p += len;

		if ((rsize * count) > (size * count))
			waste += (rsize * count) - (size * count);
		mem += (rsize * count);
	}

	len = snprintf(p, bufsize,
	    "\nTotal memory used:\t%30llu\nTotal Memory wasted:\t%30llu\n",
	    (unsigned long long)mem, (unsigned long long)waste);
	p += len;

	error = SYSCTL_OUT(req, buf, p - buf);

	free(buf, M_TEMP);
	return (error);
}

SYSCTL_OID(_kern, OID_AUTO, mprof, CTLTYPE_STRING|CTLFLAG_RD,
    NULL, 0, sysctl_kern_mprof, "A", "Malloc Profiling");
#endif /* MALLOC_PROFILE */
