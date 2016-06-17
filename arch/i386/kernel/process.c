/*
 *  linux/arch/i386/kernel/process.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *
 *  Pentium III FXSR, SSE support
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 */

/*
 * This file handles the architecture-dependent parts of process handling..
 */

#define __KERNEL_SYSCALLS__
#include <stdarg.h>

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
#include <linux/mc146818rtc.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/ldt.h>
#include <asm/processor.h>
#include <asm/i387.h>
#include <asm/irq.h>
#include <asm/desc.h>
#include <asm/mmu_context.h>
#include <asm/smpboot.h>
#ifdef CONFIG_MATH_EMULATION
#include <asm/math_emu.h>
#endif
#include <asm/apic.h>

#include <linux/irq.h>

asmlinkage void ret_from_fork(void) __asm__("ret_from_fork");

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
void default_idle(void)
{
	if (current_cpu_data.hlt_works_ok && !hlt_counter) {
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

static int __init idle_setup (char *str)
{
	if (!strncmp(str, "poll", 4)) {
		printk("using polling idle threads.\n");
		pm_idle = poll_idle;
	}

	return 1;
}

__setup("idle=", idle_setup);

static int reboot_mode;
int reboot_thru_bios;

#ifdef CONFIG_SMP
int reboot_smp = 0;
static int reboot_cpu = -1;
/* shamelessly grabbed from lib/vsprintf.c for readability */
#define is_digit(c)	((c) >= '0' && (c) <= '9')
#endif
static int __init reboot_setup(char *str)
{
	while(1) {
		switch (*str) {
		case 'w': /* "warm" reboot (no memory testing etc) */
			reboot_mode = 0x1234;
			break;
		case 'c': /* "cold" reboot (with memory testing etc) */
			reboot_mode = 0x0;
			break;
		case 'b': /* "bios" reboot by jumping through the BIOS */
			reboot_thru_bios = 1;
			break;
		case 'h': /* "hard" reboot by toggling RESET and/or crashing the CPU */
			reboot_thru_bios = 0;
			break;
#ifdef CONFIG_SMP
		case 's': /* "smp" reboot by executing reset on BSP or other CPU*/
			reboot_smp = 1;
			if (is_digit(*(str+1))) {
				reboot_cpu = (int) (*(str+1) - '0');
				if (is_digit(*(str+2))) 
					reboot_cpu = reboot_cpu*10 + (int)(*(str+2) - '0');
			}
				/* we will leave sorting out the final value 
				when we are ready to reboot, since we might not
 				have set up boot_cpu_id or smp_num_cpu */
			break;
#endif
		}
		if((str = strchr(str,',')) != NULL)
			str++;
		else
			break;
	}
	return 1;
}

__setup("reboot=", reboot_setup);

/* The following code and data reboots the machine by switching to real
   mode and jumping to the BIOS reset entry point, as if the CPU has
   really been reset.  The previous version asked the keyboard
   controller to pulse the CPU reset line, which is more thorough, but
   doesn't work with at least one type of 486 motherboard.  It is easy
   to stop this code working; hence the copious comments. */

static unsigned long long
real_mode_gdt_entries [3] =
{
	0x0000000000000000ULL,	/* Null descriptor */
	0x00009a000000ffffULL,	/* 16-bit real-mode 64k code at 0x00000000 */
	0x000092000100ffffULL	/* 16-bit real-mode 64k data at 0x00000100 */
};

static struct
{
	unsigned short       size __attribute__ ((packed));
	unsigned long long * base __attribute__ ((packed));
}
real_mode_gdt = { sizeof (real_mode_gdt_entries) - 1, real_mode_gdt_entries },
real_mode_idt = { 0x3ff, 0 },
no_idt = { 0, 0 };

/* This is 16-bit protected mode code to disable paging and the cache,
   switch to real mode and jump to the BIOS reset code.

   The instruction that switches to real mode by writing to CR0 must be
   followed immediately by a far jump instruction, which set CS to a
   valid value for real mode, and flushes the prefetch queue to avoid
   running instructions that have already been decoded in protected
   mode.

   Clears all the flags except ET, especially PG (paging), PE
   (protected-mode enable) and TS (task switch for coprocessor state
   save).  Flushes the TLB after paging has been disabled.  Sets CD and
   NW, to disable the cache on a 486, and invalidates the cache.  This
   is more like the state of a 486 after reset.  I don't know if
   something else should be done for other chips.

   More could be done here to set up the registers as if a CPU reset had
   occurred; hopefully real BIOSs don't assume much. */

static unsigned char real_mode_switch [] =
{
	0x66, 0x0f, 0x20, 0xc0,			/*    movl  %cr0,%eax        */
	0x66, 0x83, 0xe0, 0x11,			/*    andl  $0x00000011,%eax */
	0x66, 0x0d, 0x00, 0x00, 0x00, 0x60,	/*    orl   $0x60000000,%eax */
	0x66, 0x0f, 0x22, 0xc0,			/*    movl  %eax,%cr0        */
	0x66, 0x0f, 0x22, 0xd8,			/*    movl  %eax,%cr3        */
	0x66, 0x0f, 0x20, 0xc3,			/*    movl  %cr0,%ebx        */
	0x66, 0x81, 0xe3, 0x00, 0x00, 0x00, 0x60,	/*    andl  $0x60000000,%ebx */
	0x74, 0x02,				/*    jz    f                */
	0x0f, 0x09,				/*    wbinvd                 */
	0x24, 0x10,				/* f: andb  $0x10,al         */
	0x66, 0x0f, 0x22, 0xc0			/*    movl  %eax,%cr0        */
};
static unsigned char jump_to_bios [] =
{
	0xea, 0x00, 0x00, 0xff, 0xff		/*    ljmp  $0xffff,$0x0000  */
};

static inline void kb_wait(void)
{
	int i;

	for (i=0; i<0x10000; i++)
		if ((inb_p(0x64) & 0x02) == 0)
			break;
}

/*
 * Switch to real mode and then execute the code
 * specified by the code and length parameters.
 * We assume that length will aways be less that 100!
 */
void machine_real_restart(unsigned char *code, int length)
{
	unsigned long flags;

	cli();

	/* Write zero to CMOS register number 0x0f, which the BIOS POST
	   routine will recognize as telling it to do a proper reboot.  (Well
	   that's what this book in front of me says -- it may only apply to
	   the Phoenix BIOS though, it's not clear).  At the same time,
	   disable NMIs by setting the top bit in the CMOS address register,
	   as we're about to do peculiar things to the CPU.  I'm not sure if
	   `outb_p' is needed instead of just `outb'.  Use it to be on the
	   safe side.  (Yes, CMOS_WRITE does outb_p's. -  Paul G.)
	 */

	spin_lock_irqsave(&rtc_lock, flags);
	CMOS_WRITE(0x00, 0x8f);
	spin_unlock_irqrestore(&rtc_lock, flags);

	/* Remap the kernel at virtual address zero, as well as offset zero
	   from the kernel segment.  This assumes the kernel segment starts at
	   virtual address PAGE_OFFSET. */

	memcpy (swapper_pg_dir, swapper_pg_dir + USER_PGD_PTRS,
		sizeof (swapper_pg_dir [0]) * KERNEL_PGD_PTRS);

	/* Make sure the first page is mapped to the start of physical memory.
	   It is normally not mapped, to trap kernel NULL pointer dereferences. */

	pg0[0] = _PAGE_RW | _PAGE_PRESENT;

	/*
	 * Use `swapper_pg_dir' as our page directory.
	 */
	load_cr3(swapper_pg_dir);

	/* Write 0x1234 to absolute memory location 0x472.  The BIOS reads
	   this on booting to tell it to "Bypass memory test (also warm
	   boot)".  This seems like a fairly standard thing that gets set by
	   REBOOT.COM programs, and the previous reset routine did this
	   too. */

	*((unsigned short *)0x472) = reboot_mode;

	/* For the switch to real mode, copy some code to low memory.  It has
	   to be in the first 64k because it is running in 16-bit mode, and it
	   has to have the same physical and virtual address, because it turns
	   off paging.  Copy it near the end of the first page, out of the way
	   of BIOS variables. */

	memcpy ((void *) (0x1000 - sizeof (real_mode_switch) - 100),
		real_mode_switch, sizeof (real_mode_switch));
	memcpy ((void *) (0x1000 - 100), code, length);

	/* Set up the IDT for real mode. */

	__asm__ __volatile__ ("lidt %0" : : "m" (real_mode_idt));

	/* Set up a GDT from which we can load segment descriptors for real
	   mode.  The GDT is not used in real mode; it is just needed here to
	   prepare the descriptors. */

	__asm__ __volatile__ ("lgdt %0" : : "m" (real_mode_gdt));

	/* Load the data segment registers, and thus the descriptors ready for
	   real mode.  The base address of each segment is 0x100, 16 times the
	   selector value being loaded here.  This is so that the segment
	   registers don't have to be reloaded after switching to real mode:
	   the values are consistent for real mode operation already. */

	__asm__ __volatile__ ("movl $0x0010,%%eax\n"
				"\tmovl %%eax,%%ds\n"
				"\tmovl %%eax,%%es\n"
				"\tmovl %%eax,%%fs\n"
				"\tmovl %%eax,%%gs\n"
				"\tmovl %%eax,%%ss" : : : "eax");

	/* Jump to the 16-bit code that we copied earlier.  It disables paging
	   and the cache, switches to real mode, and jumps to the BIOS reset
	   entry point. */

	__asm__ __volatile__ ("ljmp $0x0008,%0"
				:
				: "i" ((void *) (0x1000 - sizeof (real_mode_switch) - 100)));
}

void machine_restart(char * __unused)
{
#if CONFIG_SMP
	int cpuid;
	
	cpuid = GET_APIC_ID(apic_read(APIC_ID));

	if (reboot_smp) {

		/* check to see if reboot_cpu is valid 
		   if its not, default to the BSP */
		if ((reboot_cpu == -1) ||  
		      (reboot_cpu > (NR_CPUS -1))  || 
		      !(phys_cpu_present_map & apicid_to_phys_cpu_present(cpuid)))
			reboot_cpu = boot_cpu_physical_apicid;

		reboot_smp = 0;  /* use this as a flag to only go through this once*/
		/* re-run this function on the other CPUs
		   it will fall though this section since we have 
		   cleared reboot_smp, and do the reboot if it is the
		   correct CPU, otherwise it halts. */
		if (reboot_cpu != cpuid)
			smp_call_function((void *)machine_restart , NULL, 1, 0);
	}

	/* if reboot_cpu is still -1, then we want a tradional reboot, 
	   and if we are not running on the reboot_cpu,, halt */
	if ((reboot_cpu != -1) && (cpuid != reboot_cpu)) {
		for (;;)
		__asm__ __volatile__ ("hlt");
	}
	/*
	 * Stop all CPUs and turn off local APICs and the IO-APIC, so
	 * other OSs see a clean IRQ state.
	 */
	smp_send_stop();
#elif CONFIG_X86_LOCAL_APIC
	if (cpu_has_apic) {
		__cli();
		disable_local_APIC();
		__sti();
	}
#endif
#ifdef CONFIG_X86_IO_APIC
	disable_IO_APIC();
#endif

	if(!reboot_thru_bios) {
		/* rebooting needs to touch the page at absolute addr 0 */
		*((unsigned short *)__va(0x472)) = reboot_mode;
		for (;;) {
			int i;
			for (i=0; i<100; i++) {
				kb_wait();
				udelay(50);
				outb(0xfe,0x64);         /* pulse reset low */
				udelay(50);
			}
			/* That didn't work - force a triple fault.. */
			__asm__ __volatile__("lidt %0": :"m" (no_idt));
			__asm__ __volatile__("int3");
		}
	}

	machine_real_restart(jump_to_bios, sizeof(jump_to_bios));
}

void machine_halt(void)
{
}

void machine_power_off(void)
{
	if (pm_power_off)
		pm_power_off();
}

extern void show_trace(unsigned long* esp);

void show_regs(struct pt_regs * regs)
{
	unsigned long cr0 = 0L, cr2 = 0L, cr3 = 0L, cr4 = 0L;

	printk("\n");
	printk("Pid: %d, comm: %20s\n", current->pid, current->comm);
	printk("EIP: %04x:[<%08lx>] CPU: %d",0xffff & regs->xcs,regs->eip, smp_processor_id());
	if (regs->xcs & 3)
		printk(" ESP: %04x:%08lx",0xffff & regs->xss,regs->esp);
	printk(" EFLAGS: %08lx    %s\n",regs->eflags, print_tainted());
	printk("EAX: %08lx EBX: %08lx ECX: %08lx EDX: %08lx\n",
		regs->eax,regs->ebx,regs->ecx,regs->edx);
	printk("ESI: %08lx EDI: %08lx EBP: %08lx",
		regs->esi, regs->edi, regs->ebp);
	printk(" DS: %04x ES: %04x\n",
		0xffff & regs->xds,0xffff & regs->xes);

	__asm__("movl %%cr0, %0": "=r" (cr0));
	__asm__("movl %%cr2, %0": "=r" (cr2));
	__asm__("movl %%cr3, %0": "=r" (cr3));
	/* This could fault if %cr4 does not exist */
	__asm__("1: movl %%cr4, %0		\n"
		"2:				\n"
		".section __ex_table,\"a\"	\n"
		".long 1b,2b			\n"
		".previous			\n"
		: "=r" (cr4): "0" (0));
	printk("CR0: %08lx CR2: %08lx CR3: %08lx CR4: %08lx\n", cr0, cr2, cr3, cr4);
	show_trace(&regs->esp);
}

/*
 * Create a kernel thread
 */
int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	long retval, d0;

	__asm__ __volatile__(
		"movl %%esp,%%esi\n\t"
		"int $0x80\n\t"		/* Linux/i386 system call */
		"cmpl %%esp,%%esi\n\t"	/* child or parent? */
		"je 1f\n\t"		/* parent - jump */
		/* Load the argument into eax, and push it.  That way, it does
		 * not matter whether the called function is compiled with
		 * -mregparm or not.  */
		"movl %4,%%eax\n\t"
		"pushl %%eax\n\t"		
		"call *%5\n\t"		/* call fn */
		"movl %3,%0\n\t"	/* exit */
		"int $0x80\n"
		"1:\t"
		:"=&a" (retval), "=&S" (d0)
		:"0" (__NR_clone), "i" (__NR_exit),
		 "r" (arg), "r" (fn),
		 "b" (flags | CLONE_VM)
		: "memory");

	return retval;
}

/*
 * Free current thread data structures etc..
 */
void exit_thread(void)
{
	/* nothing to do ... */
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
		// temporary debugging check
		if (dead_task->mm->context.size) {
			printk("WARNING: dead process %8s still has LDT? <%p/%d>\n",
					dead_task->comm,
					dead_task->mm->context.ldt,
					dead_task->mm->context.size);
			BUG();
		}
	}
	release_x86_irqs(dead_task);
}

