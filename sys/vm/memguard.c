/*
 * Copyright (c) 2005,
 *     Bosko Milekic <bmilekic@freebsd.org>
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

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/memguard.h>

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
static STAILQ_HEAD(memguard_fifo, memguard_entry) memguard_fifo_pool;

/*
 * Local prototypes.
 */
static void	memguard_guard(void *addr);
static void	memguard_unguard(void *addr);

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
	STAILQ_INIT(&memguard_fifo_pool);
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
	void *obj = NULL;
	struct memguard_entry *e = NULL;

	/* XXX: MemGuard does not handle > PAGE_SIZE objects. */ 
	if (size > PAGE_SIZE)
		panic("MEMGUARD: Cannot handle objects > PAGE_SIZE");

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
		e = STAILQ_FIRST(&memguard_fifo_pool);
		if (e != NULL) {
			STAILQ_REMOVE(&memguard_fifo_pool, e,
			    memguard_entry, entries);
			MEMGUARD_CRIT_SECTION_EXIT;
			obj = e->ptr;
			free(e, M_TEMP);
			memguard_unguard(obj);
			if (flags & M_ZERO)
				bzero(obj, PAGE_SIZE);
			return obj;
		}
		MEMGUARD_CRIT_SECTION_EXIT;
		if (flags & M_WAITOK)
			panic("MEMGUARD: Failed with M_WAITOK: " \
			    "memguard_map too small");
		return NULL;
	} else
		memguard_mapused += PAGE_SIZE;
	MEMGUARD_CRIT_SECTION_EXIT;

	if (obj == NULL)
		obj = (void *)kmem_malloc(memguard_map, PAGE_SIZE, flags);
	if (obj != NULL) {
		memguard_unguard(obj);
		if (flags & M_ZERO)
			bzero(obj, PAGE_SIZE);
	} else {
		MEMGUARD_CRIT_SECTION_ENTER;
		memguard_mapused -= PAGE_SIZE;
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

	memguard_guard(addr);
	e = malloc(sizeof(struct memguard_entry), M_TEMP, M_NOWAIT);
	if (e == NULL) {
		MEMGUARD_CRIT_SECTION_ENTER;
		memguard_mapused -= PAGE_SIZE;
		MEMGUARD_CRIT_SECTION_EXIT;
		kmem_free(memguard_map, (vm_offset_t)round_page(
		    (unsigned long)addr), PAGE_SIZE);
		return;
	}
	e->ptr = (void *)round_page((unsigned long)addr);
	MEMGUARD_CRIT_SECTION_ENTER;
	STAILQ_INSERT_TAIL(&memguard_fifo_pool, e, entries);
	MEMGUARD_CRIT_SECTION_EXIT;
}

/*
 * Guard a page containing specified object (make it read-only so that
 * future writes to it fail).
 */
static void
memguard_guard(void *addr)
{
	void *a = (void *)round_page((unsigned long)addr);
	(void)vm_map_protect(memguard_map, (vm_offset_t)a,
	    (vm_offset_t)((unsigned long)a + PAGE_SIZE), VM_PROT_READ, 0);
}

/*
 * Unguard a page containing specified object (make it read-and-write to
 * allow full data access).
 */
static void
memguard_unguard(void *addr)
{
	void *a = (void *)round_page((unsigned long)addr);
	(void)vm_map_protect(memguard_map, (vm_offset_t)a,
	    (vm_offset_t)((unsigned long)a + PAGE_SIZE),
	    VM_PROT_READ | VM_PROT_WRITE, 0);
}
