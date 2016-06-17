/*
 *  linux/arch/ppc64/kernel/process.c
 *
 *  Derived from "arch/i386/kernel/process.c"
 *    Copyright (C) 1995  Linus Torvalds
 *
 *  Updated and modified by Cort Dougan (cort@cs.nmt.edu) and
 *  Paul Mackerras (paulus@cs.anu.edu.au)
 *
 *  PowerPC version 
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  VMX/Altivec port from ppc32 (c) IBM 2003
 *   Denis Joseph Barrow (dj@de.ibm.com,barrow_dj@yahoo.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
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
#include <linux/user.h>
#include <linux/elf.h>
#include <linux/init.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <asm/prom.h>
#include <asm/ppcdebug.h>
#include <asm/machdep.h>
#include <asm/iSeries/HvCallHpt.h>
#include <asm/cputable.h>

int dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpregs);

#ifndef CONFIG_SMP
struct task_struct *last_task_used_math = NULL;
struct task_struct *last_task_used_altivec = NULL;
#endif /* CONFIG_SMP */
static struct fs_struct init_fs = INIT_FS;
static struct files_struct init_files = INIT_FILES;
static struct signal_struct init_signals = INIT_SIGNALS;
struct mm_struct init_mm = INIT_MM(init_mm);

struct mm_struct ioremap_mm = { pgd             : ioremap_dir  
                               ,page_table_lock : SPIN_LOCK_UNLOCKED };

/* this is 16-byte aligned because it has a stack in it */
union task_union __attribute((aligned(16))) init_task_union = {
	INIT_TASK(init_task_union.task)
};

#ifdef CONFIG_SMP
struct current_set_struct current_set[NR_CPUS] = {{&init_task, 0}, };
#endif

char *sysmap = NULL; 
unsigned long sysmap_size = 0;

extern char __toc_start;

#undef SHOW_TASK_SWITCHES

void
enable_kernel_fp(void)
{
#ifdef CONFIG_SMP
	if (current->thread.regs && (current->thread.regs->msr & MSR_FP))
		giveup_fpu(current);
	else
		giveup_fpu(NULL);	/* just enables FP for kernel */
#else
	giveup_fpu(last_task_used_math);
#endif /* CONFIG_SMP */
}

int
dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpregs)
{
	if (regs->msr & MSR_FP)
		giveup_fpu(current);
	memcpy(fpregs, &current->thread.fpr[0], sizeof(*fpregs));
	return 1;
}

#ifdef CONFIG_ALTIVEC
int
dump_altivec(struct pt_regs *regs, elf_vrregset_t *vrregs)
{
	if (regs->msr & MSR_VEC)
		giveup_altivec(current);
	memcpy(vrregs, &current->thread.vr[0], sizeof(*vrregs));
	return 1;
}


void
enable_kernel_altivec(void)
{
#ifdef CONFIG_SMP
	if (current->thread.regs && (current->thread.regs->msr & MSR_VEC))
		giveup_altivec(current);
	else
		giveup_altivec(NULL);	/* just enable AltiVec for kernel - force */
#else
	giveup_altivec(last_task_used_altivec);
#endif /* __SMP __ */
}
#endif /* CONFIG_ALTIVEC */


void
_switch_to(struct task_struct *prev, struct task_struct *new,
	  struct task_struct **last)
{
	struct thread_struct *new_thread, *old_thread;
	unsigned long s;
	
