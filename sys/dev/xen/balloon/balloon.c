/******************************************************************************
 * balloon.c
 *
 * Xen balloon driver - enables returning/claiming memory to/from Xen.
 *
 * Copyright (c) 2003, B Dragovic
 * Copyright (c) 2003-2004, M Williamson, K Fraser
 * Copyright (c) 2005 Dan M. Smith, IBM Corporation
 * 
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <machine/xen/xen-os.h>
#include <machine/xen/xenvar.h>
#include <machine/xen/xenfunc.h>
#include <xen/hypervisor.h>
#include <xen/xenstore/xenstorevar.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

MALLOC_DEFINE(M_BALLOON, "Balloon", "Xen Balloon Driver");

struct mtx balloon_mutex;

/*
 * Protects atomic reservation decrease/increase against concurrent increases.
 * Also protects non-atomic updates of current_pages and driver_pages, and
 * balloon lists.
 */
struct mtx balloon_lock;

/* We increase/decrease in batches which fit in a page */
static unsigned long frame_list[PAGE_SIZE / sizeof(unsigned long)];
#define ARRAY_SIZE(A)	(sizeof(A) / sizeof(A[0]))

struct balloon_stats {
	/* We aim for 'current allocation' == 'target allocation'. */
	unsigned long current_pages;
	unsigned long target_pages;
	/* We may hit the hard limit in Xen. If we do then we remember it. */
	unsigned long hard_limit;
	/*
	 * Drivers may alter the memory reservation independently, but they
	 * must inform the balloon driver so we avoid hitting the hard limit.
	 */
	unsigned long driver_pages;
	/* Number of pages in high- and low-memory balloons. */
	unsigned long balloon_low;
	unsigned long balloon_high;
};

static struct balloon_stats balloon_stats;
#define bs balloon_stats

SYSCTL_DECL(_dev_xen);
SYSCTL_NODE(_dev_xen, OID_AUTO, balloon, CTLFLAG_RD, NULL, "Balloon");
SYSCTL_ULONG(_dev_xen_balloon, OID_AUTO, current, CTLFLAG_RD,
    &bs.current_pages, 0, "Current allocation");
SYSCTL_ULONG(_dev_xen_balloon, OID_AUTO, target, CTLFLAG_RD,
    &bs.target_pages, 0, "Target allocation");
SYSCTL_ULONG(_dev_xen_balloon, OID_AUTO, driver_pages, CTLFLAG_RD,
    &bs.driver_pages, 0, "Driver pages");
SYSCTL_ULONG(_dev_xen_balloon, OID_AUTO, hard_limit, CTLFLAG_RD,
    &bs.hard_limit, 0, "Xen hard limit");
SYSCTL_ULONG(_dev_xen_balloon, OID_AUTO, low_mem, CTLFLAG_RD,
    &bs.balloon_low, 0, "Low-mem balloon");
SYSCTL_ULONG(_dev_xen_balloon, OID_AUTO, high_mem, CTLFLAG_RD,
    &bs.balloon_high, 0, "High-mem balloon");

struct balloon_entry {
	vm_page_t page;
	STAILQ_ENTRY(balloon_entry) list;
};

/* List of ballooned pages, threaded through the mem_map array. */
static STAILQ_HEAD(,balloon_entry) ballooned_pages;

/* Main work function, always executed in process context. */
static void balloon_process(void *unused);

