/*
 *  linux/arch/cris/mm/tlb.c
 *
 *  Copyright (C) 2000, 2001  Axis Communications AB
 *  
 *  Authors:   Bjorn Wesen (bjornw@axis.com)
 *
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/pgtable.h>
#include <asm/svinto.h>
#include <asm/mmu_context.h>

#define D(x)

/* CRIS in Etrax100LX TLB */

#define NUM_TLB_ENTRIES 64
#define NUM_PAGEID 64
#define INVALID_PAGEID 63
#define NO_CONTEXT -1

/* The TLB can host up to 64 different mm contexts at the same time.
 * The running context is R_MMU_CONTEXT, and each TLB entry contains a
 * page_id that has to match to give a hit. In page_id_map, we keep track
 * of which mm's we have assigned which page_id's, so that we know when
 * to invalidate TLB entries.
 *
 * The last page_id is never running - it is used as an invalid page_id
 * so we can make TLB entries that will never match.
 *
 * Notice that we need to make the flushes atomic, otherwise an interrupt
 * handler that uses vmalloced memory might cause a TLB load in the middle
 * of a flush causing.
 */

struct mm_struct *page_id_map[NUM_PAGEID];

static int map_replace_ptr = 1;  /* which page_id_map entry to replace next */

/* invalidate all TLB entries */

void
flush_tlb_all(void)
{
	int i;
	unsigned long flags;

	/* the vpn of i & 0xf is so we dont write similar TLB entries
	 * in the same 4-way entry group. details.. 
	 */

	save_and_cli(flags); /* flush needs to be atomic */
	for(i = 0; i < NUM_TLB_ENTRIES; i++) {
		*R_TLB_SELECT = ( IO_FIELD(R_TLB_SELECT, index, i) );
		*R_TLB_HI = ( IO_FIELD(R_TLB_HI, page_id, INVALID_PAGEID ) |
			      IO_FIELD(R_TLB_HI, vpn,     i & 0xf ) );
		
		*R_TLB_LO = ( IO_STATE(R_TLB_LO, global,no       ) |
			      IO_STATE(R_TLB_LO, valid, no       ) |
			      IO_STATE(R_TLB_LO, kernel,no	 ) |
			      IO_STATE(R_TLB_LO, we,    no       ) |
			      IO_FIELD(R_TLB_LO, pfn,   0        ) );
	}
	restore_flags(flags);
	D(printk("tlb: flushed all\n"));
}

/* invalidate the selected mm context only */

void
flush_tlb_mm(struct mm_struct *mm)
{
	int i;
	int page_id = mm->context;
	unsigned long flags;

	D(printk("tlb: flush mm context %d (%p)\n", page_id, mm));

	if(page_id == NO_CONTEXT)
		return;
	
	/* mark the TLB entries that match the page_id as invalid.
	 * here we could also check the _PAGE_GLOBAL bit and NOT flush
	 * global pages. is it worth the extra I/O ? 
	 */

	save_and_cli(flags);  /* flush needs to be atomic */
	for(i = 0; i < NUM_TLB_ENTRIES; i++) {
		*R_TLB_SELECT = IO_FIELD(R_TLB_SELECT, index, i);
		if (IO_EXTRACT(R_TLB_HI, page_id, *R_TLB_HI) == page_id) {
			*R_TLB_HI = ( IO_FIELD(R_TLB_HI, page_id, INVALID_PAGEID ) |
				      IO_FIELD(R_TLB_HI, vpn,     i & 0xf ) );
			
			*R_TLB_LO = ( IO_STATE(R_TLB_LO, global,no  ) |
				      IO_STATE(R_TLB_LO, valid, no  ) |
				      IO_STATE(R_TLB_LO, kernel,no  ) |
				      IO_STATE(R_TLB_LO, we,    no  ) |
				      IO_FIELD(R_TLB_LO, pfn,   0   ) );
		}
	}
	restore_flags(flags);
}

/* invalidate a single page */

void
flush_tlb_page(struct vm_area_struct *vma, 
	       unsigned long addr)
{
	struct mm_struct *mm = vma->vm_mm;
	int page_id = mm->context;
	int i;
	unsigned long flags;

	D(printk("tlb: flush page %p in context %d (%p)\n", addr, page_id, mm));

	if(page_id == NO_CONTEXT)
		return;

	addr &= PAGE_MASK; /* perhaps not necessary */

	/* invalidate those TLB entries that match both the mm context
	 * and the virtual address requested 
	 */

	save_and_cli(flags);  /* flush needs to be atomic */
	for(i = 0; i < NUM_TLB_ENTRIES; i++) {
		unsigned long tlb_hi;
		*R_TLB_SELECT = IO_FIELD(R_TLB_SELECT, index, i);
		tlb_hi = *R_TLB_HI;
		if (IO_EXTRACT(R_TLB_HI, page_id, tlb_hi) == page_id &&
		    (tlb_hi & PAGE_MASK) == addr) {
			*R_TLB_HI = IO_FIELD(R_TLB_HI, page_id, INVALID_PAGEID ) |
				addr; /* same addr as before works. */
			
			*R_TLB_LO = ( IO_STATE(R_TLB_LO, global,no  ) |
				      IO_STATE(R_TLB_LO, valid, no  ) |
				      IO_STATE(R_TLB_LO, kernel,no  ) |
				      IO_STATE(R_TLB_LO, we,    no  ) |
				      IO_FIELD(R_TLB_LO, pfn,   0   ) );
		}
	}
	restore_flags(flags);
}

