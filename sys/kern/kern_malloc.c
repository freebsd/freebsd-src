/*-
 * Copyright (c) 1987, 1991, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2005-2009 Robert N. M. Watson
 * All rights reserved.
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

/*
 * Kernel malloc(9) implementation -- general purpose kernel memory allocator
 * based on memory types.  Back end is implemented using the UMA(9) zone
 * allocator.  A set of fixed-size buckets are used for smaller allocations,
 * and a special UMA allocation interface is used for larger allocations.
 * Callers declare memory types, and statistics are maintained independently
 * for each memory type.  Statistics are maintained per-CPU for performance
 * reasons.  See malloc(9) and comments in malloc.h for a detailed
 * description.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_kdtrace.h"
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
#include <sys/sbuf.h>
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
#ifdef DEBUG_REDZONE
#include <vm/redzone.h>
#endif

#if defined(INVARIANTS) && defined(__i386__)
#include <machine/cpu.h>
#endif

#include <ddb/ddb.h>

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>

dtrace_malloc_probe_func_t	dtrace_malloc_probe;
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

/*
 * Centrally define some common malloc types.
 */
MALLOC_DEFINE(M_CACHE, "cache", "Various Dynamically allocated caches");
MALLOC_DEFINE(M_DEVBUF, "devbuf", "device driver memory");
MALLOC_DEFINE(M_TEMP, "temp", "misc temporary data buffers");

MALLOC_DEFINE(M_IP6OPT, "ip6opt", "IPv6 options");
MALLOC_DEFINE(M_IP6NDP, "ip6ndp", "IPv6 Neighbor Discovery");

static void kmeminit(void *);
SYSINIT(kmem, SI_SUB_KMEM, SI_ORDER_FIRST, kmeminit, NULL);

static MALLOC_DEFINE(M_FREE, "free", "should be on free list");

static struct malloc_type *kmemstatistics;
static vm_offset_t kmembase;
static vm_offset_t kmemlimit;
static int kmemcount;

#define KMEM_ZSHIFT	4
#define KMEM_ZBASE	16
#define KMEM_ZMASK	(KMEM_ZBASE - 1)

#define KMEM_ZMAX	PAGE_SIZE
#define KMEM_ZSIZE	(KMEM_ZMAX >> KMEM_ZSHIFT)
static uint8_t kmemsize[KMEM_ZSIZE + 1];

/*
 * Small malloc(9) memory allocations are allocated from a set of UMA buckets
 * of various sizes.
 *
 * XXX: The comment here used to read "These won't be powers of two for
 * long."  It's possible that a significant amount of wasted memory could be
 * recovered by tuning the sizes of these buckets.
 */
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

/*
 * Zone to allocate malloc type descriptions from.  For ABI reasons, memory
 * types are described by a data structure passed by the declaring code, but
 * the malloc(9) implementation has its own data structure describing the
 * type and statistics.  This permits the malloc(9)-internal data structures
 * to be modified without breaking binary-compiled kernel modules that
 * declare malloc types.
 */
static uma_zone_t mt_zone;

u_long vm_kmem_size;
SYSCTL_ULONG(_vm, OID_AUTO, kmem_size, CTLFLAG_RD, &vm_kmem_size, 0,
    "Size of kernel memory");

static u_long vm_kmem_size_min;
SYSCTL_ULONG(_vm, OID_AUTO, kmem_size_min, CTLFLAG_RD, &vm_kmem_size_min, 0,
    "Minimum size of kernel memory");

static u_long vm_kmem_size_max;
SYSCTL_ULONG(_vm, OID_AUTO, kmem_size_max, CTLFLAG_RD, &vm_kmem_size_max, 0,
    "Maximum size of kernel memory");

static u_int vm_kmem_size_scale;
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

static int sysctl_kern_malloc_stats(SYSCTL_HANDLER_ARGS);

/*
 * time_uptime of the last malloc(9) failure (induced or real).
 */
static time_t t_malloc_fail;

/*
 * malloc(9) fault injection -- cause malloc failures every (n) mallocs when
 * the caller specifies M_NOWAIT.  If set to 0, no failures are caused.
 */
