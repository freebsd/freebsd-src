/*
 *  linux/arch/x86-64/kernel/process.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *
 *  Pentium III FXSR, SSE support
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 * 
 *  X86-64 port
 *	Andi Kleen.
 * 
 *  $Id: process.c,v 1.77 2004/03/22 00:37:29 ak Exp $
 */

/*
 * This file handles the architecture-dependent parts of process handling..
 */

#define __KERNEL_SYSCALLS__
#include <stdarg.h>

#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/interrupt.h>
#include <linux/config.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/slab.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/ldt.h>
#include <asm/processor.h>
#include <asm/i387.h>
#include <asm/desc.h>
#include <asm/mmu_context.h>
#include <asm/pda.h>
#include <asm/prctl.h>
#include <asm/kdebug.h>
#include <asm/proto.h>
#include <asm/apic.h>

#include <linux/irq.h>

asmlinkage extern void ret_from_fork(void);

int hlt_counter;

/*
 * Powermanagement idle function, if any..
 */
void (*pm_idle)(void);

/*
 * Power off function, if any
 */
void (*pm_power_off)(void);

void disable_hlt(void)
{
	hlt_counter++;
}

void enable_hlt(void)
{
	hlt_counter--;
}

/*
 * We use this if we don't have any better
 * idle routine..
 */
static void default_idle(void)
{
	if (!hlt_counter) {
		__cli();
		if (!current->need_resched)
			safe_halt();
		else
			__sti();
	}
}

/*
 * On SMP it's slightly faster (but much more power-consuming!)
 * to poll the ->need_resched flag instead of waiting for the
 * cross-CPU IPI to arrive. Use this option with caution.
 */
static void poll_idle (void)
{
	int oldval;

	__sti();

	/*
	 * Deal with another CPU just having chosen a thread to
	 * run here:
	 */
	oldval = xchg(&current->need_resched, -1);

	if (!oldval)
		asm volatile(
			"2:"
			"cmpl $-1, %0;"
			"rep; nop;"
			"je 2b;"
				: :"m" (current->need_resched));
}

/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */
void cpu_idle (void)
{
	/* endless idle loop with no priority at all */
	init_idle();
	current->nice = 20;
	current->counter = -100;

	while (1) {
		void (*idle)(void) = pm_idle;
		if (!idle)
			idle = default_idle;
		while (!current->need_resched)
			idle();
		schedule();
		check_pgt_cache();
	}
}

/*
 * This is a kind of hybrid between poll and halt idle routines. This uses new
 * Monitor/Mwait instructions on P4 processors with PNI. We Monitor 
 * need_resched and go to optimized wait state through Mwait. 
 * Whenever someone changes need_resched, we would be woken up from Mwait 
 * (without an IPI).
 */
static void mwait_idle (void)
{
	int oldval;

	__sti();
	/* Setting need_resched to -1 skips sending IPI during idle resched */
	oldval = xchg(&current->need_resched, -1);
	if (!oldval) {
		do {
			__monitor((void *)&current->need_resched, 0, 0);
			if (current->need_resched != -1)
				break;
			__mwait(0, 0);
		} while (current->need_resched == -1);
	}
}

int __init select_idle_routine(struct cpuinfo_x86 *c)
{
	if (cpu_has(c, X86_FEATURE_MWAIT)) {
		printk("Monitor/Mwait feature present.\n");
		/*
		 * Take care of system with asymmetric CPUs.
		 * Use, mwait_idle only if all cpus support it.
		 * If not, we fallback to default_idle()
		 */
		if (!pm_idle) {
			pm_idle = mwait_idle;
		}
		return 1;
	}
	pm_idle = default_idle;
	return 1;
}


static int __init idle_setup (char *str)
{
	if (!strncmp(str, "poll", 4)) {
		printk("using polling idle threads.\n");
		pm_idle = poll_idle;
	} else if (!strncmp(str, "halt", 4)) {
		printk("using halt in idle threads.\n");
                pm_idle = default_idle;
	}

	return 1;
}

__setup("idle=", idle_setup);

static struct { long x; } no_idt[3];
static enum { 
	BOOT_BIOS = 'b',
	BOOT_TRIPLE = 't', 
	BOOT_KBD = 'k',
} reboot_type = BOOT_KBD;
static int reboot_mode = 0; 

/* reboot=b[ios] | t[riple] | k[bd] [, [w]arm | [c]old]
   bios	  Use the CPU reboot vector for warm reset
   warm   Don't set the cold reboot flag
   cold   Set the cold reboto flag
   triple Force a triple fault (init)
   kbd    Use the keyboard controller. cold reset (default)
 */ 
