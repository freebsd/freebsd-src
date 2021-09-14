/*-
 * SPDX-License-Identifier: (BSD-3-Clause AND MIT-CMU)
 *
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1998 Matthew Dillon.  All Rights Reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	from: @(#)vm_page.c	7.4 (Berkeley) 5/7/91
 */

/*-
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *	Resident memory management module.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/domainset.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sleepqueue.h>
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_domainset.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_phys.h>
#include <vm/vm_pagequeue.h>
#include <vm/vm_pager.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/vm_extern.h>
#include <vm/vm_dumpset.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#include <machine/md_var.h>

struct vm_domain vm_dom[MAXMEMDOM];

DPCPU_DEFINE_STATIC(struct vm_batchqueue, pqbatch[MAXMEMDOM][PQ_COUNT]);

struct mtx_padalign __exclusive_cache_line pa_lock[PA_LOCK_COUNT];

struct mtx_padalign __exclusive_cache_line vm_domainset_lock;
/* The following fields are protected by the domainset lock. */
domainset_t __exclusive_cache_line vm_min_domains;
domainset_t __exclusive_cache_line vm_severe_domains;
static int vm_min_waiters;
static int vm_severe_waiters;
static int vm_pageproc_waiters;

static SYSCTL_NODE(_vm_stats, OID_AUTO, page, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "VM page statistics");

static COUNTER_U64_DEFINE_EARLY(pqstate_commit_retries);
SYSCTL_COUNTER_U64(_vm_stats_page, OID_AUTO, pqstate_commit_retries,
    CTLFLAG_RD, &pqstate_commit_retries,
    "Number of failed per-page atomic queue state updates");

static COUNTER_U64_DEFINE_EARLY(queue_ops);
SYSCTL_COUNTER_U64(_vm_stats_page, OID_AUTO, queue_ops,
    CTLFLAG_RD, &queue_ops,
    "Number of batched queue operations");

static COUNTER_U64_DEFINE_EARLY(queue_nops);
SYSCTL_COUNTER_U64(_vm_stats_page, OID_AUTO, queue_nops,
    CTLFLAG_RD, &queue_nops,
    "Number of batched queue operations with no effects");

/*
 * bogus page -- for I/O to/from partially complete buffers,
 * or for paging into sparsely invalid regions.
 */
vm_page_t bogus_page;

vm_page_t vm_page_array;
long vm_page_array_size;
long first_page;

struct bitset *vm_page_dump;
long vm_page_dump_pages;

static TAILQ_HEAD(, vm_page) blacklist_head;
static int sysctl_vm_page_blacklist(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_vm, OID_AUTO, page_blacklist, CTLTYPE_STRING | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_vm_page_blacklist, "A", "Blacklist pages");

static uma_zone_t fakepg_zone;

static void vm_page_alloc_check(vm_page_t m);
static bool _vm_page_busy_sleep(vm_object_t obj, vm_page_t m,
    vm_pindex_t pindex, const char *wmesg, int allocflags, bool locked);
static void vm_page_clear_dirty_mask(vm_page_t m, vm_page_bits_t pagebits);
static void vm_page_enqueue(vm_page_t m, uint8_t queue);
static bool vm_page_free_prep(vm_page_t m);
static void vm_page_free_toq(vm_page_t m);
static void vm_page_init(void *dummy);
static int vm_page_insert_after(vm_page_t m, vm_object_t object,
    vm_pindex_t pindex, vm_page_t mpred);
static void vm_page_insert_radixdone(vm_page_t m, vm_object_t object,
    vm_page_t mpred);
static void vm_page_mvqueue(vm_page_t m, const uint8_t queue,
    const uint16_t nflag);
static int vm_page_reclaim_run(int req_class, int domain, u_long npages,
    vm_page_t m_run, vm_paddr_t high);
static void vm_page_release_toq(vm_page_t m, uint8_t nqueue, bool noreuse);
static int vm_domain_alloc_fail(struct vm_domain *vmd, vm_object_t object,
    int req);
static int vm_page_zone_import(void *arg, void **store, int cnt, int domain,
    int flags);
static void vm_page_zone_release(void *arg, void **store, int cnt);

SYSINIT(vm_page, SI_SUB_VM, SI_ORDER_SECOND, vm_page_init, NULL);

static void
vm_page_init(void *dummy)
{

	fakepg_zone = uma_zcreate("fakepg", sizeof(struct vm_page), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	bogus_page = vm_page_alloc(NULL, 0, VM_ALLOC_NOOBJ |
	    VM_ALLOC_NORMAL | VM_ALLOC_WIRED);
}

/*
 * The cache page zone is initialized later since we need to be able to allocate
 * pages before UMA is fully initialized.
 */
static void
vm_page_init_cache_zones(void *dummy __unused)
{
	struct vm_domain *vmd;
	struct vm_pgcache *pgcache;
	int cache, domain, maxcache, pool;

	maxcache = 0;
	TUNABLE_INT_FETCH("vm.pgcache_zone_max_pcpu", &maxcache);
	maxcache *= mp_ncpus;
	for (domain = 0; domain < vm_ndomains; domain++) {
		vmd = VM_DOMAIN(domain);
		for (pool = 0; pool < VM_NFREEPOOL; pool++) {
			pgcache = &vmd->vmd_pgcache[pool];
			pgcache->domain = domain;
			pgcache->pool = pool;
			pgcache->zone = uma_zcache_create("vm pgcache",
			    PAGE_SIZE, NULL, NULL, NULL, NULL,
			    vm_page_zone_import, vm_page_zone_release, pgcache,
			    UMA_ZONE_VM);

			/*
			 * Limit each pool's zone to 0.1% of the pages in the
			 * domain.
			 */
			cache = maxcache != 0 ? maxcache :
			    vmd->vmd_page_count / 1000;
			uma_zone_set_maxcache(pgcache->zone, cache);
		}
	}
}
SYSINIT(vm_page2, SI_SUB_VM_CONF, SI_ORDER_ANY, vm_page_init_cache_zones, NULL);

/* Make sure that u_long is at least 64 bits when PAGE_SIZE is 32K. */
#if PAGE_SIZE == 32768
#ifdef CTASSERT
CTASSERT(sizeof(u_long) >= 8);
#endif
#endif

/*
 *	vm_set_page_size:
 *
 *	Sets the page size, perhaps based upon the memory
 *	size.  Must be called before any use of page-size
 *	dependent functions.
 */
void
vm_set_page_size(void)
{
	if (vm_cnt.v_page_size == 0)
		vm_cnt.v_page_size = PAGE_SIZE;
	if (((vm_cnt.v_page_size - 1) & vm_cnt.v_page_size) != 0)
		panic("vm_set_page_size: page size not a power of two");
}

/*
 *	vm_page_blacklist_next:
 *
 *	Find the next entry in the provided string of blacklist
 *	addresses.  Entries are separated by space, comma, or newline.
 *	If an invalid integer is encountered then the rest of the
 *	string is skipped.  Updates the list pointer to the next
 *	character, or NULL if the string is exhausted or invalid.
 */
static vm_paddr_t
vm_page_blacklist_next(char **list, char *end)
{
	vm_paddr_t bad;
	char *cp, *pos;

	if (list == NULL || *list == NULL)
		return (0);
	if (**list =='\0') {
		*list = NULL;
		return (0);
	}

	/*
	 * If there's no end pointer then the buffer is coming from
	 * the kenv and we know it's null-terminated.
	 */
	if (end == NULL)
		end = *list + strlen(*list);

	/* Ensure that strtoq() won't walk off the end */
	if (*end != '\0') {
		if (*end == '\n' || *end == ' ' || *end  == ',')
			*end = '\0';
		else {
			printf("Blacklist not terminated, skipping\n");
			*list = NULL;
			return (0);
		}
	}

	for (pos = *list; *pos != '\0'; pos = cp) {
		bad = strtoq(pos, &cp, 0);
		if (*cp == '\0' || *cp == ' ' || *cp == ',' || *cp == '\n') {
			if (bad == 0) {
				if (++cp < end)
					continue;
				else
					break;
			}
		} else
			break;
		if (*cp == '\0' || ++cp >= end)
			*list = NULL;
		else
			*list = cp;
		return (trunc_page(bad));
	}
	printf("Garbage in RAM blacklist, skipping\n");
	*list = NULL;
	return (0);
}

bool
vm_page_blacklist_add(vm_paddr_t pa, bool verbose)
{
	struct vm_domain *vmd;
	vm_page_t m;
	int ret;

	m = vm_phys_paddr_to_vm_page(pa);
	if (m == NULL)
		return (true); /* page does not exist, no failure */

	vmd = vm_pagequeue_domain(m);
	vm_domain_free_lock(vmd);
	ret = vm_phys_unfree_page(m);
	vm_domain_free_unlock(vmd);
	if (ret != 0) {
		vm_domain_freecnt_inc(vmd, -1);
		TAILQ_INSERT_TAIL(&blacklist_head, m, listq);
		if (verbose)
			printf("Skipping page with pa 0x%jx\n", (uintmax_t)pa);
	}
	return (ret);
}

/*
 *	vm_page_blacklist_check:
 *
 *	Iterate through the provided string of blacklist addresses, pulling
 *	each entry out of the physical allocator free list and putting it
 *	onto a list for reporting via the vm.page_blacklist sysctl.
 */
static void
vm_page_blacklist_check(char *list, char *end)
{
	vm_paddr_t pa;
	char *next;

	next = list;
	while (next != NULL) {
		if ((pa = vm_page_blacklist_next(&next, end)) == 0)
			continue;
		vm_page_blacklist_add(pa, bootverbose);
	}
}

/*
 *	vm_page_blacklist_load:
 *
 *	Search for a special module named "ram_blacklist".  It'll be a
 *	plain text file provided by the user via the loader directive
 *	of the same name.
 */
static void
vm_page_blacklist_load(char **list, char **end)
{
	void *mod;
	u_char *ptr;
	u_int len;

	mod = NULL;
	ptr = NULL;

	mod = preload_search_by_type("ram_blacklist");
	if (mod != NULL) {
		ptr = preload_fetch_addr(mod);
		len = preload_fetch_size(mod);
        }
	*list = ptr;
	if (ptr != NULL)
		*end = ptr + len;
	else
		*end = NULL;
	return;
}

static int
sysctl_vm_page_blacklist(SYSCTL_HANDLER_ARGS)
{
	vm_page_t m;
	struct sbuf sbuf;
	int error, first;

	first = 1;
	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
	TAILQ_FOREACH(m, &blacklist_head, listq) {
		sbuf_printf(&sbuf, "%s%#jx", first ? "" : ",",
		    (uintmax_t)m->phys_addr);
		first = 0;
	}
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	return (error);
}

/*
 * Initialize a dummy page for use in scans of the specified paging queue.
 * In principle, this function only needs to set the flag PG_MARKER.
 * Nonetheless, it write busies the page as a safety precaution.
 */
void
vm_page_init_marker(vm_page_t marker, int queue, uint16_t aflags)
{

	bzero(marker, sizeof(*marker));
	marker->flags = PG_MARKER;
	marker->a.flags = aflags;
	marker->busy_lock = VPB_CURTHREAD_EXCLUSIVE;
	marker->a.queue = queue;
}

static void
vm_page_domain_init(int domain)
{
	struct vm_domain *vmd;
	struct vm_pagequeue *pq;
	int i;

	vmd = VM_DOMAIN(domain);
	bzero(vmd, sizeof(*vmd));
	*__DECONST(const char **, &vmd->vmd_pagequeues[PQ_INACTIVE].pq_name) =
	    "vm inactive pagequeue";
	*__DECONST(const char **, &vmd->vmd_pagequeues[PQ_ACTIVE].pq_name) =
	    "vm active pagequeue";
	*__DECONST(const char **, &vmd->vmd_pagequeues[PQ_LAUNDRY].pq_name) =
	    "vm laundry pagequeue";
	*__DECONST(const char **,
	    &vmd->vmd_pagequeues[PQ_UNSWAPPABLE].pq_name) =
	    "vm unswappable pagequeue";
	vmd->vmd_domain = domain;
	vmd->vmd_page_count = 0;
	vmd->vmd_free_count = 0;
	vmd->vmd_segs = 0;
	vmd->vmd_oom = FALSE;
	for (i = 0; i < PQ_COUNT; i++) {
		pq = &vmd->vmd_pagequeues[i];
		TAILQ_INIT(&pq->pq_pl);
		mtx_init(&pq->pq_mutex, pq->pq_name, "vm pagequeue",
		    MTX_DEF | MTX_DUPOK);
		pq->pq_pdpages = 0;
		vm_page_init_marker(&vmd->vmd_markers[i], i, 0);
	}
	mtx_init(&vmd->vmd_free_mtx, "vm page free queue", NULL, MTX_DEF);
	mtx_init(&vmd->vmd_pageout_mtx, "vm pageout lock", NULL, MTX_DEF);
	snprintf(vmd->vmd_name, sizeof(vmd->vmd_name), "%d", domain);

	/*
	 * inacthead is used to provide FIFO ordering for LRU-bypassing
	 * insertions.
	 */
	vm_page_init_marker(&vmd->vmd_inacthead, PQ_INACTIVE, PGA_ENQUEUED);
	TAILQ_INSERT_HEAD(&vmd->vmd_pagequeues[PQ_INACTIVE].pq_pl,
	    &vmd->vmd_inacthead, plinks.q);

	/*
	 * The clock pages are used to implement active queue scanning without
	 * requeues.  Scans start at clock[0], which is advanced after the scan
	 * ends.  When the two clock hands meet, they are reset and scanning
	 * resumes from the head of the queue.
	 */
	vm_page_init_marker(&vmd->vmd_clock[0], PQ_ACTIVE, PGA_ENQUEUED);
	vm_page_init_marker(&vmd->vmd_clock[1], PQ_ACTIVE, PGA_ENQUEUED);
	TAILQ_INSERT_HEAD(&vmd->vmd_pagequeues[PQ_ACTIVE].pq_pl,
	    &vmd->vmd_clock[0], plinks.q);
	TAILQ_INSERT_TAIL(&vmd->vmd_pagequeues[PQ_ACTIVE].pq_pl,
	    &vmd->vmd_clock[1], plinks.q);
}

/*
 * Initialize a physical page in preparation for adding it to the free
 * lists.
 */
void
vm_page_init_page(vm_page_t m, vm_paddr_t pa, int segind)
{

	m->object = NULL;
	m->ref_count = 0;
	m->busy_lock = VPB_FREED;
	m->flags = m->a.flags = 0;
	m->phys_addr = pa;
	m->a.queue = PQ_NONE;
	m->psind = 0;
	m->segind = segind;
	m->order = VM_NFREEORDER;
	m->pool = VM_FREEPOOL_DEFAULT;
	m->valid = m->dirty = 0;
	pmap_page_init(m);
}

#ifndef PMAP_HAS_PAGE_ARRAY
static vm_paddr_t
vm_page_array_alloc(vm_offset_t *vaddr, vm_paddr_t end, vm_paddr_t page_range)
{
	vm_paddr_t new_end;

	/*
	 * Reserve an unmapped guard page to trap access to vm_page_array[-1].
	 * However, because this page is allocated from KVM, out-of-bounds
	 * accesses using the direct map will not be trapped.
	 */
	*vaddr += PAGE_SIZE;

	/*
	 * Allocate physical memory for the page structures, and map it.
	 */
	new_end = trunc_page(end - page_range * sizeof(struct vm_page));
	vm_page_array = (vm_page_t)pmap_map(vaddr, new_end, end,
	    VM_PROT_READ | VM_PROT_WRITE);
	vm_page_array_size = page_range;

	return (new_end);
}
#endif

/*
 *	vm_page_startup:
 *
 *	Initializes the resident memory module.  Allocates physical memory for
 *	bootstrapping UMA and some data structures that are used to manage
 *	physical pages.  Initializes these structures, and populates the free
 *	page queues.
 */
vm_offset_t
vm_page_startup(vm_offset_t vaddr)
{
	struct vm_phys_seg *seg;
	struct vm_domain *vmd;
	vm_page_t m;
	char *list, *listend;
	vm_paddr_t end, high_avail, low_avail, new_end, size;
	vm_paddr_t page_range __unused;
	vm_paddr_t last_pa, pa, startp, endp;
	u_long pagecount;
#if MINIDUMP_PAGE_TRACKING
	u_long vm_page_dump_size;
#endif
	int biggestone, i, segind;
#ifdef WITNESS
	vm_offset_t mapped;
	int witness_size;
#endif
#if defined(__i386__) && defined(VM_PHYSSEG_DENSE)
	long ii;
#endif

	vaddr = round_page(vaddr);

	vm_phys_early_startup();
	biggestone = vm_phys_avail_largest();
	end = phys_avail[biggestone+1];

	/*
	 * Initialize the page and queue locks.
	 */
	mtx_init(&vm_domainset_lock, "vm domainset lock", NULL, MTX_DEF);
	for (i = 0; i < PA_LOCK_COUNT; i++)
		mtx_init(&pa_lock[i], "vm page", NULL, MTX_DEF);
	for (i = 0; i < vm_ndomains; i++)
		vm_page_domain_init(i);

	new_end = end;
#ifdef WITNESS
	witness_size = round_page(witness_startup_count());
	new_end -= witness_size;
	mapped = pmap_map(&vaddr, new_end, new_end + witness_size,
	    VM_PROT_READ | VM_PROT_WRITE);
	bzero((void *)mapped, witness_size);
	witness_startup((void *)mapped);
#endif

#if MINIDUMP_PAGE_TRACKING
	/*
	 * Allocate a bitmap to indicate that a random physical page
	 * needs to be included in a minidump.
	 *
	 * The amd64 port needs this to indicate which direct map pages
	 * need to be dumped, via calls to dump_add_page()/dump_drop_page().
	 *
	 * However, i386 still needs this workspace internally within the
	 * minidump code.  In theory, they are not needed on i386, but are
	 * included should the sf_buf code decide to use them.
	 */
	last_pa = 0;
	vm_page_dump_pages = 0;
	for (i = 0; dump_avail[i + 1] != 0; i += 2) {
		vm_page_dump_pages += howmany(dump_avail[i + 1], PAGE_SIZE) -
		    dump_avail[i] / PAGE_SIZE;
		if (dump_avail[i + 1] > last_pa)
			last_pa = dump_avail[i + 1];
	}
	vm_page_dump_size = round_page(BITSET_SIZE(vm_page_dump_pages));
	new_end -= vm_page_dump_size;
	vm_page_dump = (void *)(uintptr_t)pmap_map(&vaddr, new_end,
	    new_end + vm_page_dump_size, VM_PROT_READ | VM_PROT_WRITE);
	bzero((void *)vm_page_dump, vm_page_dump_size);
#else
	(void)last_pa;
#endif
#if defined(__aarch64__) || defined(__amd64__) || defined(__mips__) || \
    defined(__riscv) || defined(__powerpc64__)
	/*
	 * Include the UMA bootstrap pages, witness pages and vm_page_dump
	 * in a crash dump.  When pmap_map() uses the direct map, they are
	 * not automatically included.
	 */
	for (pa = new_end; pa < end; pa += PAGE_SIZE)
		dump_add_page(pa);
#endif
	phys_avail[biggestone + 1] = new_end;
#ifdef __amd64__
	/*
	 * Request that the physical pages underlying the message buffer be
	 * included in a crash dump.  Since the message buffer is accessed
	 * through the direct map, they are not automatically included.
	 */
	pa = DMAP_TO_PHYS((vm_offset_t)msgbufp->msg_ptr);
	last_pa = pa + round_page(msgbufsize);
	while (pa < last_pa) {
		dump_add_page(pa);
		pa += PAGE_SIZE;
	}
#endif
	/*
	 * Compute the number of pages of memory that will be available for
	 * use, taking into account the overhead of a page structure per page.
	 * In other words, solve
	 *	"available physical memory" - round_page(page_range *
	 *	    sizeof(struct vm_page)) = page_range * PAGE_SIZE 
	 * for page_range.  
	 */
	low_avail = phys_avail[0];
	high_avail = phys_avail[1];
	for (i = 0; i < vm_phys_nsegs; i++) {
		if (vm_phys_segs[i].start < low_avail)
			low_avail = vm_phys_segs[i].start;
		if (vm_phys_segs[i].end > high_avail)
			high_avail = vm_phys_segs[i].end;
	}
	/* Skip the first chunk.  It is already accounted for. */
	for (i = 2; phys_avail[i + 1] != 0; i += 2) {
		if (phys_avail[i] < low_avail)
			low_avail = phys_avail[i];
		if (phys_avail[i + 1] > high_avail)
			high_avail = phys_avail[i + 1];
	}
	first_page = low_avail / PAGE_SIZE;
#ifdef VM_PHYSSEG_SPARSE
	size = 0;
	for (i = 0; i < vm_phys_nsegs; i++)
		size += vm_phys_segs[i].end - vm_phys_segs[i].start;
	for (i = 0; phys_avail[i + 1] != 0; i += 2)
		size += phys_avail[i + 1] - phys_avail[i];
#elif defined(VM_PHYSSEG_DENSE)
	size = high_avail - low_avail;
#else
#error "Either VM_PHYSSEG_DENSE or VM_PHYSSEG_SPARSE must be defined."
#endif

#ifdef PMAP_HAS_PAGE_ARRAY
	pmap_page_array_startup(size / PAGE_SIZE);
	biggestone = vm_phys_avail_largest();
	end = new_end = phys_avail[biggestone + 1];
#else
#ifdef VM_PHYSSEG_DENSE
	/*
	 * In the VM_PHYSSEG_DENSE case, the number of pages can account for
	 * the overhead of a page structure per page only if vm_page_array is
	 * allocated from the last physical memory chunk.  Otherwise, we must
	 * allocate page structures representing the physical memory
	 * underlying vm_page_array, even though they will not be used.
	 */
	if (new_end != high_avail)
		page_range = size / PAGE_SIZE;
	else
#endif
	{
		page_range = size / (PAGE_SIZE + sizeof(struct vm_page));

		/*
		 * If the partial bytes remaining are large enough for
		 * a page (PAGE_SIZE) without a corresponding
		 * 'struct vm_page', then new_end will contain an
		 * extra page after subtracting the length of the VM
		 * page array.  Compensate by subtracting an extra
		 * page from new_end.
		 */
		if (size % (PAGE_SIZE + sizeof(struct vm_page)) >= PAGE_SIZE) {
			if (new_end == high_avail)
				high_avail -= PAGE_SIZE;
			new_end -= PAGE_SIZE;
		}
	}
	end = new_end;
	new_end = vm_page_array_alloc(&vaddr, end, page_range);
#endif

#if VM_NRESERVLEVEL > 0
	/*
	 * Allocate physical memory for the reservation management system's
	 * data structures, and map it.
	 */
	new_end = vm_reserv_startup(&vaddr, new_end);
#endif
#if defined(__aarch64__) || defined(__amd64__) || defined(__mips__) || \
    defined(__riscv) || defined(__powerpc64__)
	/*
	 * Include vm_page_array and vm_reserv_array in a crash dump.
	 */
	for (pa = new_end; pa < end; pa += PAGE_SIZE)
		dump_add_page(pa);
#endif
	phys_avail[biggestone + 1] = new_end;

	/*
	 * Add physical memory segments corresponding to the available
	 * physical pages.
	 */
	for (i = 0; phys_avail[i + 1] != 0; i += 2)
		if (vm_phys_avail_size(i) != 0)
			vm_phys_add_seg(phys_avail[i], phys_avail[i + 1]);

	/*
	 * Initialize the physical memory allocator.
	 */
	vm_phys_init();

	/*
	 * Initialize the page structures and add every available page to the
	 * physical memory allocator's free lists.
	 */
#if defined(__i386__) && defined(VM_PHYSSEG_DENSE)
	for (ii = 0; ii < vm_page_array_size; ii++) {
		m = &vm_page_array[ii];
		vm_page_init_page(m, (first_page + ii) << PAGE_SHIFT, 0);
		m->flags = PG_FICTITIOUS;
	}
#endif
	vm_cnt.v_page_count = 0;
	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		seg = &vm_phys_segs[segind];
		for (m = seg->first_page, pa = seg->start; pa < seg->end;
		    m++, pa += PAGE_SIZE)
			vm_page_init_page(m, pa, segind);

		/*
		 * Add the segment's pages that are covered by one of
		 * phys_avail's ranges to the free lists.
		 */
		for (i = 0; phys_avail[i + 1] != 0; i += 2) {
			if (seg->end < phys_avail[i] ||
			    seg->start >= phys_avail[i + 1])
				continue;

			startp = MAX(seg->start, phys_avail[i]);
			m = seg->first_page + atop(seg->start - startp);
			endp = MIN(seg->end, phys_avail[i + 1]);
			pagecount = (u_long)atop(endp - startp);
			if (pagecount == 0)
				continue;

			vmd = VM_DOMAIN(seg->domain);
			vm_domain_free_lock(vmd);
			vm_phys_enqueue_contig(m, pagecount);
			vm_domain_free_unlock(vmd);
			vm_domain_freecnt_inc(vmd, pagecount);
			vm_cnt.v_page_count += (u_int)pagecount;

			vmd = VM_DOMAIN(seg->domain);
			vmd->vmd_page_count += (u_int)pagecount;
			vmd->vmd_segs |= 1UL << m->segind;
		}
	}

	/*
	 * Remove blacklisted pages from the physical memory allocator.
	 */
	TAILQ_INIT(&blacklist_head);
	vm_page_blacklist_load(&list, &listend);
	vm_page_blacklist_check(list, listend);

	list = kern_getenv("vm.blacklist");
	vm_page_blacklist_check(list, NULL);

	freeenv(list);
#if VM_NRESERVLEVEL > 0
	/*
	 * Initialize the reservation management system.
	 */
	vm_reserv_init();
#endif

	return (vaddr);
}

