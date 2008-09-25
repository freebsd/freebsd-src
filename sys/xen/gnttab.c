/******************************************************************************
 * gnttab.c
 * 
 * Two sets of functionality:
 * 1. Granting foreign access to our memory reservation.
 * 2. Accessing others' memory reservations via grant references.
 * (i.e., mechanisms for both sender and recipient of grant references)
 * 
 * Copyright (c) 2005, Christopher Clark
 * Copyright (c) 2004, K A Fraser
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_global.h"
#include "opt_pmap.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <machine/xen/hypervisor.h>
#include <machine/xen/synch_bitops.h>
#include <xen/gnttab.h>

#define cmpxchg(a, b, c) atomic_cmpset_int((volatile u_int *)(a),(b),(c))

#if 1
#define ASSERT(_p) \
    if ( !(_p) ) { printk("Assertion '%s': line %d, file %s\n", \
    #_p , __LINE__, __FILE__); *(int*)0=0; }
#else
#define ASSERT(_p) ((void)0)
#endif

#define WPRINTK(fmt, args...) \
    printk("xen_grant: " fmt, ##args)

/* External tools reserve first few grant table entries. */
#define NR_RESERVED_ENTRIES 8
#define GNTTAB_LIST_END 0xffffffff
#define GREFS_PER_GRANT_FRAME (PAGE_SIZE / sizeof(grant_entry_t))

static grant_ref_t **gnttab_list;
static unsigned int nr_grant_frames;
static unsigned int boot_max_nr_grant_frames;
static int gnttab_free_count;
static grant_ref_t gnttab_free_head;
static struct mtx gnttab_list_lock;

static grant_entry_t *shared;

static struct gnttab_free_callback *gnttab_free_callback_list = NULL;

static int gnttab_expand(unsigned int req_entries);

#define RPP (PAGE_SIZE / sizeof(grant_ref_t))
#define gnttab_entry(entry) (gnttab_list[(entry) / RPP][(entry) % RPP])

static int
get_free_entries(int count)
{
	int ref, rc;
	grant_ref_t head;
	
	mtx_lock(&gnttab_list_lock);
	if ((gnttab_free_count < count) &&
	    ((rc = gnttab_expand(count - gnttab_free_count)) < 0)) {
		mtx_unlock(&gnttab_list_lock);
		return (rc);
	}
	ref = head = gnttab_free_head;
	gnttab_free_count -= count;
	while (count-- > 1)
		head = gnttab_entry(head);
	gnttab_free_head = gnttab_entry(head);
	gnttab_entry(head) = GNTTAB_LIST_END;
	mtx_unlock(&gnttab_list_lock);	
	return (ref);
}

#define get_free_entry() get_free_entries(1)

static void
do_free_callbacks(void)
{
	struct gnttab_free_callback *callback, *next;

	callback = gnttab_free_callback_list;
	gnttab_free_callback_list = NULL;

	while (callback != NULL) {
		next = callback->next;
		if (gnttab_free_count >= callback->count) {
			callback->next = NULL;
			callback->fn(callback->arg);
		} else {
			callback->next = gnttab_free_callback_list;
			gnttab_free_callback_list = callback;
		}
		callback = next;
	}
}

static inline void
check_free_callbacks(void)
{
	if (unlikely(gnttab_free_callback_list != NULL))
		do_free_callbacks();
}

static void
put_free_entry(grant_ref_t ref)
{

	mtx_lock(&gnttab_list_lock);
	gnttab_entry(ref) = gnttab_free_head;
	gnttab_free_head = ref;
	gnttab_free_count++;
	check_free_callbacks();
	mtx_unlock(&gnttab_list_lock);	
}

/*
 * Public grant-issuing interface functions
 */

int
gnttab_grant_foreign_access(domid_t domid, unsigned long frame, int readonly)
{
	int ref;

	if (unlikely((ref = get_free_entry()) == -1))
		return -ENOSPC;

	shared[ref].frame = frame;
	shared[ref].domid = domid;
	wmb();
	shared[ref].flags = GTF_permit_access | (readonly ? GTF_readonly : 0);

	return ref;
}