	__save_flags(s);
	__cli();

#ifdef SHOW_TASK_SWITCHES
	printk("%s/%d -> %s/%d NIP %08lx cpu %d root %x/%x\n",
	       prev->comm,prev->pid,
	       new->comm,new->pid,new->thread.regs->nip,new->processor,
	       new->fs->root,prev->fs->root);
#endif
#ifdef CONFIG_SMP
	/* avoid complexity of lazy save/restore of fpu
	 * by just saving it every time we switch out if
	 * this task used the fpu during the last quantum.
	 * 
	 * If it tries to use the fpu again, it'll trap and
	 * reload its fp regs.  So we don't have to do a restore
	 * every switch, just a save.
	 *  -- Cort
	 */
	if ( prev->thread.regs && (prev->thread.regs->msr & MSR_FP) )
		giveup_fpu(prev);
#ifdef CONFIG_ALTIVEC
	/*
	 * If the previous thread used altivec in the last quantum
	 * (thus changing altivec regs) then save them.
	 * We used to check the VRSAVE register but not all apps
	 * set it, so we don't rely on it now (and in fact we need
	 * to save & restore VSCR even if VRSAVE == 0).  -- paulus
	 *
	 * On SMP we always save/restore altivec regs just to avoid the
	 * complexity of changing processors.
	 *  -- Cort
	 */
	if ((prev->thread.regs && (prev->thread.regs->msr & MSR_VEC)))
		giveup_altivec(prev);
#endif /* CONFIG_ALTIVEC */
	/* prev->last_processor = prev->processor; */
	current_set[smp_processor_id()].task = new;
#endif /* CONFIG_SMP */
	new_thread = &new->thread;
	old_thread = &current->thread;
	*last = _switch(old_thread, new_thread);
	__restore_flags(s);
}

void show_regs(struct pt_regs * regs)
{
	int i;

	printk("NIP: %016lX XER: %016lX LR: %016lX REGS: %p TRAP: %04lx    %s\n",
	       regs->nip, regs->xer, regs->link, regs,regs->trap, print_tainted());
	printk("MSR: %016lx EE: %01x PR: %01x FP: %01x ME: %01x IR/DR: %01x%01x\n",
	       regs->msr, regs->msr&MSR_EE ? 1 : 0, regs->msr&MSR_PR ? 1 : 0,
	       regs->msr & MSR_FP ? 1 : 0,regs->msr&MSR_ME ? 1 : 0,
	       regs->msr&MSR_IR ? 1 : 0,
	       regs->msr&MSR_DR ? 1 : 0);
	printk("TASK = %p[%d] '%s' ",
	       current, current->pid, current->comm);
	printk("Last syscall: %ld ", current->thread.last_syscall);
#ifndef CONFIG_SMP
	printk("\nlast math %p last altivec %p", last_task_used_math,
	       last_task_used_altivec);
#endif

#ifdef CONFIG_SMP
	/* printk(" CPU: %d last CPU: %d", current->processor,current->last_processor); */
#endif /* CONFIG_SMP */
	
	printk("\n");
	for (i = 0;  i < 32;  i++)
	{
		long r;
		if ((i % 4) == 0)
		{
			printk("GPR%02d: ", i);
		}

		if ( __get_user(r, &(regs->gpr[i])) )
		    return;

		printk("%016lX ", r);
		if ((i % 4) == 3)
		{
			printk("\n");
		}
	}
}

void exit_thread(void)
{
#ifndef CONFIG_SMP
	if (last_task_used_math == current)
		last_task_used_math = NULL;
	if (last_task_used_altivec == current)
		last_task_used_altivec = NULL;
#endif
}

void flush_thread(void)
{
#ifndef CONFIG_SMP
	if (last_task_used_math == current)
		last_task_used_math = NULL;
	if (last_task_used_altivec == current)
		last_task_used_altivec = NULL;
#endif
}

void
release_thread(struct task_struct *t)
{
}

/*
 * Copy a thread..
 */
int
copy_thread(int nr, unsigned long clone_flags, unsigned long usp,
	    unsigned long unused,
	    struct task_struct * p, struct pt_regs * regs)
{
	unsigned long msr;
	struct pt_regs * childregs, *kregs;
	extern void ret_from_fork(void);