void
vm_page_reference(vm_page_t m)
{

	vm_page_aflag_set(m, PGA_REFERENCED);
}

/*
 *	vm_page_trybusy
 *
 *	Helper routine for grab functions to trylock busy.
 *
 *	Returns true on success and false on failure.
 */
static bool
vm_page_trybusy(vm_page_t m, int allocflags)
{

	if ((allocflags & (VM_ALLOC_SBUSY | VM_ALLOC_IGN_SBUSY)) != 0)
		return (vm_page_trysbusy(m));
	else
		return (vm_page_tryxbusy(m));
}

/*
 *	vm_page_tryacquire
 *
 *	Helper routine for grab functions to trylock busy and wire.
 *
 *	Returns true on success and false on failure.
 */
static inline bool
vm_page_tryacquire(vm_page_t m, int allocflags)
{
	bool locked;

	locked = vm_page_trybusy(m, allocflags);
	if (locked && (allocflags & VM_ALLOC_WIRED) != 0)
		vm_page_wire(m);
	return (locked);
}

/*
 *	vm_page_busy_acquire:
 *
 *	Acquire the busy lock as described by VM_ALLOC_* flags.  Will loop
 *	and drop the object lock if necessary.
 */
bool
vm_page_busy_acquire(vm_page_t m, int allocflags)
{
	vm_object_t obj;
	bool locked;

	/*
	 * The page-specific object must be cached because page
	 * identity can change during the sleep, causing the
	 * re-lock of a different object.
	 * It is assumed that a reference to the object is already
	 * held by the callers.
	 */
	obj = atomic_load_ptr(&m->object);
	for (;;) {
		if (vm_page_tryacquire(m, allocflags))
			return (true);
		if ((allocflags & VM_ALLOC_NOWAIT) != 0)
			return (false);
		if (obj != NULL)
			locked = VM_OBJECT_WOWNED(obj);
		else
			locked = false;
		MPASS(locked || vm_page_wired(m));
		if (_vm_page_busy_sleep(obj, m, m->pindex, "vmpba", allocflags,
		    locked) && locked)
			VM_OBJECT_WLOCK(obj);
		if ((allocflags & VM_ALLOC_WAITFAIL) != 0)
			return (false);
		KASSERT(m->object == obj || m->object == NULL,
		    ("vm_page_busy_acquire: page %p does not belong to %p",
		    m, obj));
	}
}

/*
 *	vm_page_busy_downgrade:
 *
 *	Downgrade an exclusive busy page into a single shared busy page.
 */
void
vm_page_busy_downgrade(vm_page_t m)
{
	u_int x;

	vm_page_assert_xbusied(m);

	x = vm_page_busy_fetch(m);
	for (;;) {
		if (atomic_fcmpset_rel_int(&m->busy_lock,
		    &x, VPB_SHARERS_WORD(1)))
			break;
	}
	if ((x & VPB_BIT_WAITERS) != 0)
		wakeup(m);
}

/*
 *
 *	vm_page_busy_tryupgrade:
 *
 *	Attempt to upgrade a single shared busy into an exclusive busy.
 */
int
vm_page_busy_tryupgrade(vm_page_t m)
{
	u_int ce, x;

	vm_page_assert_sbusied(m);

	x = vm_page_busy_fetch(m);
	ce = VPB_CURTHREAD_EXCLUSIVE;
	for (;;) {
		if (VPB_SHARERS(x) > 1)
			return (0);
		KASSERT((x & ~VPB_BIT_WAITERS) == VPB_SHARERS_WORD(1),
		    ("vm_page_busy_tryupgrade: invalid lock state"));
		if (!atomic_fcmpset_acq_int(&m->busy_lock, &x,
		    ce | (x & VPB_BIT_WAITERS)))
			continue;
		return (1);
	}
}

/*
 *	vm_page_sbusied:
 *
 *	Return a positive value if the page is shared busied, 0 otherwise.
 */
int
vm_page_sbusied(vm_page_t m)
{
	u_int x;

	x = vm_page_busy_fetch(m);
	return ((x & VPB_BIT_SHARED) != 0 && x != VPB_UNBUSIED);
}

/*
 *	vm_page_sunbusy:
 *
 *	Shared unbusy a page.
 */
void
vm_page_sunbusy(vm_page_t m)
{
	u_int x;

	vm_page_assert_sbusied(m);

	x = vm_page_busy_fetch(m);
	for (;;) {
		KASSERT(x != VPB_FREED,
		    ("vm_page_sunbusy: Unlocking freed page."));
		if (VPB_SHARERS(x) > 1) {
			if (atomic_fcmpset_int(&m->busy_lock, &x,
			    x - VPB_ONE_SHARER))
				break;
			continue;
		}
		KASSERT((x & ~VPB_BIT_WAITERS) == VPB_SHARERS_WORD(1),
		    ("vm_page_sunbusy: invalid lock state"));
		if (!atomic_fcmpset_rel_int(&m->busy_lock, &x, VPB_UNBUSIED))
			continue;
		if ((x & VPB_BIT_WAITERS) == 0)
			break;
		wakeup(m);
		break;
	}
}

/*
 *	vm_page_busy_sleep:
 *
 *	Sleep if the page is busy, using the page pointer as wchan.
 *	This is used to implement the hard-path of busying mechanism.
 *
 *	If nonshared is true, sleep only if the page is xbusy.
 *
 *	The object lock must be held on entry and will be released on exit.
 */
void
vm_page_busy_sleep(vm_page_t m, const char *wmesg, bool nonshared)
{
	vm_object_t obj;

	obj = m->object;
	VM_OBJECT_ASSERT_LOCKED(obj);
	vm_page_lock_assert(m, MA_NOTOWNED);

	if (!_vm_page_busy_sleep(obj, m, m->pindex, wmesg,
	    nonshared ? VM_ALLOC_SBUSY : 0 , true))
		VM_OBJECT_DROP(obj);
}

/*
 *	vm_page_busy_sleep_unlocked:
 *
 *	Sleep if the page is busy, using the page pointer as wchan.
 *	This is used to implement the hard-path of busying mechanism.
 *
 *	If nonshared is true, sleep only if the page is xbusy.
 *
 *	The object lock must not be held on entry.  The operation will
 *	return if the page changes identity.
 */
void
vm_page_busy_sleep_unlocked(vm_object_t obj, vm_page_t m, vm_pindex_t pindex,
    const char *wmesg, bool nonshared)
{

	VM_OBJECT_ASSERT_UNLOCKED(obj);
	vm_page_lock_assert(m, MA_NOTOWNED);

	_vm_page_busy_sleep(obj, m, pindex, wmesg,
	    nonshared ? VM_ALLOC_SBUSY : 0, false);
}

/*
 *	_vm_page_busy_sleep:
 *
 *	Internal busy sleep function.  Verifies the page identity and
 *	lockstate against parameters.  Returns true if it sleeps and
 *	false otherwise.
 *
 *	If locked is true the lock will be dropped for any true returns
 *	and held for any false returns.
 */
static bool
_vm_page_busy_sleep(vm_object_t obj, vm_page_t m, vm_pindex_t pindex,
    const char *wmesg, int allocflags, bool locked)
{
	bool xsleep;
	u_int x;

	/*
	 * If the object is busy we must wait for that to drain to zero
	 * before trying the page again.
	 */
	if (obj != NULL && vm_object_busied(obj)) {
		if (locked)
			VM_OBJECT_DROP(obj);
		vm_object_busy_wait(obj, wmesg);
		return (true);
	}

	if (!vm_page_busied(m))
		return (false);

	xsleep = (allocflags & (VM_ALLOC_SBUSY | VM_ALLOC_IGN_SBUSY)) != 0;
	sleepq_lock(m);
	x = vm_page_busy_fetch(m);
	do {
		/*
		 * If the page changes objects or becomes unlocked we can
		 * simply return.
		 */
		if (x == VPB_UNBUSIED ||
		    (xsleep && (x & VPB_BIT_SHARED) != 0) ||
		    m->object != obj || m->pindex != pindex) {
			sleepq_release(m);
			return (false);
		}
		if ((x & VPB_BIT_WAITERS) != 0)
			break;
	} while (!atomic_fcmpset_int(&m->busy_lock, &x, x | VPB_BIT_WAITERS));
	if (locked)
		VM_OBJECT_DROP(obj);
	DROP_GIANT();
	sleepq_add(m, NULL, wmesg, 0, 0);
	sleepq_wait(m, PVM);
	PICKUP_GIANT();
	return (true);
}

/*
 *	vm_page_trysbusy:
 *
 *	Try to shared busy a page.
 *	If the operation succeeds 1 is returned otherwise 0.
 *	The operation never sleeps.
 */
int
vm_page_trysbusy(vm_page_t m)
{
	vm_object_t obj;
	u_int x;

	obj = m->object;
	x = vm_page_busy_fetch(m);
	for (;;) {
		if ((x & VPB_BIT_SHARED) == 0)
			return (0);
		/*
		 * Reduce the window for transient busies that will trigger
		 * false negatives in vm_page_ps_test().
		 */
		if (obj != NULL && vm_object_busied(obj))
			return (0);
		if (atomic_fcmpset_acq_int(&m->busy_lock, &x,
		    x + VPB_ONE_SHARER))
			break;
	}

	/* Refetch the object now that we're guaranteed that it is stable. */
	obj = m->object;
	if (obj != NULL && vm_object_busied(obj)) {
		vm_page_sunbusy(m);
		return (0);
	}
	return (1);
}

/*
 *	vm_page_tryxbusy:
 *
 *	Try to exclusive busy a page.
 *	If the operation succeeds 1 is returned otherwise 0.
 *	The operation never sleeps.
 */
int
vm_page_tryxbusy(vm_page_t m)
{
	vm_object_t obj;

        if (atomic_cmpset_acq_int(&m->busy_lock, VPB_UNBUSIED,
            VPB_CURTHREAD_EXCLUSIVE) == 0)
		return (0);

	obj = m->object;
	if (obj != NULL && vm_object_busied(obj)) {
		vm_page_xunbusy(m);
		return (0);
	}
	return (1);
}

static void
vm_page_xunbusy_hard_tail(vm_page_t m)
{
	atomic_store_rel_int(&m->busy_lock, VPB_UNBUSIED);
	/* Wake the waiter. */
	wakeup(m);
}

/*
 *	vm_page_xunbusy_hard:
 *
 *	Called when unbusy has failed because there is a waiter.
 */
void
vm_page_xunbusy_hard(vm_page_t m)
{
	vm_page_assert_xbusied(m);
	vm_page_xunbusy_hard_tail(m);
}

void
vm_page_xunbusy_hard_unchecked(vm_page_t m)
{
	vm_page_assert_xbusied_unchecked(m);
	vm_page_xunbusy_hard_tail(m);
}

static void
vm_page_busy_free(vm_page_t m)
{
	u_int x;

	atomic_thread_fence_rel();
	x = atomic_swap_int(&m->busy_lock, VPB_FREED);
	if ((x & VPB_BIT_WAITERS) != 0)
		wakeup(m);
}

/*
 *	vm_page_unhold_pages:
 *
 *	Unhold each of the pages that is referenced by the given array.
 */
void
vm_page_unhold_pages(vm_page_t *ma, int count)
{

	for (; count != 0; count--) {
		vm_page_unwire(*ma, PQ_ACTIVE);
		ma++;
	}
}

vm_page_t
PHYS_TO_VM_PAGE(vm_paddr_t pa)
{
	vm_page_t m;

#ifdef VM_PHYSSEG_SPARSE
	m = vm_phys_paddr_to_vm_page(pa);
	if (m == NULL)
		m = vm_phys_fictitious_to_vm_page(pa);
	return (m);
#elif defined(VM_PHYSSEG_DENSE)
	long pi;

	pi = atop(pa);
	if (pi >= first_page && (pi - first_page) < vm_page_array_size) {
		m = &vm_page_array[pi - first_page];
		return (m);
	}
	return (vm_phys_fictitious_to_vm_page(pa));
#else
#error "Either VM_PHYSSEG_DENSE or VM_PHYSSEG_SPARSE must be defined."
#endif
}

/*
 *	vm_page_getfake:
 *
 *	Create a fictitious page with the specified physical address and
 *	memory attribute.  The memory attribute is the only the machine-
 *	dependent aspect of a fictitious page that must be initialized.
 */
vm_page_t
vm_page_getfake(vm_paddr_t paddr, vm_memattr_t memattr)
{
	vm_page_t m;

	m = uma_zalloc(fakepg_zone, M_WAITOK | M_ZERO);
	vm_page_initfake(m, paddr, memattr);
	return (m);
}

void
vm_page_initfake(vm_page_t m, vm_paddr_t paddr, vm_memattr_t memattr)
{

	if ((m->flags & PG_FICTITIOUS) != 0) {
		/*
		 * The page's memattr might have changed since the
		 * previous initialization.  Update the pmap to the
		 * new memattr.
		 */
		goto memattr;
	}
	m->phys_addr = paddr;
	m->a.queue = PQ_NONE;
	/* Fictitious pages don't use "segind". */
	m->flags = PG_FICTITIOUS;
	/* Fictitious pages don't use "order" or "pool". */
	m->oflags = VPO_UNMANAGED;
	m->busy_lock = VPB_CURTHREAD_EXCLUSIVE;
	/* Fictitious pages are unevictable. */
	m->ref_count = 1;
	pmap_page_init(m);
memattr:
	pmap_page_set_memattr(m, memattr);
}

/*
 *	vm_page_putfake:
 *
 *	Release a fictitious page.
 */
void
vm_page_putfake(vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) != 0, ("managed %p", m));
	KASSERT((m->flags & PG_FICTITIOUS) != 0,
	    ("vm_page_putfake: bad page %p", m));
	vm_page_assert_xbusied(m);
	vm_page_busy_free(m);
	uma_zfree(fakepg_zone, m);
}

/*
 *	vm_page_updatefake:
 *
 *	Update the given fictitious page to the specified physical address and
 *	memory attribute.
 */
void
vm_page_updatefake(vm_page_t m, vm_paddr_t paddr, vm_memattr_t memattr)
{

	KASSERT((m->flags & PG_FICTITIOUS) != 0,
	    ("vm_page_updatefake: bad page %p", m));
	m->phys_addr = paddr;
	pmap_page_set_memattr(m, memattr);
}

/*
 *	vm_page_free:
 *
 *	Free a page.
 */
void
vm_page_free(vm_page_t m)
{

	m->flags &= ~PG_ZERO;
	vm_page_free_toq(m);
}

/*
 *	vm_page_free_zero:
 *
 *	Free a page to the zerod-pages queue
 */
void
vm_page_free_zero(vm_page_t m)
{

	m->flags |= PG_ZERO;
	vm_page_free_toq(m);
}

/*
 * Unbusy and handle the page queueing for a page from a getpages request that
 * was optionally read ahead or behind.
 */
void
vm_page_readahead_finish(vm_page_t m)
{

	/* We shouldn't put invalid pages on queues. */
	KASSERT(!vm_page_none_valid(m), ("%s: %p is invalid", __func__, m));

	/*
	 * Since the page is not the actually needed one, whether it should
	 * be activated or deactivated is not obvious.  Empirical results
	 * have shown that deactivating the page is usually the best choice,
	 * unless the page is wanted by another thread.
	 */
	if ((vm_page_busy_fetch(m) & VPB_BIT_WAITERS) != 0)
		vm_page_activate(m);
	else
		vm_page_deactivate(m);
	vm_page_xunbusy_unchecked(m);
}

/*
 * Destroy the identity of an invalid page and free it if possible.
 * This is intended to be used when reading a page from backing store fails.
 */
void
vm_page_free_invalid(vm_page_t m)
{

	KASSERT(vm_page_none_valid(m), ("page %p is valid", m));
	KASSERT(!pmap_page_is_mapped(m), ("page %p is mapped", m));
	KASSERT(m->object != NULL, ("page %p has no object", m));
	VM_OBJECT_ASSERT_WLOCKED(m->object);

	/*
	 * We may be attempting to free the page as part of the handling for an
	 * I/O error, in which case the page was xbusied by a different thread.
	 */
	vm_page_xbusy_claim(m);

	/*
	 * If someone has wired this page while the object lock
	 * was not held, then the thread that unwires is responsible
	 * for freeing the page.  Otherwise just free the page now.
	 * The wire count of this unmapped page cannot change while
	 * we have the page xbusy and the page's object wlocked.
	 */
	if (vm_page_remove(m))
		vm_page_free(m);
}

/*
 *	vm_page_sleep_if_busy:
 *
 *	Sleep and release the object lock if the page is busied.
 *	Returns TRUE if the thread slept.
 *
 *	The given page must be unlocked and object containing it must
 *	be locked.
 */
int
vm_page_sleep_if_busy(vm_page_t m, const char *wmesg)
{
	vm_object_t obj;

	vm_page_lock_assert(m, MA_NOTOWNED);
	VM_OBJECT_ASSERT_WLOCKED(m->object);

	/*
	 * The page-specific object must be cached because page
	 * identity can change during the sleep, causing the
	 * re-lock of a different object.
	 * It is assumed that a reference to the object is already
	 * held by the callers.
	 */
	obj = m->object;
	if (_vm_page_busy_sleep(obj, m, m->pindex, wmesg, 0, true)) {
		VM_OBJECT_WLOCK(obj);
		return (TRUE);
	}
	return (FALSE);
}

/*
 *	vm_page_sleep_if_xbusy:
 *
 *	Sleep and release the object lock if the page is xbusied.
 *	Returns TRUE if the thread slept.
 *
 *	The given page must be unlocked and object containing it must
 *	be locked.
 */
int
vm_page_sleep_if_xbusy(vm_page_t m, const char *wmesg)
{
	vm_object_t obj;

	vm_page_lock_assert(m, MA_NOTOWNED);
	VM_OBJECT_ASSERT_WLOCKED(m->object);

	/*
	 * The page-specific object must be cached because page
	 * identity can change during the sleep, causing the
	 * re-lock of a different object.
	 * It is assumed that a reference to the object is already
	 * held by the callers.
	 */
	obj = m->object;
	if (_vm_page_busy_sleep(obj, m, m->pindex, wmesg, VM_ALLOC_SBUSY,
	    true)) {
		VM_OBJECT_WLOCK(obj);
		return (TRUE);
	}
	return (FALSE);
}

/*
 *	vm_page_dirty_KBI:		[ internal use only ]
 *
 *	Set all bits in the page's dirty field.
 *
 *	The object containing the specified page must be locked if the
 *	call is made from the machine-independent layer.
 *
 *	See vm_page_clear_dirty_mask().
 *
 *	This function should only be called by vm_page_dirty().
 */
void
vm_page_dirty_KBI(vm_page_t m)
{

	/* Refer to this operation by its public name. */
	KASSERT(vm_page_all_valid(m), ("vm_page_dirty: page is invalid!"));
	m->dirty = VM_PAGE_BITS_ALL;
}

/*
 *	vm_page_insert:		[ internal use only ]
 *
 *	Inserts the given mem entry into the object and object list.
 *
 *	The object must be locked.
 */
int
vm_page_insert(vm_page_t m, vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t mpred;

	VM_OBJECT_ASSERT_WLOCKED(object);
	mpred = vm_radix_lookup_le(&object->rtree, pindex);
	return (vm_page_insert_after(m, object, pindex, mpred));
}