void
gnttab_grant_foreign_access_ref(grant_ref_t ref, domid_t domid,
				unsigned long frame, int readonly)
{
	shared[ref].frame = frame;
	shared[ref].domid = domid;
	wmb();
	shared[ref].flags = GTF_permit_access | (readonly ? GTF_readonly : 0);
}

int
gnttab_query_foreign_access(grant_ref_t ref)
{
	uint16_t nflags;
	
	nflags = shared[ref].flags;
	
	return (nflags & (GTF_reading|GTF_writing));
}

int
gnttab_end_foreign_access_ref(grant_ref_t ref, int readonly)
{
	uint16_t flags, nflags;

	nflags = shared[ref].flags;
	do {
		if ( (flags = nflags) & (GTF_reading|GTF_writing) ) {
			printf("WARNING: g.e. still in use!\n");
			return (0);
		}
	} while ((nflags = synch_cmpxchg(&shared[ref].flags, flags, 0)) !=
	       flags);

	return (1);
}

void
gnttab_end_foreign_access(grant_ref_t ref, int readonly, void *page)
{
	if (gnttab_end_foreign_access_ref(ref, readonly)) {
		put_free_entry(ref);
		if (page != NULL) {
			free(page, M_DEVBUF);
		}
	}
	else {
		/* XXX This needs to be fixed so that the ref and page are
		   placed on a list to be freed up later. */
		printf("WARNING: leaking g.e. and page still in use!\n");
	}
}

int
gnttab_grant_foreign_transfer(domid_t domid, unsigned long pfn)
{
	int ref;
	
	if (unlikely((ref = get_free_entry()) == -1))
		return -ENOSPC;

	gnttab_grant_foreign_transfer_ref(ref, domid, pfn);
	
	return (ref);
}

void
gnttab_grant_foreign_transfer_ref(grant_ref_t ref, domid_t domid,
	unsigned long pfn)
{
	shared[ref].frame = pfn;
	shared[ref].domid = domid;
	wmb();
	shared[ref].flags = GTF_accept_transfer;
}

unsigned long
gnttab_end_foreign_transfer_ref(grant_ref_t ref)
{
	unsigned long frame;
	uint16_t      flags;

	/*
         * If a transfer is not even yet started, try to reclaim the grant
         * reference and return failure (== 0).
         */
	while (!((flags = shared[ref].flags) & GTF_transfer_committed)) {
		if ( synch_cmpxchg(&shared[ref].flags, flags, 0) == flags )
			return (0);
		cpu_relax();
	}

	/* If a transfer is in progress then wait until it is completed. */
	while (!(flags & GTF_transfer_completed)) {
		flags = shared[ref].flags;
		cpu_relax();
	}

	/* Read the frame number /after/ reading completion status. */
	rmb();
	frame = shared[ref].frame;
	PANIC_IF(frame == 0);

	return (frame);
}

unsigned long
gnttab_end_foreign_transfer(grant_ref_t ref)
{
	unsigned long frame = gnttab_end_foreign_transfer_ref(ref);

	put_free_entry(ref);
	return (frame);
}

void
gnttab_free_grant_reference(grant_ref_t ref)
{

	put_free_entry(ref);
}

void
gnttab_free_grant_references(grant_ref_t head)
{
	grant_ref_t ref;
	int count = 1;
	
	if (head == GNTTAB_LIST_END)
		return;
	
	mtx_lock(&gnttab_list_lock);
	ref = head;
	while (gnttab_entry(ref) != GNTTAB_LIST_END) {
		ref = gnttab_entry(ref);
		count++;
	}
	gnttab_entry(ref) = gnttab_free_head;
	gnttab_free_head = head;
	gnttab_free_count += count;
	check_free_callbacks();
	mtx_unlock(&gnttab_list_lock);
}

int
gnttab_alloc_grant_references(uint16_t count, grant_ref_t *head)
{
	int h = get_free_entries(count);

	if (h == -1)
		return -ENOSPC;

	*head = h;

	return 0;
}

int
gnttab_empty_grant_references(const grant_ref_t *private_head)
{
	return (*private_head == GNTTAB_LIST_END);
}