static int __init reboot_setup(char *str)
{
	for (;;) {
		switch (*str) {
		case 'w': 
			reboot_mode = 0x1234;
			break;

		case 'c':
			reboot_mode = 0;
			break;

		case 't':
		case 'b':
		case 'k':
			reboot_type = *str;
			break;
		}
		if((str = strchr(str,',')) != NULL)
			str++;
		else
			break;
	}
	return 1;
}
__setup("reboot=", reboot_setup);

/* overwrites random kernel memory. Should not be kernel .text */
#define WARMBOOT_TRAMP 0x1000UL

static void reboot_warm(void)
{
	extern unsigned char warm_reboot[], warm_reboot_end[];
	printk("warm reboot\n");

	__cli(); 
		
	/* restore identity mapping */
	init_level4_pgt[0] = __pml4(__pa(level3_ident_pgt) | 7); 
	__flush_tlb_all(); 

	memcpy(__va(WARMBOOT_TRAMP), warm_reboot, warm_reboot_end - warm_reboot); 

	asm volatile( "   pushq $0\n" 		/* ss */
		     "   pushq $0x2000\n" 	/* rsp */
	             "   pushfq\n"		/* eflags */
		     "   pushq %[cs]\n"
		     "   pushq %[target]\n"
		     "   iretq" :: 
		      [cs] "i" (__KERNEL_COMPAT32_CS), 
		      [target] "b" (WARMBOOT_TRAMP));
}

static void kb_wait(void)
{
	int i;

	for (i=0; i<0x10000; i++)
		if ((inb_p(0x64) & 0x02) == 0)
			break;
}


#ifdef CONFIG_SMP
static void smp_halt(void)
{
	int cpuid = safe_smp_processor_id(); 
	static int first_entry = 1;
	
	if (first_entry) { 
		first_entry = 0;
		smp_call_function((void *)machine_restart, NULL, 1, 0);		
	}

	smp_stop_cpu(); 

	/* AP calling this. Just halt */
	if (cpuid != boot_cpu_id) { 
		printk("CPU %d SMP halt\n", cpuid); 
		for (;;)
			asm("hlt");
	}

	/* Wait for all other CPUs to have run smp_stop_cpu */
	while (cpu_online_map) 
		rep_nop(); 
}
#endif

void machine_restart(char * __unused)
{
	int i;

#if CONFIG_SMP
	smp_halt();
#endif
	__cli();

#ifndef CONFIG_SMP
	disable_local_APIC();
#endif
	disable_IO_APIC();

	__sti();

	/* Tell the BIOS if we want cold or warm reboot */
	*((unsigned short *)__va(0x472)) = reboot_mode;

	for (;;) {
		/* Could also try the reset bit in the Hammer NB */
		switch (reboot_type) { 
		case BOOT_BIOS:
			reboot_warm();

		case BOOT_KBD:
			/* force cold reboot to reinit all hardware*/
		for (i=0; i<100; i++) {
			kb_wait();
			udelay(50);
			outb(0xfe,0x64);         /* pulse reset low */
			udelay(50);
		}
			
		case BOOT_TRIPLE: 
			/* force cold reboot to reinit all hardware*/
			*((unsigned short *)__va(0x472)) = 0;

			__asm__ __volatile__("lidt (%0)": :"r" (no_idt));
		__asm__ __volatile__("int3");

			reboot_type = BOOT_KBD;
			break;
		}      
	}
}

void machine_halt(void)
{
}

void machine_power_off(void)
{
	if (pm_power_off)
		pm_power_off();
}

extern int printk_address(unsigned long); 