/*
 *	vm_page_insert_after:
 *
 *	Inserts the page "m" into the specified object at offset "pindex".
 *
 *	The page "mpred" must immediately precede the offset "pindex" within
 *	the specified object.
 *
 *	The object must be locked.
 */
static int
vm_page_insert_after(vm_page_t m, vm_object_t object, vm_pindex_t pindex,
    vm_page_t mpred)
{
	vm_page_t msucc;

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(m->object == NULL,
	    ("vm_page_insert_after: page already inserted"));
	if (mpred != NULL) {
		KASSERT(mpred->object == object,
		    ("vm_page_insert_after: object doesn't contain mpred"));
		KASSERT(mpred->pindex < pindex,
		    ("vm_page_insert_after: mpred doesn't precede pindex"));
		msucc = TAILQ_NEXT(mpred, listq);
	} else
		msucc = TAILQ_FIRST(&object->memq);
	if (msucc != NULL)
		KASSERT(msucc->pindex > pindex,
		    ("vm_page_insert_after: msucc doesn't succeed pindex"));

	/*
	 * Record the object/offset pair in this page.
	 */
	m->object = object;
	m->pindex = pindex;
	m->ref_count |= VPRC_OBJREF;

	/*
	 * Now link into the object's ordered list of backed pages.
	 */
	if (vm_radix_insert(&object->rtree, m)) {
		m->object = NULL;
		m->pindex = 0;
		m->ref_count &= ~VPRC_OBJREF;
		return (1);
	}
	vm_page_insert_radixdone(m, object, mpred);
	return (0);
}

/*
 *	vm_page_insert_radixdone:
 *
 *	Complete page "m" insertion into the specified object after the
 *	radix trie hooking.
 *
 *	The page "mpred" must precede the offset "m->pindex" within the
 *	specified object.
 *
 *	The object must be locked.
 */
static void
vm_page_insert_radixdone(vm_page_t m, vm_object_t object, vm_page_t mpred)
{

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(object != NULL && m->object == object,
	    ("vm_page_insert_radixdone: page %p has inconsistent object", m));
	KASSERT((m->ref_count & VPRC_OBJREF) != 0,
	    ("vm_page_insert_radixdone: page %p is missing object ref", m));
	if (mpred != NULL) {
		KASSERT(mpred->object == object,
		    ("vm_page_insert_radixdone: object doesn't contain mpred"));
		KASSERT(mpred->pindex < m->pindex,
		    ("vm_page_insert_radixdone: mpred doesn't precede pindex"));
	}

	if (mpred != NULL)
		TAILQ_INSERT_AFTER(&object->memq, mpred, m, listq);
	else
		TAILQ_INSERT_HEAD(&object->memq, m, listq);

	/*
	 * Show that the object has one more resident page.
	 */
	object->resident_page_count++;

	/*
	 * Hold the vnode until the last page is released.
	 */
	if (object->resident_page_count == 1 && object->type == OBJT_VNODE)
		vhold(object->handle);

	/*
	 * Since we are inserting a new and possibly dirty page,
	 * update the object's generation count.
	 */
	if (pmap_page_is_write_mapped(m))
		vm_object_set_writeable_dirty(object);
}

/*
 * Do the work to remove a page from its object.  The caller is responsible for
 * updating the page's fields to reflect this removal.
 */
static void
vm_page_object_remove(vm_page_t m)
{
	vm_object_t object;
	vm_page_t mrem;

	vm_page_assert_xbusied(m);
	object = m->object;
	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT((m->ref_count & VPRC_OBJREF) != 0,
	    ("page %p is missing its object ref", m));

	/* Deferred free of swap space. */
	if ((m->a.flags & PGA_SWAP_FREE) != 0)
		vm_pager_page_unswapped(m);

	m->object = NULL;
	mrem = vm_radix_remove(&object->rtree, m->pindex);
	KASSERT(mrem == m, ("removed page %p, expected page %p", mrem, m));

	/*
	 * Now remove from the object's list of backed pages.
	 */
	TAILQ_REMOVE(&object->memq, m, listq);

	/*
	 * And show that the object has one fewer resident page.
	 */
	object->resident_page_count--;

	/*
	 * The vnode may now be recycled.
	 */
	if (object->resident_page_count == 0 && object->type == OBJT_VNODE)
		vdrop(object->handle);
}

/*
 *	vm_page_remove:
 *
 *	Removes the specified page from its containing object, but does not
 *	invalidate any backing storage.  Returns true if the object's reference
 *	was the last reference to the page, and false otherwise.
 *
 *	The object must be locked and the page must be exclusively busied.
 *	The exclusive busy will be released on return.  If this is not the
 *	final ref and the caller does not hold a wire reference it may not
 *	continue to access the page.
 */
bool
vm_page_remove(vm_page_t m)
{
	bool dropped;

	dropped = vm_page_remove_xbusy(m);
	vm_page_xunbusy(m);

	return (dropped);
}

/*
 *	vm_page_remove_xbusy
 *
 *	Removes the page but leaves the xbusy held.  Returns true if this
 *	removed the final ref and false otherwise.
 */
bool
vm_page_remove_xbusy(vm_page_t m)
{

	vm_page_object_remove(m);
	return (vm_page_drop(m, VPRC_OBJREF) == VPRC_OBJREF);
}

/*
 *	vm_page_lookup:
 *
 *	Returns the page associated with the object/offset
 *	pair specified; if none is found, NULL is returned.
 *
 *	The object must be locked.
 */
vm_page_t
vm_page_lookup(vm_object_t object, vm_pindex_t pindex)
{

	VM_OBJECT_ASSERT_LOCKED(object);
	return (vm_radix_lookup(&object->rtree, pindex));
}

/*
 *	vm_page_lookup_unlocked:
 *
 *	Returns the page associated with the object/offset pair specified;
 *	if none is found, NULL is returned.  The page may be no longer be
 *	present in the object at the time that this function returns.  Only
 *	useful for opportunistic checks such as inmem().
 */
vm_page_t
vm_page_lookup_unlocked(vm_object_t object, vm_pindex_t pindex)
{

	return (vm_radix_lookup_unlocked(&object->rtree, pindex));
}

/*
 *	vm_page_relookup:
 *
 *	Returns a page that must already have been busied by
 *	the caller.  Used for bogus page replacement.
 */
vm_page_t
vm_page_relookup(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;

	m = vm_radix_lookup_unlocked(&object->rtree, pindex);
	KASSERT(m != NULL && (vm_page_busied(m) || vm_page_wired(m)) &&
	    m->object == object && m->pindex == pindex,
	    ("vm_page_relookup: Invalid page %p", m));
	return (m);
}

/*
 * This should only be used by lockless functions for releasing transient
 * incorrect acquires.  The page may have been freed after we acquired a
 * busy lock.  In this case busy_lock == VPB_FREED and we have nothing
 * further to do.
 */
static void
vm_page_busy_release(vm_page_t m)
{
	u_int x;

	x = vm_page_busy_fetch(m);
	for (;;) {
		if (x == VPB_FREED)
			break;
		if ((x & VPB_BIT_SHARED) != 0 && VPB_SHARERS(x) > 1) {
			if (atomic_fcmpset_int(&m->busy_lock, &x,
			    x - VPB_ONE_SHARER))
				break;
			continue;
		}
		KASSERT((x & VPB_BIT_SHARED) != 0 ||
		    (x & ~VPB_BIT_WAITERS) == VPB_CURTHREAD_EXCLUSIVE,
		    ("vm_page_busy_release: %p xbusy not owned.", m));
		if (!atomic_fcmpset_rel_int(&m->busy_lock, &x, VPB_UNBUSIED))
			continue;
		if ((x & VPB_BIT_WAITERS) != 0)
			wakeup(m);
		break;
	}
}

/*
 *	vm_page_find_least:
 *
 *	Returns the page associated with the object with least pindex
 *	greater than or equal to the parameter pindex, or NULL.
 *
 *	The object must be locked.
 */
vm_page_t
vm_page_find_least(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;

	VM_OBJECT_ASSERT_LOCKED(object);
	if ((m = TAILQ_FIRST(&object->memq)) != NULL && m->pindex < pindex)
		m = vm_radix_lookup_ge(&object->rtree, pindex);
	return (m);
}

/*
 * Returns the given page's successor (by pindex) within the object if it is
 * resident; if none is found, NULL is returned.
 *
 * The object must be locked.
 */
vm_page_t
vm_page_next(vm_page_t m)
{
	vm_page_t next;

	VM_OBJECT_ASSERT_LOCKED(m->object);
	if ((next = TAILQ_NEXT(m, listq)) != NULL) {
		MPASS(next->object == m->object);
		if (next->pindex != m->pindex + 1)
			next = NULL;
	}
	return (next);
}

/*
 * Returns the given page's predecessor (by pindex) within the object if it is
 * resident; if none is found, NULL is returned.
 *
 * The object must be locked.
 */
vm_page_t
vm_page_prev(vm_page_t m)
{
	vm_page_t prev;

	VM_OBJECT_ASSERT_LOCKED(m->object);
	if ((prev = TAILQ_PREV(m, pglist, listq)) != NULL) {
		MPASS(prev->object == m->object);
		if (prev->pindex != m->pindex - 1)
			prev = NULL;
	}
	return (prev);
}

/*
 * Uses the page mnew as a replacement for an existing page at index
 * pindex which must be already present in the object.
 *
 * Both pages must be exclusively busied on enter.  The old page is
 * unbusied on exit.
 *
 * A return value of true means mold is now free.  If this is not the
 * final ref and the caller does not hold a wire reference it may not
 * continue to access the page.
 */
static bool
vm_page_replace_hold(vm_page_t mnew, vm_object_t object, vm_pindex_t pindex,
    vm_page_t mold)
{
	vm_page_t mret;
	bool dropped;

	VM_OBJECT_ASSERT_WLOCKED(object);
	vm_page_assert_xbusied(mold);
	KASSERT(mnew->object == NULL && (mnew->ref_count & VPRC_OBJREF) == 0,
	    ("vm_page_replace: page %p already in object", mnew));

	/*
	 * This function mostly follows vm_page_insert() and
	 * vm_page_remove() without the radix, object count and vnode
	 * dance.  Double check such functions for more comments.
	 */

	mnew->object = object;
	mnew->pindex = pindex;
	atomic_set_int(&mnew->ref_count, VPRC_OBJREF);
	mret = vm_radix_replace(&object->rtree, mnew);
	KASSERT(mret == mold,
	    ("invalid page replacement, mold=%p, mret=%p", mold, mret));
	KASSERT((mold->oflags & VPO_UNMANAGED) ==
	    (mnew->oflags & VPO_UNMANAGED),
	    ("vm_page_replace: mismatched VPO_UNMANAGED"));

	/* Keep the resident page list in sorted order. */
	TAILQ_INSERT_AFTER(&object->memq, mold, mnew, listq);
	TAILQ_REMOVE(&object->memq, mold, listq);
	mold->object = NULL;

	/*
	 * The object's resident_page_count does not change because we have
	 * swapped one page for another, but the generation count should
	 * change if the page is dirty.
	 */
	if (pmap_page_is_write_mapped(mnew))
		vm_object_set_writeable_dirty(object);
	dropped = vm_page_drop(mold, VPRC_OBJREF) == VPRC_OBJREF;
	vm_page_xunbusy(mold);

	return (dropped);
}

void
vm_page_replace(vm_page_t mnew, vm_object_t object, vm_pindex_t pindex,
    vm_page_t mold)
{

	vm_page_assert_xbusied(mnew);

	if (vm_page_replace_hold(mnew, object, pindex, mold))
		vm_page_free(mold);
}

/*
 *	vm_page_rename:
 *
 *	Move the given memory entry from its
 *	current object to the specified target object/offset.
 *
 *	Note: swap associated with the page must be invalidated by the move.  We
 *	      have to do this for several reasons:  (1) we aren't freeing the
 *	      page, (2) we are dirtying the page, (3) the VM system is probably
 *	      moving the page from object A to B, and will then later move
 *	      the backing store from A to B and we can't have a conflict.
 *
 *	Note: we *always* dirty the page.  It is necessary both for the
 *	      fact that we moved it, and because we may be invalidating
 *	      swap.
 *
 *	The objects must be locked.
 */
int
vm_page_rename(vm_page_t m, vm_object_t new_object, vm_pindex_t new_pindex)
{
	vm_page_t mpred;
	vm_pindex_t opidx;

	VM_OBJECT_ASSERT_WLOCKED(new_object);

	KASSERT(m->ref_count != 0, ("vm_page_rename: page %p has no refs", m));
	mpred = vm_radix_lookup_le(&new_object->rtree, new_pindex);
	KASSERT(mpred == NULL || mpred->pindex != new_pindex,
	    ("vm_page_rename: pindex already renamed"));

	/*
	 * Create a custom version of vm_page_insert() which does not depend
	 * by m_prev and can cheat on the implementation aspects of the
	 * function.
	 */
	opidx = m->pindex;
	m->pindex = new_pindex;
	if (vm_radix_insert(&new_object->rtree, m)) {
		m->pindex = opidx;
		return (1);
	}

	/*
	 * The operation cannot fail anymore.  The removal must happen before
	 * the listq iterator is tainted.
	 */
	m->pindex = opidx;
	vm_page_object_remove(m);

	/* Return back to the new pindex to complete vm_page_insert(). */
	m->pindex = new_pindex;
	m->object = new_object;

	vm_page_insert_radixdone(m, new_object, mpred);
	vm_page_dirty(m);
	return (0);
}

/*
 *	vm_page_alloc:
 *
 *	Allocate and return a page that is associated with the specified
 *	object and offset pair.  By default, this page is exclusive busied.
 *
 *	The caller must always specify an allocation class.
 *
 *	allocation classes:
 *	VM_ALLOC_NORMAL		normal process request
 *	VM_ALLOC_SYSTEM		system *really* needs a page
 *	VM_ALLOC_INTERRUPT	interrupt time request
 *
 *	optional allocation flags:
 *	VM_ALLOC_COUNT(number)	the number of additional pages that the caller
 *				intends to allocate
 *	VM_ALLOC_NOBUSY		do not exclusive busy the page
 *	VM_ALLOC_NODUMP		do not include the page in a kernel core dump
 *	VM_ALLOC_NOOBJ		page is not associated with an object and
 *				should not be exclusive busy
 *	VM_ALLOC_SBUSY		shared busy the allocated page
 *	VM_ALLOC_WIRED		wire the allocated page
 *	VM_ALLOC_ZERO		prefer a zeroed page
 */
vm_page_t
vm_page_alloc(vm_object_t object, vm_pindex_t pindex, int req)
{

	return (vm_page_alloc_after(object, pindex, req, object != NULL ?
	    vm_radix_lookup_le(&object->rtree, pindex) : NULL));
}

vm_page_t
vm_page_alloc_domain(vm_object_t object, vm_pindex_t pindex, int domain,
    int req)
{

	return (vm_page_alloc_domain_after(object, pindex, domain, req,
	    object != NULL ? vm_radix_lookup_le(&object->rtree, pindex) :
	    NULL));
}

/*
 * Allocate a page in the specified object with the given page index.  To
 * optimize insertion of the page into the object, the caller must also specifiy
 * the resident page in the object with largest index smaller than the given
 * page index, or NULL if no such page exists.
 */
vm_page_t
vm_page_alloc_after(vm_object_t object, vm_pindex_t pindex,
    int req, vm_page_t mpred)
{
	struct vm_domainset_iter di;
	vm_page_t m;
	int domain;

	vm_domainset_iter_page_init(&di, object, pindex, &domain, &req);
	do {
		m = vm_page_alloc_domain_after(object, pindex, domain, req,
		    mpred);
		if (m != NULL)
			break;
	} while (vm_domainset_iter_page(&di, object, &domain) == 0);

	return (m);
}

/*
 * Returns true if the number of free pages exceeds the minimum
 * for the request class and false otherwise.
 */
static int
_vm_domain_allocate(struct vm_domain *vmd, int req_class, int npages)
{
	u_int limit, old, new;

	if (req_class == VM_ALLOC_INTERRUPT)
		limit = 0;
	else if (req_class == VM_ALLOC_SYSTEM)
		limit = vmd->vmd_interrupt_free_min;
	else
		limit = vmd->vmd_free_reserved;

	/*
	 * Attempt to reserve the pages.  Fail if we're below the limit.
	 */
	limit += npages;
	old = vmd->vmd_free_count;
	do {
		if (old < limit)
			return (0);
		new = old - npages;
	} while (atomic_fcmpset_int(&vmd->vmd_free_count, &old, new) == 0);

	/* Wake the page daemon if we've crossed the threshold. */
	if (vm_paging_needed(vmd, new) && !vm_paging_needed(vmd, old))
		pagedaemon_wakeup(vmd->vmd_domain);

	/* Only update bitsets on transitions. */
	if ((old >= vmd->vmd_free_min && new < vmd->vmd_free_min) ||
	    (old >= vmd->vmd_free_severe && new < vmd->vmd_free_severe))
		vm_domain_set(vmd);

	return (1);
}

int
vm_domain_allocate(struct vm_domain *vmd, int req, int npages)
{
	int req_class;

	/*
	 * The page daemon is allowed to dig deeper into the free page list.
	 */
	req_class = req & VM_ALLOC_CLASS_MASK;
	if (curproc == pageproc && req_class != VM_ALLOC_INTERRUPT)
		req_class = VM_ALLOC_SYSTEM;
	return (_vm_domain_allocate(vmd, req_class, npages));
}

vm_page_t
vm_page_alloc_domain_after(vm_object_t object, vm_pindex_t pindex, int domain,
    int req, vm_page_t mpred)
{
	struct vm_domain *vmd;
	vm_page_t m;
	int flags, pool;

	KASSERT((object != NULL) == ((req & VM_ALLOC_NOOBJ) == 0) &&
	    (object != NULL || (req & VM_ALLOC_SBUSY) == 0) &&
	    ((req & (VM_ALLOC_NOBUSY | VM_ALLOC_SBUSY)) !=
	    (VM_ALLOC_NOBUSY | VM_ALLOC_SBUSY)),
	    ("inconsistent object(%p)/req(%x)", object, req));
	KASSERT(object == NULL || (req & VM_ALLOC_WAITOK) == 0,
	    ("Can't sleep and retry object insertion."));
	KASSERT(mpred == NULL || mpred->pindex < pindex,
	    ("mpred %p doesn't precede pindex 0x%jx", mpred,
	    (uintmax_t)pindex));
	if (object != NULL)
		VM_OBJECT_ASSERT_WLOCKED(object);

	flags = 0;
	m = NULL;
	pool = object != NULL ? VM_FREEPOOL_DEFAULT : VM_FREEPOOL_DIRECT;
again:
#if VM_NRESERVLEVEL > 0
	/*
	 * Can we allocate the page from a reservation?
	 */
	if (vm_object_reserv(object) &&
	    (m = vm_reserv_alloc_page(object, pindex, domain, req, mpred)) !=
	    NULL) {
		goto found;
	}
#endif
	vmd = VM_DOMAIN(domain);
	if (vmd->vmd_pgcache[pool].zone != NULL) {
		m = uma_zalloc(vmd->vmd_pgcache[pool].zone, M_NOWAIT | M_NOVM);
		if (m != NULL) {
			flags |= PG_PCPU_CACHE;
			goto found;
		}
	}
	if (vm_domain_allocate(vmd, req, 1)) {
		/*
		 * If not, allocate it from the free page queues.
		 */
		vm_domain_free_lock(vmd);
		m = vm_phys_alloc_pages(domain, pool, 0);
		vm_domain_free_unlock(vmd);
		if (m == NULL) {
			vm_domain_freecnt_inc(vmd, 1);
#if VM_NRESERVLEVEL > 0
			if (vm_reserv_reclaim_inactive(domain))
				goto again;
#endif
		}
	}
	if (m == NULL) {
		/*
		 * Not allocatable, give up.
		 */
		if (vm_domain_alloc_fail(vmd, object, req))
			goto again;
		return (NULL);
	}

	/*
	 * At this point we had better have found a good page.
	 */
found:
	vm_page_dequeue(m);
	vm_page_alloc_check(m);

	/*
	 * Initialize the page.  Only the PG_ZERO flag is inherited.
	 */
	if ((req & VM_ALLOC_ZERO) != 0)
		flags |= (m->flags & PG_ZERO);
	if ((req & VM_ALLOC_NODUMP) != 0)
		flags |= PG_NODUMP;
	m->flags = flags;
	m->a.flags = 0;
	m->oflags = object == NULL || (object->flags & OBJ_UNMANAGED) != 0 ?
	    VPO_UNMANAGED : 0;
	if ((req & (VM_ALLOC_NOBUSY | VM_ALLOC_NOOBJ | VM_ALLOC_SBUSY)) == 0)
		m->busy_lock = VPB_CURTHREAD_EXCLUSIVE;
	else if ((req & VM_ALLOC_SBUSY) != 0)
		m->busy_lock = VPB_SHARERS_WORD(1);
	else
		m->busy_lock = VPB_UNBUSIED;
	if (req & VM_ALLOC_WIRED) {
		vm_wire_add(1);
		m->ref_count = 1;
	}
	m->a.act_count = 0;

	if (object != NULL) {
		if (vm_page_insert_after(m, object, pindex, mpred)) {
			if (req & VM_ALLOC_WIRED) {
				vm_wire_sub(1);
				m->ref_count = 0;
			}
			KASSERT(m->object == NULL, ("page %p has object", m));
			m->oflags = VPO_UNMANAGED;
			m->busy_lock = VPB_UNBUSIED;
			/* Don't change PG_ZERO. */
			vm_page_free_toq(m);
			if (req & VM_ALLOC_WAITFAIL) {
				VM_OBJECT_WUNLOCK(object);
				vm_radix_wait();
				VM_OBJECT_WLOCK(object);
			}
			return (NULL);
		}

		/* Ignore device objects; the pager sets "memattr" for them. */
		if (object->memattr != VM_MEMATTR_DEFAULT &&
		    (object->flags & OBJ_FICTITIOUS) == 0)
			pmap_page_set_memattr(m, object->memattr);
	} else
		m->pindex = pindex;

	return (m);
}

