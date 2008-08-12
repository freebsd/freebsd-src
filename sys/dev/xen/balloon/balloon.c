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
#include <sys/mutex.h>

#include <machine/hypervisor-ifs.h>
#include <machine/xen-os.h>
#include <machine/xenbus.h>

/*
 * Protects atomic reservation decrease/increase against concurrent increases.
 * Also protects non-atomic updates of current_pages and driver_pages, and
 * balloon lists.
 */
struct mtx balloon_lock;
#ifdef notyet

/* We aim for 'current allocation' == 'target allocation'. */
static unsigned long current_pages;
static unsigned long target_pages;

/* VM /proc information for memory */
extern unsigned long totalram_pages;

/* We may hit the hard limit in Xen. If we do then we remember it. */
static unsigned long hard_limit;

/*
 * Drivers may alter the memory reservation independently, but they must
 * inform the balloon driver so that we can avoid hitting the hard limit.
 */
static unsigned long driver_pages;

struct balloon_entry {
	vm_page_t page;
	STAILQ_ENTRY(balloon_entry) list;
};

/* List of ballooned pages, threaded through the mem_map array. */
static STAILQ_HEAD(,balloon_entry) ballooned_pages;

static unsigned long balloon_low, balloon_high;


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

	entry = malloc(sizeof(struct balloon_entry), M_WAITOK);

	STAILQ_INSERT_HEAD(&ballooned_pages, entry, list);
	balloon_low++;
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
	
	balloon_low--;

	return page;
}

static void 
balloon_alarm(unsigned long unused)
{
	wakeup(balloon_process);
}

static unsigned long 
current_target(void)
{
	unsigned long target = min(target_pages, hard_limit);
	if (target > (current_pages + balloon_low + balloon_high))
		target = current_pages + balloon_low + balloon_high;
	return target;
}

static int 
increase_reservation(unsigned long nr_pages)
{
	unsigned long *mfn_list, pfn, i, flags;
	struct page   *page;
	long           rc;
	struct xen_memory_reservation reservation = {
		.address_bits = 0,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};

	if (nr_pages > (PAGE_SIZE / sizeof(unsigned long)))
		nr_pages = PAGE_SIZE / sizeof(unsigned long);

	mfn_list = (unsigned long *)malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (mfn_list == NULL)
		return ENOMEM;


	reservation.extent_start = mfn_list;
	reservation.nr_extents   = nr_pages;
	rc = HYPERVISOR_memory_op(
		XENMEM_increase_reservation, &reservation);
	if (rc < nr_pages) {
		int ret;
		/* We hit the Xen hard limit: reprobe. */
		reservation.extent_start = mfn_list;
		reservation.nr_extents   = rc;
		ret = HYPERVISOR_memory_op(XENMEM_decrease_reservation,
				&reservation);
		PANIC_IF(ret != rc);
		hard_limit = current_pages + rc - driver_pages;
		goto out;
	}

	for (i = 0; i < nr_pages; i++) {
		page = balloon_retrieve();
		PANIC_IF(page == NULL);

		pfn = (VM_PAGE_TO_PHYS(page) >> PAGE_SHIFT);
		PANIC_IF(phys_to_machine_mapping_valid(pfn));

		/* Update P->M and M->P tables. */
		PFNTOMFN(pfn) = mfn_list[i];
		xen_machphys_update(mfn_list[i], pfn);
            
		/* Relinquish the page back to the allocator. */
		ClearPageReserved(page);
		set_page_count(page, 1);
		vm_page_free(page);
	}

	current_pages += nr_pages;
	totalram_pages = current_pages;

 out:
	balloon_unlock(flags);

	free((mfn_list);

	return 0;
}

static int 
decrease_reservation(unsigned long nr_pages)
{
	unsigned long *mfn_list, pfn, i, flags;
	struct page   *page;
	void          *v;
	int            need_sleep = 0;
	int ret;
	struct xen_memory_reservation reservation = {
		.address_bits = 0,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};

	if (nr_pages > (PAGE_SIZE / sizeof(unsigned long)))
		nr_pages = PAGE_SIZE / sizeof(unsigned long);

	mfn_list = (unsigned long *)malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (mfn_list == NULL)
		return ENOMEM;

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
		mfn_list[i] = PFNTOMFN(pfn);
	}

	balloon_lock(flags);

	/* No more mappings: invalidate P2M and add to balloon. */
	for (i = 0; i < nr_pages; i++) {
		pfn = MFNTOPFN(mfn_list[i]);
		PFNTOMFN(pfn) = INVALID_P2M_ENTRY;
		balloon_append(PHYS_TO_VM_PAGE(pfn << PAGE_SHIFT));
	}

	reservation.extent_start = mfn_list;
	reservation.nr_extents   = nr_pages;
	ret = HYPERVISOR_memory_op(XENMEM_decrease_reservation, &reservation);
	PANIC_IF(ret != nr_pages);

	current_pages -= nr_pages;
	totalram_pages = current_pages;

	balloon_unlock(flags);

	free(mfn_list, M_DEVBUF);

	return need_sleep;
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
	
	for (;;) {
		do {
			credit = current_target() - current_pages;
			if (credit > 0)
				need_sleep = (increase_reservation(credit) != 0);
			if (credit < 0)
				need_sleep = (decrease_reservation(-credit) != 0);
			
#ifndef CONFIG_PREEMPT
			if (need_resched())
				schedule();
#endif
		} while ((credit != 0) && !need_sleep);
		
		/* Schedule more work if there is some still to be done. */
		if (current_target() != current_pages)
			timeout(balloon_alarm, NULL, ticks + HZ);

			msleep(balloon_process, balloon_lock, 0, "balloon", -1);
	}

}

