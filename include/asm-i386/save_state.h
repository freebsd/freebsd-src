#ifndef __ASM_I386_SAVE_STATE_H
#define __ASM_I386_SAVE_STATE_H

/*
 * Copyright 2001-2002 Pavel Machek <pavel@suse.cz>
 * Based on code
 * Copyright 2001 Patrick Mochel <mochel@osdl.org>
 */
#include <asm/desc.h>
#include <asm/i387.h>

/* image of the saved processor state */
struct saved_context {
	u32 eax, ebx, ecx, edx;
	u32 esp, ebp, esi, edi;
	u16 es, fs, gs, ss;
	u32 cr0, cr2, cr3, cr4;
	u16 gdt_pad;
	u16 gdt_limit;
	u32 gdt_base;
	u16 idt_pad;
	u16 idt_limit;
	u32 idt_base;
	u16 ldt;
	u16 tss;
	u32 tr;
	u32 safety;
	u32 return_address;
	u32 eflags;
} __attribute__((packed));

static struct saved_context saved_context;

#define loaddebug(thread,register) \
               __asm__("movl %0,%%db" #register  \
                       : /* no output */ \
                       :"r" ((thread)->debugreg[register]))


/*
 * save_processor_context
 * 
 * Save the state of the processor before we go to sleep.
 *
 * return_stack is the value of the stack pointer (%esp) as the caller sees it.
 * A good way could not be found to obtain it from here (don't want to make _too_
 * many assumptions about the layout of the stack this far down.) Also, the 
 * handy little __builtin_frame_pointer(level) where level > 0, is blatantly 
 * buggy - it returns the value of the stack at the proper location, not the 
 * location, like it should (as of gcc 2.91.66)
 * 
 * Note that the context and timing of this function is pretty critical.
 * With a minimal amount of things going on in the caller and in here, gcc
 * does a good job of being just a dumb compiler.  Watch the assembly output
 * if anything changes, though, and make sure everything is going in the right
 * place. 
 */
static inline void save_processor_context (void)
{
	kernel_fpu_begin();

	/*
	 * descriptor tables
	 */
	asm volatile ("sgdt (%0)" : "=m" (saved_context.gdt_limit));
	asm volatile ("sidt (%0)" : "=m" (saved_context.idt_limit));
	asm volatile ("sldt (%0)" : "=m" (saved_context.ldt));
	asm volatile ("str (%0)"  : "=m" (saved_context.tr));

	/*
	 * save the general registers.
	 * note that gcc has constructs to specify output of certain registers,
	 * but they're not used here, because it assumes that you want to modify
	 * those registers, so it tries to be smart and save them beforehand.
	 * It's really not necessary, and kinda fishy (check the assembly output),
	 * so it's avoided. 
	 */
	asm volatile ("movl %%esp, (%0)" : "=m" (saved_context.esp));
	asm volatile ("movl %%eax, (%0)" : "=m" (saved_context.eax));
	asm volatile ("movl %%ebx, (%0)" : "=m" (saved_context.ebx));
	asm volatile ("movl %%ecx, (%0)" : "=m" (saved_context.ecx));
	asm volatile ("movl %%edx, (%0)" : "=m" (saved_context.edx));
	asm volatile ("movl %%ebp, (%0)" : "=m" (saved_context.ebp));
	asm volatile ("movl %%esi, (%0)" : "=m" (saved_context.esi));
	asm volatile ("movl %%edi, (%0)" : "=m" (saved_context.edi));

	/*
	 * segment registers
	 */
	asm volatile ("movw %%es, %0" : "=r" (saved_context.es));
	asm volatile ("movw %%fs, %0" : "=r" (saved_context.fs));
	asm volatile ("movw %%gs, %0" : "=r" (saved_context.gs));
	asm volatile ("movw %%ss, %0" : "=r" (saved_context.ss));

	/*
	 * control registers 
	 */
	asm volatile ("movl %%cr0, %0" : "=r" (saved_context.cr0));
	asm volatile ("movl %%cr2, %0" : "=r" (saved_context.cr2));
	asm volatile ("movl %%cr3, %0" : "=r" (saved_context.cr3));
	asm volatile ("movl %%cr4, %0" : "=r" (saved_context.cr4));

	/*
	 * eflags
	 */
	asm volatile ("pushfl ; popl (%0)" : "=m" (saved_context.eflags));
}