/*
 *	vm_page_alloc_contig:
 *
 *	Allocate a contiguous set of physical pages of the given size "npages"
 *	from the free lists.  All of the physical pages must be at or above
 *	the given physical address "low" and below the given physical address
 *	"high".  The given value "alignment" determines the alignment of the
 *	first physical page in the set.  If the given value "boundary" is
 *	non-zero, then the set of physical pages cannot cross any physical
 *	address boundary that is a multiple of that value.  Both "alignment"
 *	and "boundary" must be a power of two.
 *
 *	If the specified memory attribute, "memattr", is VM_MEMATTR_DEFAULT,
 *	then the memory attribute setting for the physical pages is configured
 *	to the object's memory attribute setting.  Otherwise, the memory
 *	attribute setting for the physical pages is configured to "memattr",
 *	overriding the object's memory attribute setting.  However, if the
 *	object's memory attribute setting is not VM_MEMATTR_DEFAULT, then the
 *	memory attribute setting for the physical pages cannot be configured
 *	to VM_MEMATTR_DEFAULT.
 *
 *	The specified object may not contain fictitious pages.
 *
 *	The caller must always specify an allocation class.
 *
 *	allocation classes:
 *	VM_ALLOC_NORMAL		normal process request
 *	VM_ALLOC_SYSTEM		system *really* needs a page
 *	VM_ALLOC_INTERRUPT	interrupt time request
 *
 *	optional allocation flags:
 *	VM_ALLOC_NOBUSY		do not exclusive busy the page
 *	VM_ALLOC_NODUMP		do not include the page in a kernel core dump
 *	VM_ALLOC_NOOBJ		page is not associated with an object and
 *				should not be exclusive busy
 *	VM_ALLOC_SBUSY		shared busy the allocated page
 *	VM_ALLOC_WIRED		wire the allocated page
 *	VM_ALLOC_ZERO		prefer a zeroed page
 */
vm_page_t
vm_page_alloc_contig(vm_object_t object, vm_pindex_t pindex, int req,
    u_long npages, vm_paddr_t low, vm_paddr_t high, u_long alignment,
    vm_paddr_t boundary, vm_memattr_t memattr)
{
	struct vm_domainset_iter di;
	vm_page_t m;
	int domain;

	vm_domainset_iter_page_init(&di, object, pindex, &domain, &req);
	do {
		m = vm_page_alloc_contig_domain(object, pindex, domain, req,
		    npages, low, high, alignment, boundary, memattr);
		if (m != NULL)
			break;
	} while (vm_domainset_iter_page(&di, object, &domain) == 0);

	return (m);
}

vm_page_t
vm_page_alloc_contig_domain(vm_object_t object, vm_pindex_t pindex, int domain,
    int req, u_long npages, vm_paddr_t low, vm_paddr_t high, u_long alignment,
    vm_paddr_t boundary, vm_memattr_t memattr)
{
	struct vm_domain *vmd;
	vm_page_t m, m_ret, mpred;
	u_int busy_lock, flags, oflags;

	mpred = NULL;	/* XXX: pacify gcc */
	KASSERT((object != NULL) == ((req & VM_ALLOC_NOOBJ) == 0) &&
	    (object != NULL || (req & VM_ALLOC_SBUSY) == 0) &&
	    ((req & (VM_ALLOC_NOBUSY | VM_ALLOC_SBUSY)) !=
	    (VM_ALLOC_NOBUSY | VM_ALLOC_SBUSY)),
	    ("vm_page_alloc_contig: inconsistent object(%p)/req(%x)", object,
	    req));
	KASSERT(object == NULL || (req & VM_ALLOC_WAITOK) == 0,
	    ("Can't sleep and retry object insertion."));
	if (object != NULL) {
		VM_OBJECT_ASSERT_WLOCKED(object);
		KASSERT((object->flags & OBJ_FICTITIOUS) == 0,
		    ("vm_page_alloc_contig: object %p has fictitious pages",
		    object));
	}
	KASSERT(npages > 0, ("vm_page_alloc_contig: npages is zero"));

	if (object != NULL) {
		mpred = vm_radix_lookup_le(&object->rtree, pindex);
		KASSERT(mpred == NULL || mpred->pindex != pindex,
		    ("vm_page_alloc_contig: pindex already allocated"));
	}

	/*
	 * Can we allocate the pages without the number of free pages falling
	 * below the lower bound for the allocation class?
	 */
	m_ret = NULL;
again:
#if VM_NRESERVLEVEL > 0
	/*
	 * Can we allocate the pages from a reservation?
	 */
	if (vm_object_reserv(object) &&
	    (m_ret = vm_reserv_alloc_contig(object, pindex, domain, req,
	    mpred, npages, low, high, alignment, boundary)) != NULL) {
		goto found;
	}
#endif
	vmd = VM_DOMAIN(domain);
	if (vm_domain_allocate(vmd, req, npages)) {
		/*
		 * allocate them from the free page queues.
		 */
		vm_domain_free_lock(vmd);
		m_ret = vm_phys_alloc_contig(domain, npages, low, high,
		    alignment, boundary);
		vm_domain_free_unlock(vmd);
		if (m_ret == NULL) {
			vm_domain_freecnt_inc(vmd, npages);
#if VM_NRESERVLEVEL > 0
			if (vm_reserv_reclaim_contig(domain, npages, low,
			    high, alignment, boundary))
				goto again;
#endif
		}
	}
	if (m_ret == NULL) {
		if (vm_domain_alloc_fail(vmd, object, req))
			goto again;
		return (NULL);
	}
#if VM_NRESERVLEVEL > 0
found:
#endif
	for (m = m_ret; m < &m_ret[npages]; m++) {
		vm_page_dequeue(m);
		vm_page_alloc_check(m);
	}

	/*
	 * Initialize the pages.  Only the PG_ZERO flag is inherited.
	 */
	flags = 0;
	if ((req & VM_ALLOC_ZERO) != 0)
		flags = PG_ZERO;
	if ((req & VM_ALLOC_NODUMP) != 0)
		flags |= PG_NODUMP;
	oflags = object == NULL || (object->flags & OBJ_UNMANAGED) != 0 ?
	    VPO_UNMANAGED : 0;
	if ((req & (VM_ALLOC_NOBUSY | VM_ALLOC_NOOBJ | VM_ALLOC_SBUSY)) == 0)
		busy_lock = VPB_CURTHREAD_EXCLUSIVE;
	else if ((req & VM_ALLOC_SBUSY) != 0)
		busy_lock = VPB_SHARERS_WORD(1);
	else
		busy_lock = VPB_UNBUSIED;
	if ((req & VM_ALLOC_WIRED) != 0)
		vm_wire_add(npages);
	if (object != NULL) {
		if (object->memattr != VM_MEMATTR_DEFAULT &&
		    memattr == VM_MEMATTR_DEFAULT)
			memattr = object->memattr;
	}
	for (m = m_ret; m < &m_ret[npages]; m++) {
		m->a.flags = 0;
		m->flags = (m->flags | PG_NODUMP) & flags;
		m->busy_lock = busy_lock;
		if ((req & VM_ALLOC_WIRED) != 0)
			m->ref_count = 1;
		m->a.act_count = 0;
		m->oflags = oflags;
		if (object != NULL) {
			if (vm_page_insert_after(m, object, pindex, mpred)) {
				if ((req & VM_ALLOC_WIRED) != 0)
					vm_wire_sub(npages);
				KASSERT(m->object == NULL,
				    ("page %p has object", m));
				mpred = m;
				for (m = m_ret; m < &m_ret[npages]; m++) {
					if (m <= mpred &&
					    (req & VM_ALLOC_WIRED) != 0)
						m->ref_count = 0;
					m->oflags = VPO_UNMANAGED;
					m->busy_lock = VPB_UNBUSIED;
					/* Don't change PG_ZERO. */
					vm_page_free_toq(m);
				}
				if (req & VM_ALLOC_WAITFAIL) {
					VM_OBJECT_WUNLOCK(object);
					vm_radix_wait();
					VM_OBJECT_WLOCK(object);
				}
				return (NULL);
			}
			mpred = m;
		} else
			m->pindex = pindex;
		if (memattr != VM_MEMATTR_DEFAULT)
			pmap_page_set_memattr(m, memattr);
		pindex++;
	}
	return (m_ret);
}

/*
 * Check a page that has been freshly dequeued from a freelist.
 */
static void
vm_page_alloc_check(vm_page_t m)
{

	KASSERT(m->object == NULL, ("page %p has object", m));
	KASSERT(m->a.queue == PQ_NONE &&
	    (m->a.flags & PGA_QUEUE_STATE_MASK) == 0,
	    ("page %p has unexpected queue %d, flags %#x",
	    m, m->a.queue, (m->a.flags & PGA_QUEUE_STATE_MASK)));
	KASSERT(m->ref_count == 0, ("page %p has references", m));
	KASSERT(vm_page_busy_freed(m), ("page %p is not freed", m));
	KASSERT(m->dirty == 0, ("page %p is dirty", m));
	KASSERT(pmap_page_get_memattr(m) == VM_MEMATTR_DEFAULT,
	    ("page %p has unexpected memattr %d",
	    m, pmap_page_get_memattr(m)));
	KASSERT(m->valid == 0, ("free page %p is valid", m));
	pmap_vm_page_alloc_check(m);
}

/*
 * 	vm_page_alloc_freelist:
 *
 *	Allocate a physical page from the specified free page list.
 *
 *	The caller must always specify an allocation class.
 *
 *	allocation classes:
 *	VM_ALLOC_NORMAL		normal process request
 *	VM_ALLOC_SYSTEM		system *really* needs a page
 *	VM_ALLOC_INTERRUPT	interrupt time request
 *
 *	optional allocation flags:
 *	VM_ALLOC_COUNT(number)	the number of additional pages that the caller
 *				intends to allocate
 *	VM_ALLOC_WIRED		wire the allocated page
 *	VM_ALLOC_ZERO		prefer a zeroed page
 */
vm_page_t
vm_page_alloc_freelist(int freelist, int req)
{
	struct vm_domainset_iter di;
	vm_page_t m;
	int domain;

	vm_domainset_iter_page_init(&di, NULL, 0, &domain, &req);
	do {
		m = vm_page_alloc_freelist_domain(domain, freelist, req);
		if (m != NULL)
			break;
	} while (vm_domainset_iter_page(&di, NULL, &domain) == 0);

	return (m);
}

vm_page_t
vm_page_alloc_freelist_domain(int domain, int freelist, int req)
{
	struct vm_domain *vmd;
	vm_page_t m;
	u_int flags;

	m = NULL;
	vmd = VM_DOMAIN(domain);
again:
	if (vm_domain_allocate(vmd, req, 1)) {
		vm_domain_free_lock(vmd);
		m = vm_phys_alloc_freelist_pages(domain, freelist,
		    VM_FREEPOOL_DIRECT, 0);
		vm_domain_free_unlock(vmd);
		if (m == NULL)
			vm_domain_freecnt_inc(vmd, 1);
	}
	if (m == NULL) {
		if (vm_domain_alloc_fail(vmd, NULL, req))
			goto again;
		return (NULL);
	}
	vm_page_dequeue(m);
	vm_page_alloc_check(m);

	/*
	 * Initialize the page.  Only the PG_ZERO flag is inherited.
	 */
	m->a.flags = 0;
	flags = 0;
	if ((req & VM_ALLOC_ZERO) != 0)
		flags = PG_ZERO;
	m->flags &= flags;
	if ((req & VM_ALLOC_WIRED) != 0) {
		vm_wire_add(1);
		m->ref_count = 1;
	}
	/* Unmanaged pages don't use "act_count". */
	m->oflags = VPO_UNMANAGED;
	return (m);
}

static int
vm_page_zone_import(void *arg, void **store, int cnt, int domain, int flags)
{
	struct vm_domain *vmd;
	struct vm_pgcache *pgcache;
	int i;

	pgcache = arg;
	vmd = VM_DOMAIN(pgcache->domain);

	/*
	 * The page daemon should avoid creating extra memory pressure since its
	 * main purpose is to replenish the store of free pages.
	 */
	if (vmd->vmd_severeset || curproc == pageproc ||
	    !_vm_domain_allocate(vmd, VM_ALLOC_NORMAL, cnt))
		return (0);
	domain = vmd->vmd_domain;
	vm_domain_free_lock(vmd);
	i = vm_phys_alloc_npages(domain, pgcache->pool, cnt,
	    (vm_page_t *)store);
	vm_domain_free_unlock(vmd);
	if (cnt != i)
		vm_domain_freecnt_inc(vmd, cnt - i);

	return (i);
}

static void
vm_page_zone_release(void *arg, void **store, int cnt)
{
	struct vm_domain *vmd;
	struct vm_pgcache *pgcache;
	vm_page_t m;
	int i;

	pgcache = arg;
	vmd = VM_DOMAIN(pgcache->domain);
	vm_domain_free_lock(vmd);
	for (i = 0; i < cnt; i++) {
		m = (vm_page_t)store[i];
		vm_phys_free_pages(m, 0);
	}
	vm_domain_free_unlock(vmd);
	vm_domain_freecnt_inc(vmd, cnt);
}

#define	VPSC_ANY	0	/* No restrictions. */
#define	VPSC_NORESERV	1	/* Skip reservations; implies VPSC_NOSUPER. */
#define	VPSC_NOSUPER	2	/* Skip superpages. */

/*
 *	vm_page_scan_contig:
 *
 *	Scan vm_page_array[] between the specified entries "m_start" and
 *	"m_end" for a run of contiguous physical pages that satisfy the
 *	specified conditions, and return the lowest page in the run.  The
 *	specified "alignment" determines the alignment of the lowest physical
 *	page in the run.  If the specified "boundary" is non-zero, then the
 *	run of physical pages cannot span a physical address that is a
 *	multiple of "boundary".
 *
 *	"m_end" is never dereferenced, so it need not point to a vm_page
 *	structure within vm_page_array[].
 *
 *	"npages" must be greater than zero.  "m_start" and "m_end" must not
 *	span a hole (or discontiguity) in the physical address space.  Both
 *	"alignment" and "boundary" must be a power of two.
 */
vm_page_t
vm_page_scan_contig(u_long npages, vm_page_t m_start, vm_page_t m_end,
    u_long alignment, vm_paddr_t boundary, int options)
{
	vm_object_t object;
	vm_paddr_t pa;
	vm_page_t m, m_run;
#if VM_NRESERVLEVEL > 0
	int level;
#endif
	int m_inc, order, run_ext, run_len;

	KASSERT(npages > 0, ("npages is 0"));
	KASSERT(powerof2(alignment), ("alignment is not a power of 2"));
	KASSERT(powerof2(boundary), ("boundary is not a power of 2"));
	m_run = NULL;
	run_len = 0;
	for (m = m_start; m < m_end && run_len < npages; m += m_inc) {
		KASSERT((m->flags & PG_MARKER) == 0,
		    ("page %p is PG_MARKER", m));
		KASSERT((m->flags & PG_FICTITIOUS) == 0 || m->ref_count >= 1,
		    ("fictitious page %p has invalid ref count", m));

		/*
		 * If the current page would be the start of a run, check its
		 * physical address against the end, alignment, and boundary
		 * conditions.  If it doesn't satisfy these conditions, either
		 * terminate the scan or advance to the next page that
		 * satisfies the failed condition.
		 */
		if (run_len == 0) {
			KASSERT(m_run == NULL, ("m_run != NULL"));
			if (m + npages > m_end)
				break;
			pa = VM_PAGE_TO_PHYS(m);
			if ((pa & (alignment - 1)) != 0) {
				m_inc = atop(roundup2(pa, alignment) - pa);
				continue;
			}
			if (rounddown2(pa ^ (pa + ptoa(npages) - 1),
			    boundary) != 0) {
				m_inc = atop(roundup2(pa, boundary) - pa);
				continue;
			}
		} else
			KASSERT(m_run != NULL, ("m_run == NULL"));

retry:
		m_inc = 1;
		if (vm_page_wired(m))
			run_ext = 0;
#if VM_NRESERVLEVEL > 0
		else if ((level = vm_reserv_level(m)) >= 0 &&
		    (options & VPSC_NORESERV) != 0) {
			run_ext = 0;
			/* Advance to the end of the reservation. */
			pa = VM_PAGE_TO_PHYS(m);
			m_inc = atop(roundup2(pa + 1, vm_reserv_size(level)) -
			    pa);
		}
#endif
		else if ((object = atomic_load_ptr(&m->object)) != NULL) {
			/*
			 * The page is considered eligible for relocation if
			 * and only if it could be laundered or reclaimed by
			 * the page daemon.
			 */
			VM_OBJECT_RLOCK(object);
			if (object != m->object) {
				VM_OBJECT_RUNLOCK(object);
				goto retry;
			}
			/* Don't care: PG_NODUMP, PG_ZERO. */
			if (object->type != OBJT_DEFAULT &&
			    (object->flags & OBJ_SWAP) == 0 &&
			    object->type != OBJT_VNODE) {
				run_ext = 0;
#if VM_NRESERVLEVEL > 0
			} else if ((options & VPSC_NOSUPER) != 0 &&
			    (level = vm_reserv_level_iffullpop(m)) >= 0) {
				run_ext = 0;
				/* Advance to the end of the superpage. */
				pa = VM_PAGE_TO_PHYS(m);
				m_inc = atop(roundup2(pa + 1,
				    vm_reserv_size(level)) - pa);
#endif
			} else if (object->memattr == VM_MEMATTR_DEFAULT &&
			    vm_page_queue(m) != PQ_NONE && !vm_page_busied(m)) {
				/*
				 * The page is allocated but eligible for
				 * relocation.  Extend the current run by one
				 * page.
				 */
				KASSERT(pmap_page_get_memattr(m) ==
				    VM_MEMATTR_DEFAULT,
				    ("page %p has an unexpected memattr", m));
				KASSERT((m->oflags & (VPO_SWAPINPROG |
				    VPO_SWAPSLEEP | VPO_UNMANAGED)) == 0,
				    ("page %p has unexpected oflags", m));
				/* Don't care: PGA_NOSYNC. */
				run_ext = 1;
			} else
				run_ext = 0;
			VM_OBJECT_RUNLOCK(object);
#if VM_NRESERVLEVEL > 0
		} else if (level >= 0) {
			/*
			 * The page is reserved but not yet allocated.  In
			 * other words, it is still free.  Extend the current
			 * run by one page.
			 */
			run_ext = 1;
#endif
		} else if ((order = m->order) < VM_NFREEORDER) {
			/*
			 * The page is enqueued in the physical memory
			 * allocator's free page queues.  Moreover, it is the
			 * first page in a power-of-two-sized run of
			 * contiguous free pages.  Add these pages to the end
			 * of the current run, and jump ahead.
			 */
			run_ext = 1 << order;
			m_inc = 1 << order;
		} else {
			/*
			 * Skip the page for one of the following reasons: (1)
			 * It is enqueued in the physical memory allocator's
			 * free page queues.  However, it is not the first
			 * page in a run of contiguous free pages.  (This case
			 * rarely occurs because the scan is performed in
			 * ascending order.) (2) It is not reserved, and it is
			 * transitioning from free to allocated.  (Conversely,
			 * the transition from allocated to free for managed
			 * pages is blocked by the page busy lock.) (3) It is
			 * allocated but not contained by an object and not
			 * wired, e.g., allocated by Xen's balloon driver.
			 */
			run_ext = 0;
		}

		/*
		 * Extend or reset the current run of pages.
		 */
		if (run_ext > 0) {
			if (run_len == 0)
				m_run = m;
			run_len += run_ext;
		} else {
			if (run_len > 0) {
				m_run = NULL;
				run_len = 0;
			}
		}
	}
	if (run_len >= npages)
		return (m_run);
	return (NULL);
}

/*
 *	vm_page_reclaim_run:
 *
 *	Try to relocate each of the allocated virtual pages within the
 *	specified run of physical pages to a new physical address.  Free the
 *	physical pages underlying the relocated virtual pages.  A virtual page
 *	is relocatable if and only if it could be laundered or reclaimed by
 *	the page daemon.  Whenever possible, a virtual page is relocated to a
 *	physical address above "high".
 *
 *	Returns 0 if every physical page within the run was already free or
 *	just freed by a successful relocation.  Otherwise, returns a non-zero
 *	value indicating why the last attempt to relocate a virtual page was
 *	unsuccessful.
 *
 *	"req_class" must be an allocation class.
 */