/*
 * Save a segment.
 */
#define savesegment(seg,value) \
	asm volatile("movl %%" #seg ",%0":"=m" (*(int *)&(value)))

int copy_thread(int nr, unsigned long clone_flags, unsigned long esp,
	unsigned long unused,
	struct task_struct * p, struct pt_regs * regs)
{
	struct pt_regs * childregs;

	childregs = ((struct pt_regs *) (THREAD_SIZE + (unsigned long) p)) - 1;
	struct_cpy(childregs, regs);
	childregs->eax = 0;
	childregs->esp = esp;

	p->thread.esp = (unsigned long) childregs;
	p->thread.esp0 = (unsigned long) (childregs+1);

	p->thread.eip = (unsigned long) ret_from_fork;

	savesegment(fs,p->thread.fs);
	savesegment(gs,p->thread.gs);

	unlazy_fpu(current);
	struct_cpy(&p->thread.i387, &current->thread.i387);

	return 0;
}

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs * regs, struct user * dump)
{
	int i;

/* changed the size calculations - should hopefully work better. lbt */
	dump->magic = CMAGIC;
	dump->start_code = 0;
	dump->start_stack = regs->esp & ~(PAGE_SIZE - 1);
	dump->u_tsize = ((unsigned long) current->mm->end_code) >> PAGE_SHIFT;
	dump->u_dsize = ((unsigned long) (current->mm->brk + (PAGE_SIZE-1))) >> PAGE_SHIFT;
	dump->u_dsize -= dump->u_tsize;
	dump->u_ssize = 0;
	for (i = 0; i < 8; i++)
		dump->u_debugreg[i] = current->thread.debugreg[i];  

	if (dump->start_stack < TASK_SIZE)
		dump->u_ssize = ((unsigned long) (TASK_SIZE - dump->start_stack)) >> PAGE_SHIFT;

	dump->regs.ebx = regs->ebx;
	dump->regs.ecx = regs->ecx;
	dump->regs.edx = regs->edx;
	dump->regs.esi = regs->esi;
	dump->regs.edi = regs->edi;
	dump->regs.ebp = regs->ebp;
	dump->regs.eax = regs->eax;
	dump->regs.ds = regs->xds;
	dump->regs.es = regs->xes;
	savesegment(fs,dump->regs.fs);
	savesegment(gs,dump->regs.gs);
	dump->regs.orig_eax = regs->orig_eax;
	dump->regs.eip = regs->eip;
	dump->regs.cs = regs->xcs;
	dump->regs.eflags = regs->eflags;
	dump->regs.esp = regs->esp;
	dump->regs.ss = regs->xss;

	dump->u_fpvalid = dump_fpu (regs, &dump->i387);
}

