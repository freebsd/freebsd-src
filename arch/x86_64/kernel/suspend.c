/*
 * Suspend support specific for i386.
 *
 * Distribute under GPLv2
 *
 * Copyright (c) 2002 Pavel Machek <pavel@suse.cz>
 * Copyright (c) 2001 Patrick Mochel <mochel@osdl.org>
 * 
 * AK currently useless for 24 because the core ACPI code doesn't support S3.
 *    and most of the infrastructure is missing.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/sysrq.h>
#include <linux/proc_fs.h>
#include <linux/irq.h>
#include <linux/pm.h>
#include <asm/uaccess.h>
#include <asm/acpi.h>
#include <asm/io.h>
#include <asm/proto.h>
#include <asm/i387.h>
#include <asm/desc.h>

void fix_processor_context(void);

/* Image of the saved processor state. If you touch this, fix acpi_wakeup.S. */
struct saved_context {
  	u16 ds, es, fs, gs, ss;
	unsigned long gs_base, gs_kernel_base, fs_base;
	unsigned long cr0, cr2, cr3, cr4;
	u16 gdt_pad;
	u16 gdt_limit;
	unsigned long gdt_base;
	u16 idt_pad;
	u16 idt_limit;
	unsigned long idt_base;
	u16 ldt;
	u16 tss;
	unsigned long tr;
	unsigned long safety;
	unsigned long return_address;
	unsigned long eflags;
} __attribute__((packed));

struct saved_context saved_context;

unsigned long saved_context_eax, saved_context_ebx, saved_context_ecx, saved_context_edx;
unsigned long saved_context_esp, saved_context_ebp, saved_context_esi, saved_context_edi;
unsigned long saved_context_r08, saved_context_r09, saved_context_r10, saved_context_r11;
unsigned long saved_context_r12, saved_context_r13, saved_context_r14, saved_context_r15;
unsigned long saved_context_eflags;

void save_processor_state (void)
{
	kernel_fpu_begin();

	/*
	 * descriptor tables
	 */
	asm volatile ("sgdt %0" : "=m" (saved_context.gdt_limit));
	asm volatile ("sidt %0" : "=m" (saved_context.idt_limit));
	asm volatile ("sldt %0" : "=m" (saved_context.ldt));
	asm volatile ("str %0"  : "=m" (saved_context.tr));

	/* XMM0..XMM15 should be handled by kernel_fpu_begin(). */
	/* EFER should be constant for kernel version, no need to handle it. */
	/*
	 * segment registers
	 */
	asm volatile ("movw %%ds, %0" : "=m" (saved_context.ds));
	asm volatile ("movw %%es, %0" : "=m" (saved_context.es));
	asm volatile ("movw %%fs, %0" : "=m" (saved_context.fs));
	asm volatile ("movw %%gs, %0" : "=m" (saved_context.gs));
	asm volatile ("movw %%ss, %0" : "=m" (saved_context.ss));

	rdmsrl(MSR_FS_BASE, saved_context.fs_base);
	rdmsrl(MSR_GS_BASE, saved_context.gs_base);
	rdmsrl(MSR_KERNEL_GS_BASE, saved_context.gs_kernel_base);

	/*
	 * control registers 
	 */
	asm volatile ("movq %%cr0, %0" : "=r" (saved_context.cr0));
	asm volatile ("movq %%cr2, %0" : "=r" (saved_context.cr2));
	asm volatile ("movq %%cr3, %0" : "=r" (saved_context.cr3));
	asm volatile ("movq %%cr4, %0" : "=r" (saved_context.cr4));
}

static void
do_fpu_end(void)
{
        /* restore FPU regs if necessary */
        kernel_fpu_end();
}

void restore_processor_state(void)
{
	/*
	 * control registers
	 */
	asm volatile ("movq %0, %%cr4" :: "r" (saved_context.cr4));
	asm volatile ("movq %0, %%cr3" :: "r" (saved_context.cr3));
	asm volatile ("movq %0, %%cr2" :: "r" (saved_context.cr2));
	asm volatile ("movq %0, %%cr0" :: "r" (saved_context.cr0));

	/*
	 * segment registers
	 */
	asm volatile ("movw %0, %%ds" :: "r" (saved_context.ds));
	asm volatile ("movw %0, %%es" :: "r" (saved_context.es));
	asm volatile ("movw %0, %%fs" :: "r" (saved_context.fs));
	load_gs_index(saved_context.gs);
	asm volatile ("movw %0, %%ss" :: "r" (saved_context.ss));

	wrmsrl(MSR_FS_BASE, saved_context.fs_base);
	wrmsrl(MSR_GS_BASE, saved_context.gs_base);
	wrmsrl(MSR_KERNEL_GS_BASE, saved_context.gs_kernel_base);

	/*
	 * now restore the descriptor tables to their proper values
	 * ltr is done i fix_processor_context().
	 */
	asm volatile ("lgdt %0" :: "m" (saved_context.gdt_limit));
	asm volatile ("lidt %0" :: "m" (saved_context.idt_limit));
	asm volatile ("lldt %0" :: "m" (saved_context.ldt));

	fix_processor_context();

	do_fpu_end();
}

#define loaddebug(thread,register) \
		set_debug((thread)->debugreg[register], register)

void fix_processor_context(void)
{
	int cpu = smp_processor_id();
	struct tss_struct * t = init_tss + cpu;

	set_tss_desc(cpu,t);

	/* clear busy TSS */
	gdt_cpu_table[cpu].tss.type = 9;

	syscall_init();                      /* This sets MSR_*STAR and related */
	load_TR(cpu);				/* This does ltr */
	load_LDT(current->active_mm);		/* This does lldt */

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