static int
vm_page_reclaim_run(int req_class, int domain, u_long npages, vm_page_t m_run,
    vm_paddr_t high)
{
	struct vm_domain *vmd;
	struct spglist free;
	vm_object_t object;
	vm_paddr_t pa;
	vm_page_t m, m_end, m_new;
	int error, order, req;

	KASSERT((req_class & VM_ALLOC_CLASS_MASK) == req_class,
	    ("req_class is not an allocation class"));
	SLIST_INIT(&free);
	error = 0;
	m = m_run;
	m_end = m_run + npages;
	for (; error == 0 && m < m_end; m++) {
		KASSERT((m->flags & (PG_FICTITIOUS | PG_MARKER)) == 0,
		    ("page %p is PG_FICTITIOUS or PG_MARKER", m));

		/*
		 * Racily check for wirings.  Races are handled once the object
		 * lock is held and the page is unmapped.
		 */
		if (vm_page_wired(m))
			error = EBUSY;
		else if ((object = atomic_load_ptr(&m->object)) != NULL) {
			/*
			 * The page is relocated if and only if it could be
			 * laundered or reclaimed by the page daemon.
			 */
			VM_OBJECT_WLOCK(object);
			/* Don't care: PG_NODUMP, PG_ZERO. */
			if (m->object != object ||
			    (object->type != OBJT_DEFAULT &&
			    (object->flags & OBJ_SWAP) == 0 &&
			    object->type != OBJT_VNODE))
				error = EINVAL;
			else if (object->memattr != VM_MEMATTR_DEFAULT)
				error = EINVAL;
			else if (vm_page_queue(m) != PQ_NONE &&
			    vm_page_tryxbusy(m) != 0) {
				if (vm_page_wired(m)) {
					vm_page_xunbusy(m);
					error = EBUSY;
					goto unlock;
				}
				KASSERT(pmap_page_get_memattr(m) ==
				    VM_MEMATTR_DEFAULT,
				    ("page %p has an unexpected memattr", m));
				KASSERT(m->oflags == 0,
				    ("page %p has unexpected oflags", m));
				/* Don't care: PGA_NOSYNC. */
				if (!vm_page_none_valid(m)) {
					/*
					 * First, try to allocate a new page
					 * that is above "high".  Failing
					 * that, try to allocate a new page
					 * that is below "m_run".  Allocate
					 * the new page between the end of
					 * "m_run" and "high" only as a last
					 * resort.
					 */
					req = req_class | VM_ALLOC_NOOBJ;
					if ((m->flags & PG_NODUMP) != 0)
						req |= VM_ALLOC_NODUMP;
					if (trunc_page(high) !=
					    ~(vm_paddr_t)PAGE_MASK) {
						m_new = vm_page_alloc_contig(
						    NULL, 0, req, 1,
						    round_page(high),
						    ~(vm_paddr_t)0,
						    PAGE_SIZE, 0,
						    VM_MEMATTR_DEFAULT);
					} else
						m_new = NULL;
					if (m_new == NULL) {
						pa = VM_PAGE_TO_PHYS(m_run);
						m_new = vm_page_alloc_contig(
						    NULL, 0, req, 1,
						    0, pa - 1, PAGE_SIZE, 0,
						    VM_MEMATTR_DEFAULT);
					}
					if (m_new == NULL) {
						pa += ptoa(npages);
						m_new = vm_page_alloc_contig(
						    NULL, 0, req, 1,
						    pa, high, PAGE_SIZE, 0,
						    VM_MEMATTR_DEFAULT);
					}
					if (m_new == NULL) {
						vm_page_xunbusy(m);
						error = ENOMEM;
						goto unlock;
					}

					/*
					 * Unmap the page and check for new
					 * wirings that may have been acquired
					 * through a pmap lookup.
					 */
					if (object->ref_count != 0 &&
					    !vm_page_try_remove_all(m)) {
						vm_page_xunbusy(m);
						vm_page_free(m_new);
						error = EBUSY;
						goto unlock;
					}

					/*
					 * Replace "m" with the new page.  For
					 * vm_page_replace(), "m" must be busy
					 * and dequeued.  Finally, change "m"
					 * as if vm_page_free() was called.
					 */
					m_new->a.flags = m->a.flags &
					    ~PGA_QUEUE_STATE_MASK;
					KASSERT(m_new->oflags == VPO_UNMANAGED,
					    ("page %p is managed", m_new));
					m_new->oflags = 0;
					pmap_copy_page(m, m_new);
					m_new->valid = m->valid;
					m_new->dirty = m->dirty;
					m->flags &= ~PG_ZERO;
					vm_page_dequeue(m);
					if (vm_page_replace_hold(m_new, object,
					    m->pindex, m) &&
					    vm_page_free_prep(m))
						SLIST_INSERT_HEAD(&free, m,
						    plinks.s.ss);

					/*
					 * The new page must be deactivated
					 * before the object is unlocked.
					 */
					vm_page_deactivate(m_new);
				} else {
					m->flags &= ~PG_ZERO;
					vm_page_dequeue(m);
					if (vm_page_free_prep(m))
						SLIST_INSERT_HEAD(&free, m,
						    plinks.s.ss);
					KASSERT(m->dirty == 0,
					    ("page %p is dirty", m));
				}
			} else
				error = EBUSY;
unlock:
			VM_OBJECT_WUNLOCK(object);
		} else {
			MPASS(vm_page_domain(m) == domain);
			vmd = VM_DOMAIN(domain);
			vm_domain_free_lock(vmd);
			order = m->order;
			if (order < VM_NFREEORDER) {
				/*
				 * The page is enqueued in the physical memory
				 * allocator's free page queues.  Moreover, it
				 * is the first page in a power-of-two-sized
				 * run of contiguous free pages.  Jump ahead
				 * to the last page within that run, and
				 * continue from there.
				 */
				m += (1 << order) - 1;
			}
#if VM_NRESERVLEVEL > 0
			else if (vm_reserv_is_page_free(m))
				order = 0;
#endif
			vm_domain_free_unlock(vmd);
			if (order == VM_NFREEORDER)
				error = EINVAL;
		}
	}
	if ((m = SLIST_FIRST(&free)) != NULL) {
		int cnt;

		vmd = VM_DOMAIN(domain);
		cnt = 0;
		vm_domain_free_lock(vmd);
		do {
			MPASS(vm_page_domain(m) == domain);
			SLIST_REMOVE_HEAD(&free, plinks.s.ss);
			vm_phys_free_pages(m, 0);
			cnt++;
		} while ((m = SLIST_FIRST(&free)) != NULL);
		vm_domain_free_unlock(vmd);
		vm_domain_freecnt_inc(vmd, cnt);
	}
	return (error);
}

#define	NRUNS	16

CTASSERT(powerof2(NRUNS));

#define	RUN_INDEX(count)	((count) & (NRUNS - 1))

#define	MIN_RECLAIM	8

/*
 *	vm_page_reclaim_contig:
 *
 *	Reclaim allocated, contiguous physical memory satisfying the specified
 *	conditions by relocating the virtual pages using that physical memory.
 *	Returns true if reclamation is successful and false otherwise.  Since
 *	relocation requires the allocation of physical pages, reclamation may
 *	fail due to a shortage of free pages.  When reclamation fails, callers
 *	are expected to perform vm_wait() before retrying a failed allocation
 *	operation, e.g., vm_page_alloc_contig().
 *
 *	The caller must always specify an allocation class through "req".
 *
 *	allocation classes:
 *	VM_ALLOC_NORMAL		normal process request
 *	VM_ALLOC_SYSTEM		system *really* needs a page
 *	VM_ALLOC_INTERRUPT	interrupt time request
 *
 *	The optional allocation flags are ignored.
 *
 *	"npages" must be greater than zero.  Both "alignment" and "boundary"
 *	must be a power of two.
 */
bool
vm_page_reclaim_contig_domain(int domain, int req, u_long npages,
    vm_paddr_t low, vm_paddr_t high, u_long alignment, vm_paddr_t boundary)
{
	struct vm_domain *vmd;
	vm_paddr_t curr_low;
	vm_page_t m_run, m_runs[NRUNS];
	u_long count, minalign, reclaimed;
	int error, i, options, req_class;

	KASSERT(npages > 0, ("npages is 0"));
	KASSERT(powerof2(alignment), ("alignment is not a power of 2"));
	KASSERT(powerof2(boundary), ("boundary is not a power of 2"));

	/*
	 * The caller will attempt an allocation after some runs have been
	 * reclaimed and added to the vm_phys buddy lists.  Due to limitations
	 * of vm_phys_alloc_contig(), round up the requested length to the next
	 * power of two or maximum chunk size, and ensure that each run is
	 * suitably aligned.
	 */
	minalign = 1ul << imin(flsl(npages - 1), VM_NFREEORDER - 1);
	npages = roundup2(npages, minalign);
	if (alignment < ptoa(minalign))
		alignment = ptoa(minalign);

	/*
	 * The page daemon is allowed to dig deeper into the free page list.
	 */
	req_class = req & VM_ALLOC_CLASS_MASK;
	if (curproc == pageproc && req_class != VM_ALLOC_INTERRUPT)
		req_class = VM_ALLOC_SYSTEM;

	/*
	 * Return if the number of free pages cannot satisfy the requested
	 * allocation.
	 */
	vmd = VM_DOMAIN(domain);
	count = vmd->vmd_free_count;
	if (count < npages + vmd->vmd_free_reserved || (count < npages +
	    vmd->vmd_interrupt_free_min && req_class == VM_ALLOC_SYSTEM) ||
	    (count < npages && req_class == VM_ALLOC_INTERRUPT))
		return (false);

	/*
	 * Scan up to three times, relaxing the restrictions ("options") on
	 * the reclamation of reservations and superpages each time.
	 */
	for (options = VPSC_NORESERV;;) {
		/*
		 * Find the highest runs that satisfy the given constraints
		 * and restrictions, and record them in "m_runs".
		 */
		curr_low = low;
		count = 0;
		for (;;) {
			m_run = vm_phys_scan_contig(domain, npages, curr_low,
			    high, alignment, boundary, options);
			if (m_run == NULL)
				break;
			curr_low = VM_PAGE_TO_PHYS(m_run) + ptoa(npages);
			m_runs[RUN_INDEX(count)] = m_run;
			count++;
		}

		/*
		 * Reclaim the highest runs in LIFO (descending) order until
		 * the number of reclaimed pages, "reclaimed", is at least
		 * MIN_RECLAIM.  Reset "reclaimed" each time because each
		 * reclamation is idempotent, and runs will (likely) recur
		 * from one scan to the next as restrictions are relaxed.
		 */
		reclaimed = 0;
		for (i = 0; count > 0 && i < NRUNS; i++) {
			count--;
			m_run = m_runs[RUN_INDEX(count)];
			error = vm_page_reclaim_run(req_class, domain, npages,
			    m_run, high);
			if (error == 0) {
				reclaimed += npages;
				if (reclaimed >= MIN_RECLAIM)
					return (true);
			}
		}

		/*
		 * Either relax the restrictions on the next scan or return if
		 * the last scan had no restrictions.
		 */
		if (options == VPSC_NORESERV)
			options = VPSC_NOSUPER;
		else if (options == VPSC_NOSUPER)
			options = VPSC_ANY;
		else if (options == VPSC_ANY)
			return (reclaimed != 0);
	}
}

bool
vm_page_reclaim_contig(int req, u_long npages, vm_paddr_t low, vm_paddr_t high,
    u_long alignment, vm_paddr_t boundary)
{
	struct vm_domainset_iter di;
	int domain;
	bool ret;

	vm_domainset_iter_page_init(&di, NULL, 0, &domain, &req);
	do {
		ret = vm_page_reclaim_contig_domain(domain, req, npages, low,
		    high, alignment, boundary);
		if (ret)
			break;
	} while (vm_domainset_iter_page(&di, NULL, &domain) == 0);

	return (ret);
}

/*
 * Set the domain in the appropriate page level domainset.
 */
void
vm_domain_set(struct vm_domain *vmd)
{

	mtx_lock(&vm_domainset_lock);
	if (!vmd->vmd_minset && vm_paging_min(vmd)) {
		vmd->vmd_minset = 1;
		DOMAINSET_SET(vmd->vmd_domain, &vm_min_domains);
	}
	if (!vmd->vmd_severeset && vm_paging_severe(vmd)) {
		vmd->vmd_severeset = 1;
		DOMAINSET_SET(vmd->vmd_domain, &vm_severe_domains);
	}
	mtx_unlock(&vm_domainset_lock);
}

/*
 * Clear the domain from the appropriate page level domainset.
 */
void
vm_domain_clear(struct vm_domain *vmd)
{

	mtx_lock(&vm_domainset_lock);
	if (vmd->vmd_minset && !vm_paging_min(vmd)) {
		vmd->vmd_minset = 0;
		DOMAINSET_CLR(vmd->vmd_domain, &vm_min_domains);
		if (vm_min_waiters != 0) {
			vm_min_waiters = 0;
			wakeup(&vm_min_domains);
		}
	}
	if (vmd->vmd_severeset && !vm_paging_severe(vmd)) {
		vmd->vmd_severeset = 0;
		DOMAINSET_CLR(vmd->vmd_domain, &vm_severe_domains);
		if (vm_severe_waiters != 0) {
			vm_severe_waiters = 0;
			wakeup(&vm_severe_domains);
		}
	}

	/*
	 * If pageout daemon needs pages, then tell it that there are
	 * some free.
	 */
	if (vmd->vmd_pageout_pages_needed &&
	    vmd->vmd_free_count >= vmd->vmd_pageout_free_min) {
		wakeup(&vmd->vmd_pageout_pages_needed);
		vmd->vmd_pageout_pages_needed = 0;
	}

	/* See comments in vm_wait_doms(). */
	if (vm_pageproc_waiters) {
		vm_pageproc_waiters = 0;
		wakeup(&vm_pageproc_waiters);
	}
	mtx_unlock(&vm_domainset_lock);
}

/*
 * Wait for free pages to exceed the min threshold globally.
 */
void
vm_wait_min(void)
{

	mtx_lock(&vm_domainset_lock);
	while (vm_page_count_min()) {
		vm_min_waiters++;
		msleep(&vm_min_domains, &vm_domainset_lock, PVM, "vmwait", 0);
	}
	mtx_unlock(&vm_domainset_lock);
}

/*
 * Wait for free pages to exceed the severe threshold globally.
 */
void
vm_wait_severe(void)
{

	mtx_lock(&vm_domainset_lock);
	while (vm_page_count_severe()) {
		vm_severe_waiters++;
		msleep(&vm_severe_domains, &vm_domainset_lock, PVM,
		    "vmwait", 0);
	}
	mtx_unlock(&vm_domainset_lock);
}

u_int
vm_wait_count(void)
{

	return (vm_severe_waiters + vm_min_waiters + vm_pageproc_waiters);
}

int
vm_wait_doms(const domainset_t *wdoms, int mflags)
{
	int error;

	error = 0;

	/*
	 * We use racey wakeup synchronization to avoid expensive global
	 * locking for the pageproc when sleeping with a non-specific vm_wait.
	 * To handle this, we only sleep for one tick in this instance.  It
	 * is expected that most allocations for the pageproc will come from
	 * kmem or vm_page_grab* which will use the more specific and
	 * race-free vm_wait_domain().
	 */
	if (curproc == pageproc) {
		mtx_lock(&vm_domainset_lock);
		vm_pageproc_waiters++;
		error = msleep(&vm_pageproc_waiters, &vm_domainset_lock,
		    PVM | PDROP | mflags, "pageprocwait", 1);
	} else {
		/*
		 * XXX Ideally we would wait only until the allocation could
		 * be satisfied.  This condition can cause new allocators to
		 * consume all freed pages while old allocators wait.
		 */
		mtx_lock(&vm_domainset_lock);
		if (vm_page_count_min_set(wdoms)) {
			vm_min_waiters++;
			error = msleep(&vm_min_domains, &vm_domainset_lock,
			    PVM | PDROP | mflags, "vmwait", 0);
		} else
			mtx_unlock(&vm_domainset_lock);
	}
	return (error);
}

/*
 *	vm_wait_domain:
 *
 *	Sleep until free pages are available for allocation.
 *	- Called in various places after failed memory allocations.
 */
void
vm_wait_domain(int domain)
{
	struct vm_domain *vmd;
	domainset_t wdom;

	vmd = VM_DOMAIN(domain);
	vm_domain_free_assert_unlocked(vmd);

	if (curproc == pageproc) {
		mtx_lock(&vm_domainset_lock);
		if (vmd->vmd_free_count < vmd->vmd_pageout_free_min) {
			vmd->vmd_pageout_pages_needed = 1;
			msleep(&vmd->vmd_pageout_pages_needed,
			    &vm_domainset_lock, PDROP | PSWP, "VMWait", 0);
		} else
			mtx_unlock(&vm_domainset_lock);
	} else {
		if (pageproc == NULL)
			panic("vm_wait in early boot");
		DOMAINSET_ZERO(&wdom);
		DOMAINSET_SET(vmd->vmd_domain, &wdom);
		vm_wait_doms(&wdom, 0);
	}
}

static int
vm_wait_flags(vm_object_t obj, int mflags)
{
	struct domainset *d;

	d = NULL;

	/*
	 * Carefully fetch pointers only once: the struct domainset
	 * itself is ummutable but the pointer might change.
	 */
	if (obj != NULL)
		d = obj->domain.dr_policy;
	if (d == NULL)
		d = curthread->td_domain.dr_policy;

	return (vm_wait_doms(&d->ds_mask, mflags));
}

/*
 *	vm_wait:
 *
 *	Sleep until free pages are available for allocation in the
 *	affinity domains of the obj.  If obj is NULL, the domain set
 *	for the calling thread is used.
 *	Called in various places after failed memory allocations.
 */
void
vm_wait(vm_object_t obj)
{
	(void)vm_wait_flags(obj, 0);
}

int
vm_wait_intr(vm_object_t obj)
{
	return (vm_wait_flags(obj, PCATCH));
}

/*
 *	vm_domain_alloc_fail:
 *
 *	Called when a page allocation function fails.  Informs the
 *	pagedaemon and performs the requested wait.  Requires the
 *	domain_free and object lock on entry.  Returns with the
 *	object lock held and free lock released.  Returns an error when
 *	retry is necessary.
 *
 */
static int
vm_domain_alloc_fail(struct vm_domain *vmd, vm_object_t object, int req)
{

	vm_domain_free_assert_unlocked(vmd);

	atomic_add_int(&vmd->vmd_pageout_deficit,
	    max((u_int)req >> VM_ALLOC_COUNT_SHIFT, 1));
	if (req & (VM_ALLOC_WAITOK | VM_ALLOC_WAITFAIL)) {
		if (object != NULL) 
			VM_OBJECT_WUNLOCK(object);
		vm_wait_domain(vmd->vmd_domain);
		if (object != NULL) 
			VM_OBJECT_WLOCK(object);
		if (req & VM_ALLOC_WAITOK)
			return (EAGAIN);
	}

	return (0);
}

/*
 *	vm_waitpfault:
 *
 *	Sleep until free pages are available for allocation.
 *	- Called only in vm_fault so that processes page faulting
 *	  can be easily tracked.
 *	- Sleeps at a lower priority than vm_wait() so that vm_wait()ing
 *	  processes will be able to grab memory first.  Do not change
 *	  this balance without careful testing first.
 */
void
vm_waitpfault(struct domainset *dset, int timo)
{

	/*
	 * XXX Ideally we would wait only until the allocation could
	 * be satisfied.  This condition can cause new allocators to
	 * consume all freed pages while old allocators wait.
	 */
	mtx_lock(&vm_domainset_lock);
	if (vm_page_count_min_set(&dset->ds_mask)) {
		vm_min_waiters++;
		msleep(&vm_min_domains, &vm_domainset_lock, PUSER | PDROP,
		    "pfault", timo);
	} else
		mtx_unlock(&vm_domainset_lock);
}

static struct vm_pagequeue *
_vm_page_pagequeue(vm_page_t m, uint8_t queue)
{

	return (&vm_pagequeue_domain(m)->vmd_pagequeues[queue]);
}

#ifdef INVARIANTS
static struct vm_pagequeue *
vm_page_pagequeue(vm_page_t m)
{

	return (_vm_page_pagequeue(m, vm_page_astate_load(m).queue));
}
#endif

static __always_inline bool
vm_page_pqstate_fcmpset(vm_page_t m, vm_page_astate_t *old, vm_page_astate_t new)
{
	vm_page_astate_t tmp;

	tmp = *old;
	do {
		if (__predict_true(vm_page_astate_fcmpset(m, old, new)))
			return (true);
		counter_u64_add(pqstate_commit_retries, 1);
	} while (old->_bits == tmp._bits);

	return (false);
}

/*
 * Do the work of committing a queue state update that moves the page out of
 * its current queue.
 */
static bool
_vm_page_pqstate_commit_dequeue(struct vm_pagequeue *pq, vm_page_t m,
    vm_page_astate_t *old, vm_page_astate_t new)
{
	vm_page_t next;

	vm_pagequeue_assert_locked(pq);
	KASSERT(vm_page_pagequeue(m) == pq,
	    ("%s: queue %p does not match page %p", __func__, pq, m));
	KASSERT(old->queue != PQ_NONE && new.queue != old->queue,
	    ("%s: invalid queue indices %d %d",
	    __func__, old->queue, new.queue));

	/*
	 * Once the queue index of the page changes there is nothing
	 * synchronizing with further updates to the page's physical
	 * queue state.  Therefore we must speculatively remove the page
	 * from the queue now and be prepared to roll back if the queue
	 * state update fails.  If the page is not physically enqueued then
	 * we just update its queue index.
	 */
	if ((old->flags & PGA_ENQUEUED) != 0) {
		new.flags &= ~PGA_ENQUEUED;
		next = TAILQ_NEXT(m, plinks.q);
		TAILQ_REMOVE(&pq->pq_pl, m, plinks.q);
		vm_pagequeue_cnt_dec(pq);
		if (!vm_page_pqstate_fcmpset(m, old, new)) {
			if (next == NULL)
				TAILQ_INSERT_TAIL(&pq->pq_pl, m, plinks.q);
			else
				TAILQ_INSERT_BEFORE(next, m, plinks.q);
			vm_pagequeue_cnt_inc(pq);
			return (false);
		} else {
			return (true);
		}
	} else {
		return (vm_page_pqstate_fcmpset(m, old, new));
	}
}

static bool
vm_page_pqstate_commit_dequeue(vm_page_t m, vm_page_astate_t *old,
    vm_page_astate_t new)
{
	struct vm_pagequeue *pq;
	vm_page_astate_t as;
	bool ret;

	pq = _vm_page_pagequeue(m, old->queue);

	/*
	 * The queue field and PGA_ENQUEUED flag are stable only so long as the
	 * corresponding page queue lock is held.
	 */
	vm_pagequeue_lock(pq);
	as = vm_page_astate_load(m);
	if (__predict_false(as._bits != old->_bits)) {
		*old = as;
		ret = false;
	} else {
		ret = _vm_page_pqstate_commit_dequeue(pq, m, old, new);
	}
	vm_pagequeue_unlock(pq);
	return (ret);
}

/*
 * Commit a queue state update that enqueues or requeues a page.
 */
static bool
_vm_page_pqstate_commit_requeue(struct vm_pagequeue *pq, vm_page_t m,
    vm_page_astate_t *old, vm_page_astate_t new)
{
	struct vm_domain *vmd;

	vm_pagequeue_assert_locked(pq);
	KASSERT(old->queue != PQ_NONE && new.queue == old->queue,
	    ("%s: invalid queue indices %d %d",
	    __func__, old->queue, new.queue));

	new.flags |= PGA_ENQUEUED;
	if (!vm_page_pqstate_fcmpset(m, old, new))
		return (false);

	if ((old->flags & PGA_ENQUEUED) != 0)
		TAILQ_REMOVE(&pq->pq_pl, m, plinks.q);
	else
		vm_pagequeue_cnt_inc(pq);

	/*
	 * Give PGA_REQUEUE_HEAD precedence over PGA_REQUEUE.  In particular, if
	 * both flags are set in close succession, only PGA_REQUEUE_HEAD will be
	 * applied, even if it was set first.
	 */
	if ((old->flags & PGA_REQUEUE_HEAD) != 0) {
		vmd = vm_pagequeue_domain(m);
		KASSERT(pq == &vmd->vmd_pagequeues[PQ_INACTIVE],
		    ("%s: invalid page queue for page %p", __func__, m));
		TAILQ_INSERT_BEFORE(&vmd->vmd_inacthead, m, plinks.q);
	} else {
		TAILQ_INSERT_TAIL(&pq->pq_pl, m, plinks.q);
	}
	return (true);
}

/*
 * Commit a queue state update that encodes a request for a deferred queue
 * operation.
 */
static bool
vm_page_pqstate_commit_request(vm_page_t m, vm_page_astate_t *old,
    vm_page_astate_t new)
{

	KASSERT(old->queue == new.queue || new.queue != PQ_NONE,
	    ("%s: invalid state, queue %d flags %x",
	    __func__, new.queue, new.flags));

	if (old->_bits != new._bits &&
	    !vm_page_pqstate_fcmpset(m, old, new))
		return (false);
	vm_page_pqbatch_submit(m, new.queue);
	return (true);
}