/* Resets the Xen limit, sets new target, and kicks off processing. */
static void 
set_new_target(unsigned long target)
{
	/* No need for lock. Not read-modify-write updates. */
	hard_limit   = ~0UL;
	target_pages = target;
	wakeup(balloon_process);
}

static struct xenbus_watch target_watch =
{
	.node = "memory/target"
};

/* React to a change in the target key */
static void 
watch_target(struct xenbus_watch *watch,
	     const char **vec, unsigned int len)
{
	unsigned long long new_target;
	int err;

	err = xenbus_scanf(NULL, "memory", "target", "%llu", &new_target);
	if (err != 1) {
		/* This is ok (for domain0 at least) - so just return */
		return;
	} 
        
	/* The given memory/target value is in KiB, so it needs converting to
	   pages.  PAGE_SHIFT converts bytes to pages, hence PAGE_SHIFT - 10.
	*/
	set_new_target(new_target >> (PAGE_SHIFT - 10));
    
}

static void 
balloon_init_watcher(void *)
{
	int err;

	err = register_xenbus_watch(&target_watch);
	if (err)
		printf("Failed to set balloon watcher\n");

}

static void 
balloon_init(void *)
{
	unsigned long pfn;
	struct page *page;

	IPRINTK("Initialising balloon driver.\n");

	if (xen_init() < 0)
		return -1;

	current_pages = min(xen_start_info->nr_pages, max_pfn);
	target_pages  = current_pages;
	balloon_low   = 0;
	balloon_high  = 0;
	driver_pages  = 0UL;
	hard_limit    = ~0UL;

	init_timer(&balloon_timer);
	balloon_timer.data = 0;
	balloon_timer.function = balloon_alarm;
    
	/* Initialise the balloon with excess memory space. */
	for (pfn = xen_start_info->nr_pages; pfn < max_pfn; pfn++) {
		page = PHYS_TO_VM_PAGE(pfn << PAGE_SHIFT);
		balloon_append(page);
	}

	target_watch.callback = watch_target;
    
	return 0;
}

void 
balloon_update_driver_allowance(long delta)
{
	unsigned long flags;

	balloon_lock(flags);
	driver_pages += delta;
	balloon_unlock(flags);
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
	PANIC_IF(ret != 1);
	return 0;
}

#endif
vm_page_t
balloon_alloc_empty_page_range(unsigned long nr_pages)
{
	unsigned long flags;
	vm_page_t pages;
	int i;
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
		PANIC_IF(HYPERVISOR_memory_op(XENMEM_decrease_reservation, &reservation) != nr_pages);
	}

	current_pages -= nr_pages;

	wakeup(balloon_process);

	return pages;
}

void 
balloon_dealloc_empty_page_range(vm_page_t page, unsigned long nr_pages)
{
	unsigned long i, flags;

	for (i = 0; i < nr_pages; i++)
		balloon_append(page + i);

	wakeup(balloon_process);
}

#endif