#ifdef MALLOC_MAKE_FAILURES
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
 * An allocation has succeeded -- update malloc type statistics for the
 * amount of bucket size.  Occurs within a critical section so that the
 * thread isn't preempted and doesn't migrate while updating per-PCU
 * statistics.
 */
static void
malloc_type_zone_allocated(struct malloc_type *mtp, unsigned long size,
    int zindx)
{
	struct malloc_type_internal *mtip;
	struct malloc_type_stats *mtsp;

	critical_enter();
	mtip = mtp->ks_handle;
	mtsp = &mtip->mti_stats[curcpu];
	if (size > 0) {
		mtsp->mts_memalloced += size;
		mtsp->mts_numallocs++;
	}
	if (zindx != -1)
		mtsp->mts_size |= 1 << zindx;

#ifdef KDTRACE_HOOKS
	if (dtrace_malloc_probe != NULL) {
		uint32_t probe_id = mtip->mti_probes[DTMALLOC_PROBE_MALLOC];
		if (probe_id != 0)
			(dtrace_malloc_probe)(probe_id,
			    (uintptr_t) mtp, (uintptr_t) mtip,
			    (uintptr_t) mtsp, size, zindx);
	}
#endif

	critical_exit();
}

void
malloc_type_allocated(struct malloc_type *mtp, unsigned long size)
{

	if (size > 0)
		malloc_type_zone_allocated(mtp, size, -1);
}

/*
 * A free operation has occurred -- update malloc type statistics for the
 * amount of the bucket size.  Occurs within a critical section so that the
 * thread isn't preempted and doesn't migrate while updating per-CPU
 * statistics.
 */