	/* Copy registers */
	childregs = ((struct pt_regs *)
		     ((unsigned long)p + sizeof(union task_union)
		      - STACK_FRAME_OVERHEAD)) - 2;
	*childregs = *regs;
	childregs->gpr[3] = 0;  /* Result from fork() */
	p->thread.regs = childregs;
	p->thread.ksp = (unsigned long) childregs - STACK_FRAME_OVERHEAD;
	p->thread.ksp -= sizeof(struct pt_regs ) + STACK_FRAME_OVERHEAD;
	kregs = (struct pt_regs *)(p->thread.ksp + STACK_FRAME_OVERHEAD);
	/* The PPC64 compiler makes use of a TOC to contain function 
	 * pointers.  The function (ret_from_except) is actually a pointer
	 * to the TOC entry.  The first entry is a pointer to the actual
	 * function.
 	 */
	kregs->nip = *((unsigned long *)ret_from_fork);
	asm volatile("mfmsr %0" : "=r" (msr):);
	kregs->msr = msr;
	kregs->gpr[1] = (unsigned long)childregs - STACK_FRAME_OVERHEAD;
	kregs->gpr[2] = (((unsigned long)&__toc_start) + 0x8000);
	
	if (usp >= (unsigned long) regs) {
		/* Stack is in kernel space - must adjust */
		childregs->gpr[1] = (unsigned long)(childregs + 1);
		*((unsigned long *) childregs->gpr[1]) = 0;
	} else {
		/* Provided stack is in user space */
		childregs->gpr[1] = usp;
	}
	p->thread.last_syscall = -1;
	  
	/*
	 * copy fpu info - assume lazy fpu switch now always
	 *  -- Cort
	 */
	if (regs->msr & MSR_FP) {
		giveup_fpu(current);
		childregs->msr &= ~(MSR_FP | MSR_FE0 | MSR_FE1);
	}
	memcpy(&p->thread.fpr, &current->thread.fpr, sizeof(p->thread.fpr));
	p->thread.fpscr = current->thread.fpscr;
	p->thread.fpexc_mode = current->thread.fpexc_mode;

#ifdef CONFIG_ALTIVEC
	/*
	 * copy altiVec info - assume lazy altiVec switch
	 * - kumar
	 */
	if (regs->msr & MSR_VEC)
		giveup_altivec(current);
	memcpy(&p->thread.vr, &current->thread.vr, sizeof(p->thread.vr));
	p->thread.vscr = current->thread.vscr;
	childregs->msr &= ~MSR_VEC;
#endif /* CONFIG_ALTIVEC */

	return 0;
}

/*
 * Set up a thread for executing a new program
 */
void start_thread(struct pt_regs *regs, unsigned long fdptr, unsigned long sp)
{
	unsigned long entry, toc, load_addr = regs->gpr[2];

	/* fdptr is a relocated pointer to the function descriptor for
         * the elf _start routine.  The first entry in the function
         * descriptor is the entry address of _start and the second
         * entry is the TOC value we need to use.
         */
	set_fs(USER_DS);
	__get_user(entry, (unsigned long *)fdptr);
	__get_user(toc, (unsigned long *)fdptr+1);

	/* Check whether the e_entry function descriptor entries
	 * need to be relocated before we can use them.
	 */
	if ( load_addr != 0 ) {
		entry += load_addr;
		toc   += load_addr;
	}

	regs->nip = entry;
	regs->gpr[1] = sp;
	regs->gpr[2] = toc;
	regs->msr = MSR_USER64;
#ifndef CONFIG_SMP
	if (last_task_used_math == current)
		last_task_used_math = 0;
	if (last_task_used_altivec == current)
		last_task_used_altivec = 0;
#endif /* CONFIG_SMP */
	memset(current->thread.fpr, 0, sizeof(current->thread.fpr));
	current->thread.fpscr = 0;
#ifdef CONFIG_ALTIVEC
	memset(&current->thread.vr[0], 0,offsetof(struct thread_struct,vrsave[2])-
	       offsetof(struct thread_struct,vr[0]));
	current->thread.vscr.u[3] = 0x00010000; /* Java mode disabled */
#endif /* CONFIG_ALTIVEC */
}