/* Prints also some state that isn't saved in the pt_regs */ 
void __show_regs(struct pt_regs * regs)
{
	unsigned long cr0 = 0L, cr2 = 0L, cr3 = 0L, cr4 = 0L, fs, gs, shadowgs;
	unsigned int fsindex,gsindex;
	unsigned int ds,cs,es; 

	printk("\n");
	printk("Pid: %d, comm: %.20s %s\n", current->pid, current->comm, print_tainted());
	printk("RIP: %04lx:", regs->cs & 0xffff);
	printk_address(regs->rip); 
	printk("\nRSP: %04lx:%016lx  EFLAGS: %08lx\n", regs->ss, regs->rsp, regs->eflags);
	printk("RAX: %016lx RBX: %016lx RCX: %016lx\n",
	       regs->rax, regs->rbx, regs->rcx);
	printk("RDX: %016lx RSI: %016lx RDI: %016lx\n",
	       regs->rdx, regs->rsi, regs->rdi); 
	printk("RBP: %016lx R08: %016lx R09: %016lx\n",
	       regs->rbp, regs->r8, regs->r9); 
	printk("R10: %016lx R11: %016lx R12: %016lx\n",
	       regs->r10, regs->r11, regs->r12); 
	printk("R13: %016lx R14: %016lx R15: %016lx\n",
	       regs->r13, regs->r14, regs->r15); 

	asm("movl %%ds,%0" : "=r" (ds)); 
	asm("movl %%cs,%0" : "=r" (cs)); 
	asm("movl %%es,%0" : "=r" (es)); 
	asm("movl %%fs,%0" : "=r" (fsindex));
	asm("movl %%gs,%0" : "=r" (gsindex));

	rdmsrl(MSR_FS_BASE, fs);
	rdmsrl(MSR_GS_BASE, gs); 
	rdmsrl(MSR_KERNEL_GS_BASE, shadowgs); 

	asm("movq %%cr0, %0": "=r" (cr0));
	asm("movq %%cr2, %0": "=r" (cr2));
	asm("movq %%cr3, %0": "=r" (cr3));
	asm("movq %%cr4, %0": "=r" (cr4));

	printk("FS:  %016lx(%04x) GS:%016lx(%04x) knlGS:%016lx\n", 
	       fs,fsindex,gs,gsindex,shadowgs); 
	printk("CS:  %04x DS: %04x ES: %04x CR0: %016lx\n", cs, ds, es, cr0); 
	printk("CR2: %016lx CR3: %016lx CR4: %016lx\n", cr2, cr3, cr4);
}

void show_regs(struct pt_regs * regs)
{
	__show_regs(regs);
	show_trace(&regs->rsp);
}

/*
 * No need to lock the MM as we are the last user
 */
void release_segments(struct mm_struct *mm)
{
	void * ldt = mm->context.segments;

	/*
	 * free the LDT
	 */
	if (ldt) {
		mm->context.segments = NULL;
		clear_LDT();
		vfree(ldt);
	}
}

/* 
 * Free current thread data structures etc..
 */
void exit_thread(void)
{
	struct task_struct *me = current;
	if (me->thread.io_bitmap_ptr) { 
		(init_tss + smp_processor_id())->io_map_base = 
			INVALID_IO_BITMAP_OFFSET;  
		kfree(me->thread.io_bitmap_ptr); 
		me->thread.io_bitmap_ptr = NULL; 		
	} 
}

void flush_thread(void)
{
	struct task_struct *tsk = current;

	memset(tsk->thread.debugreg, 0, sizeof(unsigned long)*8);
	/*
	 * Forget coprocessor state..
	 */
	clear_fpu(tsk);
	tsk->used_math = 0;
}

void release_thread(struct task_struct *dead_task)
{
	if (dead_task->mm) {
		void * ldt = dead_task->mm->context.segments;

		// temporary debugging check
		if (ldt) {
			printk("WARNING: dead process %8s still has LDT? <%p>\n",
					dead_task->comm, ldt);
			BUG();
		}
	}
}

/*
 * we do not have to muck with descriptors here, that is
 * done in switch_mm() as needed.
 */
void copy_segments(struct task_struct *p, struct mm_struct *new_mm)
{
	struct mm_struct * old_mm;
	void *old_ldt, *ldt;
 
	ldt = NULL;
	old_mm = current->mm;
	if (old_mm && (old_ldt = old_mm->context.segments) != NULL) {
		/*
		 * Completely new LDT, we initialize it from the parent:
		 */
		ldt = vmalloc(LDT_ENTRIES*LDT_ENTRY_SIZE);
		if (!ldt)
			printk(KERN_WARNING "ldt allocation failed\n");
		else
			memcpy(ldt, old_ldt, LDT_ENTRIES*LDT_ENTRY_SIZE);
	}
	new_mm->context.segments = ldt;
	new_mm->context.cpuvalid = 0UL;
	return;
}

int copy_thread(int nr, unsigned long clone_flags, unsigned long rsp, 
		unsigned long unused,
	struct task_struct * p, struct pt_regs * regs)
{
	struct pt_regs * childregs;
	struct task_struct *me = current;

	childregs = ((struct pt_regs *) (THREAD_SIZE + (unsigned long) p)) - 1;

	*childregs = *regs;

	childregs->rax = 0;
	childregs->rsp = rsp;
	if (rsp == ~0) {
		childregs->rsp = (unsigned long)childregs;
	}