void
malloc_type_freed(struct malloc_type *mtp, unsigned long size)
{
	struct malloc_type_internal *mtip;
	struct malloc_type_stats *mtsp;

	critical_enter();
	mtip = mtp->ks_handle;
	mtsp = &mtip->mti_stats[curcpu];
	mtsp->mts_memfreed += size;
	mtsp->mts_numfrees++;

#ifdef KDTRACE_HOOKS
	if (dtrace_malloc_probe != NULL) {
		uint32_t probe_id = mtip->mti_probes[DTMALLOC_PROBE_FREE];
		if (probe_id != 0)
			(dtrace_malloc_probe)(probe_id,
			    (uintptr_t) mtp, (uintptr_t) mtip,
			    (uintptr_t) mtsp, size, 0);
	}
#endif

	critical_exit();
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
malloc(unsigned long size, struct malloc_type *mtp, int flags)
{
	int indx;
	caddr_t va;
	uma_zone_t zone;
#if defined(DIAGNOSTIC) || defined(DEBUG_REDZONE)
	unsigned long osize = size;
#endif

#ifdef INVARIANTS
	KASSERT(mtp->ks_magic == M_MAGIC, ("malloc: bad malloc type magic"));
	/*
	 * Check that exactly one of M_WAITOK or M_NOWAIT is specified.
	 */
	indx = flags & (M_WAITOK | M_NOWAIT);
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
	if (memguard_cmp(mtp))
		return memguard_alloc(size, flags);
#endif

#ifdef DEBUG_REDZONE
	size = redzone_size_ntor(size);
#endif

	if (size <= KMEM_ZMAX) {
		if (size & KMEM_ZMASK)
			size = (size & ~KMEM_ZMASK) + KMEM_ZBASE;
		indx = kmemsize[size >> KMEM_ZSHIFT];
		zone = kmemzones[indx].kz_zone;
#ifdef MALLOC_PROFILE
		krequests[size >> KMEM_ZSHIFT]++;
#endif
		va = uma_zalloc(zone, flags);
		if (va != NULL)
			size = zone->uz_size;
		malloc_type_zone_allocated(mtp, va == NULL ? 0 : size, indx);
	} else {
		size = roundup(size, PAGE_SIZE);
		zone = NULL;
		va = uma_large_malloc(size, flags);
		malloc_type_allocated(mtp, va == NULL ? 0 : size);
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
#ifdef DEBUG_REDZONE
	if (va != NULL)
		va = redzone_setup(va, osize);
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
free(void *addr, struct malloc_type *mtp)
{
	uma_slab_t slab;
	u_long size;

	KASSERT(mtp->ks_magic == M_MAGIC, ("free: bad malloc type magic"));

	/* free(NULL, ...) does nothing */
	if (addr == NULL)
		return;

#ifdef DEBUG_MEMGUARD
	if (memguard_cmp(mtp)) {
		memguard_free(addr);
		return;
	}
#endif

#ifdef DEBUG_REDZONE
	redzone_check(addr);
	addr = redzone_addr_ntor(addr);
#endif

	slab = vtoslab((vm_offset_t)addr & (~UMA_SLAB_MASK));

	if (slab == NULL)
		panic("free: address %p(%p) has not been allocated.\n",
		    addr, (void *)((u_long)addr & (~UMA_SLAB_MASK)));


	if (!(slab->us_flags & UMA_SLAB_MALLOC)) {
#ifdef INVARIANTS
		struct malloc_type **mtpp = addr;
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
		mtpp = (struct malloc_type **)
		    ((unsigned long)mtpp & ~UMA_ALIGN_PTR);
		mtpp += (size - sizeof(struct malloc_type *)) /
		    sizeof(struct malloc_type *);
		*mtpp = mtp;
#endif
		uma_zfree_arg(LIST_FIRST(&slab->us_keg->uk_zones), addr, slab);
	} else {
		size = slab->us_size;
		uma_large_free(slab);
	}
	malloc_type_freed(mtp, size);
}

/*
 *	realloc: change the size of a memory block
 */
void *
realloc(void *addr, unsigned long size, struct malloc_type *mtp, int flags)
{
	uma_slab_t slab;
	unsigned long alloc;
	void *newaddr;

	KASSERT(mtp->ks_magic == M_MAGIC,
	    ("realloc: bad malloc type magic"));

	/* realloc(NULL, ...) is equivalent to malloc(...) */
	if (addr == NULL)
		return (malloc(size, mtp, flags));

	/*
	 * XXX: Should report free of old memory and alloc of new memory to
	 * per-CPU stats.
	 */

#ifdef DEBUG_MEMGUARD
if (memguard_cmp(mtp)) {
	slab = NULL;
	alloc = size;
} else {
#endif

#ifdef DEBUG_REDZONE
	slab = NULL;
	alloc = redzone_get_size(addr);
#else
	slab = vtoslab((vm_offset_t)addr & ~(UMA_SLAB_MASK));

	/* Sanity check */
	KASSERT(slab != NULL,
	    ("realloc: address %p out of range", (void *)addr));

	/* Get the size of the original block */
	if (!(slab->us_flags & UMA_SLAB_MALLOC))
		alloc = slab->us_keg->uk_size;
	else
		alloc = slab->us_size;

	/* Reuse the original block if appropriate */
	if (size <= alloc
	    && (size > (alloc >> REALLOC_FRACTION) || alloc == MINALLOCSIZE))
		return (addr);
#endif /* !DEBUG_REDZONE */

#ifdef DEBUG_MEMGUARD
}
#endif

	/* Allocate a new, bigger (or smaller) block */
	if ((newaddr = malloc(size, mtp, flags)) == NULL)
		return (NULL);

	/* Copy over original contents */
	bcopy(addr, newaddr, min(size, alloc));
	free(addr, mtp);
	return (newaddr);
}

/*
 *	reallocf: same as realloc() but free memory on failure.
 */
void *
reallocf(void *addr, unsigned long size, struct malloc_type *mtp, int flags)
{
	void *mem;

	if ((mem = realloc(addr, size, mtp, flags)) == NULL)
		free(addr, mtp);
	return (mem);
}

/*
 * Initialize the kernel memory allocator
 */
/* ARGSUSED*/
static void
kmeminit(void *dummy)
{
	uint8_t indx;
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

#if defined(VM_KMEM_SIZE_MIN)
	vm_kmem_size_min = VM_KMEM_SIZE_MIN;
#endif
	TUNABLE_ULONG_FETCH("vm.kmem_size_min", &vm_kmem_size_min);
	if (vm_kmem_size_min > 0 && vm_kmem_size < vm_kmem_size_min) {
		vm_kmem_size = vm_kmem_size_min;
	}

#if defined(VM_KMEM_SIZE_MAX)
	vm_kmem_size_max = VM_KMEM_SIZE_MAX;
#endif
	TUNABLE_ULONG_FETCH("vm.kmem_size_max", &vm_kmem_size_max);
	if (vm_kmem_size_max > 0 && vm_kmem_size >= vm_kmem_size_max)
		vm_kmem_size = vm_kmem_size_max;

	/* Allow final override from the kernel environment */
	TUNABLE_ULONG_FETCH("vm.kmem_size", &vm_kmem_size);

	/*
	 * Limit kmem virtual size to twice the physical memory.
	 * This allows for kmem map sparseness, but limits the size
	 * to something sane. Be careful to not overflow the 32bit
	 * ints while doing the check.
	 */
	if (((vm_kmem_size / 2) / PAGE_SIZE) > cnt.v_page_count)
		vm_kmem_size = 2 * cnt.v_page_count * PAGE_SIZE;

	/*
	 * Tune settings based on the kmem map's size at this time.
	 */
	init_param3(vm_kmem_size / PAGE_SIZE);

	kmem_map = kmem_suballoc(kernel_map, &kmembase, &kmemlimit,
	    vm_kmem_size, TRUE);
	kmem_map->system_map = 1;

#ifdef DEBUG_MEMGUARD
	/*
	 * Initialize MemGuard if support compiled in.  MemGuard is a
	 * replacement allocator used for detecting tamper-after-free
	 * scenarios as they occur.  It is only used for debugging.
	 */
	vm_memguard_divisor = 10;
	TUNABLE_INT_FETCH("vm.memguard.divisor", &vm_memguard_divisor);

	/* Pick a conservative value if provided value sucks. */
	if ((vm_memguard_divisor <= 0) ||
	    ((vm_kmem_size / vm_memguard_divisor) == 0))
		vm_memguard_divisor = 10;
	memguard_init(kmem_map, vm_kmem_size / vm_memguard_divisor);
#endif

	uma_startup2();

	mt_zone = uma_zcreate("mt_zone", sizeof(struct malloc_type_internal),
#ifdef INVARIANTS
	    mtrash_ctor, mtrash_dtor, mtrash_init, mtrash_fini,
#else
	    NULL, NULL, NULL, NULL,
#endif
	    UMA_ALIGN_PTR, UMA_ZONE_MALLOC);
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
	struct malloc_type_internal *mtip;
	struct malloc_type *mtp;

	KASSERT(cnt.v_page_count != 0, ("malloc_register before vm_init"));

	mtp = data;
	if (mtp->ks_magic != M_MAGIC)
		panic("malloc_init: bad malloc type magic");

	mtip = uma_zalloc(mt_zone, M_WAITOK | M_ZERO);
	mtp->ks_handle = mtip;

	mtx_lock(&malloc_mtx);
	mtp->ks_next = kmemstatistics;
	kmemstatistics = mtp;
	kmemcount++;
	mtx_unlock(&malloc_mtx);
}

void
malloc_uninit(void *data)
{
	struct malloc_type_internal *mtip;
	struct malloc_type_stats *mtsp;
	struct malloc_type *mtp, *temp;
	uma_slab_t slab;
	long temp_allocs, temp_bytes;
	int i;

	mtp = data;
	KASSERT(mtp->ks_magic == M_MAGIC,
	    ("malloc_uninit: bad malloc type magic"));
	KASSERT(mtp->ks_handle != NULL, ("malloc_deregister: cookie NULL"));

	mtx_lock(&malloc_mtx);
	mtip = mtp->ks_handle;
	mtp->ks_handle = NULL;
	if (mtp != kmemstatistics) {
		for (temp = kmemstatistics; temp != NULL;
		    temp = temp->ks_next) {
			if (temp->ks_next == mtp) {
				temp->ks_next = mtp->ks_next;
				break;
			}
		}
		KASSERT(temp,
		    ("malloc_uninit: type '%s' not found", mtp->ks_shortdesc));
	} else
		kmemstatistics = mtp->ks_next;
	kmemcount--;
	mtx_unlock(&malloc_mtx);

	/*
	 * Look for memory leaks.
	 */
	temp_allocs = temp_bytes = 0;
	for (i = 0; i < MAXCPU; i++) {
		mtsp = &mtip->mti_stats[i];
		temp_allocs += mtsp->mts_numallocs;
		temp_allocs -= mtsp->mts_numfrees;
		temp_bytes += mtsp->mts_memalloced;
		temp_bytes -= mtsp->mts_memfreed;
	}
	if (temp_allocs > 0 || temp_bytes > 0) {
		printf("Warning: memory type %s leaked memory on destroy "
		    "(%ld allocations, %ld bytes leaked).\n", mtp->ks_shortdesc,
		    temp_allocs, temp_bytes);
	}

	slab = vtoslab((vm_offset_t) mtip & (~UMA_SLAB_MASK));
	uma_zfree_arg(mt_zone, mtip, slab);
}

struct malloc_type *
malloc_desc2type(const char *desc)
{
	struct malloc_type *mtp;

	mtx_assert(&malloc_mtx, MA_OWNED);
	for (mtp = kmemstatistics; mtp != NULL; mtp = mtp->ks_next) {
		if (strcmp(mtp->ks_shortdesc, desc) == 0)
			return (mtp);
	}
	return (NULL);
}

static int
sysctl_kern_malloc_stats(SYSCTL_HANDLER_ARGS)
{
	struct malloc_type_stream_header mtsh;
	struct malloc_type_internal *mtip;
	struct malloc_type_header mth;
	struct malloc_type *mtp;
	int buflen, count, error, i;
	struct sbuf sbuf;
	char *buffer;

	mtx_lock(&malloc_mtx);
restart:
	mtx_assert(&malloc_mtx, MA_OWNED);
	count = kmemcount;
	mtx_unlock(&malloc_mtx);
	buflen = sizeof(mtsh) + count * (sizeof(mth) +
	    sizeof(struct malloc_type_stats) * MAXCPU) + 1;
	buffer = malloc(buflen, M_TEMP, M_WAITOK | M_ZERO);
	mtx_lock(&malloc_mtx);
	if (count < kmemcount) {
		free(buffer, M_TEMP);
		goto restart;
	}

	sbuf_new(&sbuf, buffer, buflen, SBUF_FIXEDLEN);

	/*
	 * Insert stream header.
	 */
	bzero(&mtsh, sizeof(mtsh));
	mtsh.mtsh_version = MALLOC_TYPE_STREAM_VERSION;
	mtsh.mtsh_maxcpus = MAXCPU;
	mtsh.mtsh_count = kmemcount;
	if (sbuf_bcat(&sbuf, &mtsh, sizeof(mtsh)) < 0) {
		mtx_unlock(&malloc_mtx);
		error = ENOMEM;
		goto out;
	}

	/*
	 * Insert alternating sequence of type headers and type statistics.
	 */
	for (mtp = kmemstatistics; mtp != NULL; mtp = mtp->ks_next) {
		mtip = (struct malloc_type_internal *)mtp->ks_handle;

		/*
		 * Insert type header.
		 */
		bzero(&mth, sizeof(mth));
		strlcpy(mth.mth_name, mtp->ks_shortdesc, MALLOC_MAX_NAME);
		if (sbuf_bcat(&sbuf, &mth, sizeof(mth)) < 0) {
			mtx_unlock(&malloc_mtx);
			error = ENOMEM;
			goto out;
		}

		/*
		 * Insert type statistics for each CPU.
		 */
		for (i = 0; i < MAXCPU; i++) {
			if (sbuf_bcat(&sbuf, &mtip->mti_stats[i],
			    sizeof(mtip->mti_stats[i])) < 0) {
				mtx_unlock(&malloc_mtx);
				error = ENOMEM;
				goto out;
			}
		}
	}
	mtx_unlock(&malloc_mtx);
	sbuf_finish(&sbuf);
	error = SYSCTL_OUT(req, sbuf_data(&sbuf), sbuf_len(&sbuf));
out:
	sbuf_delete(&sbuf);
	free(buffer, M_TEMP);
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, malloc_stats, CTLFLAG_RD|CTLTYPE_STRUCT,
    0, 0, sysctl_kern_malloc_stats, "s,malloc_type_ustats",
    "Return malloc types");

SYSCTL_INT(_kern, OID_AUTO, malloc_count, CTLFLAG_RD, &kmemcount, 0,
    "Count of kernel malloc types");

void
malloc_type_list(malloc_type_list_func_t *func, void *arg)
{
	struct malloc_type *mtp, **bufmtp;
	int count, i;
	size_t buflen;

	mtx_lock(&malloc_mtx);
restart:
	mtx_assert(&malloc_mtx, MA_OWNED);
	count = kmemcount;
	mtx_unlock(&malloc_mtx);

	buflen = sizeof(struct malloc_type *) * count;
	bufmtp = malloc(buflen, M_TEMP, M_WAITOK);

	mtx_lock(&malloc_mtx);

	if (count < kmemcount) {
		free(bufmtp, M_TEMP);
		goto restart;
	}

	for (mtp = kmemstatistics, i = 0; mtp != NULL; mtp = mtp->ks_next, i++)
		bufmtp[i] = mtp;

	mtx_unlock(&malloc_mtx);

	for (i = 0; i < count; i++)
		(func)(bufmtp[i], arg);

	free(bufmtp, M_TEMP);
}

#ifdef DDB
DB_SHOW_COMMAND(malloc, db_show_malloc)
{
	struct malloc_type_internal *mtip;
	struct malloc_type *mtp;
	uint64_t allocs, frees;
	uint64_t alloced, freed;
	int i;

	db_printf("%18s %12s  %12s %12s\n", "Type", "InUse", "MemUse",
	    "Requests");
	for (mtp = kmemstatistics; mtp != NULL; mtp = mtp->ks_next) {
		mtip = (struct malloc_type_internal *)mtp->ks_handle;
		allocs = 0;
		frees = 0;
		alloced = 0;
		freed = 0;
		for (i = 0; i < MAXCPU; i++) {
			allocs += mtip->mti_stats[i].mts_numallocs;
			frees += mtip->mti_stats[i].mts_numfrees;
			alloced += mtip->mti_stats[i].mts_memalloced;
			freed += mtip->mti_stats[i].mts_memfreed;
		}
		db_printf("%18s %12ju %12juK %12ju\n",
		    mtp->ks_shortdesc, allocs - frees,
		    (alloced - freed + 1023) / 1024, allocs);
	}
}
#endif

#ifdef MALLOC_PROFILE

static int
sysctl_kern_mprof(SYSCTL_HANDLER_ARGS)
{
	int linesize = 64;
	struct sbuf sbuf;
	uint64_t count;
	uint64_t waste;
	uint64_t mem;
	int bufsize;
	int error;
	char *buf;
	int rsize;
	int size;
	int i;

	bufsize = linesize * (KMEM_ZSIZE + 1);
	bufsize += 128; 	/* For the stats line */
	bufsize += 128; 	/* For the banner line */
	waste = 0;
	mem = 0;

	buf = malloc(bufsize, M_TEMP, M_WAITOK|M_ZERO);
	sbuf_new(&sbuf, buf, bufsize, SBUF_FIXEDLEN);
	sbuf_printf(&sbuf, 
	    "\n  Size                    Requests  Real Size\n");
	for (i = 0; i < KMEM_ZSIZE; i++) {
		size = i << KMEM_ZSHIFT;
		rsize = kmemzones[kmemsize[i]].kz_size;
		count = (long long unsigned)krequests[i];

		sbuf_printf(&sbuf, "%6d%28llu%11d\n", size,
		    (unsigned long long)count, rsize);

		if ((rsize * count) > (size * count))
			waste += (rsize * count) - (size * count);
		mem += (rsize * count);
	}
	sbuf_printf(&sbuf,
	    "\nTotal memory used:\t%30llu\nTotal Memory wasted:\t%30llu\n",
	    (unsigned long long)mem, (unsigned long long)waste);
	sbuf_finish(&sbuf);

	error = SYSCTL_OUT(req, sbuf_data(&sbuf), sbuf_len(&sbuf));

	sbuf_delete(&sbuf);
	free(buf, M_TEMP);
	return (error);
}

SYSCTL_OID(_kern, OID_AUTO, mprof, CTLTYPE_STRING|CTLFLAG_RD,
    NULL, 0, sysctl_kern_mprof, "A", "Malloc Profiling");
#endif /* MALLOC_PROFILE */
