/*
 * Copyright (c) 2005,
 *     Bosko Milekic <bmilekic@FreeBSD.org>.  All rights reserved.
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
__FBSDID("$FreeBSD$");

/*
 * MemGuard is a simple replacement allocator for debugging only
 * which provides ElectricFence-style memory barrier protection on
 * objects being allocated, and is used to detect tampering-after-free
 * scenarios.
 *
 * See the memguard(9) man page for more information on using MemGuard.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/memguard.h>

/*
 * The maximum number of pages allowed per allocation.  If you're using
 * MemGuard to override very large items (> MAX_PAGES_PER_ITEM in size),
 * you need to increase MAX_PAGES_PER_ITEM.
 */
#define	MAX_PAGES_PER_ITEM	64

SYSCTL_NODE(_vm, OID_AUTO, memguard, CTLFLAG_RW, NULL, "MemGuard data");
/*
 * The vm_memguard_divisor variable controls how much of kmem_map should be
 * reserved for MemGuard.
 */
u_int vm_memguard_divisor;
SYSCTL_UINT(_vm_memguard, OID_AUTO, divisor, CTLFLAG_RD, &vm_memguard_divisor,
    0, "(kmem_size/memguard_divisor) == memguard submap size");     

/*
 * Short description (ks_shortdesc) of memory type to monitor.
 */
