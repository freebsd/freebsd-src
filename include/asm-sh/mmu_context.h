/*
 * Copyright (C) 1999 Niibe Yutaka
 *
 * ASID handling idea taken from MIPS implementation.
 */
#ifndef __ASM_SH_MMU_CONTEXT_H
#define __ASM_SH_MMU_CONTEXT_H

/* The MMU "context" consists of two things:
     (a) TLB cache version (or round, cycle whatever expression you like)
     (b) ASID (Address Space IDentifier)
 */

/*
 * Cache of MMU context last used.
 */
extern unsigned long mmu_context_cache;

#define MMU_CONTEXT_ASID_MASK		0x000000ff
#define MMU_CONTEXT_VERSION_MASK	0xffffff00
#define MMU_CONTEXT_FIRST_VERSION	0x00000100
#define NO_CONTEXT			0

/* ASID is 8-bit value, so it can't be 0x100 */
#define MMU_NO_ASID			0x100

/*
 * Virtual Page Number mask
 */
#define MMU_VPN_MASK	0xfffff000

/*
 * Get MMU context if needed.
 */
static __inline__ void
get_mmu_context(struct mm_struct *mm)
{
	extern void flush_tlb_all(void);
	unsigned long mc = mmu_context_cache;

	/* Check if we have old version of context. */
	if (((mm->context ^ mc) & MMU_CONTEXT_VERSION_MASK) == 0)
		/* It's up to date, do nothing */
		return;

	/* It's old, we need to get new context with new version. */
	mc = ++mmu_context_cache;
	if (!(mc & MMU_CONTEXT_ASID_MASK)) {
		/*
		 * We exhaust ASID of this version.
		 * Flush all TLB and start new cycle.
		 */
		flush_tlb_all();
		/*
		 * Fix version; Note that we avoid version #0
		 * to distingush NO_CONTEXT.
		 */
		if (!mc)
			mmu_context_cache = mc = MMU_CONTEXT_FIRST_VERSION;
	}
	mm->context = mc;
}

/*
 * Initialize the context related info for a new mm_struct
 * instance.
 */
static __inline__ int init_new_context(struct task_struct *tsk,
				       struct mm_struct *mm)
{
	mm->context = NO_CONTEXT;
	return 0;
}

/*
 * Destroy context related info for an mm_struct that is about
 * to be put to rest.
 */
static __inline__ void destroy_context(struct mm_struct *mm)
{
	/* Do nothing */
}

/* Other MMU related constants. */

#if defined(__sh3__)
#define MMU_PTEH	0xFFFFFFF0	/* Page table entry register HIGH */
#define MMU_PTEL	0xFFFFFFF4	/* Page table entry register LOW */
#define MMU_TTB		0xFFFFFFF8	/* Translation table base register */
#define MMU_TEA		0xFFFFFFFC	/* TLB Exception Address */

#define MMUCR		0xFFFFFFE0	/* MMU Control Register */

#define MMU_TLB_ADDRESS_ARRAY	0xF2000000
#define MMU_PAGE_ASSOC_BIT	0x80

#define MMU_NTLB_ENTRIES	128	/* for 7708 */
#define MMU_CONTROL_INIT	0x007	/* SV=0, TF=1, IX=1, AT=1 */

#elif defined(__SH4__)
#define MMU_PTEH	0xFF000000	/* Page table entry register HIGH */
#define MMU_PTEL	0xFF000004	/* Page table entry register LOW */
#define MMU_TTB		0xFF000008	/* Translation table base register */
#define MMU_TEA		0xFF00000C	/* TLB Exception Address */
#define MMU_PTEA	0xFF000034	/* Page table entry assistance register */

#define MMUCR		0xFF000010	/* MMU Control Register */

#define MMU_ITLB_ADDRESS_ARRAY	0xF2000000
#define MMU_UTLB_ADDRESS_ARRAY	0xF6000000
#define MMU_PAGE_ASSOC_BIT	0x80

#define MMU_NTLB_ENTRIES	64	/* for 7750 */
#define MMU_CONTROL_INIT	0x205	/* SQMD=1, SV=0, TI=1, AT=1 */

#define MMU_ITLB_DATA_ARRAY	0xF3000000
#define MMU_UTLB_DATA_ARRAY	0xF7000000

#define MMU_UTLB_ENTRIES	   64
#define MMU_U_ENTRY_SHIFT	    8
#define MMU_UTLB_VALID		0x100
#define MMU_ITLB_ENTRIES	    4
#define MMU_I_ENTRY_SHIFT	    8
#define MMU_ITLB_VALID		0x100
#endif

static __inline__ void set_asid(unsigned long asid)
{
	unsigned long __dummy;

	__asm__ __volatile__ ("mov.l	%2, %0\n\t"
			      "and	%3, %0\n\t"
			      "or	%1, %0\n\t"
			      "mov.l	%0, %2"
			      : "=&r" (__dummy)
			      : "r" (asid), "m" (__m(MMU_PTEH)),
			        "r" (0xffffff00));
}

static __inline__ unsigned long get_asid(void)
{
	unsigned long asid;

	__asm__ __volatile__ ("mov.l	%1, %0"
			      : "=r" (asid)
			      : "m" (__m(MMU_PTEH)));
	asid &= MMU_CONTEXT_ASID_MASK;
	return asid;
}

/*
 * After we have set current->mm to a new value, this activates
 * the context for the new mm so we see the new mappings.
 */
static __inline__ void activate_context(struct mm_struct *mm)
{
	get_mmu_context(mm);
	set_asid(mm->context & MMU_CONTEXT_ASID_MASK);
}

/* MMU_TTB can be used for optimizing the fault handling.
   (Currently not used) */
static __inline__ void switch_mm(struct mm_struct *prev,
				 struct mm_struct *next,
				 struct task_struct *tsk, unsigned int cpu)
{
	if (prev != next) {
		unsigned long __pgdir = (unsigned long)next->pgd;

		__asm__ __volatile__("mov.l	%0, %1"
				     : /* no output */
				     : "r" (__pgdir), "m" (__m(MMU_TTB)));
		activate_context(next);
	}
}

#define activate_mm(prev, next) \
	switch_mm((prev),(next),NULL,smp_processor_id())

static __inline__ void
enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk, unsigned cpu)
{
}

#endif /* __ASM_SH_MMU_CONTEXT_H */