/*
 * This special macro can be used to load a debugging register
 */
#define loaddebug(thread,register) \
		__asm__("movl %0,%%db" #register  \
			: /* no output */ \
			:"r" (thread->debugreg[register]))

/*
 *	switch_to(x,yn) should switch tasks from x to y.
 *
 * We fsave/fwait so that an exception goes off at the right time
 * (as a call from the fsave or fwait in effect) rather than to
 * the wrong process. Lazy FP saving no longer makes any sense
 * with modern CPU's, and this simplifies a lot of things (SMP
 * and UP become the same).
 *
 * NOTE! We used to use the x86 hardware context switching. The
 * reason for not using it any more becomes apparent when you
 * try to recover gracefully from saved state that is no longer
 * valid (stale segment register values in particular). With the
 * hardware task-switch, there is no way to fix up bad state in
 * a reasonable manner.
 *
 * The fact that Intel documents the hardware task-switching to
 * be slow is a fairly red herring - this code is not noticeably
 * faster. However, there _is_ some room for improvement here,
 * so the performance issues may eventually be a valid point.
 * More important, however, is the fact that this allows us much
 * more flexibility.
 */
void __switch_to(struct task_struct *prev_p, struct task_struct *next_p)
{
	struct thread_struct *prev = &prev_p->thread,
				 *next = &next_p->thread;
	struct tss_struct *tss = init_tss + smp_processor_id();

	unlazy_fpu(prev_p);

	/*
	 * Reload esp0, LDT and the page table pointer:
	 */
	tss->esp0 = next->esp0;

	/*
	 * Save away %fs and %gs. No need to save %es and %ds, as
	 * those are always kernel segments while inside the kernel.
	 */
	asm volatile("movl %%fs,%0":"=m" (*(int *)&prev->fs));
	asm volatile("movl %%gs,%0":"=m" (*(int *)&prev->gs));

	/*
	 * Restore %fs and %gs.
	 */
	loadsegment(fs, next->fs);
	loadsegment(gs, next->gs);

	/*
	 * Now maybe reload the debug registers
	 */
	if (next->debugreg[7]){
		loaddebug(next, 0);
		loaddebug(next, 1);
		loaddebug(next, 2);
		loaddebug(next, 3);
		/* no 4 and 5 */
		loaddebug(next, 6);
		loaddebug(next, 7);
	}

	if (prev->ioperm || next->ioperm) {
		if (next->ioperm) {
			/*
			 * 4 cachelines copy ... not good, but not that
			 * bad either. Anyone got something better?
			 * This only affects processes which use ioperm().
			 * [Putting the TSSs into 4k-tlb mapped regions
			 * and playing VM tricks to switch the IO bitmap
			 * is not really acceptable.]
			 */
			memcpy(tss->io_bitmap, next->io_bitmap,
				 IO_BITMAP_BYTES);
			tss->bitmap = IO_BITMAP_OFFSET;
		} else
			/*
			 * a bitmap offset pointing outside of the TSS limit
			 * causes a nicely controllable SIGSEGV if a process
			 * tries to use a port IO instruction. The first
			 * sys_ioperm() call sets up the bitmap properly.
			 */
			tss->bitmap = INVALID_IO_BITMAP_OFFSET;
	}
}