	p->thread.rsp = (unsigned long) childregs;
	p->thread.rsp0 = (unsigned long) (childregs+1);
	p->thread.userrsp = current->thread.userrsp; 

	p->thread.rip = (unsigned long) ret_from_fork;

	p->thread.fs = me->thread.fs;
	p->thread.gs = me->thread.gs;

	asm("movl %%gs,%0" : "=m" (p->thread.gsindex));
	asm("movl %%fs,%0" : "=m" (p->thread.fsindex));
	asm("movl %%es,%0" : "=m" (p->thread.es));
	asm("movl %%ds,%0" : "=m" (p->thread.ds));

	unlazy_fpu(current);	
	p->thread.i387 = current->thread.i387;

	if (unlikely(me->thread.io_bitmap_ptr != NULL)) { 
		p->thread.io_bitmap_ptr = kmalloc((IO_BITMAP_SIZE+1)*4, GFP_KERNEL);
		if (!p->thread.io_bitmap_ptr) 
			return -ENOMEM;
		memcpy(p->thread.io_bitmap_ptr, me->thread.io_bitmap_ptr, 
		       (IO_BITMAP_SIZE+1)*4);
	} 

	return 0;
}

/*
 * This special macro can be used to load a debugging register
 */
#define loaddebug(thread,register) \
		set_debug(thread->debugreg[register], register)

/*
 *	switch_to(x,y) should switch tasks from x to y.
 *
 * This could still be optimized: 
 * - fold all the options into a flag word and test it with a single test.
 * - could test fs/gs bitsliced
 */
struct task_struct *__switch_to(struct task_struct *prev_p, struct task_struct *next_p)
{
	struct thread_struct *prev = &prev_p->thread,
				 *next = &next_p->thread;
	struct tss_struct *tss = init_tss + smp_processor_id();

	unlazy_fpu(prev_p);

	/*
	 * Reload rsp0, LDT and the page table pointer:
	 */
	tss->rsp0 = next->rsp0;

	/* 
	 * Switch DS and ES.	 
	 */
	asm volatile("movl %%es,%0" : "=m" (prev->es)); 
	if (unlikely(next->es | prev->es))
		loadsegment(es, next->es); 
	
	asm volatile ("movl %%ds,%0" : "=m" (prev->ds)); 
	if (unlikely(next->ds | prev->ds))
		loadsegment(ds, next->ds);

	/* 
	 * Switch FS and GS.
	 */
	{ 
		unsigned fsindex;
		asm volatile("movl %%fs,%0" : "=g" (fsindex)); 
		/* segment register != 0 always requires a reload. 
		   also reload when it has changed. 
		   when prev process used 64bit base always reload
		   to avoid an information leak. */
		if (unlikely((fsindex | next->fsindex) || prev->fs)) {
			loadsegment(fs, next->fsindex);
			/* check if the user use a selector != 0
			 * if yes clear 64bit base, since overloaded base
			 * is allways mapped to the Null selector
			 */
			if (fsindex)
			prev->fs = 0; 
		}
		/* when next process has a 64bit base use it */
		if (next->fs) 
			wrmsrl(MSR_FS_BASE, next->fs); 
		prev->fsindex = fsindex;
	}
	{
		unsigned gsindex;
		asm volatile("movl %%gs,%0" : "=g" (gsindex)); 
		if (unlikely((gsindex | next->gsindex) || prev->gs)) {
			load_gs_index(next->gsindex);
			if (gsindex)
			prev->gs = 0;				
		}
		if (next->gs)
			wrmsrl(MSR_KERNEL_GS_BASE, next->gs); 
		prev->gsindex = gsindex;
	}

	/* 
	 * Switch the PDA context.
	 */
	prev->userrsp = read_pda(oldrsp); 
	write_pda(oldrsp, next->userrsp); 
	write_pda(pcurrent, next_p); 
	write_pda(kernelstack, (unsigned long)next_p + THREAD_SIZE - PDA_STACKOFFSET);

	/*
	 * Now maybe reload the debug registers
	 */
	if (unlikely(next->debugreg[7])) {
		loaddebug(next, 0);
		loaddebug(next, 1);
		loaddebug(next, 2);
		loaddebug(next, 3);
		/* no 4 and 5 */
		loaddebug(next, 6);
		loaddebug(next, 7);
	}


