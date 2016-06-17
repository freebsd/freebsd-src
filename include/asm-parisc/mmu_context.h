#ifndef __PARISC_MMU_CONTEXT_H
#define __PARISC_MMU_CONTEXT_H

#include <asm/pgalloc.h>

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk, unsigned cpu)
{
}

/* on PA-RISC, we actually have enough contexts to justify an allocator
 * for them.  prumpf */

extern unsigned long alloc_sid(void);
extern void free_sid(unsigned long);

static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	if (atomic_read(&mm->mm_users) != 1)
	    BUG();

	mm->context = alloc_sid();
	return 0;
}

static inline void
destroy_context(struct mm_struct *mm)
{
	free_sid(mm->context);
	mm->context = 0;
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next, struct task_struct *tsk, unsigned cpu)
{

	if (prev != next) {
		mtctl(__pa(next->pgd), 25);
		load_context(next->context);
	}
}

static inline void activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
	/*
	 * Activate_mm is our one chance to allocate a space id
	 * for a new mm created in the exec path. There's also
	 * some lazy tlb stuff, which is currently dead code, but
	 * we only allocate a space id if one hasn't been allocated
	 * already, so we should be OK.
	 */

	if (next == &init_mm) BUG(); /* Should never happen */

	if (next->context == 0)
	    next->context = alloc_sid();

	switch_mm(prev,next,current,0);
}
#endif
