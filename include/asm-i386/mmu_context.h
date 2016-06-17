#ifndef __I386_MMU_CONTEXT_H
#define __I386_MMU_CONTEXT_H

#include <linux/config.h>
#include <asm/desc.h>
#include <asm/atomic.h>
#include <asm/pgalloc.h>

/*
 * hooks to add arch specific data into the mm struct.
 * Note that destroy_context is called even if init_new_context
 * fails.
 */
int init_new_context(struct task_struct *tsk, struct mm_struct *mm);
void destroy_context(struct mm_struct *mm);

#ifdef CONFIG_SMP

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk, unsigned cpu)
{
	if(cpu_tlbstate[cpu].state == TLBSTATE_OK)
		cpu_tlbstate[cpu].state = TLBSTATE_LAZY;	
}
#else
static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk, unsigned cpu)
{
}
#endif

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next, struct task_struct *tsk, unsigned cpu)
{
	if (prev != next) {
		/* stop flush ipis for the previous mm */
		clear_bit(cpu, &prev->cpu_vm_mask);
#ifdef CONFIG_SMP
		cpu_tlbstate[cpu].state = TLBSTATE_OK;
		cpu_tlbstate[cpu].active_mm = next;
#endif
		set_bit(cpu, &next->cpu_vm_mask);
		/* Re-load page tables */
		load_cr3(next->pgd);
	 	/* load_LDT, if either the previous or next thread
		 * has a non-default LDT.
		 */
		if (next->context.size+prev->context.size)
			load_LDT(&next->context);
	}
#ifdef CONFIG_SMP
	else {
		cpu_tlbstate[cpu].state = TLBSTATE_OK;
		if(cpu_tlbstate[cpu].active_mm != next)
			out_of_line_bug();
		if(!test_and_set_bit(cpu, &next->cpu_vm_mask)) {
			/* We were in lazy tlb mode and leave_mm disabled 
			 * tlb flush IPI delivery. We must reload %cr3.
			 */
			load_cr3(next->pgd);
			load_LDT(&next->context);
		}
	}
#endif
}

#define activate_mm(prev, next) \
	switch_mm((prev),(next),NULL,smp_processor_id())

#endif