int
gnttab_claim_grant_reference(grant_ref_t *private_head)
{
	grant_ref_t g = *private_head;

	if (unlikely(g == GNTTAB_LIST_END))
		return -ENOSPC;
	*private_head = gnttab_entry(g);

	return (g);
}

void
gnttab_release_grant_reference(grant_ref_t *private_head, grant_ref_t  release)
{
	gnttab_entry(release) = *private_head;
	*private_head = release;
}

void
gnttab_request_free_callback(struct gnttab_free_callback *callback,
			     void (*fn)(void *), void *arg, uint16_t count)
{

	mtx_lock(&gnttab_list_lock);
	if (callback->next)
		goto out;
	callback->fn = fn;
	callback->arg = arg;
	callback->count = count;
	callback->next = gnttab_free_callback_list;
	gnttab_free_callback_list = callback;
	check_free_callbacks();
 out:
	mtx_unlock(&gnttab_list_lock);

}

void
gnttab_cancel_free_callback(struct gnttab_free_callback *callback)
{
	struct gnttab_free_callback **pcb;

	mtx_lock(&gnttab_list_lock);
	for (pcb = &gnttab_free_callback_list; *pcb; pcb = &(*pcb)->next) {
		if (*pcb == callback) {
			*pcb = callback->next;
			break;
		}
	}
	mtx_unlock(&gnttab_list_lock);
}


static int
grow_gnttab_list(unsigned int more_frames)
{
	unsigned int new_nr_grant_frames, extra_entries, i;

	new_nr_grant_frames = nr_grant_frames + more_frames;
	extra_entries       = more_frames * GREFS_PER_GRANT_FRAME;

	for (i = nr_grant_frames; i < new_nr_grant_frames; i++)
	{
		gnttab_list[i] = (grant_ref_t *)malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);

		if (!gnttab_list[i])
			goto grow_nomem;
	}

	for (i = GREFS_PER_GRANT_FRAME * nr_grant_frames;
	     i < GREFS_PER_GRANT_FRAME * new_nr_grant_frames - 1; i++)
		gnttab_entry(i) = i + 1;

	gnttab_entry(i) = gnttab_free_head;
	gnttab_free_head = GREFS_PER_GRANT_FRAME * nr_grant_frames;
	gnttab_free_count += extra_entries;

	nr_grant_frames = new_nr_grant_frames;

	check_free_callbacks();

	return 0;
	
grow_nomem:
	for ( ; i >= nr_grant_frames; i--)
		free(gnttab_list[i], M_DEVBUF);
	return (-ENOMEM);
}

static unsigned int
__max_nr_grant_frames(void)
{
	struct gnttab_query_size query;
	int rc;

	query.dom = DOMID_SELF;

	rc = HYPERVISOR_grant_table_op(GNTTABOP_query_size, &query, 1);
	if ((rc < 0) || (query.status != GNTST_okay))
		return (4); /* Legacy max supported number of frames */

	return (query.max_nr_frames);
}

static inline
unsigned int max_nr_grant_frames(void)
{
	unsigned int xen_max = __max_nr_grant_frames();

	if (xen_max > boot_max_nr_grant_frames)
		return (boot_max_nr_grant_frames);
	return (xen_max);
}

#ifdef notyet
/*
 * XXX needed for backend support
 *
 */
static int
map_pte_fn(pte_t *pte, struct page *pmd_page,
		      unsigned long addr, void *data)
{
	unsigned long **frames = (unsigned long **)data;

	set_pte_at(&init_mm, addr, pte, pfn_pte_ma((*frames)[0], PAGE_KERNEL));
	(*frames)++;
	return 0;
}

static int
unmap_pte_fn(pte_t *pte, struct page *pmd_page,
			unsigned long addr, void *data)
{

	set_pte_at(&init_mm, addr, pte, __pte(0));
	return 0;
}
#endif