asmlinkage int sys_fork(struct pt_regs regs)
{
	return do_fork(SIGCHLD, regs.esp, &regs, 0);
}

asmlinkage int sys_clone(struct pt_regs regs)
{
	unsigned long clone_flags;
	unsigned long newsp;

	clone_flags = regs.ebx;
	newsp = regs.ecx;
	if (!newsp)
		newsp = regs.esp;
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
asmlinkage int sys_vfork(struct pt_regs regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs.esp, &regs, 0);
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys_execve(struct pt_regs regs)
{
	int error;
	char * filename;

	filename = getname((char *) regs.ebx);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, (char **) regs.ecx, (char **) regs.edx, &regs);
	if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);
out:
	return error;
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
	unsigned long ebp, esp, eip;
	unsigned long stack_page;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;
	stack_page = (unsigned long)p;
	esp = p->thread.esp;
	if (!stack_page || esp < stack_page || esp > 8188+stack_page)
		return 0;
	/* include/asm-i386/system.h:switch_to() pushes ebp last. */
	ebp = *(unsigned long *) esp;
	do {
		if (ebp < stack_page || ebp > 8184+stack_page)
			return 0;
		eip = *(unsigned long *) (ebp+4);
		if (eip < first_sched || eip >= last_sched)
			return eip;
		ebp = *(unsigned long *) ebp;
	} while (count++ < 16);
	return 0;
}
#undef last_sched
#undef first_sched