# define PR_FP_EXC_DISABLED     0       /* FP exceptions disabled */
# define PR_FP_EXC_NONRECOV     1       /* async non-recoverable exc. mode */
# define PR_FP_EXC_ASYNC        2       /* async recoverable exception mode */
# define PR_FP_EXC_PRECISE      3       /* precise exception mode */

int set_fpexc_mode(struct task_struct *tsk, unsigned int val)
{
	struct pt_regs *regs = tsk->thread.regs;

	if (val > PR_FP_EXC_PRECISE)
		return -EINVAL;
	tsk->thread.fpexc_mode = __pack_fe01(val);
	if (regs != NULL && (regs->msr & MSR_FP) != 0)
		regs->msr = (regs->msr & ~(MSR_FE0|MSR_FE1))
			| tsk->thread.fpexc_mode;
	return 0;
}

int get_fpexc_mode(struct task_struct *tsk, unsigned long adr)
{
	unsigned int val;

	val = __unpack_fe01(tsk->thread.fpexc_mode);
	return put_user(val, (unsigned int *) adr);
}

int sys_clone(int p1, int p2, int p3, int p4, int p5, int p6,
	      struct pt_regs *regs)
{
	return do_fork(p1, regs->gpr[1], regs, 0);
}

int sys_fork(int p1, int p2, int p3, int p4, int p5, int p6,
	     struct pt_regs *regs)
{
	return do_fork(SIGCHLD, regs->gpr[1], regs, 0);
}

int sys_vfork(int p1, int p2, int p3, int p4, int p5, int p6,
			 struct pt_regs *regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs->gpr[1], regs, 0);
}

int sys_execve(unsigned long a0, unsigned long a1, unsigned long a2,
	       unsigned long a3, unsigned long a4, unsigned long a5,
	       struct pt_regs *regs)
{
	int error;
	char * filename;
	
	filename = getname((char *) a0);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	if (regs->msr & MSR_FP)
		giveup_fpu(current);
#ifdef CONFIG_ALTIVEC
	if (regs->msr & MSR_VEC)
		giveup_altivec(current);
#endif /* CONFIG_ALTIVEC */
	error = do_execve(filename, (char **) a1, (char **) a2, regs);
  
	if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);

out:
	return error;
}

struct task_struct * alloc_task_struct(void)
{
	struct task_struct * new_task_ptr;
    
	new_task_ptr = ((struct task_struct *) 
			__get_free_pages(GFP_KERNEL, get_order(THREAD_SIZE)));
    
	return new_task_ptr;
}

void free_task_struct(struct task_struct * task_ptr)
{
	free_pages((unsigned long)(task_ptr), get_order(THREAD_SIZE));
}

void initialize_paca_hardware_interrupt_stack(void)
{
	extern struct systemcfg *systemcfg;

	int i;
	unsigned long stack;
	unsigned long end_of_stack =0;

	for (i=1; i < systemcfg->processorCount; i++) {
		/* Carve out storage for the hardware interrupt stack */
		stack = __get_free_pages(GFP_KERNEL, get_order(8*PAGE_SIZE));

		if ( !stack ) {     
			printk("ERROR, cannot find space for hardware stack.\n");
			panic(" no hardware stack ");
		}


		/* Store the stack value in the PACA for the processor */
		paca[i].xHrdIntStack = stack + (8*PAGE_SIZE) - STACK_FRAME_OVERHEAD;
		paca[i].xHrdIntCount = 0;

	}

	/*
	 * __get_free_pages() might give us a page > KERNBASE+256M which
	 * is mapped with large ptes so we can't set up the guard page.
	 */
	if (cur_cpu_spec->cpu_features & CPU_FTR_16M_PAGE)
		return;

	for (i=0; i < systemcfg->processorCount; i++) {
		/* set page at the top of stack to be protected - prevent overflow */
		end_of_stack = paca[i].xHrdIntStack - (8*PAGE_SIZE - STACK_FRAME_OVERHEAD);
		ppc_md.hpte_updateboltedpp(PP_RXRX,end_of_stack);
	}
}

