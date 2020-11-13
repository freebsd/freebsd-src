/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1987, 1991, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2005-2009 Robert N. M. Watson
 * Copyright (c) 2008 Otto Moerbeek <otto@drijf.net> (mallocarray)
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
 * 3. Neither the name of the University nor the names of its contributors
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
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/vmmeter.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/vmem.h>
#ifdef EPOCH_TRACE
#include <sys/epoch.h>
#endif

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_domainset.h>
#include <vm/vm_pageout.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/vm_pagequeue.h>
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

bool	__read_frequently			dtrace_malloc_enabled;
dtrace_malloc_probe_func_t __read_mostly	dtrace_malloc_probe;
#endif

#if defined(INVARIANTS) || defined(MALLOC_MAKE_FAILURES) ||		\
    defined(DEBUG_MEMGUARD) || defined(DEBUG_REDZONE)
#define	MALLOC_DEBUG	1
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

static struct malloc_type *kmemstatistics;
static int kmemcount;

#define KMEM_ZSHIFT	4
#define KMEM_ZBASE	16
#define KMEM_ZMASK	(KMEM_ZBASE - 1)

#define KMEM_ZMAX	65536
#define KMEM_ZSIZE	(KMEM_ZMAX >> KMEM_ZSHIFT)
static uint8_t kmemsize[KMEM_ZSIZE + 1];

#ifndef MALLOC_DEBUG_MAXZONES
#define	MALLOC_DEBUG_MAXZONES	1
#endif
static int numzones = MALLOC_DEBUG_MAXZONES;

/*
 * Small malloc(9) memory allocations are allocated from a set of UMA buckets
 * of various sizes.
 *
 * Warning: the layout of the struct is duplicated in libmemstat for KVM support.
 *
 * XXX: The comment here used to read "These won't be powers of two for
 * long."  It's possible that a significant amount of wasted memory could be
 * recovered by tuning the sizes of these buckets.
 */
struct {
	int kz_size;
	const char *kz_name;
	uma_zone_t kz_zone[MALLOC_DEBUG_MAXZONES];
} kmemzones[] = {
	{16, "malloc-16", },
	{32, "malloc-32", },
	{64, "malloc-64", },
	{128, "malloc-128", },
	{256, "malloc-256", },
	{384, "malloc-384", },
	{512, "malloc-512", },
	{1024, "malloc-1024", },
	{2048, "malloc-2048", },
	{4096, "malloc-4096", },
	{8192, "malloc-8192", },
	{16384, "malloc-16384", },
	{32768, "malloc-32768", },
	{65536, "malloc-65536", },
	{0, NULL},
};

u_long vm_kmem_size;
SYSCTL_ULONG(_vm, OID_AUTO, kmem_size, CTLFLAG_RDTUN, &vm_kmem_size, 0,
    "Size of kernel memory");

static u_long kmem_zmax = KMEM_ZMAX;
SYSCTL_ULONG(_vm, OID_AUTO, kmem_zmax, CTLFLAG_RDTUN, &kmem_zmax, 0,
    "Maximum allocation size that malloc(9) would use UMA as backend");

static u_long vm_kmem_size_min;
SYSCTL_ULONG(_vm, OID_AUTO, kmem_size_min, CTLFLAG_RDTUN, &vm_kmem_size_min, 0,
    "Minimum size of kernel memory");

static u_long vm_kmem_size_max;
SYSCTL_ULONG(_vm, OID_AUTO, kmem_size_max, CTLFLAG_RDTUN, &vm_kmem_size_max, 0,
    "Maximum size of kernel memory");

static u_int vm_kmem_size_scale;
SYSCTL_UINT(_vm, OID_AUTO, kmem_size_scale, CTLFLAG_RDTUN, &vm_kmem_size_scale, 0,
    "Scale factor for kernel memory size");

static int sysctl_kmem_map_size(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_vm, OID_AUTO, kmem_map_size,
    CTLFLAG_RD | CTLTYPE_ULONG | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_kmem_map_size, "LU", "Current kmem allocation size");

static int sysctl_kmem_map_free(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_vm, OID_AUTO, kmem_map_free,
    CTLFLAG_RD | CTLTYPE_ULONG | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_kmem_map_free, "LU", "Free space in kmem");