static char vm_memguard_desc[128] = "";
static struct malloc_type *vm_memguard_mtype = NULL;
TUNABLE_STR("vm.memguard.desc", vm_memguard_desc, sizeof(vm_memguard_desc));
static int
memguard_sysctl_desc(SYSCTL_HANDLER_ARGS)
{
	struct malloc_type_internal *mtip;
	struct malloc_type_stats *mtsp;
	struct malloc_type *mtp;
	char desc[128];
	long bytes;
	int error, i;

	strlcpy(desc, vm_memguard_desc, sizeof(desc));
	error = sysctl_handle_string(oidp, desc, sizeof(desc), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	/*
	 * We can change memory type when no memory has been allocated for it
	 * or when there is no such memory type yet (ie. it will be loaded with
	 * kernel module).
	 */
	bytes = 0;
	mtx_lock(&malloc_mtx);
	mtp = malloc_desc2type(desc);
	if (mtp != NULL) {
		mtip = mtp->ks_handle;
		for (i = 0; i < MAXCPU; i++) {
			mtsp = &mtip->mti_stats[i];
			bytes += mtsp->mts_memalloced;
			bytes -= mtsp->mts_memfreed;
		}
	}
	if (bytes > 0)
		error = EBUSY;
	else {
		/*
		 * If mtp is NULL, it will be initialized in memguard_cmp().
		 */
		vm_memguard_mtype = mtp;
		strlcpy(vm_memguard_desc, desc, sizeof(vm_memguard_desc));
	}
	mtx_unlock(&malloc_mtx);
	return (error);
}
SYSCTL_PROC(_vm_memguard, OID_AUTO, desc, CTLTYPE_STRING | CTLFLAG_RW, 0, 0,
    memguard_sysctl_desc, "A", "Short description of memory type to monitor");

/*
 * Global MemGuard data.
 */
static vm_map_t memguard_map;
static unsigned long memguard_mapsize;
static unsigned long memguard_mapused;
struct memguard_entry {
	STAILQ_ENTRY(memguard_entry) entries;
	void *ptr;
};
static struct memguard_fifo {
	struct memguard_entry *stqh_first;
	struct memguard_entry **stqh_last;
	int index;
} memguard_fifo_pool[MAX_PAGES_PER_ITEM];

/*
 * Local prototypes.
 */
static void memguard_guard(void *addr, int numpgs);
static void memguard_unguard(void *addr, int numpgs);
static struct memguard_fifo *vtomgfifo(vm_offset_t va);
static void vsetmgfifo(vm_offset_t va, struct memguard_fifo *mgfifo);
static void vclrmgfifo(vm_offset_t va);

/*
 * Local macros.  MemGuard data is global, so replace these with whatever
 * your system uses to protect global data (if it is kernel-level
 * parallelized).  This is for porting among BSDs.
 */
#define	MEMGUARD_CRIT_SECTION_DECLARE	static struct mtx memguard_mtx
#define	MEMGUARD_CRIT_SECTION_INIT				\
	mtx_init(&memguard_mtx, "MemGuard mtx", NULL, MTX_DEF)
#define	MEMGUARD_CRIT_SECTION_ENTER	mtx_lock(&memguard_mtx)
#define	MEMGUARD_CRIT_SECTION_EXIT	mtx_unlock(&memguard_mtx)
MEMGUARD_CRIT_SECTION_DECLARE;

/*
 * Initialize the MemGuard mock allocator.  All objects from MemGuard come
 * out of a single VM map (contiguous chunk of address space).
 */
void
memguard_init(vm_map_t parent_map, unsigned long size)
{
	char *base, *limit;
	int i;

	/* size must be multiple of PAGE_SIZE */
	size /= PAGE_SIZE;
	size++;
	size *= PAGE_SIZE;

	memguard_map = kmem_suballoc(parent_map, (vm_offset_t *)&base,
	    (vm_offset_t *)&limit, (vm_size_t)size);
	memguard_map->system_map = 1;
	memguard_mapsize = size;
	memguard_mapused = 0;

	MEMGUARD_CRIT_SECTION_INIT;
	MEMGUARD_CRIT_SECTION_ENTER;
	for (i = 0; i < MAX_PAGES_PER_ITEM; i++) {
		STAILQ_INIT(&memguard_fifo_pool[i]);
		memguard_fifo_pool[i].index = i;
	}
	MEMGUARD_CRIT_SECTION_EXIT;

	printf("MEMGUARD DEBUGGING ALLOCATOR INITIALIZED:\n");
	printf("\tMEMGUARD map base: %p\n", base);
	printf("\tMEMGUARD map limit: %p\n", limit);
	printf("\tMEMGUARD map size: %ld (Bytes)\n", size);
}

/*
 * Allocate a single object of specified size with specified flags (either
 * M_WAITOK or M_NOWAIT).
 */
void *
memguard_alloc(unsigned long size, int flags)
{
	void *obj;
	struct memguard_entry *e = NULL;
	int numpgs;

	numpgs = size / PAGE_SIZE;
	if ((size % PAGE_SIZE) != 0)
		numpgs++;
	if (numpgs > MAX_PAGES_PER_ITEM)
		panic("MEMGUARD: You must increase MAX_PAGES_PER_ITEM " \
		    "in memguard.c (requested: %d pages)", numpgs);
	if (numpgs == 0)
		return NULL;

	/*
	 * If we haven't exhausted the memguard_map yet, allocate from
	 * it and grab a new page, even if we have recycled pages in our
	 * FIFO.  This is because we wish to allow recycled pages to live
	 * guarded in the FIFO for as long as possible in order to catch
	 * even very late tamper-after-frees, even though it means that
	 * we end up wasting more memory, this is only a DEBUGGING allocator
	 * after all.
	 */
	MEMGUARD_CRIT_SECTION_ENTER;
	if (memguard_mapused >= memguard_mapsize) {
		e = STAILQ_FIRST(&memguard_fifo_pool[numpgs - 1]);
		if (e != NULL) {
			STAILQ_REMOVE(&memguard_fifo_pool[numpgs - 1], e,
			    memguard_entry, entries);
			MEMGUARD_CRIT_SECTION_EXIT;
			obj = e->ptr;
			free(e, M_TEMP);
			memguard_unguard(obj, numpgs);
			if (flags & M_ZERO)
				bzero(obj, PAGE_SIZE * numpgs);
			return obj;
		}
		MEMGUARD_CRIT_SECTION_EXIT;
		if (flags & M_WAITOK)
			panic("MEMGUARD: Failed with M_WAITOK: " \
			    "memguard_map too small");
		return NULL;
	}
	memguard_mapused += (PAGE_SIZE * numpgs);
	MEMGUARD_CRIT_SECTION_EXIT;

	obj = (void *)kmem_malloc(memguard_map, PAGE_SIZE * numpgs, flags);
	if (obj != NULL) {
		vsetmgfifo((vm_offset_t)obj, &memguard_fifo_pool[numpgs - 1]);
		if (flags & M_ZERO)
			bzero(obj, PAGE_SIZE * numpgs);
	} else {
		MEMGUARD_CRIT_SECTION_ENTER;
		memguard_mapused -= (PAGE_SIZE * numpgs);
		MEMGUARD_CRIT_SECTION_EXIT;
	}
	return obj;
}

/*
 * Free specified single object.
 */
void
memguard_free(void *addr)
{
	struct memguard_entry *e;
	struct memguard_fifo *mgfifo;
	int idx;
	int *temp;

	addr = (void *)trunc_page((unsigned long)addr);

	/*
	 * Page should not be guarded by now, so force a write.
	 * The purpose of this is to increase the likelihood of catching a
	 * double-free, but not necessarily a tamper-after-free (the second
	 * thread freeing might not write before freeing, so this forces it
	 * to and, subsequently, trigger a fault).
	 */
	temp = (int *)((unsigned long)addr + (PAGE_SIZE/2)); 	/* in page */
	*temp = 0xd34dc0d3;

	mgfifo = vtomgfifo((vm_offset_t)addr);
	idx = mgfifo->index;
	memguard_guard(addr, idx + 1);
	e = malloc(sizeof(struct memguard_entry), M_TEMP, M_NOWAIT);
	if (e == NULL) {
		MEMGUARD_CRIT_SECTION_ENTER;
		memguard_mapused -= (PAGE_SIZE * (idx + 1));
		MEMGUARD_CRIT_SECTION_EXIT;
		memguard_unguard(addr, idx + 1);	/* just in case */
		vclrmgfifo((vm_offset_t)addr);
		kmem_free(memguard_map, (vm_offset_t)addr,
		    PAGE_SIZE * (idx + 1));
		return;
	}
	e->ptr = addr;
	MEMGUARD_CRIT_SECTION_ENTER;
	STAILQ_INSERT_TAIL(mgfifo, e, entries);
	MEMGUARD_CRIT_SECTION_EXIT;
}

int
memguard_cmp(struct malloc_type *mtp)
{

#if 1
	/*
	 * The safest way of comparsion is to always compare short description
	 * string of memory type, but it is also the slowest way.
	 */
	return (strcmp(mtp->ks_shortdesc, vm_memguard_desc) == 0);
#else
	/*
	 * If we compare pointers, there are two possible problems:
	 * 1. Memory type was unloaded and new memory type was allocated at the
	 *    same address.
	 * 2. Memory type was unloaded and loaded again, but allocated at a
	 *    different address.
	 */
	if (vm_memguard_mtype != NULL)
		return (mtp == vm_memguard_mtype);
	if (strcmp(mtp->ks_shortdesc, vm_memguard_desc) == 0) {
		vm_memguard_mtype = mtp;
		return (1);
	}
	return (0);
#endif
}

/*
 * Guard a page containing specified object (make it read-only so that
 * future writes to it fail).
 */
static void
memguard_guard(void *addr, int numpgs)
{
	void *a = (void *)trunc_page((unsigned long)addr);
	if (vm_map_protect(memguard_map, (vm_offset_t)a,
	    (vm_offset_t)((unsigned long)a + (PAGE_SIZE * numpgs)),
	    VM_PROT_READ, FALSE) != KERN_SUCCESS)
		panic("MEMGUARD: Unable to guard page!");
}

/*
 * Unguard a page containing specified object (make it read-and-write to
 * allow full data access).
 */
static void
memguard_unguard(void *addr, int numpgs)
{
	void *a = (void *)trunc_page((unsigned long)addr);
	if (vm_map_protect(memguard_map, (vm_offset_t)a,
	    (vm_offset_t)((unsigned long)a + (PAGE_SIZE * numpgs)),
	    VM_PROT_DEFAULT, FALSE) != KERN_SUCCESS)
		panic("MEMGUARD: Unable to unguard page!");
}

/*
 * vtomgfifo() converts a virtual address of the first page allocated for
 * an item to a memguard_fifo_pool reference for the corresponding item's
 * size.
 *
 * vsetmgfifo() sets a reference in an underlying page for the specified
 * virtual address to an appropriate memguard_fifo_pool.
 *
 * These routines are very similar to those defined by UMA in uma_int.h.
 * The difference is that these routines store the mgfifo in one of the
 * page's fields that is unused when the page is wired rather than the
 * object field, which is used.
 */
static struct memguard_fifo *
vtomgfifo(vm_offset_t va)
{
	vm_page_t p;
	struct memguard_fifo *mgfifo;

	p = PHYS_TO_VM_PAGE(pmap_kextract(va));
	KASSERT(p->wire_count != 0 && p->queue == PQ_NONE,
	    ("MEMGUARD: Expected wired page in vtomgfifo!"));
	mgfifo = (struct memguard_fifo *)p->pageq.tqe_next;
	return mgfifo;
}

static void
vsetmgfifo(vm_offset_t va, struct memguard_fifo *mgfifo)
{
	vm_page_t p;

	p = PHYS_TO_VM_PAGE(pmap_kextract(va));
	KASSERT(p->wire_count != 0 && p->queue == PQ_NONE,
	    ("MEMGUARD: Expected wired page in vsetmgfifo!"));
	p->pageq.tqe_next = (vm_page_t)mgfifo;
}

static void vclrmgfifo(vm_offset_t va)
{
	vm_page_t p;

	p = PHYS_TO_VM_PAGE(pmap_kextract(va));
	KASSERT(p->wire_count != 0 && p->queue == PQ_NONE,
	    ("MEMGUARD: Expected wired page in vclrmgfifo!"));
	p->pageq.tqe_next = NULL;
}