/* invalidate a page range */

void
flush_tlb_range(struct mm_struct *mm, 
		unsigned long start,
		unsigned long end)
{
	int page_id = mm->context;
	int i;
	unsigned long flags;

	D(printk("tlb: flush range %p<->%p in context %d (%p)\n",
		 start, end, page_id, mm));

	if(page_id == NO_CONTEXT)
		return;

	start &= PAGE_MASK;  /* probably not necessary */
	end &= PAGE_MASK;    /* dito */

	/* invalidate those TLB entries that match both the mm context
	 * and the virtual address range
	 */

	save_and_cli(flags);  /* flush needs to be atomic */
	for(i = 0; i < NUM_TLB_ENTRIES; i++) {
		unsigned long tlb_hi, vpn;
		*R_TLB_SELECT = IO_FIELD(R_TLB_SELECT, index, i);
		tlb_hi = *R_TLB_HI;
		vpn = tlb_hi & PAGE_MASK;
		if (IO_EXTRACT(R_TLB_HI, page_id, tlb_hi) == page_id &&
		    vpn >= start && vpn < end) {
			*R_TLB_HI = ( IO_FIELD(R_TLB_HI, page_id, INVALID_PAGEID ) |
				      IO_FIELD(R_TLB_HI, vpn,     i & 0xf ) );
			
			*R_TLB_LO = ( IO_STATE(R_TLB_LO, global,no  ) |
				      IO_STATE(R_TLB_LO, valid, no  ) |
				      IO_STATE(R_TLB_LO, kernel,no  ) |
				      IO_STATE(R_TLB_LO, we,    no  ) |
				      IO_FIELD(R_TLB_LO, pfn,   0   ) );
		}
	}
	restore_flags(flags);
}

/* dump the entire TLB for debug purposes */

#if 0
void
dump_tlb_all(void)
{
	int i;
	unsigned long flags;
	
	printk("TLB dump. LO is: pfn | reserved | global | valid | kernel | we  |\n");

	save_and_cli(flags);
	for(i = 0; i < NUM_TLB_ENTRIES; i++) {
		*R_TLB_SELECT = ( IO_FIELD(R_TLB_SELECT, index, i) );
		printk("Entry %d: HI 0x%08lx, LO 0x%08lx\n",
		       i, *R_TLB_HI, *R_TLB_LO);
	}
	restore_flags(flags);
}
#endif

/*
 * Initialize the context related info for a new mm_struct
 * instance.
 */

int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	mm->context = NO_CONTEXT;
	return 0;
}

/* the following functions are similar to those used in the PPC port */

static inline void
alloc_context(struct mm_struct *mm)
{
	struct mm_struct *old_mm;

	D(printk("tlb: alloc context %d (%p)\n", map_replace_ptr, mm));

	/* did we replace an mm ? */

	old_mm = page_id_map[map_replace_ptr];

	if(old_mm) {
		/* throw out any TLB entries belonging to the mm we replace
		 * in the map
		 */
		flush_tlb_mm(old_mm);

		old_mm->context = NO_CONTEXT;
	}

	/* insert it into the page_id_map */

	mm->context = map_replace_ptr;
	page_id_map[map_replace_ptr] = mm;

	map_replace_ptr++;

	if(map_replace_ptr == INVALID_PAGEID)
		map_replace_ptr = 0;         /* wrap around */	
}

/* 
 * if needed, get a new MMU context for the mm. otherwise nothing is done.
 */

void
get_mmu_context(struct mm_struct *mm)
{
	if(mm->context == NO_CONTEXT)
		alloc_context(mm);
}

/* called in schedule() just before actually doing the switch_to */

void 
switch_mm(struct mm_struct *prev, struct mm_struct *next,
	  struct task_struct *tsk, int cpu)
{
	/* make sure we have a context */

	get_mmu_context(next);

	/* remember the pgd for the fault handlers
	 * this is similar to the pgd register in some other CPU's.
	 * we need our own copy of it because current and active_mm
	 * might be invalid at points where we still need to derefer
	 * the pgd.
	 */

	current_pgd = next->pgd;

	/* switch context in the MMU */
	
	D(printk("switching mmu_context to %d (%p)\n", next->context, next));

	*R_MMU_CONTEXT = IO_FIELD(R_MMU_CONTEXT, page_id, next->context);
}


/* called by __exit_mm to destroy the used MMU context if any before
 * destroying the mm itself. this is only called when the last user of the mm
 * drops it.
 *
 * the only thing we really need to do here is mark the used PID slot
 * as empty.
 */

void
destroy_context(struct mm_struct *mm)
{
	if(mm->context != NO_CONTEXT) {
		D(printk("destroy_context %d (%p)\n", mm->context, mm));
		flush_tlb_mm(mm);  /* TODO this might be redundant ? */
		page_id_map[mm->context] = NULL;
		/* mm->context = NO_CONTEXT; redundant.. mm will be freed */
	}
}

/* called once during VM initialization, from init.c */

void __init
tlb_init(void)
{
	int i;

	/* clear the page_id map */

	for (i = 1; i < sizeof (page_id_map) / sizeof (page_id_map[0]); i++)
		page_id_map[i] = NULL;
	
	/* invalidate the entire TLB */

	flush_tlb_all();

	/* the init_mm has context 0 from the boot */

	page_id_map[0] = &init_mm;
}