extern char _stext[], _etext[];

char * ppc_find_proc_name( unsigned * p, char * buf, unsigned buflen )
{
	unsigned long tb_flags;
	unsigned short name_len;
	unsigned long tb_start, code_start, code_ptr, code_offset;
	unsigned code_len;
	strcpy( buf, "Unknown" );
	code_ptr = (unsigned long)p;
	code_offset = 0;
	if ( ( (unsigned long)p >= (unsigned long)_stext ) && ( (unsigned long)p <= (unsigned long)_etext ) ) {
		while ( (unsigned long)p <= (unsigned long)_etext ) {
			if ( *p == 0 ) {
				tb_start = (unsigned long)p;
				++p;	/* Point to traceback flags */
				tb_flags = *((unsigned long *)p);
				p += 2;	/* Skip over traceback flags */
				if ( tb_flags & TB_NAME_PRESENT ) {
					if ( tb_flags & TB_PARMINFO )
						++p;	/* skip over parminfo data */
					if ( tb_flags & TB_HAS_TBOFF ) {
						code_len = *p;	/* get code length */
						code_start = tb_start - code_len;
						code_offset = code_ptr - code_start + 1;
						if ( code_offset > 0x100000 )
							break;
						++p;		/* skip over code size */
					}
					name_len = *((unsigned short *)p);
					if ( name_len > (buflen-20) )
						name_len = buflen-20;
					memcpy( buf, ((char *)p)+2, name_len );
					buf[name_len] = 0;
					if ( code_offset )
						sprintf( buf+name_len, "+0x%lx", code_offset-1 ); 
				}
				break;
			}
			++p;
		}
	}
	return buf;
}

void
print_backtrace(unsigned long *sp)
{
	int cnt = 0;
	unsigned long i;
	char name_buf[256];

	printk("Call backtrace: \n");
	while (sp) {
		if (__get_user(i, &sp[2]))
			break;
		printk("%016lX ", i);
		printk("%s\n", ppc_find_proc_name((unsigned *)i, name_buf, 256));
		if (cnt > 32) break;
		if (__get_user(sp, (unsigned long **)sp))
			break;
	}
	printk("\n");
}

/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched    (*(unsigned long *)scheduling_functions_start_here)
#define last_sched     (*(unsigned long *)scheduling_functions_end_here)

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long ip, sp;
	unsigned long stack_page = (unsigned long)p;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;
	sp = p->thread.ksp;
	do {
		sp = *(unsigned long *)sp;
		if (sp < (stack_page + (2 * PAGE_SIZE)) ||
		    sp >= (stack_page + THREAD_SIZE))
			return 0;
		if (count > 0) {
			ip = *(unsigned long *)(sp + 16);
			/*
			 * XXX we mask the upper 32 bits until procps
			 * gets fixed.
			 */
			if (ip < first_sched || ip >= last_sched)
				return (ip);
		}
	} while (count++ < 16);
	return 0;
}

void show_trace_task(struct task_struct *p)
{
	unsigned long ip, sp;
	unsigned long stack_page = (unsigned long)p;
	int count = 0;

	if (!p)
		return;

	printk("Call Trace: ");
	sp = p->thread.ksp;
	do {
		sp = *(unsigned long *)sp;
		if (sp < (stack_page + (2 * PAGE_SIZE)) ||
		    sp >= (stack_page + THREAD_SIZE))
			break;
		if (count > 0) {
			ip = *(unsigned long *)(sp + 16);
			printk("[%016lx] ", ip);
		}
	} while (count++ < 16);
	printk("\n");
}