static SYSCTL_NODE(_vm, OID_AUTO, malloc, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Malloc information");

static u_int vm_malloc_zone_count = nitems(kmemzones);
SYSCTL_UINT(_vm_malloc, OID_AUTO, zone_count,
    CTLFLAG_RD, &vm_malloc_zone_count, 0,
    "Number of malloc zones");

static int sysctl_vm_malloc_zone_sizes(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_vm_malloc, OID_AUTO, zone_sizes,
    CTLFLAG_RD | CTLTYPE_OPAQUE | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_vm_malloc_zone_sizes, "S", "Zone sizes used by malloc");

/*
 * The malloc_mtx protects the kmemstatistics linked list.
 */
struct mtx malloc_mtx;

static int sysctl_kern_malloc_stats(SYSCTL_HANDLER_ARGS);

#if defined(MALLOC_MAKE_FAILURES) || (MALLOC_DEBUG_MAXZONES > 1)
static SYSCTL_NODE(_debug, OID_AUTO, malloc, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Kernel malloc debugging options");
#endif

/*
 * malloc(9) fault injection -- cause malloc failures every (n) mallocs when
 * the caller specifies M_NOWAIT.  If set to 0, no failures are caused.
 */
#ifdef MALLOC_MAKE_FAILURES
static int malloc_failure_rate;
static int malloc_nowait_count;
static int malloc_failure_count;
SYSCTL_INT(_debug_malloc, OID_AUTO, failure_rate, CTLFLAG_RWTUN,
    &malloc_failure_rate, 0, "Every (n) mallocs with M_NOWAIT will fail");
SYSCTL_INT(_debug_malloc, OID_AUTO, failure_count, CTLFLAG_RD,
    &malloc_failure_count, 0, "Number of imposed M_NOWAIT malloc failures");
#endif

static int
sysctl_kmem_map_size(SYSCTL_HANDLER_ARGS)
{
	u_long size;

	size = uma_size();
	return (sysctl_handle_long(oidp, &size, 0, req));
}

static int
sysctl_kmem_map_free(SYSCTL_HANDLER_ARGS)
{
	u_long size, limit;

	/* The sysctl is unsigned, implement as a saturation value. */
	size = uma_size();
	limit = uma_limit();
	if (size > limit)
		size = 0;
	else
		size = limit - size;
	return (sysctl_handle_long(oidp, &size, 0, req));
}

static int
sysctl_vm_malloc_zone_sizes(SYSCTL_HANDLER_ARGS)
{
	int sizes[nitems(kmemzones)];
	int i;

	for (i = 0; i < nitems(kmemzones); i++) {
		sizes[i] = kmemzones[i].kz_size;
	}

	return (SYSCTL_OUT(req, &sizes, sizeof(sizes)));
}

/*
 * malloc(9) uma zone separation -- sub-page buffer overruns in one
 * malloc type will affect only a subset of other malloc types.
 */
#if MALLOC_DEBUG_MAXZONES > 1
static void
tunable_set_numzones(void)
{

	TUNABLE_INT_FETCH("debug.malloc.numzones",
	    &numzones);

	/* Sanity check the number of malloc uma zones. */
	if (numzones <= 0)
		numzones = 1;
	if (numzones > MALLOC_DEBUG_MAXZONES)
		numzones = MALLOC_DEBUG_MAXZONES;
}
SYSINIT(numzones, SI_SUB_TUNABLES, SI_ORDER_ANY, tunable_set_numzones, NULL);
SYSCTL_INT(_debug_malloc, OID_AUTO, numzones, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &numzones, 0, "Number of malloc uma subzones");

/*
 * Any number that changes regularly is an okay choice for the
 * offset.  Build numbers are pretty good of you have them.
 */
static u_int zone_offset = __FreeBSD_version;
TUNABLE_INT("debug.malloc.zone_offset", &zone_offset);
SYSCTL_UINT(_debug_malloc, OID_AUTO, zone_offset, CTLFLAG_RDTUN,
    &zone_offset, 0, "Separate malloc types by examining the "
    "Nth character in the malloc type short description.");

static void
mtp_set_subzone(struct malloc_type *mtp)
{
	struct malloc_type_internal *mtip;
	const char *desc;
	size_t len;
	u_int val;

	mtip = &mtp->ks_mti;
	desc = mtp->ks_shortdesc;
	if (desc == NULL || (len = strlen(desc)) == 0)
		val = 0;
	else
		val = desc[zone_offset % len];
	mtip->mti_zone = (val % numzones);
}

static inline u_int
mtp_get_subzone(struct malloc_type *mtp)
{
	struct malloc_type_internal *mtip;

	mtip = &mtp->ks_mti;

	KASSERT(mtip->mti_zone < numzones,
	    ("mti_zone %u out of range %d",
	    mtip->mti_zone, numzones));
	return (mtip->mti_zone);
}
#elif MALLOC_DEBUG_MAXZONES == 0
#error "MALLOC_DEBUG_MAXZONES must be positive."
#else
static void
mtp_set_subzone(struct malloc_type *mtp)
{
	struct malloc_type_internal *mtip;

	mtip = &mtp->ks_mti;
	mtip->mti_zone = 0;
}

static inline u_int
mtp_get_subzone(struct malloc_type *mtp)
{

	return (0);
}
#endif /* MALLOC_DEBUG_MAXZONES > 1 */

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
	mtip = &mtp->ks_mti;
	mtsp = zpcpu_get(mtip->mti_stats);
	if (size > 0) {
		mtsp->mts_memalloced += size;
		mtsp->mts_numallocs++;
	}
	if (zindx != -1)
		mtsp->mts_size |= 1 << zindx;

#ifdef KDTRACE_HOOKS
	if (__predict_false(dtrace_malloc_enabled)) {
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
	mtip = &mtp->ks_mti;
	mtsp = zpcpu_get(mtip->mti_stats);
	mtsp->mts_memfreed += size;
	mtsp->mts_numfrees++;

#ifdef KDTRACE_HOOKS
	if (__predict_false(dtrace_malloc_enabled)) {
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
 *	contigmalloc:
 *
 *	Allocate a block of physically contiguous memory.
 *
 *	If M_NOWAIT is set, this routine will not block and return NULL if
 *	the allocation fails.
 */
void *
contigmalloc(unsigned long size, struct malloc_type *type, int flags,
    vm_paddr_t low, vm_paddr_t high, unsigned long alignment,
    vm_paddr_t boundary)
{
	void *ret;

	ret = (void *)kmem_alloc_contig(size, flags, low, high, alignment,
	    boundary, VM_MEMATTR_DEFAULT);
	if (ret != NULL)
		malloc_type_allocated(type, round_page(size));
	return (ret);
}

void *
contigmalloc_domainset(unsigned long size, struct malloc_type *type,
    struct domainset *ds, int flags, vm_paddr_t low, vm_paddr_t high,
    unsigned long alignment, vm_paddr_t boundary)
{
	void *ret;

	ret = (void *)kmem_alloc_contig_domainset(ds, size, flags, low, high,
	    alignment, boundary, VM_MEMATTR_DEFAULT);
	if (ret != NULL)
		malloc_type_allocated(type, round_page(size));
	return (ret);
}

/*
 *	contigfree:
 *
 *	Free a block of memory allocated by contigmalloc.
 *
 *	This routine may not block.
 */
void
contigfree(void *addr, unsigned long size, struct malloc_type *type)
{

	kmem_free((vm_offset_t)addr, size);
	malloc_type_freed(type, round_page(size));
}

#ifdef MALLOC_DEBUG
static int
malloc_dbg(caddr_t *vap, size_t *sizep, struct malloc_type *mtp,
    int flags)
{
#ifdef INVARIANTS
	int indx;

	KASSERT(mtp->ks_version == M_VERSION, ("malloc: bad malloc type version"));
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
			*vap = NULL;
			return (EJUSTRETURN);
		}
	}
#endif
	if (flags & M_WAITOK) {
		KASSERT(curthread->td_intr_nesting_level == 0,
		   ("malloc(M_WAITOK) in interrupt context"));
		if (__predict_false(!THREAD_CAN_SLEEP())) {
#ifdef EPOCH_TRACE
			epoch_trace_list(curthread);
#endif
			KASSERT(1, 
			    ("malloc(M_WAITOK) with sleeping prohibited"));
		}
	}
	KASSERT(curthread->td_critnest == 0 || SCHEDULER_STOPPED(),
	    ("malloc: called with spinlock or critical section held"));

#ifdef DEBUG_MEMGUARD
	if (memguard_cmp_mtp(mtp, *sizep)) {
		*vap = memguard_alloc(*sizep, flags);
		if (*vap != NULL)
			return (EJUSTRETURN);
		/* This is unfortunate but should not be fatal. */
	}
#endif

#ifdef DEBUG_REDZONE
	*sizep = redzone_size_ntor(*sizep);
#endif

	return (0);
}
#endif

/*
 * Handle large allocations and frees by using kmem_malloc directly.
 */
static inline bool
malloc_large_slab(uma_slab_t slab)
{
	uintptr_t va;

	va = (uintptr_t)slab;
	return ((va & 1) != 0);
}

static inline size_t
malloc_large_size(uma_slab_t slab)
{
	uintptr_t va;

	va = (uintptr_t)slab;
	return (va >> 1);
}

static caddr_t
malloc_large(size_t *size, struct domainset *policy, int flags)
{
	vm_offset_t va;
	size_t sz;

	sz = roundup(*size, PAGE_SIZE);
	va = kmem_malloc_domainset(policy, sz, flags);
	if (va != 0) {
		/* The low bit is unused for slab pointers. */
		vsetzoneslab(va, NULL, (void *)((sz << 1) | 1));
		uma_total_inc(sz);
		*size = sz;
	}
	return ((caddr_t)va);
}

static void
free_large(void *addr, size_t size)
{

	kmem_free((vm_offset_t)addr, size);
	uma_total_dec(size);
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
(malloc)(size_t size, struct malloc_type *mtp, int flags)
{
	int indx;
	caddr_t va;
	uma_zone_t zone;
#if defined(DEBUG_REDZONE)
	unsigned long osize = size;
#endif

	MPASS((flags & M_EXEC) == 0);
#ifdef MALLOC_DEBUG
	va = NULL;
	if (malloc_dbg(&va, &size, mtp, flags) != 0)
		return (va);
#endif

	if (size <= kmem_zmax) {
		if (size & KMEM_ZMASK)
			size = (size & ~KMEM_ZMASK) + KMEM_ZBASE;
		indx = kmemsize[size >> KMEM_ZSHIFT];
		zone = kmemzones[indx].kz_zone[mtp_get_subzone(mtp)];
		va = uma_zalloc(zone, flags);
		if (va != NULL)
			size = zone->uz_size;
		malloc_type_zone_allocated(mtp, va == NULL ? 0 : size, indx);
	} else {
		va = malloc_large(&size, DOMAINSET_RR(), flags);
		malloc_type_allocated(mtp, va == NULL ? 0 : size);
	}
	if (__predict_false(va == NULL)) {
		KASSERT((flags & M_WAITOK) == 0,
		    ("malloc(M_WAITOK) returned NULL"));
	}
#ifdef DEBUG_REDZONE
	if (va != NULL)
		va = redzone_setup(va, osize);
#endif
	return ((void *) va);
}

static void *
malloc_domain(size_t *sizep, int *indxp, struct malloc_type *mtp, int domain,
    int flags)
{
	uma_zone_t zone;
	caddr_t va;
	size_t size;
	int indx;

	size = *sizep;
	KASSERT(size <= kmem_zmax && (flags & M_EXEC) == 0,
	    ("malloc_domain: Called with bad flag / size combination."));
	if (size & KMEM_ZMASK)
		size = (size & ~KMEM_ZMASK) + KMEM_ZBASE;
	indx = kmemsize[size >> KMEM_ZSHIFT];
	zone = kmemzones[indx].kz_zone[mtp_get_subzone(mtp)];
	va = uma_zalloc_domain(zone, NULL, domain, flags);
	if (va != NULL)
		*sizep = zone->uz_size;
	*indxp = indx;
	return ((void *)va);
}

void *
malloc_domainset(size_t size, struct malloc_type *mtp, struct domainset *ds,
    int flags)
{
	struct vm_domainset_iter di;
	caddr_t va;
	int domain;
	int indx;

#if defined(DEBUG_REDZONE)
	unsigned long osize = size;
#endif
	MPASS((flags & M_EXEC) == 0);
#ifdef MALLOC_DEBUG
	va = NULL;
	if (malloc_dbg(&va, &size, mtp, flags) != 0)
		return (va);
#endif
	if (size <= kmem_zmax) {
		vm_domainset_iter_policy_init(&di, ds, &domain, &flags);
		do {
			va = malloc_domain(&size, &indx, mtp, domain, flags);
		} while (va == NULL &&
		    vm_domainset_iter_policy(&di, &domain) == 0);
		malloc_type_zone_allocated(mtp, va == NULL ? 0 : size, indx);
	} else {
		/* Policy is handled by kmem. */
		va = malloc_large(&size, ds, flags);
		malloc_type_allocated(mtp, va == NULL ? 0 : size);
	}
	if (__predict_false(va == NULL)) {
		KASSERT((flags & M_WAITOK) == 0,
		    ("malloc(M_WAITOK) returned NULL"));
	}
#ifdef DEBUG_REDZONE
	if (va != NULL)
		va = redzone_setup(va, osize);
#endif
	return (va);
}

/*
 * Allocate an executable area.
 */
void *
malloc_exec(size_t size, struct malloc_type *mtp, int flags)
{
	caddr_t va;
#if defined(DEBUG_REDZONE)
	unsigned long osize = size;
#endif

	flags |= M_EXEC;
#ifdef MALLOC_DEBUG
	va = NULL;
	if (malloc_dbg(&va, &size, mtp, flags) != 0)
		return (va);
#endif
	va = malloc_large(&size, DOMAINSET_RR(), flags);
	malloc_type_allocated(mtp, va == NULL ? 0 : size);
	if (__predict_false(va == NULL)) {
		KASSERT((flags & M_WAITOK) == 0,
		    ("malloc(M_WAITOK) returned NULL"));
	}
#ifdef DEBUG_REDZONE
	if (va != NULL)
		va = redzone_setup(va, osize);
#endif
	return ((void *) va);
}

void *
malloc_domainset_exec(size_t size, struct malloc_type *mtp, struct domainset *ds,
    int flags)
{
	caddr_t va;
#if defined(DEBUG_REDZONE)
	unsigned long osize = size;
#endif

	flags |= M_EXEC;
#ifdef MALLOC_DEBUG
	va = NULL;
	if (malloc_dbg(&va, &size, mtp, flags) != 0)
		return (va);
#endif
	/* Policy is handled by kmem. */
	va = malloc_large(&size, ds, flags);
	malloc_type_allocated(mtp, va == NULL ? 0 : size);
	if (__predict_false(va == NULL)) {
		KASSERT((flags & M_WAITOK) == 0,
		    ("malloc(M_WAITOK) returned NULL"));
	}
#ifdef DEBUG_REDZONE
	if (va != NULL)
		va = redzone_setup(va, osize);
#endif
	return (va);
}

void *
mallocarray(size_t nmemb, size_t size, struct malloc_type *type, int flags)
{

	if (WOULD_OVERFLOW(nmemb, size))
		panic("mallocarray: %zu * %zu overflowed", nmemb, size);

	return (malloc(size * nmemb, type, flags));
}

#ifdef INVARIANTS
static void
free_save_type(void *addr, struct malloc_type *mtp, u_long size)
{
	struct malloc_type **mtpp = addr;

	/*
	 * Cache a pointer to the malloc_type that most recently freed
	 * this memory here.  This way we know who is most likely to
	 * have stepped on it later.
	 *
	 * This code assumes that size is a multiple of 8 bytes for
	 * 64 bit machines
	 */
	mtpp = (struct malloc_type **) ((unsigned long)mtpp & ~UMA_ALIGN_PTR);
	mtpp += (size - sizeof(struct malloc_type *)) /
	    sizeof(struct malloc_type *);
	*mtpp = mtp;
}
#endif

#ifdef MALLOC_DEBUG
static int
free_dbg(void **addrp, struct malloc_type *mtp)
{
	void *addr;

	addr = *addrp;
	KASSERT(mtp->ks_version == M_VERSION, ("free: bad malloc type version"));
	KASSERT(curthread->td_critnest == 0 || SCHEDULER_STOPPED(),
	    ("free: called with spinlock or critical section held"));

	/* free(NULL, ...) does nothing */
	if (addr == NULL)
		return (EJUSTRETURN);

#ifdef DEBUG_MEMGUARD
	if (is_memguard_addr(addr)) {
		memguard_free(addr);
		return (EJUSTRETURN);
	}
#endif

#ifdef DEBUG_REDZONE
	redzone_check(addr);
	*addrp = redzone_addr_ntor(addr);
#endif

	return (0);
}
#endif

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
	uma_zone_t zone;
	uma_slab_t slab;
	u_long size;

#ifdef MALLOC_DEBUG
	if (free_dbg(&addr, mtp) != 0)
		return;
#endif
	/* free(NULL, ...) does nothing */
	if (addr == NULL)
		return;

	vtozoneslab((vm_offset_t)addr & (~UMA_SLAB_MASK), &zone, &slab);
	if (slab == NULL)
		panic("free: address %p(%p) has not been allocated.\n",
		    addr, (void *)((u_long)addr & (~UMA_SLAB_MASK)));

	if (__predict_true(!malloc_large_slab(slab))) {
		size = zone->uz_size;
#ifdef INVARIANTS
		free_save_type(addr, mtp, size);
#endif
		uma_zfree_arg(zone, addr, slab);
	} else {
		size = malloc_large_size(slab);
		free_large(addr, size);
	}
	malloc_type_freed(mtp, size);
}

/*
 *	zfree:
 *
 *	Zero then free a block of memory allocated by malloc.
 *
 *	This routine may not block.
 */
void
zfree(void *addr, struct malloc_type *mtp)
{
	uma_zone_t zone;
	uma_slab_t slab;
	u_long size;

#ifdef MALLOC_DEBUG
	if (free_dbg(&addr, mtp) != 0)
		return;
#endif
	/* free(NULL, ...) does nothing */
	if (addr == NULL)
		return;

	vtozoneslab((vm_offset_t)addr & (~UMA_SLAB_MASK), &zone, &slab);
	if (slab == NULL)
		panic("free: address %p(%p) has not been allocated.\n",
		    addr, (void *)((u_long)addr & (~UMA_SLAB_MASK)));

	if (__predict_true(!malloc_large_slab(slab))) {
		size = zone->uz_size;
#ifdef INVARIANTS
		free_save_type(addr, mtp, size);
#endif
		explicit_bzero(addr, size);
		uma_zfree_arg(zone, addr, slab);
	} else {
		size = malloc_large_size(slab);
		explicit_bzero(addr, size);
		free_large(addr, size);
	}
	malloc_type_freed(mtp, size);
}

/*
 *	realloc: change the size of a memory block
 */
void *
realloc(void *addr, size_t size, struct malloc_type *mtp, int flags)
{
	uma_zone_t zone;
	uma_slab_t slab;
	unsigned long alloc;
	void *newaddr;

	KASSERT(mtp->ks_version == M_VERSION,
	    ("realloc: bad malloc type version"));
	KASSERT(curthread->td_critnest == 0 || SCHEDULER_STOPPED(),
	    ("realloc: called with spinlock or critical section held"));

	/* realloc(NULL, ...) is equivalent to malloc(...) */
	if (addr == NULL)
		return (malloc(size, mtp, flags));

	/*
	 * XXX: Should report free of old memory and alloc of new memory to
	 * per-CPU stats.
	 */

#ifdef DEBUG_MEMGUARD
	if (is_memguard_addr(addr))
		return (memguard_realloc(addr, size, mtp, flags));
#endif

#ifdef DEBUG_REDZONE
	slab = NULL;
	zone = NULL;
	alloc = redzone_get_size(addr);
#else
	vtozoneslab((vm_offset_t)addr & (~UMA_SLAB_MASK), &zone, &slab);

	/* Sanity check */
	KASSERT(slab != NULL,
	    ("realloc: address %p out of range", (void *)addr));

	/* Get the size of the original block */
	if (!malloc_large_slab(slab))
		alloc = zone->uz_size;
	else
		alloc = malloc_large_size(slab);

	/* Reuse the original block if appropriate */
	if (size <= alloc
	    && (size > (alloc >> REALLOC_FRACTION) || alloc == MINALLOCSIZE))
		return (addr);
#endif /* !DEBUG_REDZONE */

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
reallocf(void *addr, size_t size, struct malloc_type *mtp, int flags)
{
	void *mem;

	if ((mem = realloc(addr, size, mtp, flags)) == NULL)
		free(addr, mtp);
	return (mem);
}

/*
 * 	malloc_size: returns the number of bytes allocated for a request of the
 * 		     specified size
 */
size_t
malloc_size(size_t size)
{
	int indx;

	if (size > kmem_zmax)
		return (0);
	if (size & KMEM_ZMASK)
		size = (size & ~KMEM_ZMASK) + KMEM_ZBASE;
	indx = kmemsize[size >> KMEM_ZSHIFT];
	return (kmemzones[indx].kz_size);
}

/*
 *	malloc_usable_size: returns the usable size of the allocation.
 */
size_t
malloc_usable_size(const void *addr)
{
#ifndef DEBUG_REDZONE
	uma_zone_t zone;
	uma_slab_t slab;
#endif
	u_long size;

	if (addr == NULL)
		return (0);

#ifdef DEBUG_MEMGUARD
	if (is_memguard_addr(__DECONST(void *, addr)))
		return (memguard_get_req_size(addr));
#endif

#ifdef DEBUG_REDZONE
	size = redzone_get_size(__DECONST(void *, addr));
#else
	vtozoneslab((vm_offset_t)addr & (~UMA_SLAB_MASK), &zone, &slab);
	if (slab == NULL)
		panic("malloc_usable_size: address %p(%p) is not allocated.\n",
		    addr, (void *)((u_long)addr & (~UMA_SLAB_MASK)));

	if (!malloc_large_slab(slab))
		size = zone->uz_size;
	else
		size = malloc_large_size(slab);
#endif
	return (size);
}

CTASSERT(VM_KMEM_SIZE_SCALE >= 1);

/*
 * Initialize the kernel memory (kmem) arena.
 */
void
kmeminit(void)
{
	u_long mem_size;
	u_long tmp;

#ifdef VM_KMEM_SIZE
	if (vm_kmem_size == 0)
		vm_kmem_size = VM_KMEM_SIZE;
#endif
#ifdef VM_KMEM_SIZE_MIN
	if (vm_kmem_size_min == 0)
		vm_kmem_size_min = VM_KMEM_SIZE_MIN;
#endif
#ifdef VM_KMEM_SIZE_MAX
	if (vm_kmem_size_max == 0)
		vm_kmem_size_max = VM_KMEM_SIZE_MAX;
#endif
	/*
	 * Calculate the amount of kernel virtual address (KVA) space that is
	 * preallocated to the kmem arena.  In order to support a wide range
	 * of machines, it is a function of the physical memory size,
	 * specifically,
	 *
	 *	min(max(physical memory size / VM_KMEM_SIZE_SCALE,
	 *	    VM_KMEM_SIZE_MIN), VM_KMEM_SIZE_MAX)
	 *
	 * Every architecture must define an integral value for
	 * VM_KMEM_SIZE_SCALE.  However, the definitions of VM_KMEM_SIZE_MIN
	 * and VM_KMEM_SIZE_MAX, which represent respectively the floor and
	 * ceiling on this preallocation, are optional.  Typically,
	 * VM_KMEM_SIZE_MAX is itself a function of the available KVA space on
	 * a given architecture.
	 */
	mem_size = vm_cnt.v_page_count;
	if (mem_size <= 32768) /* delphij XXX 128MB */
		kmem_zmax = PAGE_SIZE;

	if (vm_kmem_size_scale < 1)
		vm_kmem_size_scale = VM_KMEM_SIZE_SCALE;

	/*
	 * Check if we should use defaults for the "vm_kmem_size"
	 * variable:
	 */
	if (vm_kmem_size == 0) {
		vm_kmem_size = mem_size / vm_kmem_size_scale;
		vm_kmem_size = vm_kmem_size * PAGE_SIZE < vm_kmem_size ?
		    vm_kmem_size_max : vm_kmem_size * PAGE_SIZE;
		if (vm_kmem_size_min > 0 && vm_kmem_size < vm_kmem_size_min)
			vm_kmem_size = vm_kmem_size_min;
		if (vm_kmem_size_max > 0 && vm_kmem_size >= vm_kmem_size_max)
			vm_kmem_size = vm_kmem_size_max;
	}
	if (vm_kmem_size == 0)
		panic("Tune VM_KMEM_SIZE_* for the platform");

	/*
	 * The amount of KVA space that is preallocated to the
	 * kmem arena can be set statically at compile-time or manually
	 * through the kernel environment.  However, it is still limited to
	 * twice the physical memory size, which has been sufficient to handle
	 * the most severe cases of external fragmentation in the kmem arena. 
	 */
	if (vm_kmem_size / 2 / PAGE_SIZE > mem_size)
		vm_kmem_size = 2 * mem_size * PAGE_SIZE;

	vm_kmem_size = round_page(vm_kmem_size);
#ifdef DEBUG_MEMGUARD
	tmp = memguard_fudge(vm_kmem_size, kernel_map);
#else
	tmp = vm_kmem_size;
#endif
	uma_set_limit(tmp);

#ifdef DEBUG_MEMGUARD
	/*
	 * Initialize MemGuard if support compiled in.  MemGuard is a
	 * replacement allocator used for detecting tamper-after-free
	 * scenarios as they occur.  It is only used for debugging.
	 */
	memguard_init(kernel_arena);
#endif
}

/*
 * Initialize the kernel memory allocator
 */
/* ARGSUSED*/
static void
mallocinit(void *dummy)
{
	int i;
	uint8_t indx;

	mtx_init(&malloc_mtx, "malloc", NULL, MTX_DEF);

	kmeminit();

	if (kmem_zmax < PAGE_SIZE || kmem_zmax > KMEM_ZMAX)
		kmem_zmax = KMEM_ZMAX;

	for (i = 0, indx = 0; kmemzones[indx].kz_size != 0; indx++) {
		int size = kmemzones[indx].kz_size;
		const char *name = kmemzones[indx].kz_name;
		int subzone;

		for (subzone = 0; subzone < numzones; subzone++) {
			kmemzones[indx].kz_zone[subzone] =
			    uma_zcreate(name, size,
#ifdef INVARIANTS
			    mtrash_ctor, mtrash_dtor, mtrash_init, mtrash_fini,
#else
			    NULL, NULL, NULL, NULL,
#endif
			    UMA_ALIGN_PTR, UMA_ZONE_MALLOC);
		}
		for (;i <= size; i+= KMEM_ZBASE)
			kmemsize[i >> KMEM_ZSHIFT] = indx;
	}
}
SYSINIT(kmem, SI_SUB_KMEM, SI_ORDER_SECOND, mallocinit, NULL);

void
malloc_init(void *data)
{
	struct malloc_type_internal *mtip;
	struct malloc_type *mtp;

	KASSERT(vm_cnt.v_page_count != 0, ("malloc_register before vm_init"));

	mtp = data;
	if (mtp->ks_version != M_VERSION)
		panic("malloc_init: type %s with unsupported version %lu",
		    mtp->ks_shortdesc, mtp->ks_version);

	mtip = &mtp->ks_mti;
	mtip->mti_stats = uma_zalloc_pcpu(pcpu_zone_64, M_WAITOK | M_ZERO);
	mtp_set_subzone(mtp);

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
	long temp_allocs, temp_bytes;
	int i;

	mtp = data;
	KASSERT(mtp->ks_version == M_VERSION,
	    ("malloc_uninit: bad malloc type version"));

	mtx_lock(&malloc_mtx);
	mtip = &mtp->ks_mti;
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
	for (i = 0; i <= mp_maxid; i++) {
		mtsp = zpcpu_get_cpu(mtip->mti_stats, i);
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

	uma_zfree_pcpu(pcpu_zone_64, mtip->mti_stats);
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
	struct malloc_type_stats *mtsp, zeromts;
	struct malloc_type_header mth;
	struct malloc_type *mtp;
	int error, i;
	struct sbuf sbuf;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
	sbuf_clear_flags(&sbuf, SBUF_INCLUDENUL);
	mtx_lock(&malloc_mtx);

	bzero(&zeromts, sizeof(zeromts));

	/*
	 * Insert stream header.
	 */
	bzero(&mtsh, sizeof(mtsh));
	mtsh.mtsh_version = MALLOC_TYPE_STREAM_VERSION;
	mtsh.mtsh_maxcpus = MAXCPU;
	mtsh.mtsh_count = kmemcount;
	(void)sbuf_bcat(&sbuf, &mtsh, sizeof(mtsh));

	/*
	 * Insert alternating sequence of type headers and type statistics.
	 */
	for (mtp = kmemstatistics; mtp != NULL; mtp = mtp->ks_next) {
		mtip = &mtp->ks_mti;

		/*
		 * Insert type header.
		 */
		bzero(&mth, sizeof(mth));
		strlcpy(mth.mth_name, mtp->ks_shortdesc, MALLOC_MAX_NAME);
		(void)sbuf_bcat(&sbuf, &mth, sizeof(mth));

		/*
		 * Insert type statistics for each CPU.
		 */
		for (i = 0; i <= mp_maxid; i++) {
			mtsp = zpcpu_get_cpu(mtip->mti_stats, i);
			(void)sbuf_bcat(&sbuf, mtsp, sizeof(*mtsp));
		}
		/*
		 * Fill in the missing CPUs.
		 */
		for (; i < MAXCPU; i++) {
			(void)sbuf_bcat(&sbuf, &zeromts, sizeof(zeromts));
		}
	}
	mtx_unlock(&malloc_mtx);
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, malloc_stats,
    CTLFLAG_RD | CTLTYPE_STRUCT | CTLFLAG_MPSAFE, 0, 0,
    sysctl_kern_malloc_stats, "s,malloc_type_ustats",
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
static int64_t
get_malloc_stats(const struct malloc_type_internal *mtip, uint64_t *allocs,
    uint64_t *inuse)
{
	const struct malloc_type_stats *mtsp;
	uint64_t frees, alloced, freed;
	int i;

	*allocs = 0;
	frees = 0;
	alloced = 0;
	freed = 0;
	for (i = 0; i <= mp_maxid; i++) {
		mtsp = zpcpu_get_cpu(mtip->mti_stats, i);

		*allocs += mtsp->mts_numallocs;
		frees += mtsp->mts_numfrees;
		alloced += mtsp->mts_memalloced;
		freed += mtsp->mts_memfreed;
	}
	*inuse = *allocs - frees;
	return (alloced - freed);
}

DB_SHOW_COMMAND(malloc, db_show_malloc)
{
	const char *fmt_hdr, *fmt_entry;
	struct malloc_type *mtp;
	uint64_t allocs, inuse;
	int64_t size;
	/* variables for sorting */
	struct malloc_type *last_mtype, *cur_mtype;
	int64_t cur_size, last_size;
	int ties;

	if (modif[0] == 'i') {
		fmt_hdr = "%s,%s,%s,%s\n";
		fmt_entry = "\"%s\",%ju,%jdK,%ju\n";
	} else {
		fmt_hdr = "%18s %12s  %12s %12s\n";
		fmt_entry = "%18s %12ju %12jdK %12ju\n";
	}

	db_printf(fmt_hdr, "Type", "InUse", "MemUse", "Requests");

	/* Select sort, largest size first. */
	last_mtype = NULL;
	last_size = INT64_MAX;
	for (;;) {
		cur_mtype = NULL;
		cur_size = -1;
		ties = 0;

		for (mtp = kmemstatistics; mtp != NULL; mtp = mtp->ks_next) {
			/*
			 * In the case of size ties, print out mtypes
			 * in the order they are encountered.  That is,
			 * when we encounter the most recently output
			 * mtype, we have already printed all preceding
			 * ties, and we must print all following ties.
			 */
			if (mtp == last_mtype) {
				ties = 1;
				continue;
			}
			size = get_malloc_stats(&mtp->ks_mti, &allocs,
			    &inuse);
			if (size > cur_size && size < last_size + ties) {
				cur_size = size;
				cur_mtype = mtp;
			}
		}
		if (cur_mtype == NULL)
			break;

		size = get_malloc_stats(&cur_mtype->ks_mti, &allocs, &inuse);
		db_printf(fmt_entry, cur_mtype->ks_shortdesc, inuse,
		    howmany(size, 1024), allocs);

		if (db_pager_quit)
			break;

		last_mtype = cur_mtype;
		last_size = cur_size;
	}
}

#if MALLOC_DEBUG_MAXZONES > 1
DB_SHOW_COMMAND(multizone_matches, db_show_multizone_matches)
{
	struct malloc_type_internal *mtip;
	struct malloc_type *mtp;
	u_int subzone;

	if (!have_addr) {
		db_printf("Usage: show multizone_matches <malloc type/addr>\n");
		return;
	}
	mtp = (void *)addr;
	if (mtp->ks_version != M_VERSION) {
		db_printf("Version %lx does not match expected %x\n",
		    mtp->ks_version, M_VERSION);
		return;
	}

	mtip = &mtp->ks_mti;
	subzone = mtip->mti_zone;

	for (mtp = kmemstatistics; mtp != NULL; mtp = mtp->ks_next) {
		mtip = &mtp->ks_mti;
		if (mtip->mti_zone != subzone)
			continue;
		db_printf("%s\n", mtp->ks_shortdesc);
		if (db_pager_quit)
			break;
	}
}
#endif /* MALLOC_DEBUG_MAXZONES > 1 */
#endif /* DDB */