/*
 * A generic queue state update function.  This handles more cases than the
 * specialized functions above.
 */
bool
vm_page_pqstate_commit(vm_page_t m, vm_page_astate_t *old, vm_page_astate_t new)
{

	if (old->_bits == new._bits)
		return (true);

	if (old->queue != PQ_NONE && new.queue != old->queue) {
		if (!vm_page_pqstate_commit_dequeue(m, old, new))
			return (false);
		if (new.queue != PQ_NONE)
			vm_page_pqbatch_submit(m, new.queue);
	} else {
		if (!vm_page_pqstate_fcmpset(m, old, new))
			return (false);
		if (new.queue != PQ_NONE &&
		    ((new.flags & ~old->flags) & PGA_QUEUE_OP_MASK) != 0)
			vm_page_pqbatch_submit(m, new.queue);
	}
	return (true);
}

/*
 * Apply deferred queue state updates to a page.
 */
static inline void
vm_pqbatch_process_page(struct vm_pagequeue *pq, vm_page_t m, uint8_t queue)
{
	vm_page_astate_t new, old;

	CRITICAL_ASSERT(curthread);
	vm_pagequeue_assert_locked(pq);
	KASSERT(queue < PQ_COUNT,
	    ("%s: invalid queue index %d", __func__, queue));
	KASSERT(pq == _vm_page_pagequeue(m, queue),
	    ("%s: page %p does not belong to queue %p", __func__, m, pq));

	for (old = vm_page_astate_load(m);;) {
		if (__predict_false(old.queue != queue ||
		    (old.flags & PGA_QUEUE_OP_MASK) == 0)) {
			counter_u64_add(queue_nops, 1);
			break;
		}
		KASSERT((m->oflags & VPO_UNMANAGED) == 0,
		    ("%s: page %p is unmanaged", __func__, m));

		new = old;
		if ((old.flags & PGA_DEQUEUE) != 0) {
			new.flags &= ~PGA_QUEUE_OP_MASK;
			new.queue = PQ_NONE;
			if (__predict_true(_vm_page_pqstate_commit_dequeue(pq,
			    m, &old, new))) {
				counter_u64_add(queue_ops, 1);
				break;
			}
		} else {
			new.flags &= ~(PGA_REQUEUE | PGA_REQUEUE_HEAD);
			if (__predict_true(_vm_page_pqstate_commit_requeue(pq,
			    m, &old, new))) {
				counter_u64_add(queue_ops, 1);
				break;
			}
		}
	}
}

static void
vm_pqbatch_process(struct vm_pagequeue *pq, struct vm_batchqueue *bq,
    uint8_t queue)
{
	int i;

	for (i = 0; i < bq->bq_cnt; i++)
		vm_pqbatch_process_page(pq, bq->bq_pa[i], queue);
	vm_batchqueue_init(bq);
}

/*
 *	vm_page_pqbatch_submit:		[ internal use only ]
 *
 *	Enqueue a page in the specified page queue's batched work queue.
 *	The caller must have encoded the requested operation in the page
 *	structure's a.flags field.
 */
void
vm_page_pqbatch_submit(vm_page_t m, uint8_t queue)
{
	struct vm_batchqueue *bq;
	struct vm_pagequeue *pq;
	int domain;

	KASSERT(queue < PQ_COUNT, ("invalid queue %d", queue));

	domain = vm_page_domain(m);
	critical_enter();
	bq = DPCPU_PTR(pqbatch[domain][queue]);
	if (vm_batchqueue_insert(bq, m)) {
		critical_exit();
		return;
	}
	critical_exit();

	pq = &VM_DOMAIN(domain)->vmd_pagequeues[queue];
	vm_pagequeue_lock(pq);
	critical_enter();
	bq = DPCPU_PTR(pqbatch[domain][queue]);
	vm_pqbatch_process(pq, bq, queue);
	vm_pqbatch_process_page(pq, m, queue);
	vm_pagequeue_unlock(pq);
	critical_exit();
}

/*
 *	vm_page_pqbatch_drain:		[ internal use only ]
 *
 *	Force all per-CPU page queue batch queues to be drained.  This is
 *	intended for use in severe memory shortages, to ensure that pages
 *	do not remain stuck in the batch queues.
 */
void
vm_page_pqbatch_drain(void)
{
	struct thread *td;
	struct vm_domain *vmd;
	struct vm_pagequeue *pq;
	int cpu, domain, queue;

	td = curthread;
	CPU_FOREACH(cpu) {
		thread_lock(td);
		sched_bind(td, cpu);
		thread_unlock(td);

		for (domain = 0; domain < vm_ndomains; domain++) {
			vmd = VM_DOMAIN(domain);
			for (queue = 0; queue < PQ_COUNT; queue++) {
				pq = &vmd->vmd_pagequeues[queue];
				vm_pagequeue_lock(pq);
				critical_enter();
				vm_pqbatch_process(pq,
				    DPCPU_PTR(pqbatch[domain][queue]), queue);
				critical_exit();
				vm_pagequeue_unlock(pq);
			}
		}
	}
	thread_lock(td);
	sched_unbind(td);
	thread_unlock(td);
}

/*
 *	vm_page_dequeue_deferred:	[ internal use only ]
 *
 *	Request removal of the given page from its current page
 *	queue.  Physical removal from the queue may be deferred
 *	indefinitely.
 */
void
vm_page_dequeue_deferred(vm_page_t m)
{
	vm_page_astate_t new, old;

	old = vm_page_astate_load(m);
	do {
		if (old.queue == PQ_NONE) {
			KASSERT((old.flags & PGA_QUEUE_STATE_MASK) == 0,
			    ("%s: page %p has unexpected queue state",
			    __func__, m));
			break;
		}
		new = old;
		new.flags |= PGA_DEQUEUE;
	} while (!vm_page_pqstate_commit_request(m, &old, new));
}

/*
 *	vm_page_dequeue:
 *
 *	Remove the page from whichever page queue it's in, if any, before
 *	returning.
 */
void
vm_page_dequeue(vm_page_t m)
{
	vm_page_astate_t new, old;

	old = vm_page_astate_load(m);
	do {
		if (old.queue == PQ_NONE) {
			KASSERT((old.flags & PGA_QUEUE_STATE_MASK) == 0,
			    ("%s: page %p has unexpected queue state",
			    __func__, m));
			break;
		}
		new = old;
		new.flags &= ~PGA_QUEUE_OP_MASK;
		new.queue = PQ_NONE;
	} while (!vm_page_pqstate_commit_dequeue(m, &old, new));

}

/*
 * Schedule the given page for insertion into the specified page queue.
 * Physical insertion of the page may be deferred indefinitely.
 */
static void
vm_page_enqueue(vm_page_t m, uint8_t queue)
{

	KASSERT(m->a.queue == PQ_NONE &&
	    (m->a.flags & PGA_QUEUE_STATE_MASK) == 0,
	    ("%s: page %p is already enqueued", __func__, m));
	KASSERT(m->ref_count > 0,
	    ("%s: page %p does not carry any references", __func__, m));

	m->a.queue = queue;
	if ((m->a.flags & PGA_REQUEUE) == 0)
		vm_page_aflag_set(m, PGA_REQUEUE);
	vm_page_pqbatch_submit(m, queue);
}

/*
 *	vm_page_free_prep:
 *
 *	Prepares the given page to be put on the free list,
 *	disassociating it from any VM object. The caller may return
 *	the page to the free list only if this function returns true.
 *
 *	The object, if it exists, must be locked, and then the page must
 *	be xbusy.  Otherwise the page must be not busied.  A managed
 *	page must be unmapped.
 */
static bool
vm_page_free_prep(vm_page_t m)
{

	/*
	 * Synchronize with threads that have dropped a reference to this
	 * page.
	 */
	atomic_thread_fence_acq();

#if defined(DIAGNOSTIC) && defined(PHYS_TO_DMAP)
	if (PMAP_HAS_DMAP && (m->flags & PG_ZERO) != 0) {
		uint64_t *p;
		int i;
		p = (uint64_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));
		for (i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++, p++)
			KASSERT(*p == 0, ("vm_page_free_prep %p PG_ZERO %d %jx",
			    m, i, (uintmax_t)*p));
	}
#endif
	if ((m->oflags & VPO_UNMANAGED) == 0) {
		KASSERT(!pmap_page_is_mapped(m),
		    ("vm_page_free_prep: freeing mapped page %p", m));
		KASSERT((m->a.flags & (PGA_EXECUTABLE | PGA_WRITEABLE)) == 0,
		    ("vm_page_free_prep: mapping flags set in page %p", m));
	} else {
		KASSERT(m->a.queue == PQ_NONE,
		    ("vm_page_free_prep: unmanaged page %p is queued", m));
	}
	VM_CNT_INC(v_tfree);

	if (m->object != NULL) {
		KASSERT(((m->oflags & VPO_UNMANAGED) != 0) ==
		    ((m->object->flags & OBJ_UNMANAGED) != 0),
		    ("vm_page_free_prep: managed flag mismatch for page %p",
		    m));
		vm_page_assert_xbusied(m);

		/*
		 * The object reference can be released without an atomic
		 * operation.
		 */
		KASSERT((m->flags & PG_FICTITIOUS) != 0 ||
		    m->ref_count == VPRC_OBJREF,
		    ("vm_page_free_prep: page %p has unexpected ref_count %u",
		    m, m->ref_count));
		vm_page_object_remove(m);
		m->ref_count -= VPRC_OBJREF;
	} else
		vm_page_assert_unbusied(m);

	vm_page_busy_free(m);

	/*
	 * If fictitious remove object association and
	 * return.
	 */
	if ((m->flags & PG_FICTITIOUS) != 0) {
		KASSERT(m->ref_count == 1,
		    ("fictitious page %p is referenced", m));
		KASSERT(m->a.queue == PQ_NONE,
		    ("fictitious page %p is queued", m));
		return (false);
	}

	/*
	 * Pages need not be dequeued before they are returned to the physical
	 * memory allocator, but they must at least be marked for a deferred
	 * dequeue.
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0)
		vm_page_dequeue_deferred(m);

	m->valid = 0;
	vm_page_undirty(m);

	if (m->ref_count != 0)
		panic("vm_page_free_prep: page %p has references", m);

	/*
	 * Restore the default memory attribute to the page.
	 */
	if (pmap_page_get_memattr(m) != VM_MEMATTR_DEFAULT)
		pmap_page_set_memattr(m, VM_MEMATTR_DEFAULT);

#if VM_NRESERVLEVEL > 0
	/*
	 * Determine whether the page belongs to a reservation.  If the page was
	 * allocated from a per-CPU cache, it cannot belong to a reservation, so
	 * as an optimization, we avoid the check in that case.
	 */
	if ((m->flags & PG_PCPU_CACHE) == 0 && vm_reserv_free_page(m))
		return (false);
#endif

	return (true);
}

/*
 *	vm_page_free_toq:
 *
 *	Returns the given page to the free list, disassociating it
 *	from any VM object.
 *
 *	The object must be locked.  The page must be exclusively busied if it
 *	belongs to an object.
 */
static void
vm_page_free_toq(vm_page_t m)
{
	struct vm_domain *vmd;
	uma_zone_t zone;

	if (!vm_page_free_prep(m))
		return;

	vmd = vm_pagequeue_domain(m);
	zone = vmd->vmd_pgcache[m->pool].zone;
	if ((m->flags & PG_PCPU_CACHE) != 0 && zone != NULL) {
		uma_zfree(zone, m);
		return;
	}
	vm_domain_free_lock(vmd);
	vm_phys_free_pages(m, 0);
	vm_domain_free_unlock(vmd);
	vm_domain_freecnt_inc(vmd, 1);
}

/*
 *	vm_page_free_pages_toq:
 *
 *	Returns a list of pages to the free list, disassociating it
 *	from any VM object.  In other words, this is equivalent to
 *	calling vm_page_free_toq() for each page of a list of VM objects.
 */
void
vm_page_free_pages_toq(struct spglist *free, bool update_wire_count)
{
	vm_page_t m;
	int count;

	if (SLIST_EMPTY(free))
		return;

	count = 0;
	while ((m = SLIST_FIRST(free)) != NULL) {
		count++;
		SLIST_REMOVE_HEAD(free, plinks.s.ss);
		vm_page_free_toq(m);
	}

	if (update_wire_count)
		vm_wire_sub(count);
}

/*
 * Mark this page as wired down.  For managed pages, this prevents reclamation
 * by the page daemon, or when the containing object, if any, is destroyed.
 */
void
vm_page_wire(vm_page_t m)
{
	u_int old;

#ifdef INVARIANTS
	if (m->object != NULL && !vm_page_busied(m) &&
	    !vm_object_busied(m->object))
		VM_OBJECT_ASSERT_LOCKED(m->object);
#endif
	KASSERT((m->flags & PG_FICTITIOUS) == 0 ||
	    VPRC_WIRE_COUNT(m->ref_count) >= 1,
	    ("vm_page_wire: fictitious page %p has zero wirings", m));

	old = atomic_fetchadd_int(&m->ref_count, 1);
	KASSERT(VPRC_WIRE_COUNT(old) != VPRC_WIRE_COUNT_MAX,
	    ("vm_page_wire: counter overflow for page %p", m));
	if (VPRC_WIRE_COUNT(old) == 0) {
		if ((m->oflags & VPO_UNMANAGED) == 0)
			vm_page_aflag_set(m, PGA_DEQUEUE);
		vm_wire_add(1);
	}
}

/*
 * Attempt to wire a mapped page following a pmap lookup of that page.
 * This may fail if a thread is concurrently tearing down mappings of the page.
 * The transient failure is acceptable because it translates to the
 * failure of the caller pmap_extract_and_hold(), which should be then
 * followed by the vm_fault() fallback, see e.g. vm_fault_quick_hold_pages().
 */
bool
vm_page_wire_mapped(vm_page_t m)
{
	u_int old;

	old = m->ref_count;
	do {
		KASSERT(old > 0,
		    ("vm_page_wire_mapped: wiring unreferenced page %p", m));
		if ((old & VPRC_BLOCKED) != 0)
			return (false);
	} while (!atomic_fcmpset_int(&m->ref_count, &old, old + 1));

	if (VPRC_WIRE_COUNT(old) == 0) {
		if ((m->oflags & VPO_UNMANAGED) == 0)
			vm_page_aflag_set(m, PGA_DEQUEUE);
		vm_wire_add(1);
	}
	return (true);
}

/*
 * Release a wiring reference to a managed page.  If the page still belongs to
 * an object, update its position in the page queues to reflect the reference.
 * If the wiring was the last reference to the page, free the page.
 */
static void
vm_page_unwire_managed(vm_page_t m, uint8_t nqueue, bool noreuse)
{
	u_int old;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("%s: page %p is unmanaged", __func__, m));

	/*
	 * Update LRU state before releasing the wiring reference.
	 * Use a release store when updating the reference count to
	 * synchronize with vm_page_free_prep().
	 */
	old = m->ref_count;
	do {
		KASSERT(VPRC_WIRE_COUNT(old) > 0,
		    ("vm_page_unwire: wire count underflow for page %p", m));

		if (old > VPRC_OBJREF + 1) {
			/*
			 * The page has at least one other wiring reference.  An
			 * earlier iteration of this loop may have called
			 * vm_page_release_toq() and cleared PGA_DEQUEUE, so
			 * re-set it if necessary.
			 */
			if ((vm_page_astate_load(m).flags & PGA_DEQUEUE) == 0)
				vm_page_aflag_set(m, PGA_DEQUEUE);
		} else if (old == VPRC_OBJREF + 1) {
			/*
			 * This is the last wiring.  Clear PGA_DEQUEUE and
			 * update the page's queue state to reflect the
			 * reference.  If the page does not belong to an object
			 * (i.e., the VPRC_OBJREF bit is clear), we only need to
			 * clear leftover queue state.
			 */
			vm_page_release_toq(m, nqueue, noreuse);
		} else if (old == 1) {
			vm_page_aflag_clear(m, PGA_DEQUEUE);
		}
	} while (!atomic_fcmpset_rel_int(&m->ref_count, &old, old - 1));

	if (VPRC_WIRE_COUNT(old) == 1) {
		vm_wire_sub(1);
		if (old == 1)
			vm_page_free(m);
	}
}

/*
 * Release one wiring of the specified page, potentially allowing it to be
 * paged out.
 *
 * Only managed pages belonging to an object can be paged out.  If the number
 * of wirings transitions to zero and the page is eligible for page out, then
 * the page is added to the specified paging queue.  If the released wiring
 * represented the last reference to the page, the page is freed.
 */
void
vm_page_unwire(vm_page_t m, uint8_t nqueue)
{

	KASSERT(nqueue < PQ_COUNT,
	    ("vm_page_unwire: invalid queue %u request for page %p",
	    nqueue, m));

	if ((m->oflags & VPO_UNMANAGED) != 0) {
		if (vm_page_unwire_noq(m) && m->ref_count == 0)
			vm_page_free(m);
		return;
	}
	vm_page_unwire_managed(m, nqueue, false);
}

/*
 * Unwire a page without (re-)inserting it into a page queue.  It is up
 * to the caller to enqueue, requeue, or free the page as appropriate.
 * In most cases involving managed pages, vm_page_unwire() should be used
 * instead.
 */
bool
vm_page_unwire_noq(vm_page_t m)
{
	u_int old;

	old = vm_page_drop(m, 1);
	KASSERT(VPRC_WIRE_COUNT(old) != 0,
	    ("%s: counter underflow for page %p", __func__,  m));
	KASSERT((m->flags & PG_FICTITIOUS) == 0 || VPRC_WIRE_COUNT(old) > 1,
	    ("%s: missing ref on fictitious page %p", __func__, m));

	if (VPRC_WIRE_COUNT(old) > 1)
		return (false);
	if ((m->oflags & VPO_UNMANAGED) == 0)
		vm_page_aflag_clear(m, PGA_DEQUEUE);
	vm_wire_sub(1);
	return (true);
}

/*
 * Ensure that the page ends up in the specified page queue.  If the page is
 * active or being moved to the active queue, ensure that its act_count is
 * at least ACT_INIT but do not otherwise mess with it.
 */
static __always_inline void
vm_page_mvqueue(vm_page_t m, const uint8_t nqueue, const uint16_t nflag)
{
	vm_page_astate_t old, new;

	KASSERT(m->ref_count > 0,
	    ("%s: page %p does not carry any references", __func__, m));
	KASSERT(nflag == PGA_REQUEUE || nflag == PGA_REQUEUE_HEAD,
	    ("%s: invalid flags %x", __func__, nflag));

	if ((m->oflags & VPO_UNMANAGED) != 0 || vm_page_wired(m))
		return;

	old = vm_page_astate_load(m);
	do {
		if ((old.flags & PGA_DEQUEUE) != 0)
			break;
		new = old;
		new.flags &= ~PGA_QUEUE_OP_MASK;
		if (nqueue == PQ_ACTIVE)
			new.act_count = max(old.act_count, ACT_INIT);
		if (old.queue == nqueue) {
			if (nqueue != PQ_ACTIVE)
				new.flags |= nflag;
		} else {
			new.flags |= nflag;
			new.queue = nqueue;
		}
	} while (!vm_page_pqstate_commit(m, &old, new));
}

/*
 * Put the specified page on the active list (if appropriate).
 */
void
vm_page_activate(vm_page_t m)
{

	vm_page_mvqueue(m, PQ_ACTIVE, PGA_REQUEUE);
}

/*
 * Move the specified page to the tail of the inactive queue, or requeue
 * the page if it is already in the inactive queue.
 */
void
vm_page_deactivate(vm_page_t m)
{

	vm_page_mvqueue(m, PQ_INACTIVE, PGA_REQUEUE);
}

void
vm_page_deactivate_noreuse(vm_page_t m)
{

	vm_page_mvqueue(m, PQ_INACTIVE, PGA_REQUEUE_HEAD);
}

/*
 * Put a page in the laundry, or requeue it if it is already there.
 */
void
vm_page_launder(vm_page_t m)
{

	vm_page_mvqueue(m, PQ_LAUNDRY, PGA_REQUEUE);
}

/*
 * Put a page in the PQ_UNSWAPPABLE holding queue.
 */
void
vm_page_unswappable(vm_page_t m)
{

	KASSERT(!vm_page_wired(m) && (m->oflags & VPO_UNMANAGED) == 0,
	    ("page %p already unswappable", m));

	vm_page_dequeue(m);
	vm_page_enqueue(m, PQ_UNSWAPPABLE);
}

/*
 * Release a page back to the page queues in preparation for unwiring.
 */
static void
vm_page_release_toq(vm_page_t m, uint8_t nqueue, const bool noreuse)
{
	vm_page_astate_t old, new;
	uint16_t nflag;

	/*
	 * Use a check of the valid bits to determine whether we should
	 * accelerate reclamation of the page.  The object lock might not be
	 * held here, in which case the check is racy.  At worst we will either
	 * accelerate reclamation of a valid page and violate LRU, or
	 * unnecessarily defer reclamation of an invalid page.
	 *
	 * If we were asked to not cache the page, place it near the head of the
	 * inactive queue so that is reclaimed sooner.
	 */
	if (noreuse || m->valid == 0) {
		nqueue = PQ_INACTIVE;
		nflag = PGA_REQUEUE_HEAD;
	} else {
		nflag = PGA_REQUEUE;
	}

	old = vm_page_astate_load(m);
	do {
		new = old;

		/*
		 * If the page is already in the active queue and we are not
		 * trying to accelerate reclamation, simply mark it as
		 * referenced and avoid any queue operations.
		 */
		new.flags &= ~PGA_QUEUE_OP_MASK;
		if (nflag != PGA_REQUEUE_HEAD && old.queue == PQ_ACTIVE)
			new.flags |= PGA_REFERENCED;
		else {
			new.flags |= nflag;
			new.queue = nqueue;
		}
	} while (!vm_page_pqstate_commit(m, &old, new));
}

/*
 * Unwire a page and either attempt to free it or re-add it to the page queues.
 */
void
vm_page_release(vm_page_t m, int flags)
{
	vm_object_t object;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("vm_page_release: page %p is unmanaged", m));

	if ((flags & VPR_TRYFREE) != 0) {
		for (;;) {
			object = atomic_load_ptr(&m->object);
			if (object == NULL)
				break;
			/* Depends on type-stability. */
			if (vm_page_busied(m) || !VM_OBJECT_TRYWLOCK(object))
				break;
			if (object == m->object) {
				vm_page_release_locked(m, flags);
				VM_OBJECT_WUNLOCK(object);
				return;
			}
			VM_OBJECT_WUNLOCK(object);
		}
	}
	vm_page_unwire_managed(m, PQ_INACTIVE, flags != 0);
}