static int
gnttab_map(unsigned int start_idx, unsigned int end_idx)
{
	struct gnttab_setup_table setup;
#ifdef __LP64__
	uint64_t *frames;
#else	
	uint32_t *frames;
#endif
	unsigned int nr_gframes = end_idx + 1;
	int i, rc;

	frames = malloc(nr_gframes * sizeof(unsigned long), M_DEVBUF, M_NOWAIT);
	if (!frames)
		return -ENOMEM;

	setup.dom        = DOMID_SELF;
	setup.nr_frames  = nr_gframes;
	set_xen_guest_handle(setup.frame_list, frames);

	rc = HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup, 1);
	if (rc == -ENOSYS) {
		free(frames, M_DEVBUF);
		return -ENOSYS;
	}
	PANIC_IF(rc || setup.status);

	if (shared == NULL) {
		vm_offset_t area;
		
		area = kmem_alloc_nofault(kernel_map,
		    PAGE_SIZE * max_nr_grant_frames());
		PANIC_IF(area == 0);
		shared = (grant_entry_t *)area;
	}
	for (i = 0; i < nr_gframes; i++)
		PT_SET_MA(((caddr_t)shared) + i*PAGE_SIZE, 
		    ((vm_paddr_t)frames[i]) << PAGE_SHIFT | PG_RW | PG_V);

	free(frames, M_DEVBUF);
	
	return 0;
}

int
gnttab_resume(void)
{
	if (max_nr_grant_frames() < nr_grant_frames)
		return -ENOSYS;
	return gnttab_map(0, nr_grant_frames - 1);
}

int
gnttab_suspend(void)
{	
	int i, pages;

	pages = (PAGE_SIZE*nr_grant_frames) >> PAGE_SHIFT;

	for (i = 0; i < pages; i++)
		PT_SET_MA(shared + (i*PAGE_SIZE), (vm_paddr_t)0);

	return (0);
}

static int
gnttab_expand(unsigned int req_entries)
{
	int rc;
	unsigned int cur, extra;

	cur = nr_grant_frames;
	extra = ((req_entries + (GREFS_PER_GRANT_FRAME-1)) /
		 GREFS_PER_GRANT_FRAME);
	if (cur + extra > max_nr_grant_frames())
		return -ENOSPC;

	if ((rc = gnttab_map(cur, cur + extra - 1)) == 0)
		rc = grow_gnttab_list(extra);

	return rc;
}

static int 
gnttab_init(void *unused)
{
	int i;
	unsigned int max_nr_glist_frames;
	unsigned int nr_init_grefs;

	if (!is_running_on_xen())
		return -ENODEV;

	nr_grant_frames = 1;
	boot_max_nr_grant_frames = __max_nr_grant_frames();

	/* Determine the maximum number of frames required for the
	 * grant reference free list on the current hypervisor.
	 */
	max_nr_glist_frames = (boot_max_nr_grant_frames *
			       GREFS_PER_GRANT_FRAME /
			       (PAGE_SIZE / sizeof(grant_ref_t)));

	gnttab_list = malloc(max_nr_glist_frames * sizeof(grant_ref_t *),
	    M_DEVBUF, M_NOWAIT);

	if (gnttab_list == NULL)
		return -ENOMEM;

	for (i = 0; i < nr_grant_frames; i++) {
		gnttab_list[i] = (grant_ref_t *)malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
		if (gnttab_list[i] == NULL)
			goto ini_nomem;
	}
	
	if (gnttab_resume() < 0)
		return -ENODEV;
	
	nr_init_grefs = nr_grant_frames * GREFS_PER_GRANT_FRAME;

	for (i = NR_RESERVED_ENTRIES; i < nr_init_grefs - 1; i++)
		gnttab_entry(i) = i + 1;

	gnttab_entry(nr_init_grefs - 1) = GNTTAB_LIST_END;
	gnttab_free_count = nr_init_grefs - NR_RESERVED_ENTRIES;
	gnttab_free_head  = NR_RESERVED_ENTRIES;

	printk("Grant table initialized\n");
	return 0;

ini_nomem:
	for (i--; i >= 0; i--)
		free(gnttab_list[i], M_DEVBUF);
	free(gnttab_list, M_DEVBUF);
	return -ENOMEM;

}

MTX_SYSINIT(gnttab, &gnttab_list_lock, "GNTTAB LOCK", MTX_DEF); 
SYSINIT(gnttab, SI_SUB_PSEUDO, SI_ORDER_FIRST, gnttab_init, NULL);
