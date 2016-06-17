/*
 * IA32 helper functions
 *
 * Copyright (C) 1999 Arun Sharma <arun.sharma@intel.com>
 * Copyright (C) 2000 Asit K. Mallick <asit.k.mallick@intel.com>
 * Copyright (C) 2001-2002 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * 06/16/00	A. Mallick	added csd/ssd/tssd for ia32 thread context
 * 02/19/01	D. Mosberger	dropped tssd; it's not needed
 * 09/14/01	D. Mosberger	fixed memory management for gdt/tss page
 * 09/29/01	D. Mosberger	added ia32_load_segment_descriptors()
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/personality.h>
#include <linux/sched.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <asm/ia32.h>

extern void die_if_kernel (char *str, struct pt_regs *regs, long err);

struct exec_domain ia32_exec_domain;
struct page *ia32_shared_page[(2*IA32_PAGE_SIZE + PAGE_SIZE - 1)/PAGE_SIZE];
unsigned long *ia32_gdt;

static unsigned long
load_desc (u16 selector)
{
	unsigned long *table, limit, index;

	if (!selector)
		return 0;
	if (selector & IA32_SEGSEL_TI) {
		table = (unsigned long *) IA32_LDT_OFFSET;
		limit = IA32_LDT_ENTRIES;
	} else {
		table = ia32_gdt;
		limit = IA32_PAGE_SIZE / sizeof(ia32_gdt[0]);
	}
	index = selector >> IA32_SEGSEL_INDEX_SHIFT;
	if (index >= limit)
		return 0;
	return IA32_SEG_UNSCRAMBLE(table[index]);
}

void
ia32_load_segment_descriptors (struct task_struct *task)
{
	struct pt_regs *regs = ia64_task_regs(task);

	/* Setup the segment descriptors */
	regs->r24 = load_desc(regs->r16 >> 16);		/* ESD */
	regs->r27 = load_desc(regs->r16 >>  0);		/* DSD */
	regs->r28 = load_desc(regs->r16 >> 32);		/* FSD */
	regs->r29 = load_desc(regs->r16 >> 48);		/* GSD */
	regs->ar_csd = load_desc(regs->r17 >>  0);	/* CSD */
	regs->ar_ssd = load_desc(regs->r17 >> 16);	/* SSD */
}

void
ia32_save_state (struct task_struct *t)
{
	unsigned long eflag, fsr, fcr, fir, fdr;

	asm ("mov %0=ar.eflag;"
	     "mov %1=ar.fsr;"
	     "mov %2=ar.fcr;"
	     "mov %3=ar.fir;"
	     "mov %4=ar.fdr;"
	     : "=r"(eflag), "=r"(fsr), "=r"(fcr), "=r"(fir), "=r"(fdr));
	t->thread.eflag = eflag;
	t->thread.fsr = fsr;
	t->thread.fcr = fcr;
	t->thread.fir = fir;
	t->thread.fdr = fdr;
	ia64_set_kr(IA64_KR_IO_BASE, t->thread.old_iob);
	ia64_set_kr(IA64_KR_TSSD, t->thread.old_k1);
}

void
ia32_load_state (struct task_struct *t)
{
	unsigned long eflag, fsr, fcr, fir, fdr, tssd;
	struct pt_regs *regs = ia64_task_regs(t);
	int nr = smp_processor_id();	/* LDT and TSS depend on CPU number: */

	eflag = t->thread.eflag;
	fsr = t->thread.fsr;
	fcr = t->thread.fcr;
	fir = t->thread.fir;
	fdr = t->thread.fdr;
	tssd = load_desc(_TSS(nr));					/* TSSD */

	asm volatile ("mov ar.eflag=%0;"
		      "mov ar.fsr=%1;"
		      "mov ar.fcr=%2;"
		      "mov ar.fir=%3;"
		      "mov ar.fdr=%4;"
		      :: "r"(eflag), "r"(fsr), "r"(fcr), "r"(fir), "r"(fdr));
	current->thread.old_iob = ia64_get_kr(IA64_KR_IO_BASE);
	current->thread.old_k1 = ia64_get_kr(IA64_KR_TSSD);
	ia64_set_kr(IA64_KR_IO_BASE, IA32_IOBASE);
	ia64_set_kr(IA64_KR_TSSD, tssd);

	regs->r17 = (_TSS(nr) << 48) | (_LDT(nr) << 32) | (__u32) regs->r17;
	regs->r30 = load_desc(_LDT(nr));				/* LDTD */
}