	/* 
	 * Handle the IO bitmap 
	 */ 
	if (unlikely(prev->io_bitmap_ptr || next->io_bitmap_ptr)) {
		if (next->io_bitmap_ptr) {
			/*
			 * 4 cachelines copy ... not good, but not that
			 * bad either. Anyone got something better?
			 * This only affects processes which use ioperm().
			 * [Putting the TSSs into 4k-tlb mapped regions
			 * and playing VM tricks to switch the IO bitmap
			 * is not really acceptable.]
			 */
			memcpy(tss->io_bitmap, next->io_bitmap_ptr,
				 IO_BITMAP_SIZE*sizeof(u32));
			tss->io_map_base = IO_BITMAP_OFFSET;
		} else {
			/*
			 * a bitmap offset pointing outside of the TSS limit
			 * causes a nicely controllable SIGSEGV if a process
			 * tries to use a port IO instruction. The first
			 * sys_ioperm() call sets up the bitmap properly.
			 */
			tss->io_map_base = INVALID_IO_BITMAP_OFFSET;
		}
	}


	return prev_p;
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage 
long sys_execve(char *name, char **argv,char **envp, struct pt_regs regs)
{
	long error;
	char * filename;

	filename = getname(name);
	error = PTR_ERR(filename);
	if (IS_ERR(filename)) 
		return error;
	error = do_execve(filename, argv, envp, &regs); 
	if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);
	return error;
}

void set_personality_64bit(void)
{
	/* inherit personality from parent */

	/* Make sure to be in 64bit mode */
	current->thread.flags = 0;
}

asmlinkage long sys_fork(struct pt_regs regs)
{
	return do_fork(SIGCHLD, regs.rsp, &regs, 0);
}

asmlinkage long sys_clone(unsigned long clone_flags, unsigned long newsp, struct pt_regs regs)
{
	if (!newsp)
		newsp = regs.rsp;
	return do_fork(clone_flags, newsp, &regs, 0);
}

/*
 * This is trivial, and on the face of it looks like it
 * could equally well be done in user mode.
 *
 * Not so, for quite unobvious reasons - register pressure.
 * In user mode vfork() cannot have a stack frame, and if
 * done by calling the "clone()" system call directly, you
 * do not have enough call-clobbered registers to hold all
 * the information you need.
 */
asmlinkage long sys_vfork(struct pt_regs regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs.rsp, &regs, 0);
}

/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched	((unsigned long) scheduling_functions_start_here)
#define last_sched	((unsigned long) scheduling_functions_end_here)

unsigned long get_wchan(struct task_struct *p)
{
	u64 fp,rip;
	int count = 0;

	if (!p || p == current || p->state==TASK_RUNNING)
		return 0; 
	if (p->thread.rsp < (u64)p || p->thread.rsp > (u64)p + THREAD_SIZE)
		return 0;
	fp = *(u64 *)(p->thread.rsp);
	do { 
		if (fp < (unsigned long)p || fp > (unsigned long)p+THREAD_SIZE)
			return 0; 
		rip = *(u64 *)(fp+8); 
		if (rip < first_sched || rip >= last_sched)
			return rip; 
		fp = *(u64 *)fp; 
	} while (count++ < 16); 
	return 0;
}
#undef last_sched
#undef first_sched

asmlinkage long sys_arch_prctl(int code, unsigned long addr)
{ 
	int ret = 0; 
	unsigned long tmp; 

	switch (code) { 
	case ARCH_SET_GS:
		if (addr >= TASK_SIZE) 
			return -EPERM; 
		asm volatile("movl %0,%%gs" :: "r" (0)); 
		current->thread.gsindex = 0;
		current->thread.gs = addr;
		ret = checking_wrmsrl(MSR_KERNEL_GS_BASE, addr); 
		break;
	case ARCH_SET_FS:
		/* Not strictly needed for fs, but do it for symmetry
		   with gs. */
		if (addr >= TASK_SIZE)
			return -EPERM; 
		asm volatile("movl %0,%%fs" :: "r" (0)); 
		current->thread.fsindex = 0;
		current->thread.fs = addr;
		ret = checking_wrmsrl(MSR_FS_BASE, addr); 
		break;

		/* Returned value may not be correct when the user changed fs/gs */ 
	case ARCH_GET_FS:
		rdmsrl(MSR_FS_BASE, tmp);
		ret = put_user(tmp, (unsigned long *)addr); 
		break; 

	case ARCH_GET_GS: 
		rdmsrl(MSR_KERNEL_GS_BASE, tmp); 
		ret = put_user(tmp, (unsigned long *)addr); 
		break;

	default:
		ret = -EINVAL;
		break;
	} 
	return ret;	
} 