#define IPRINTK(fmt, args...) \
	printk(KERN_INFO "xen_mem: " fmt, ##args)
#define WPRINTK(fmt, args...) \
	printk(KERN_WARNING "xen_mem: " fmt, ##args)

/* balloon_append: add the given page to the balloon. */
static void 
balloon_append(vm_page_t page)
{
	struct balloon_entry *entry;

	entry = malloc(sizeof(struct balloon_entry), M_BALLOON, M_WAITOK);
	entry->page = page;
	STAILQ_INSERT_HEAD(&ballooned_pages, entry, list);
	bs.balloon_low++;
}

/* balloon_retrieve: rescue a page from the balloon, if it is not empty. */
static vm_page_t
balloon_retrieve(void)
{
	vm_page_t page;
	struct balloon_entry *entry;

	if (STAILQ_EMPTY(&ballooned_pages))
		return NULL;

	entry = STAILQ_FIRST(&ballooned_pages);
	STAILQ_REMOVE_HEAD(&ballooned_pages, list);

	page = entry->page;
	free(entry, M_DEVBUF);
	
	bs.balloon_low--;

	return page;
}

static void 
balloon_alarm(void *unused)
{
	wakeup(balloon_process);
}

static unsigned long 
current_target(void)
{
	unsigned long target = min(bs.target_pages, bs.hard_limit);
	if (target > (bs.current_pages + bs.balloon_low + bs.balloon_high))
		target = bs.current_pages + bs.balloon_low + bs.balloon_high;
	return target;
}

static unsigned long
minimum_target(void)
{
#ifdef XENHVM
#define max_pfn physmem
#else
#define max_pfn HYPERVISOR_shared_info->arch.max_pfn
#endif
	unsigned long min_pages, curr_pages = current_target();

#define MB2PAGES(mb) ((mb) << (20 - PAGE_SHIFT))
	/* Simple continuous piecewiese linear function:
	 *  max MiB -> min MiB	gradient
	 *       0	   0
	 *      16	  16
	 *      32	  24
	 *     128	  72	(1/2)
	 *     512 	 168	(1/4)
	 *    2048	 360	(1/8)
	 *    8192	 552	(1/32)
	 *   32768	1320
	 *  131072	4392
	 */
	if (max_pfn < MB2PAGES(128))
		min_pages = MB2PAGES(8) + (max_pfn >> 1);
	else if (max_pfn < MB2PAGES(512))
		min_pages = MB2PAGES(40) + (max_pfn >> 2);
	else if (max_pfn < MB2PAGES(2048))
		min_pages = MB2PAGES(104) + (max_pfn >> 3);
	else
		min_pages = MB2PAGES(296) + (max_pfn >> 5);
#undef MB2PAGES

	/* Don't enforce growth */
	return min(min_pages, curr_pages);
#ifndef CONFIG_XEN
#undef max_pfn
#endif
}

static int 
increase_reservation(unsigned long nr_pages)
{
	unsigned long  pfn, i;
	struct balloon_entry *entry;
	vm_page_t      page;
	long           rc;
	struct xen_memory_reservation reservation = {
		.address_bits = 0,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};

	if (nr_pages > ARRAY_SIZE(frame_list))
		nr_pages = ARRAY_SIZE(frame_list);

	mtx_lock(&balloon_lock);

	for (entry = STAILQ_FIRST(&ballooned_pages), i = 0;
	     i < nr_pages; i++, entry = STAILQ_NEXT(entry, list)) {
		KASSERT(entry, ("ballooned_pages list corrupt"));
		page = entry->page;
		frame_list[i] = (VM_PAGE_TO_PHYS(page) >> PAGE_SHIFT);
	}

	set_xen_guest_handle(reservation.extent_start, frame_list);
	reservation.nr_extents   = nr_pages;
	rc = HYPERVISOR_memory_op(
		XENMEM_populate_physmap, &reservation);
	if (rc < nr_pages) {
		if (rc > 0) {
			int ret;

			/* We hit the Xen hard limit: reprobe. */
			reservation.nr_extents = rc;
			ret = HYPERVISOR_memory_op(XENMEM_decrease_reservation,
					&reservation);
			KASSERT(ret == rc, ("HYPERVISOR_memory_op failed"));
		}
		if (rc >= 0)
			bs.hard_limit = (bs.current_pages + rc -
					 bs.driver_pages);
		goto out;
	}

	for (i = 0; i < nr_pages; i++) {
		page = balloon_retrieve();
		KASSERT(page, ("balloon_retrieve failed"));

		pfn = (VM_PAGE_TO_PHYS(page) >> PAGE_SHIFT);
		KASSERT((xen_feature(XENFEAT_auto_translated_physmap) ||
			!phys_to_machine_mapping_valid(pfn)),
		    ("auto translated physmap but mapping is valid"));

		set_phys_to_machine(pfn, frame_list[i]);

#if 0
#ifndef XENHVM
		/* Link back into the page tables if not highmem. */
		if (pfn < max_low_pfn) {
			int ret;
			ret = HYPERVISOR_update_va_mapping(
				(unsigned long)__va(pfn << PAGE_SHIFT),
				pfn_pte_ma(frame_list[i], PAGE_KERNEL),
				0);
			PASSING(ret == 0,
			    ("HYPERVISOR_update_va_mapping failed"));
		}
#endif
#endif

		/* Relinquish the page back to the allocator. */
		vm_page_unwire(page, 0);
		vm_page_free(page);
	}

	bs.current_pages += nr_pages;
	//totalram_pages = bs.current_pages;

 out:
	mtx_unlock(&balloon_lock);

	return 0;
}

static int
decrease_reservation(unsigned long nr_pages)
{
	unsigned long  pfn, i;
	vm_page_t      page;
	int            need_sleep = 0;
	int ret;
	struct xen_memory_reservation reservation = {
		.address_bits = 0,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};

	if (nr_pages > ARRAY_SIZE(frame_list))
		nr_pages = ARRAY_SIZE(frame_list);

	for (i = 0; i < nr_pages; i++) {
		int color = 0;
		if ((page = vm_page_alloc(NULL, color++, 
			    VM_ALLOC_NORMAL | VM_ALLOC_NOOBJ | 
			    VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL) {
			nr_pages = i;
			need_sleep = 1;
			break;
		}

		pfn = (VM_PAGE_TO_PHYS(page) >> PAGE_SHIFT);
		frame_list[i] = PFNTOMFN(pfn);

#if 0
		if (!PageHighMem(page)) {
			v = phys_to_virt(pfn << PAGE_SHIFT);
			scrub_pages(v, 1);
#ifdef CONFIG_XEN
			ret = HYPERVISOR_update_va_mapping(
				(unsigned long)v, __pte_ma(0), 0);
			BUG_ON(ret);
#endif
		}
#endif
#ifdef CONFIG_XEN_SCRUB_PAGES
		else {
			v = kmap(page);
			scrub_pages(v, 1);
			kunmap(page);
		}
#endif
	}

#ifdef CONFIG_XEN
	/* Ensure that ballooned highmem pages don't have kmaps. */
	kmap_flush_unused();
	flush_tlb_all();
#endif

	mtx_lock(&balloon_lock);

	/* No more mappings: invalidate P2M and add to balloon. */
	for (i = 0; i < nr_pages; i++) {
		pfn = MFNTOPFN(frame_list[i]);
		set_phys_to_machine(pfn, INVALID_P2M_ENTRY);
		balloon_append(PHYS_TO_VM_PAGE(pfn << PAGE_SHIFT));
	}

	set_xen_guest_handle(reservation.extent_start, frame_list);
	reservation.nr_extents   = nr_pages;
	ret = HYPERVISOR_memory_op(XENMEM_decrease_reservation, &reservation);
	KASSERT(ret == nr_pages, ("HYPERVISOR_memory_op failed"));

	bs.current_pages -= nr_pages;
	//totalram_pages = bs.current_pages;

	mtx_unlock(&balloon_lock);

	return (need_sleep);
}

/*
 * We avoid multiple worker processes conflicting via the balloon mutex.
 * We may of course race updates of the target counts (which are protected
 * by the balloon lock), or with changes to the Xen hard limit, but we will
 * recover from these in time.
 */
static void 
balloon_process(void *unused)
{
	int need_sleep = 0;
	long credit;
	
	mtx_lock(&balloon_mutex);
	for (;;) {
		do {
			credit = current_target() - bs.current_pages;
			if (credit > 0)
				need_sleep = (increase_reservation(credit) != 0);
			if (credit < 0)
				need_sleep = (decrease_reservation(-credit) != 0);
			
		} while ((credit != 0) && !need_sleep);
		
		/* Schedule more work if there is some still to be done. */
		if (current_target() != bs.current_pages)
			timeout(balloon_alarm, NULL, ticks + hz);

		msleep(balloon_process, &balloon_mutex, 0, "balloon", -1);
	}
	mtx_unlock(&balloon_mutex);
}

/* Resets the Xen limit, sets new target, and kicks off processing. */
static void 
set_new_target(unsigned long target)
{
	/* No need for lock. Not read-modify-write updates. */
	bs.hard_limit   = ~0UL;
	bs.target_pages = max(target, minimum_target());
	wakeup(balloon_process);
}

static struct xs_watch target_watch =
{
	.node = "memory/target"
};

/* React to a change in the target key */
static void 
watch_target(struct xs_watch *watch,
	     const char **vec, unsigned int len)
{
	unsigned long long new_target;
	int err;

	err = xs_scanf(XST_NIL, "memory", "target", NULL,
	    "%llu", &new_target);
	if (err) {
		/* This is ok (for domain0 at least) - so just return */
		return;
	} 
        
	/* The given memory/target value is in KiB, so it needs converting to
	   pages.  PAGE_SHIFT converts bytes to pages, hence PAGE_SHIFT - 10.
	*/
	set_new_target(new_target >> (PAGE_SHIFT - 10));
    
}

static void 
balloon_init_watcher(void *arg)
{
	int err;

	err = xs_register_watch(&target_watch);
	if (err)
		printf("Failed to set balloon watcher\n");

}
SYSINIT(balloon_init_watcher, SI_SUB_PSEUDO, SI_ORDER_ANY,
    balloon_init_watcher, NULL);

static void 
balloon_init(void *arg)
{
#ifndef XENHVM
	vm_page_t page;
	unsigned long pfn;

#define max_pfn HYPERVISOR_shared_info->arch.max_pfn
#endif

	if (!is_running_on_xen())
		return;

	mtx_init(&balloon_lock, "balloon_lock", NULL, MTX_DEF);
	mtx_init(&balloon_mutex, "balloon_mutex", NULL, MTX_DEF);

#ifndef XENHVM
	bs.current_pages = min(xen_start_info->nr_pages, max_pfn);
#else
	bs.current_pages = physmem;
#endif
	bs.target_pages  = bs.current_pages;
	bs.balloon_low   = 0;
	bs.balloon_high  = 0;
	bs.driver_pages  = 0UL;
	bs.hard_limit    = ~0UL;

	kproc_create(balloon_process, NULL, NULL, 0, 0, "balloon");
//	init_timer(&balloon_timer);
//	balloon_timer.data = 0;
//	balloon_timer.function = balloon_alarm;
    
#ifndef XENHVM
	/* Initialise the balloon with excess memory space. */
	for (pfn = xen_start_info->nr_pages; pfn < max_pfn; pfn++) {
		page = PHYS_TO_VM_PAGE(pfn << PAGE_SHIFT);
		balloon_append(page);
	}
#undef max_pfn
#endif

	target_watch.callback = watch_target;
    
	return;
}
SYSINIT(balloon_init, SI_SUB_PSEUDO, SI_ORDER_ANY, balloon_init, NULL);

void balloon_update_driver_allowance(long delta);

void 
balloon_update_driver_allowance(long delta)
{
	mtx_lock(&balloon_lock);
	bs.driver_pages += delta;
	mtx_unlock(&balloon_lock);
}

#if 0
static int dealloc_pte_fn(
	pte_t *pte, struct page *pte_page, unsigned long addr, void *data)
{
	unsigned long mfn = pte_mfn(*pte);
	int ret;
	struct xen_memory_reservation reservation = {
		.extent_start = &mfn,
		.nr_extents   = 1,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};
	set_pte_at(&init_mm, addr, pte, __pte_ma(0));
	set_phys_to_machine(__pa(addr) >> PAGE_SHIFT, INVALID_P2M_ENTRY);
	ret = HYPERVISOR_memory_op(XENMEM_decrease_reservation, &reservation);
	KASSERT(ret == 1, ("HYPERVISOR_memory_op failed"));
	return 0;
}

#endif

#if 0
vm_page_t
balloon_alloc_empty_page_range(unsigned long nr_pages)
{
	vm_page_t pages;
	int i, rc;
	unsigned long *mfn_list;
	struct xen_memory_reservation reservation = {
		.address_bits = 0,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};

	pages = vm_page_alloc_contig(nr_pages, 0, -1, 4, 4)
	if (pages == NULL)
		return NULL;
	
	mfn_list = malloc(nr_pages*sizeof(unsigned long), M_DEVBUF, M_WAITOK);
	
	for (i = 0; i < nr_pages; i++) {
		mfn_list[i] = PFNTOMFN(VM_PAGE_TO_PHYS(pages[i]) >> PAGE_SHIFT);
		PFNTOMFN(i) = INVALID_P2M_ENTRY;
		reservation.extent_start = mfn_list;
		reservation.nr_extents = nr_pages;
		rc = HYPERVISOR_memory_op(XENMEM_decrease_reservation,
		    &reservation);
		KASSERT(rc == nr_pages, ("HYPERVISOR_memory_op failed"));
	}

	current_pages -= nr_pages;

	wakeup(balloon_process);

	return pages;
}

void 
balloon_dealloc_empty_page_range(vm_page_t page, unsigned long nr_pages)
{
	unsigned long i;

	for (i = 0; i < nr_pages; i++)
		balloon_append(page + i);

	wakeup(balloon_process);
}
#endif