/*
 * Setup IA32 GDT and TSS
 */
void
ia32_gdt_init (void)
{
	unsigned long *tss;
	unsigned long ldt_size;
	int nr;

	ia32_shared_page[0] = alloc_page(GFP_KERNEL);
	ia32_gdt = page_address(ia32_shared_page[0]);
	tss = ia32_gdt + IA32_PAGE_SIZE/sizeof(ia32_gdt[0]);

	if (IA32_PAGE_SIZE == PAGE_SIZE) {
		ia32_shared_page[1] = alloc_page(GFP_KERNEL);
		tss = page_address(ia32_shared_page[1]);
	}

	/* CS descriptor in IA-32 (scrambled) format */
	ia32_gdt[__USER_CS >> 3] = IA32_SEG_DESCRIPTOR(0, (IA32_PAGE_OFFSET-1) >> IA32_PAGE_SHIFT,
						       0xb, 1, 3, 1, 1, 1, 1);

	/* DS descriptor in IA-32 (scrambled) format */
	ia32_gdt[__USER_DS >> 3] = IA32_SEG_DESCRIPTOR(0, (IA32_PAGE_OFFSET-1) >> IA32_PAGE_SHIFT,
						       0x3, 1, 3, 1, 1, 1, 1);

	/* We never change the TSS and LDT descriptors, so we can share them across all CPUs.  */
	ldt_size = PAGE_ALIGN(IA32_LDT_ENTRIES*IA32_LDT_ENTRY_SIZE);
	for (nr = 0; nr < NR_CPUS; ++nr) {
		ia32_gdt[_TSS(nr) >> IA32_SEGSEL_INDEX_SHIFT]
			= IA32_SEG_DESCRIPTOR(IA32_TSS_OFFSET, 235,
					      0xb, 0, 3, 1, 1, 1, 0);
		ia32_gdt[_LDT(nr) >> IA32_SEGSEL_INDEX_SHIFT]
			= IA32_SEG_DESCRIPTOR(IA32_LDT_OFFSET, ldt_size - 1,
					      0x2, 0, 3, 1, 1, 1, 0);
	}
}

/*
 * Handle bad IA32 interrupt via syscall
 */
void
ia32_bad_interrupt (unsigned long int_num, struct pt_regs *regs)
{
	siginfo_t siginfo;

	die_if_kernel("Bad IA-32 interrupt", regs, int_num);

	siginfo.si_signo = SIGTRAP;
	siginfo.si_errno = int_num;	/* XXX is it OK to abuse si_errno like this? */
	siginfo.si_flags = 0;
	siginfo.si_isr = 0;
	siginfo.si_addr = 0;
	siginfo.si_imm = 0;
	siginfo.si_code = TRAP_BRKPT;
	force_sig_info(SIGTRAP, &siginfo, current);
}

static int __init
ia32_init (void)
{
	ia32_exec_domain.name = "Linux/x86";
	ia32_exec_domain.handler = NULL;
	ia32_exec_domain.pers_low = PER_LINUX32;
	ia32_exec_domain.pers_high = PER_LINUX32;
	ia32_exec_domain.signal_map = default_exec_domain.signal_map;
	ia32_exec_domain.signal_invmap = default_exec_domain.signal_invmap;
	register_exec_domain(&ia32_exec_domain);
	return 0;
}

__initcall(ia32_init);