/* See vm_page_release(). */
void
vm_page_release_locked(vm_page_t m, int flags)
{

	VM_OBJECT_ASSERT_WLOCKED(m->object);
	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("vm_page_release_locked: page %p is unmanaged", m));

	if (vm_page_unwire_noq(m)) {
		if ((flags & VPR_TRYFREE) != 0 &&
		    (m->object->ref_count == 0 || !pmap_page_is_mapped(m)) &&
		    m->dirty == 0 && vm_page_tryxbusy(m)) {
			/*
			 * An unlocked lookup may have wired the page before the
			 * busy lock was acquired, in which case the page must
			 * not be freed.
			 */
			if (__predict_true(!vm_page_wired(m))) {
				vm_page_free(m);
				return;
			}
			vm_page_xunbusy(m);
		} else {
			vm_page_release_toq(m, PQ_INACTIVE, flags != 0);
		}
	}
}

static bool
vm_page_try_blocked_op(vm_page_t m, void (*op)(vm_page_t))
{
	u_int old;

	KASSERT(m->object != NULL && (m->oflags & VPO_UNMANAGED) == 0,
	    ("vm_page_try_blocked_op: page %p has no object", m));
	KASSERT(vm_page_busied(m),
	    ("vm_page_try_blocked_op: page %p is not busy", m));
	VM_OBJECT_ASSERT_LOCKED(m->object);

	old = m->ref_count;
	do {
		KASSERT(old != 0,
		    ("vm_page_try_blocked_op: page %p has no references", m));
		if (VPRC_WIRE_COUNT(old) != 0)
			return (false);
	} while (!atomic_fcmpset_int(&m->ref_count, &old, old | VPRC_BLOCKED));

	(op)(m);

	/*
	 * If the object is read-locked, new wirings may be created via an
	 * object lookup.
	 */
	old = vm_page_drop(m, VPRC_BLOCKED);
	KASSERT(!VM_OBJECT_WOWNED(m->object) ||
	    old == (VPRC_BLOCKED | VPRC_OBJREF),
	    ("vm_page_try_blocked_op: unexpected refcount value %u for %p",
	    old, m));
	return (true);
}

/*
 * Atomically check for wirings and remove all mappings of the page.
 */
bool
vm_page_try_remove_all(vm_page_t m)
{

	return (vm_page_try_blocked_op(m, pmap_remove_all));
}

/*
 * Atomically check for wirings and remove all writeable mappings of the page.
 */
bool
vm_page_try_remove_write(vm_page_t m)
{

	return (vm_page_try_blocked_op(m, pmap_remove_write));
}

/*
 * vm_page_advise
 *
 * 	Apply the specified advice to the given page.
 */
void
vm_page_advise(vm_page_t m, int advice)
{

	VM_OBJECT_ASSERT_WLOCKED(m->object);
	vm_page_assert_xbusied(m);

	if (advice == MADV_FREE)
		/*
		 * Mark the page clean.  This will allow the page to be freed
		 * without first paging it out.  MADV_FREE pages are often
		 * quickly reused by malloc(3), so we do not do anything that
		 * would result in a page fault on a later access.
		 */
		vm_page_undirty(m);
	else if (advice != MADV_DONTNEED) {
		if (advice == MADV_WILLNEED)
			vm_page_activate(m);
		return;
	}

	if (advice != MADV_FREE && m->dirty == 0 && pmap_is_modified(m))
		vm_page_dirty(m);

	/*
	 * Clear any references to the page.  Otherwise, the page daemon will
	 * immediately reactivate the page.
	 */
	vm_page_aflag_clear(m, PGA_REFERENCED);

	/*
	 * Place clean pages near the head of the inactive queue rather than
	 * the tail, thus defeating the queue's LRU operation and ensuring that
	 * the page will be reused quickly.  Dirty pages not already in the
	 * laundry are moved there.
	 */
	if (m->dirty == 0)
		vm_page_deactivate_noreuse(m);
	else if (!vm_page_in_laundry(m))
		vm_page_launder(m);
}

/*
 *	vm_page_grab_release
 *
 *	Helper routine for grab functions to release busy on return.
 */
static inline void
vm_page_grab_release(vm_page_t m, int allocflags)
{

	if ((allocflags & VM_ALLOC_NOBUSY) != 0) {
		if ((allocflags & VM_ALLOC_IGN_SBUSY) != 0)
			vm_page_sunbusy(m);
		else
			vm_page_xunbusy(m);
	}
}

/*
 *	vm_page_grab_sleep
 *
 *	Sleep for busy according to VM_ALLOC_ parameters.  Returns true
 *	if the caller should retry and false otherwise.
 *
 *	If the object is locked on entry the object will be unlocked with
 *	false returns and still locked but possibly having been dropped
 *	with true returns.
 */
static bool
vm_page_grab_sleep(vm_object_t object, vm_page_t m, vm_pindex_t pindex,
    const char *wmesg, int allocflags, bool locked)
{

	if ((allocflags & VM_ALLOC_NOWAIT) != 0)
		return (false);

	/*
	 * Reference the page before unlocking and sleeping so that
	 * the page daemon is less likely to reclaim it.
	 */
	if (locked && (allocflags & VM_ALLOC_NOCREAT) == 0)
		vm_page_reference(m);

	if (_vm_page_busy_sleep(object, m, pindex, wmesg, allocflags, locked) &&
	    locked)
		VM_OBJECT_WLOCK(object);
	if ((allocflags & VM_ALLOC_WAITFAIL) != 0)
		return (false);

	return (true);
}

/*
 * Assert that the grab flags are valid.
 */
static inline void
vm_page_grab_check(int allocflags)
{

	KASSERT((allocflags & VM_ALLOC_NOBUSY) == 0 ||
	    (allocflags & VM_ALLOC_WIRED) != 0,
	    ("vm_page_grab*: the pages must be busied or wired"));

	KASSERT((allocflags & VM_ALLOC_SBUSY) == 0 ||
	    (allocflags & VM_ALLOC_IGN_SBUSY) != 0,
	    ("vm_page_grab*: VM_ALLOC_SBUSY/VM_ALLOC_IGN_SBUSY mismatch"));
}

/*
 * Calculate the page allocation flags for grab.
 */
static inline int
vm_page_grab_pflags(int allocflags)
{
	int pflags;

	pflags = allocflags &
	    ~(VM_ALLOC_NOWAIT | VM_ALLOC_WAITOK | VM_ALLOC_WAITFAIL |
	    VM_ALLOC_NOBUSY);
	if ((allocflags & VM_ALLOC_NOWAIT) == 0)
		pflags |= VM_ALLOC_WAITFAIL;
	if ((allocflags & VM_ALLOC_IGN_SBUSY) != 0)
		pflags |= VM_ALLOC_SBUSY;

	return (pflags);
}

/*
 * Grab a page, waiting until we are waken up due to the page
 * changing state.  We keep on waiting, if the page continues
 * to be in the object.  If the page doesn't exist, first allocate it
 * and then conditionally zero it.
 *
 * This routine may sleep.
 *
 * The object must be locked on entry.  The lock will, however, be released
 * and reacquired if the routine sleeps.
 */
