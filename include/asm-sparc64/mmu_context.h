/* $Id: mmu_context.h,v 1.51 2001/08/17 04:55:09 kanoj Exp $ */
#ifndef __SPARC64_MMU_CONTEXT_H
#define __SPARC64_MMU_CONTEXT_H

/* Derived heavily from Linus's Alpha/AXP ASN code... */

#include <asm/page.h>

/*
 * For the 8k pagesize kernel, use only 10 hw context bits to optimize some shifts in
 * the fast tlbmiss handlers, instead of all 13 bits (specifically for vpte offset
 * calculation). For other pagesizes, this optimization in the tlbhandlers can not be 
 * done; but still, all 13 bits can not be used because the tlb handlers use "andcc"
 * instruction which sign extends 13 bit arguments.
 */
#if PAGE_SHIFT == 13
#define CTX_VERSION_SHIFT	10
#define TAG_CONTEXT_BITS	0x3ff
#else
#define CTX_VERSION_SHIFT	12
#define TAG_CONTEXT_BITS	0xfff
#endif

#ifndef __ASSEMBLY__

#include <linux/spinlock.h>
#include <asm/system.h>
#include <asm/spitfire.h>

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk, unsigned cpu)
{
}

extern spinlock_t ctx_alloc_lock;
extern unsigned long tlb_context_cache;
extern unsigned long mmu_context_bmap[];

#define CTX_VERSION_MASK	((~0UL) << CTX_VERSION_SHIFT)
#define CTX_FIRST_VERSION	((1UL << CTX_VERSION_SHIFT) + 1UL)
#define CTX_VALID(__ctx)	\
	 (!(((__ctx) ^ tlb_context_cache) & CTX_VERSION_MASK))
#define CTX_HWBITS(__ctx)	((__ctx) & ~CTX_VERSION_MASK)

extern void get_new_mmu_context(struct mm_struct *mm);

/* Initialize a new mmu context.  This is invoked when a new
 * address space instance (unique or shared) is instantiated.
 * This just needs to set mm->context to an invalid context.
 */
#define init_new_context(__tsk, __mm)	(((__mm)->context = 0UL), 0)

/* Destroy a dead context.  This occurs when mmput drops the
 * mm_users count to zero, the mmaps have been released, and
 * all the page tables have been flushed.  Our job is to destroy
 * any remaining processor-specific state, and in the sparc64
 * case this just means freeing up the mmu context ID held by
 * this task if valid.
 */
#define destroy_context(__mm)					\
do {	spin_lock(&ctx_alloc_lock);				\
	if (CTX_VALID((__mm)->context)) {			\
		unsigned long nr = CTX_HWBITS((__mm)->context);	\
		mmu_context_bmap[nr>>6] &= ~(1UL << (nr & 63));	\
	}							\
	spin_unlock(&ctx_alloc_lock);				\
} while(0)

/* Reload the two core values used by TLB miss handler
 * processing on sparc64.  They are:
 * 1) The physical address of mm->pgd, when full page
 *    table walks are necessary, this is where the
 *    search begins.
 * 2) A "PGD cache".  For 32-bit tasks only pgd[0] is
 *    ever used since that maps the entire low 4GB
 *    completely.  To speed up TLB miss processing we
 *    make this value available to the handlers.  This
 *    decreases the amount of memory traffic incurred.
 */
#define reload_tlbmiss_state(__tsk, __mm) \
do { \
	register unsigned long paddr asm("o5"); \
	register unsigned long pgd_cache asm("o4"); \
	paddr = __pa((__mm)->pgd); \
	pgd_cache = 0UL; \
	if ((__tsk)->thread.flags & SPARC_FLAG_32BIT) \
		pgd_cache = pgd_val((__mm)->pgd[0]) << 11UL; \
	__asm__ __volatile__("wrpr	%%g0, 0x494, %%pstate\n\t" \
			     "mov	%3, %%g4\n\t" \
			     "mov	%0, %%g7\n\t" \
			     "stxa	%1, [%%g4] %2\n\t" \
			     "membar	#Sync\n\t" \
			     "wrpr	%%g0, 0x096, %%pstate" \
			     : /* no outputs */ \
			     : "r" (paddr), "r" (pgd_cache),\
			       "i" (ASI_DMMU), "i" (TSB_REG)); \
} while(0)

/* Set MMU context in the actual hardware. */
#define load_secondary_context(__mm) \
	__asm__ __volatile__("stxa	%0, [%1] %2\n\t" \
			     "flush	%%g6" \
			     : /* No outputs */ \
			     : "r" (CTX_HWBITS((__mm)->context)), \
			       "r" (0x10), "i" (ASI_DMMU))

extern void __flush_tlb_mm(unsigned long, unsigned long);

/* Switch the current MM context. */
static inline void switch_mm(struct mm_struct *old_mm, struct mm_struct *mm, struct task_struct *tsk, int cpu)
{
	unsigned long ctx_valid;

	spin_lock(&mm->page_table_lock);
	if (CTX_VALID(mm->context))
		ctx_valid = 1;
        else
		ctx_valid = 0;

	if (!ctx_valid || (old_mm != mm)) {
		if (!ctx_valid)
			get_new_mmu_context(mm);

		load_secondary_context(mm);
		reload_tlbmiss_state(tsk, mm);
	}

	{
		unsigned long vm_mask = (1UL << cpu);

		/* Even if (mm == old_mm) we _must_ check
		 * the cpu_vm_mask.  If we do not we could
		 * corrupt the TLB state because of how
		 * smp_flush_tlb_{page,range,mm} on sparc64
		 * and lazy tlb switches work. -DaveM
		 */
		if (!ctx_valid || !(mm->cpu_vm_mask & vm_mask)) {
			mm->cpu_vm_mask |= vm_mask;
			__flush_tlb_mm(CTX_HWBITS(mm->context), SECONDARY_CONTEXT);
		}
	}
	spin_unlock(&mm->page_table_lock);
}

/* Activate a new MM instance for the current task. */
static inline void activate_mm(struct mm_struct *active_mm, struct mm_struct *mm)
{
	unsigned long vm_mask;

	spin_lock(&mm->page_table_lock);
	if (!CTX_VALID(mm->context))
		get_new_mmu_context(mm);
	vm_mask = (1UL << smp_processor_id());
	if (!(mm->cpu_vm_mask & vm_mask))
		mm->cpu_vm_mask |= vm_mask;
	spin_unlock(&mm->page_table_lock);

	load_secondary_context(mm);
	__flush_tlb_mm(CTX_HWBITS(mm->context), SECONDARY_CONTEXT);
	reload_tlbmiss_state(current, mm);
}

#endif /* !(__ASSEMBLY__) */

#endif /* !(__SPARC64_MMU_CONTEXT_H) */