static void fix_processor_context(void)
{
	int nr = smp_processor_id();
	struct tss_struct * t = &init_tss[nr];

	set_tss_desc(nr,t);	/* This just modifies memory; should not be neccessary. But... This is neccessary, because 386 hardware has concept of busy tsc or some similar stupidity. */
        gdt_table[__TSS(nr)].b &= 0xfffffdff;

	load_TR(nr);		/* This does ltr */
	__load_LDT(nr);		/* This does lldt */

	/*
	 * Now maybe reload the debug registers
	 */
	if (current->thread.debugreg[7]){
                loaddebug(&current->thread, 0);
                loaddebug(&current->thread, 1);
                loaddebug(&current->thread, 2);
                loaddebug(&current->thread, 3);
                /* no 4 and 5 */
                loaddebug(&current->thread, 6);
                loaddebug(&current->thread, 7);
	}

}

static void
do_fpu_end(void)
{
        /* restore FPU regs if necessary */
	/* Do it out of line so that gcc does not move cr0 load to some stupid place */
        kernel_fpu_end();
}

/*
 * restore_processor_context
 * 
 * Restore the processor context as it was before we went to sleep
 * - descriptor tables
 * - control registers
 * - segment registers
 * - flags
 * 
 * Note that it is critical that this function is declared inline.  
 * It was separated out from restore_state to make that function
 * a little clearer, but it needs to be inlined because we won't have a
 * stack when we get here (so we can't push a return address).
 */
static inline void restore_processor_context (void)
{
	/*
	 * first restore %ds, so we can access our data properly
	 */
	asm volatile (".align 4");
	asm volatile ("movw %0, %%ds" :: "r" ((u16)__KERNEL_DS));


	/*
	 * control registers
	 */
	asm volatile ("movl %0, %%cr4" :: "r" (saved_context.cr4));
	asm volatile ("movl %0, %%cr3" :: "r" (saved_context.cr3));
	asm volatile ("movl %0, %%cr2" :: "r" (saved_context.cr2));
	asm volatile ("movl %0, %%cr0" :: "r" (saved_context.cr0));
	
	/*
	 * segment registers
	 */
	asm volatile ("movw %0, %%es" :: "r" (saved_context.es));
	asm volatile ("movw %0, %%fs" :: "r" (saved_context.fs));
	asm volatile ("movw %0, %%gs" :: "r" (saved_context.gs));
	asm volatile ("movw %0, %%ss" :: "r" (saved_context.ss));

	/*
	 * the other general registers
	 *
	 * note that even though gcc has constructs to specify memory 
	 * input into certain registers, it will try to be too smart
	 * and save them at the beginning of the function.  This is esp.
	 * bad since we don't have a stack set up when we enter, and we 
	 * want to preserve the values on exit. So, we set them manually.
	 */
	asm volatile ("movl %0, %%esp" :: "m" (saved_context.esp));
	asm volatile ("movl %0, %%ebp" :: "m" (saved_context.ebp));
	asm volatile ("movl %0, %%eax" :: "m" (saved_context.eax));
	asm volatile ("movl %0, %%ebx" :: "m" (saved_context.ebx));
	asm volatile ("movl %0, %%ecx" :: "m" (saved_context.ecx));
	asm volatile ("movl %0, %%edx" :: "m" (saved_context.edx));
	asm volatile ("movl %0, %%esi" :: "m" (saved_context.esi));
	asm volatile ("movl %0, %%edi" :: "m" (saved_context.edi));

	/*
	 * now restore the descriptor tables to their proper values
	 */
	asm volatile ("lgdt (%0)" :: "m" (saved_context.gdt_limit));
	asm volatile ("lidt (%0)" :: "m" (saved_context.idt_limit));
	asm volatile ("lldt (%0)" :: "m" (saved_context.ldt));

	fix_processor_context();
	do_fpu_end();
}

#endif 