vm_page_t
vm_page_grab(vm_object_t object, vm_pindex_t pindex, int allocflags)
{
	vm_page_t m;

	VM_OBJECT_ASSERT_WLOCKED(object);
	vm_page_grab_check(allocflags);

retrylookup:
	if ((m = vm_page_lookup(object, pindex)) != NULL) {
		if (!vm_page_tryacquire(m, allocflags)) {
			if (vm_page_grab_sleep(object, m, pindex, "pgrbwt",
			    allocflags, true))
				goto retrylookup;
			return (NULL);
		}
		goto out;
	}
	if ((allocflags & VM_ALLOC_NOCREAT) != 0)
		return (NULL);
	m = vm_page_alloc(object, pindex, vm_page_grab_pflags(allocflags));
	if (m == NULL) {
		if ((allocflags & (VM_ALLOC_NOWAIT | VM_ALLOC_WAITFAIL)) != 0)
			return (NULL);
		goto retrylookup;
	}
	if (allocflags & VM_ALLOC_ZERO && (m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);

out:
	vm_page_grab_release(m, allocflags);

	return (m);
}

/*
 * Locklessly attempt to acquire a page given a (object, pindex) tuple
 * and an optional previous page to avoid the radix lookup.  The resulting
 * page will be validated against the identity tuple and busied or wired
 * as requested.  A NULL *mp return guarantees that the page was not in
 * radix at the time of the call but callers must perform higher level
 * synchronization or retry the operation under a lock if they require
 * an atomic answer.  This is the only lock free validation routine,
 * other routines can depend on the resulting page state.
 *
 * The return value indicates whether the operation failed due to caller
 * flags.  The return is tri-state with mp:
 *
 * (true, *mp != NULL) - The operation was successful.
 * (true, *mp == NULL) - The page was not found in tree.
 * (false, *mp == NULL) - WAITFAIL or NOWAIT prevented acquisition.
 */
static bool
vm_page_acquire_unlocked(vm_object_t object, vm_pindex_t pindex,
    vm_page_t prev, vm_page_t *mp, int allocflags)
{
	vm_page_t m;

	vm_page_grab_check(allocflags);
	MPASS(prev == NULL || vm_page_busied(prev) || vm_page_wired(prev));

	*mp = NULL;
	for (;;) {
		/*
		 * We may see a false NULL here because the previous page
		 * has been removed or just inserted and the list is loaded
		 * without barriers.  Switch to radix to verify.
		 */
		if (prev == NULL || (m = TAILQ_NEXT(prev, listq)) == NULL ||
		    QMD_IS_TRASHED(m) || m->pindex != pindex ||
		    atomic_load_ptr(&m->object) != object) {
			prev = NULL;
			/*
			 * This guarantees the result is instantaneously
			 * correct.
			 */
			m = vm_radix_lookup_unlocked(&object->rtree, pindex);
		}
		if (m == NULL)
			return (true);
		if (vm_page_trybusy(m, allocflags)) {
			if (m->object == object && m->pindex == pindex)
				break;
			/* relookup. */
			vm_page_busy_release(m);
			cpu_spinwait();
			continue;
		}
		if (!vm_page_grab_sleep(object, m, pindex, "pgnslp",
		    allocflags, false))
			return (false);
	}
	if ((allocflags & VM_ALLOC_WIRED) != 0)
		vm_page_wire(m);
	vm_page_grab_release(m, allocflags);
	*mp = m;
	return (true);
}

/*
 * Try to locklessly grab a page and fall back to the object lock if NOCREAT
 * is not set.
 */
vm_page_t
vm_page_grab_unlocked(vm_object_t object, vm_pindex_t pindex, int allocflags)
{
	vm_page_t m;

	vm_page_grab_check(allocflags);

	if (!vm_page_acquire_unlocked(object, pindex, NULL, &m, allocflags))
		return (NULL);
	if (m != NULL)
		return (m);

	/*
	 * The radix lockless lookup should never return a false negative
	 * errors.  If the user specifies NOCREAT they are guaranteed there
	 * was no page present at the instant of the call.  A NOCREAT caller
	 * must handle create races gracefully.
	 */
	if ((allocflags & VM_ALLOC_NOCREAT) != 0)
		return (NULL);

	VM_OBJECT_WLOCK(object);
	m = vm_page_grab(object, pindex, allocflags);
	VM_OBJECT_WUNLOCK(object);

	return (m);
}

/*
 * Grab a page and make it valid, paging in if necessary.  Pages missing from
 * their pager are zero filled and validated.  If a VM_ALLOC_COUNT is supplied
 * and the page is not valid as many as VM_INITIAL_PAGEIN pages can be brought
 * in simultaneously.  Additional pages will be left on a paging queue but
 * will neither be wired nor busy regardless of allocflags.
 */
int
vm_page_grab_valid(vm_page_t *mp, vm_object_t object, vm_pindex_t pindex, int allocflags)
{
	vm_page_t m;
	vm_page_t ma[VM_INITIAL_PAGEIN];
	int after, i, pflags, rv;

	KASSERT((allocflags & VM_ALLOC_SBUSY) == 0 ||
	    (allocflags & VM_ALLOC_IGN_SBUSY) != 0,
	    ("vm_page_grab_valid: VM_ALLOC_SBUSY/VM_ALLOC_IGN_SBUSY mismatch"));
	KASSERT((allocflags &
	    (VM_ALLOC_NOWAIT | VM_ALLOC_WAITFAIL | VM_ALLOC_ZERO)) == 0,
	    ("vm_page_grab_valid: Invalid flags 0x%X", allocflags));
	VM_OBJECT_ASSERT_WLOCKED(object);
	pflags = allocflags & ~(VM_ALLOC_NOBUSY | VM_ALLOC_SBUSY |
	    VM_ALLOC_WIRED);
	pflags |= VM_ALLOC_WAITFAIL;

retrylookup:
	if ((m = vm_page_lookup(object, pindex)) != NULL) {
		/*
		 * If the page is fully valid it can only become invalid
		 * with the object lock held.  If it is not valid it can
		 * become valid with the busy lock held.  Therefore, we
		 * may unnecessarily lock the exclusive busy here if we
		 * race with I/O completion not using the object lock.
		 * However, we will not end up with an invalid page and a
		 * shared lock.
		 */
		if (!vm_page_trybusy(m,
		    vm_page_all_valid(m) ? allocflags : 0)) {
			(void)vm_page_grab_sleep(object, m, pindex, "pgrbwt",
			    allocflags, true);
			goto retrylookup;
		}
		if (vm_page_all_valid(m))
			goto out;
		if ((allocflags & VM_ALLOC_NOCREAT) != 0) {
			vm_page_busy_release(m);
			*mp = NULL;
			return (VM_PAGER_FAIL);
		}
	} else if ((allocflags & VM_ALLOC_NOCREAT) != 0) {
		*mp = NULL;
		return (VM_PAGER_FAIL);
	} else if ((m = vm_page_alloc(object, pindex, pflags)) == NULL) {
		goto retrylookup;
	}

	vm_page_assert_xbusied(m);
	if (vm_pager_has_page(object, pindex, NULL, &after)) {
		after = MIN(after, VM_INITIAL_PAGEIN);
		after = MIN(after, allocflags >> VM_ALLOC_COUNT_SHIFT);
		after = MAX(after, 1);
		ma[0] = m;
		for (i = 1; i < after; i++) {
			if ((ma[i] = vm_page_next(ma[i - 1])) != NULL) {
				if (ma[i]->valid || !vm_page_tryxbusy(ma[i]))
					break;
			} else {
				ma[i] = vm_page_alloc(object, m->pindex + i,
				    VM_ALLOC_NORMAL);
				if (ma[i] == NULL)
					break;
			}
		}
		after = i;
		vm_object_pip_add(object, after);
		VM_OBJECT_WUNLOCK(object);
		rv = vm_pager_get_pages(object, ma, after, NULL, NULL);
		VM_OBJECT_WLOCK(object);
		vm_object_pip_wakeupn(object, after);
		/* Pager may have replaced a page. */
		m = ma[0];
		if (rv != VM_PAGER_OK) {
			for (i = 0; i < after; i++) {
				if (!vm_page_wired(ma[i]))
					vm_page_free(ma[i]);
				else
					vm_page_xunbusy(ma[i]);
			}
			*mp = NULL;
			return (rv);
		}
		for (i = 1; i < after; i++)
			vm_page_readahead_finish(ma[i]);
		MPASS(vm_page_all_valid(m));
	} else {
		vm_page_zero_invalid(m, TRUE);
	}
out:
	if ((allocflags & VM_ALLOC_WIRED) != 0)
		vm_page_wire(m);
	if ((allocflags & VM_ALLOC_SBUSY) != 0 && vm_page_xbusied(m))
		vm_page_busy_downgrade(m);
	else if ((allocflags & VM_ALLOC_NOBUSY) != 0)
		vm_page_busy_release(m);
	*mp = m;
	return (VM_PAGER_OK);
}

/*
 * Locklessly grab a valid page.  If the page is not valid or not yet
 * allocated this will fall back to the object lock method.
 */
int
vm_page_grab_valid_unlocked(vm_page_t *mp, vm_object_t object,
    vm_pindex_t pindex, int allocflags)
{
	vm_page_t m;
	int flags;
	int error;

	KASSERT((allocflags & VM_ALLOC_SBUSY) == 0 ||
	    (allocflags & VM_ALLOC_IGN_SBUSY) != 0,
	    ("vm_page_grab_valid_unlocked: VM_ALLOC_SBUSY/VM_ALLOC_IGN_SBUSY "
	    "mismatch"));
	KASSERT((allocflags &
	    (VM_ALLOC_NOWAIT | VM_ALLOC_WAITFAIL | VM_ALLOC_ZERO)) == 0,
	    ("vm_page_grab_valid_unlocked: Invalid flags 0x%X", allocflags));

	/*
	 * Attempt a lockless lookup and busy.  We need at least an sbusy
	 * before we can inspect the valid field and return a wired page.
	 */
	flags = allocflags & ~(VM_ALLOC_NOBUSY | VM_ALLOC_WIRED);
	if (!vm_page_acquire_unlocked(object, pindex, NULL, mp, flags))
		return (VM_PAGER_FAIL);
	if ((m = *mp) != NULL) {
		if (vm_page_all_valid(m)) {
			if ((allocflags & VM_ALLOC_WIRED) != 0)
				vm_page_wire(m);
			vm_page_grab_release(m, allocflags);
			return (VM_PAGER_OK);
		}
		vm_page_busy_release(m);
	}
	if ((allocflags & VM_ALLOC_NOCREAT) != 0) {
		*mp = NULL;
		return (VM_PAGER_FAIL);
	}
	VM_OBJECT_WLOCK(object);
	error = vm_page_grab_valid(mp, object, pindex, allocflags);
	VM_OBJECT_WUNLOCK(object);

	return (error);
}

/*
 * Return the specified range of pages from the given object.  For each
 * page offset within the range, if a page already exists within the object
 * at that offset and it is busy, then wait for it to change state.  If,
 * instead, the page doesn't exist, then allocate it.
 *
 * The caller must always specify an allocation class.
 *
 * allocation classes:
 *	VM_ALLOC_NORMAL		normal process request
 *	VM_ALLOC_SYSTEM		system *really* needs the pages
 *
 * The caller must always specify that the pages are to be busied and/or
 * wired.
 *
 * optional allocation flags:
 *	VM_ALLOC_IGN_SBUSY	do not sleep on soft busy pages
 *	VM_ALLOC_NOBUSY		do not exclusive busy the page
 *	VM_ALLOC_NOWAIT		do not sleep
 *	VM_ALLOC_SBUSY		set page to sbusy state
 *	VM_ALLOC_WIRED		wire the pages
 *	VM_ALLOC_ZERO		zero and validate any invalid pages
 *
 * If VM_ALLOC_NOWAIT is not specified, this routine may sleep.  Otherwise, it
 * may return a partial prefix of the requested range.
 */
int
vm_page_grab_pages(vm_object_t object, vm_pindex_t pindex, int allocflags,
    vm_page_t *ma, int count)
{
	vm_page_t m, mpred;
	int pflags;
	int i;

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(((u_int)allocflags >> VM_ALLOC_COUNT_SHIFT) == 0,
	    ("vm_page_grap_pages: VM_ALLOC_COUNT() is not allowed"));
	KASSERT(count > 0,
	    ("vm_page_grab_pages: invalid page count %d", count));
	vm_page_grab_check(allocflags);

	pflags = vm_page_grab_pflags(allocflags);
	i = 0;
retrylookup:
	m = vm_radix_lookup_le(&object->rtree, pindex + i);
	if (m == NULL || m->pindex != pindex + i) {
		mpred = m;
		m = NULL;
	} else
		mpred = TAILQ_PREV(m, pglist, listq);
	for (; i < count; i++) {
		if (m != NULL) {
			if (!vm_page_tryacquire(m, allocflags)) {
				if (vm_page_grab_sleep(object, m, pindex + i,
				    "grbmaw", allocflags, true))
					goto retrylookup;
				break;
			}
		} else {
			if ((allocflags & VM_ALLOC_NOCREAT) != 0)
				break;
			m = vm_page_alloc_after(object, pindex + i,
			    pflags | VM_ALLOC_COUNT(count - i), mpred);
			if (m == NULL) {
				if ((allocflags & (VM_ALLOC_NOWAIT |
				    VM_ALLOC_WAITFAIL)) != 0)
					break;
				goto retrylookup;
			}
		}
		if (vm_page_none_valid(m) &&
		    (allocflags & VM_ALLOC_ZERO) != 0) {
			if ((m->flags & PG_ZERO) == 0)
				pmap_zero_page(m);
			vm_page_valid(m);
		}
		vm_page_grab_release(m, allocflags);
		ma[i] = mpred = m;
		m = vm_page_next(m);
	}
	return (i);
}

/*
 * Unlocked variant of vm_page_grab_pages().  This accepts the same flags
 * and will fall back to the locked variant to handle allocation.
 */
int
vm_page_grab_pages_unlocked(vm_object_t object, vm_pindex_t pindex,
    int allocflags, vm_page_t *ma, int count)
{
	vm_page_t m, pred;
	int flags;
	int i;

	KASSERT(count > 0,
	    ("vm_page_grab_pages_unlocked: invalid page count %d", count));
	vm_page_grab_check(allocflags);

	/*
	 * Modify flags for lockless acquire to hold the page until we
	 * set it valid if necessary.
	 */
	flags = allocflags & ~VM_ALLOC_NOBUSY;
	pred = NULL;
	for (i = 0; i < count; i++, pindex++) {
		if (!vm_page_acquire_unlocked(object, pindex, pred, &m, flags))
			return (i);
		if (m == NULL)
			break;
		if ((flags & VM_ALLOC_ZERO) != 0 && vm_page_none_valid(m)) {
			if ((m->flags & PG_ZERO) == 0)
				pmap_zero_page(m);
			vm_page_valid(m);
		}
		/* m will still be wired or busy according to flags. */
		vm_page_grab_release(m, allocflags);
		pred = ma[i] = m;
	}
	if (i == count || (allocflags & VM_ALLOC_NOCREAT) != 0)
		return (i);
	count -= i;
	VM_OBJECT_WLOCK(object);
	i += vm_page_grab_pages(object, pindex, allocflags, &ma[i], count);
	VM_OBJECT_WUNLOCK(object);

	return (i);
}

/*
 * Mapping function for valid or dirty bits in a page.
 *
 * Inputs are required to range within a page.
 */
vm_page_bits_t
vm_page_bits(int base, int size)
{
	int first_bit;
	int last_bit;

	KASSERT(
	    base + size <= PAGE_SIZE,
	    ("vm_page_bits: illegal base/size %d/%d", base, size)
	);

	if (size == 0)		/* handle degenerate case */
		return (0);

	first_bit = base >> DEV_BSHIFT;
	last_bit = (base + size - 1) >> DEV_BSHIFT;

	return (((vm_page_bits_t)2 << last_bit) -
	    ((vm_page_bits_t)1 << first_bit));
}

void
vm_page_bits_set(vm_page_t m, vm_page_bits_t *bits, vm_page_bits_t set)
{

#if PAGE_SIZE == 32768
	atomic_set_64((uint64_t *)bits, set);
#elif PAGE_SIZE == 16384
	atomic_set_32((uint32_t *)bits, set);
#elif (PAGE_SIZE == 8192) && defined(atomic_set_16)
	atomic_set_16((uint16_t *)bits, set);
#elif (PAGE_SIZE == 4096) && defined(atomic_set_8)
	atomic_set_8((uint8_t *)bits, set);
#else		/* PAGE_SIZE <= 8192 */
	uintptr_t addr;
	int shift;

	addr = (uintptr_t)bits;
	/*
	 * Use a trick to perform a 32-bit atomic on the
	 * containing aligned word, to not depend on the existence
	 * of atomic_{set, clear}_{8, 16}.
	 */
	shift = addr & (sizeof(uint32_t) - 1);
#if BYTE_ORDER == BIG_ENDIAN
	shift = (sizeof(uint32_t) - sizeof(vm_page_bits_t) - shift) * NBBY;
#else
	shift *= NBBY;
#endif
	addr &= ~(sizeof(uint32_t) - 1);
	atomic_set_32((uint32_t *)addr, set << shift);
#endif		/* PAGE_SIZE */
}

static inline void
vm_page_bits_clear(vm_page_t m, vm_page_bits_t *bits, vm_page_bits_t clear)
{

#if PAGE_SIZE == 32768
	atomic_clear_64((uint64_t *)bits, clear);
#elif PAGE_SIZE == 16384
	atomic_clear_32((uint32_t *)bits, clear);
#elif (PAGE_SIZE == 8192) && defined(atomic_clear_16)
	atomic_clear_16((uint16_t *)bits, clear);
#elif (PAGE_SIZE == 4096) && defined(atomic_clear_8)
	atomic_clear_8((uint8_t *)bits, clear);
#else		/* PAGE_SIZE <= 8192 */
	uintptr_t addr;
	int shift;

	addr = (uintptr_t)bits;
	/*
	 * Use a trick to perform a 32-bit atomic on the
	 * containing aligned word, to not depend on the existence
	 * of atomic_{set, clear}_{8, 16}.
	 */
	shift = addr & (sizeof(uint32_t) - 1);
#if BYTE_ORDER == BIG_ENDIAN
	shift = (sizeof(uint32_t) - sizeof(vm_page_bits_t) - shift) * NBBY;
#else
	shift *= NBBY;
#endif
	addr &= ~(sizeof(uint32_t) - 1);
	atomic_clear_32((uint32_t *)addr, clear << shift);
#endif		/* PAGE_SIZE */
}

static inline vm_page_bits_t
vm_page_bits_swap(vm_page_t m, vm_page_bits_t *bits, vm_page_bits_t newbits)
{
#if PAGE_SIZE == 32768
	uint64_t old;

	old = *bits;
	while (atomic_fcmpset_64(bits, &old, newbits) == 0);
	return (old);
#elif PAGE_SIZE == 16384
	uint32_t old;

	old = *bits;
	while (atomic_fcmpset_32(bits, &old, newbits) == 0);
	return (old);
#elif (PAGE_SIZE == 8192) && defined(atomic_fcmpset_16)
	uint16_t old;

	old = *bits;
	while (atomic_fcmpset_16(bits, &old, newbits) == 0);
	return (old);
#elif (PAGE_SIZE == 4096) && defined(atomic_fcmpset_8)
	uint8_t old;

	old = *bits;
	while (atomic_fcmpset_8(bits, &old, newbits) == 0);
	return (old);
#else		/* PAGE_SIZE <= 4096*/
	uintptr_t addr;
	uint32_t old, new, mask;
	int shift;

	addr = (uintptr_t)bits;
	/*
	 * Use a trick to perform a 32-bit atomic on the
	 * containing aligned word, to not depend on the existence
	 * of atomic_{set, swap, clear}_{8, 16}.
	 */
	shift = addr & (sizeof(uint32_t) - 1);
#if BYTE_ORDER == BIG_ENDIAN
	shift = (sizeof(uint32_t) - sizeof(vm_page_bits_t) - shift) * NBBY;
#else
	shift *= NBBY;
#endif
	addr &= ~(sizeof(uint32_t) - 1);
	mask = VM_PAGE_BITS_ALL << shift;

	old = *bits;
	do {
		new = old & ~mask;
		new |= newbits << shift;
	} while (atomic_fcmpset_32((uint32_t *)addr, &old, new) == 0);
	return (old >> shift);
#endif		/* PAGE_SIZE */
}

/*
 *	vm_page_set_valid_range:
 *
 *	Sets portions of a page valid.  The arguments are expected
 *	to be DEV_BSIZE aligned but if they aren't the bitmap is inclusive
 *	of any partial chunks touched by the range.  The invalid portion of
 *	such chunks will be zeroed.
 *
 *	(base + size) must be less then or equal to PAGE_SIZE.
 */
void
vm_page_set_valid_range(vm_page_t m, int base, int size)
{
	int endoff, frag;
	vm_page_bits_t pagebits;

	vm_page_assert_busied(m);
	if (size == 0)	/* handle degenerate case */
		return;

	/*
	 * If the base is not DEV_BSIZE aligned and the valid
	 * bit is clear, we have to zero out a portion of the
	 * first block.
	 */
	if ((frag = rounddown2(base, DEV_BSIZE)) != base &&
	    (m->valid & (1 << (base >> DEV_BSHIFT))) == 0)
		pmap_zero_page_area(m, frag, base - frag);

	/*
	 * If the ending offset is not DEV_BSIZE aligned and the
	 * valid bit is clear, we have to zero out a portion of
	 * the last block.
	 */
	endoff = base + size;
	if ((frag = rounddown2(endoff, DEV_BSIZE)) != endoff &&
	    (m->valid & (1 << (endoff >> DEV_BSHIFT))) == 0)
		pmap_zero_page_area(m, endoff,
		    DEV_BSIZE - (endoff & (DEV_BSIZE - 1)));

	/*
	 * Assert that no previously invalid block that is now being validated
	 * is already dirty.
	 */
	KASSERT((~m->valid & vm_page_bits(base, size) & m->dirty) == 0,
	    ("vm_page_set_valid_range: page %p is dirty", m));

	/*
	 * Set valid bits inclusive of any overlap.
	 */
	pagebits = vm_page_bits(base, size);
	if (vm_page_xbusied(m))
		m->valid |= pagebits;
	else
		vm_page_bits_set(m, &m->valid, pagebits);
}

/*
 * Set the page dirty bits and free the invalid swap space if
 * present.  Returns the previous dirty bits.
 */
vm_page_bits_t
vm_page_set_dirty(vm_page_t m)
{
	vm_page_bits_t old;

	VM_PAGE_OBJECT_BUSY_ASSERT(m);

	if (vm_page_xbusied(m) && !pmap_page_is_write_mapped(m)) {
		old = m->dirty;
		m->dirty = VM_PAGE_BITS_ALL;
	} else
		old = vm_page_bits_swap(m, &m->dirty, VM_PAGE_BITS_ALL);
	if (old == 0 && (m->a.flags & PGA_SWAP_SPACE) != 0)
		vm_pager_page_unswapped(m);

	return (old);
}

/*
 * Clear the given bits from the specified page's dirty field.
 */
static __inline void
vm_page_clear_dirty_mask(vm_page_t m, vm_page_bits_t pagebits)
{

	vm_page_assert_busied(m);

	/*
	 * If the page is xbusied and not write mapped we are the
	 * only thread that can modify dirty bits.  Otherwise, The pmap
	 * layer can call vm_page_dirty() without holding a distinguished
	 * lock.  The combination of page busy and atomic operations
	 * suffice to guarantee consistency of the page dirty field.
	 */
	if (vm_page_xbusied(m) && !pmap_page_is_write_mapped(m))
		m->dirty &= ~pagebits;
	else
		vm_page_bits_clear(m, &m->dirty, pagebits);
}

/*
 *	vm_page_set_validclean:
 *
 *	Sets portions of a page valid and clean.  The arguments are expected
 *	to be DEV_BSIZE aligned but if they aren't the bitmap is inclusive
 *	of any partial chunks touched by the range.  The invalid portion of
 *	such chunks will be zero'd.
 *
 *	(base + size) must be less then or equal to PAGE_SIZE.
 */
void
vm_page_set_validclean(vm_page_t m, int base, int size)
{
	vm_page_bits_t oldvalid, pagebits;
	int endoff, frag;

	vm_page_assert_busied(m);
	if (size == 0)	/* handle degenerate case */
		return;

	/*
	 * If the base is not DEV_BSIZE aligned and the valid
	 * bit is clear, we have to zero out a portion of the
	 * first block.
	 */
	if ((frag = rounddown2(base, DEV_BSIZE)) != base &&
	    (m->valid & ((vm_page_bits_t)1 << (base >> DEV_BSHIFT))) == 0)
		pmap_zero_page_area(m, frag, base - frag);

	/*
	 * If the ending offset is not DEV_BSIZE aligned and the
	 * valid bit is clear, we have to zero out a portion of
	 * the last block.
	 */
	endoff = base + size;
	if ((frag = rounddown2(endoff, DEV_BSIZE)) != endoff &&
	    (m->valid & ((vm_page_bits_t)1 << (endoff >> DEV_BSHIFT))) == 0)
		pmap_zero_page_area(m, endoff,
		    DEV_BSIZE - (endoff & (DEV_BSIZE - 1)));

	/*
	 * Set valid, clear dirty bits.  If validating the entire
	 * page we can safely clear the pmap modify bit.  We also
	 * use this opportunity to clear the PGA_NOSYNC flag.  If a process
	 * takes a write fault on a MAP_NOSYNC memory area the flag will
	 * be set again.
	 *
	 * We set valid bits inclusive of any overlap, but we can only
	 * clear dirty bits for DEV_BSIZE chunks that are fully within
	 * the range.
	 */
	oldvalid = m->valid;
	pagebits = vm_page_bits(base, size);
	if (vm_page_xbusied(m))
		m->valid |= pagebits;
	else
		vm_page_bits_set(m, &m->valid, pagebits);
#if 0	/* NOT YET */
	if ((frag = base & (DEV_BSIZE - 1)) != 0) {
		frag = DEV_BSIZE - frag;
		base += frag;
		size -= frag;
		if (size < 0)
			size = 0;
	}
	pagebits = vm_page_bits(base, size & (DEV_BSIZE - 1));
#endif
	if (base == 0 && size == PAGE_SIZE) {
		/*
		 * The page can only be modified within the pmap if it is
		 * mapped, and it can only be mapped if it was previously
		 * fully valid.
		 */
		if (oldvalid == VM_PAGE_BITS_ALL)
			/*
			 * Perform the pmap_clear_modify() first.  Otherwise,
			 * a concurrent pmap operation, such as
			 * pmap_protect(), could clear a modification in the
			 * pmap and set the dirty field on the page before
			 * pmap_clear_modify() had begun and after the dirty
			 * field was cleared here.
			 */
			pmap_clear_modify(m);
		m->dirty = 0;
		vm_page_aflag_clear(m, PGA_NOSYNC);
	} else if (oldvalid != VM_PAGE_BITS_ALL && vm_page_xbusied(m))
		m->dirty &= ~pagebits;
	else
		vm_page_clear_dirty_mask(m, pagebits);
}

void
vm_page_clear_dirty(vm_page_t m, int base, int size)
{

	vm_page_clear_dirty_mask(m, vm_page_bits(base, size));
}

/*
 *	vm_page_set_invalid:
 *
 *	Invalidates DEV_BSIZE'd chunks within a page.  Both the
 *	valid and dirty bits for the effected areas are cleared.
 */
void
vm_page_set_invalid(vm_page_t m, int base, int size)
{
	vm_page_bits_t bits;
	vm_object_t object;

	/*
	 * The object lock is required so that pages can't be mapped
	 * read-only while we're in the process of invalidating them.
	 */
	object = m->object;
	VM_OBJECT_ASSERT_WLOCKED(object);
	vm_page_assert_busied(m);

	if (object->type == OBJT_VNODE && base == 0 && IDX_TO_OFF(m->pindex) +
	    size >= object->un_pager.vnp.vnp_size)
		bits = VM_PAGE_BITS_ALL;
	else
		bits = vm_page_bits(base, size);
	if (object->ref_count != 0 && vm_page_all_valid(m) && bits != 0)
		pmap_remove_all(m);
	KASSERT((bits == 0 && vm_page_all_valid(m)) ||
	    !pmap_page_is_mapped(m),
	    ("vm_page_set_invalid: page %p is mapped", m));
	if (vm_page_xbusied(m)) {
		m->valid &= ~bits;
		m->dirty &= ~bits;
	} else {
		vm_page_bits_clear(m, &m->valid, bits);
		vm_page_bits_clear(m, &m->dirty, bits);
	}
}

/*
 *	vm_page_invalid:
 *
 *	Invalidates the entire page.  The page must be busy, unmapped, and
 *	the enclosing object must be locked.  The object locks protects
 *	against concurrent read-only pmap enter which is done without
 *	busy.
 */
void
vm_page_invalid(vm_page_t m)
{

	vm_page_assert_busied(m);
	VM_OBJECT_ASSERT_LOCKED(m->object);
	MPASS(!pmap_page_is_mapped(m));

	if (vm_page_xbusied(m))
		m->valid = 0;
	else
		vm_page_bits_clear(m, &m->valid, VM_PAGE_BITS_ALL);
}

/*
 * vm_page_zero_invalid()
 *
 *	The kernel assumes that the invalid portions of a page contain
 *	garbage, but such pages can be mapped into memory by user code.
 *	When this occurs, we must zero out the non-valid portions of the
 *	page so user code sees what it expects.
 *
 *	Pages are most often semi-valid when the end of a file is mapped
 *	into memory and the file's size is not page aligned.
 */
void
vm_page_zero_invalid(vm_page_t m, boolean_t setvalid)
{
	int b;
	int i;

	/*
	 * Scan the valid bits looking for invalid sections that
	 * must be zeroed.  Invalid sub-DEV_BSIZE'd areas ( where the
	 * valid bit may be set ) have already been zeroed by
	 * vm_page_set_validclean().
	 */
	for (b = i = 0; i <= PAGE_SIZE / DEV_BSIZE; ++i) {
		if (i == (PAGE_SIZE / DEV_BSIZE) ||
		    (m->valid & ((vm_page_bits_t)1 << i))) {
			if (i > b) {
				pmap_zero_page_area(m,
				    b << DEV_BSHIFT, (i - b) << DEV_BSHIFT);
			}
			b = i + 1;
		}
	}

	/*
	 * setvalid is TRUE when we can safely set the zero'd areas
	 * as being valid.  We can do this if there are no cache consistancy
	 * issues.  e.g. it is ok to do with UFS, but not ok to do with NFS.
	 */
	if (setvalid)
		vm_page_valid(m);
}

/*
 *	vm_page_is_valid:
 *
 *	Is (partial) page valid?  Note that the case where size == 0
 *	will return FALSE in the degenerate case where the page is
 *	entirely invalid, and TRUE otherwise.
 *
 *	Some callers envoke this routine without the busy lock held and
 *	handle races via higher level locks.  Typical callers should
 *	hold a busy lock to prevent invalidation.
 */
int
vm_page_is_valid(vm_page_t m, int base, int size)
{
	vm_page_bits_t bits;

	bits = vm_page_bits(base, size);
	return (m->valid != 0 && (m->valid & bits) == bits);
}

/*
 * Returns true if all of the specified predicates are true for the entire
 * (super)page and false otherwise.
 */
bool
vm_page_ps_test(vm_page_t m, int flags, vm_page_t skip_m)
{
	vm_object_t object;
	int i, npages;

	object = m->object;
	if (skip_m != NULL && skip_m->object != object)
		return (false);
	VM_OBJECT_ASSERT_LOCKED(object);
	npages = atop(pagesizes[m->psind]);

	/*
	 * The physically contiguous pages that make up a superpage, i.e., a
	 * page with a page size index ("psind") greater than zero, will
	 * occupy adjacent entries in vm_page_array[].
	 */
	for (i = 0; i < npages; i++) {
		/* Always test object consistency, including "skip_m". */
		if (m[i].object != object)
			return (false);
		if (&m[i] == skip_m)
			continue;
		if ((flags & PS_NONE_BUSY) != 0 && vm_page_busied(&m[i]))
			return (false);
		if ((flags & PS_ALL_DIRTY) != 0) {
			/*
			 * Calling vm_page_test_dirty() or pmap_is_modified()
			 * might stop this case from spuriously returning
			 * "false".  However, that would require a write lock
			 * on the object containing "m[i]".
			 */
			if (m[i].dirty != VM_PAGE_BITS_ALL)
				return (false);
		}
		if ((flags & PS_ALL_VALID) != 0 &&
		    m[i].valid != VM_PAGE_BITS_ALL)
			return (false);
	}
	return (true);
}

/*
 * Set the page's dirty bits if the page is modified.
 */
void
vm_page_test_dirty(vm_page_t m)
{

	vm_page_assert_busied(m);
	if (m->dirty != VM_PAGE_BITS_ALL && pmap_is_modified(m))
		vm_page_dirty(m);
}

void
vm_page_valid(vm_page_t m)
{

	vm_page_assert_busied(m);
	if (vm_page_xbusied(m))
		m->valid = VM_PAGE_BITS_ALL;
	else
		vm_page_bits_set(m, &m->valid, VM_PAGE_BITS_ALL);
}

void
vm_page_lock_KBI(vm_page_t m, const char *file, int line)
{

	mtx_lock_flags_(vm_page_lockptr(m), 0, file, line);
}

void
vm_page_unlock_KBI(vm_page_t m, const char *file, int line)
{

	mtx_unlock_flags_(vm_page_lockptr(m), 0, file, line);
}

int
vm_page_trylock_KBI(vm_page_t m, const char *file, int line)
{

	return (mtx_trylock_flags_(vm_page_lockptr(m), 0, file, line));
}

#if defined(INVARIANTS) || defined(INVARIANT_SUPPORT)
void
vm_page_assert_locked_KBI(vm_page_t m, const char *file, int line)
{

	vm_page_lock_assert_KBI(m, MA_OWNED, file, line);
}

void
vm_page_lock_assert_KBI(vm_page_t m, int a, const char *file, int line)
{

	mtx_assert_(vm_page_lockptr(m), a, file, line);
}
#endif

#ifdef INVARIANTS
void
vm_page_object_busy_assert(vm_page_t m)
{

	/*
	 * Certain of the page's fields may only be modified by the
	 * holder of a page or object busy.
	 */
	if (m->object != NULL && !vm_page_busied(m))
		VM_OBJECT_ASSERT_BUSY(m->object);
}

void
vm_page_assert_pga_writeable(vm_page_t m, uint16_t bits)
{

	if ((bits & PGA_WRITEABLE) == 0)
		return;

	/*
	 * The PGA_WRITEABLE flag can only be set if the page is
	 * managed, is exclusively busied or the object is locked.
	 * Currently, this flag is only set by pmap_enter().
	 */
	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("PGA_WRITEABLE on unmanaged page"));
	if (!vm_page_xbusied(m))
		VM_OBJECT_ASSERT_BUSY(m->object);
}
#endif

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <ddb/ddb.h>

DB_SHOW_COMMAND(page, vm_page_print_page_info)
{

	db_printf("vm_cnt.v_free_count: %d\n", vm_free_count());
	db_printf("vm_cnt.v_inactive_count: %d\n", vm_inactive_count());
	db_printf("vm_cnt.v_active_count: %d\n", vm_active_count());
	db_printf("vm_cnt.v_laundry_count: %d\n", vm_laundry_count());
	db_printf("vm_cnt.v_wire_count: %d\n", vm_wire_count());
	db_printf("vm_cnt.v_free_reserved: %d\n", vm_cnt.v_free_reserved);
	db_printf("vm_cnt.v_free_min: %d\n", vm_cnt.v_free_min);
	db_printf("vm_cnt.v_free_target: %d\n", vm_cnt.v_free_target);
	db_printf("vm_cnt.v_inactive_target: %d\n", vm_cnt.v_inactive_target);
}

DB_SHOW_COMMAND(pageq, vm_page_print_pageq_info)
{
	int dom;

	db_printf("pq_free %d\n", vm_free_count());
	for (dom = 0; dom < vm_ndomains; dom++) {
		db_printf(
    "dom %d page_cnt %d free %d pq_act %d pq_inact %d pq_laund %d pq_unsw %d\n",
		    dom,
		    vm_dom[dom].vmd_page_count,
		    vm_dom[dom].vmd_free_count,
		    vm_dom[dom].vmd_pagequeues[PQ_ACTIVE].pq_cnt,
		    vm_dom[dom].vmd_pagequeues[PQ_INACTIVE].pq_cnt,
		    vm_dom[dom].vmd_pagequeues[PQ_LAUNDRY].pq_cnt,
		    vm_dom[dom].vmd_pagequeues[PQ_UNSWAPPABLE].pq_cnt);
	}
}

DB_SHOW_COMMAND(pginfo, vm_page_print_pginfo)
{
	vm_page_t m;
	boolean_t phys, virt;

	if (!have_addr) {
		db_printf("show pginfo addr\n");
		return;
	}

	phys = strchr(modif, 'p') != NULL;
	virt = strchr(modif, 'v') != NULL;
	if (virt)
		m = PHYS_TO_VM_PAGE(pmap_kextract(addr));
	else if (phys)
		m = PHYS_TO_VM_PAGE(addr);
	else
		m = (vm_page_t)addr;
	db_printf(
    "page %p obj %p pidx 0x%jx phys 0x%jx q %d ref 0x%x\n"
    "  af 0x%x of 0x%x f 0x%x act %d busy %x valid 0x%x dirty 0x%x\n",
	    m, m->object, (uintmax_t)m->pindex, (uintmax_t)m->phys_addr,
	    m->a.queue, m->ref_count, m->a.flags, m->oflags,
	    m->flags, m->a.act_count, m->busy_lock, m->valid, m->dirty);
}
#endif /* DDB */
