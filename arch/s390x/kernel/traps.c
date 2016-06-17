/*
 *  arch/s390/kernel/traps.c
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 *               Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com),
 *
 *  Derived from "arch/i386/kernel/traps.c"
 *    Copyright (C) 1991, 1992 Linus Torvalds
 */

/*
 * 'Traps.c' handles hardware traps and faults after we have saved some
 * state in 'asm.s'.
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/atomic.h>
#if CONFIG_REMOTE_DEBUG
#include <asm/gdb-stub.h>
#endif
#include <asm/cpcmd.h>
#include <asm/s390_ext.h>

/* Called from entry.S only */
extern void handle_per_exception(struct pt_regs *regs);

typedef void pgm_check_handler_t(struct pt_regs *, long);
pgm_check_handler_t *pgm_check_table[128];

#ifdef CONFIG_SYSCTL
#ifdef CONFIG_PROCESS_DEBUG
int sysctl_userprocess_debug = 1;
#else
int sysctl_userprocess_debug = 0;
#endif
#endif

extern pgm_check_handler_t do_protection_exception;
extern pgm_check_handler_t do_segment_exception;
extern pgm_check_handler_t do_region_exception;
extern pgm_check_handler_t do_page_exception;
#ifdef CONFIG_PFAULT
extern int pfault_init(void);
extern void pfault_fini(void);
extern void pfault_interrupt(struct pt_regs *regs, __u16 error_code);
static ext_int_info_t ext_int_pfault;
#endif

int kstack_depth_to_print = 20;

/*
 * If the address is either in the .text section of the
 * kernel, or in the vmalloc'ed module regions, it *may* 
 * be the address of a calling routine
 */
extern char _stext, _etext;

#ifdef CONFIG_MODULES

extern struct module *module_list;
extern struct module kernel_module;

static inline int kernel_text_address(unsigned long addr)
{
	int retval = 0;
	struct module *mod;

	if (addr >= (unsigned long) &_stext &&
	    addr <= (unsigned long) &_etext)
		return 1;

	for (mod = module_list; mod != &kernel_module; mod = mod->next) {
		/* mod_bound tests for addr being inside the vmalloc'ed
		 * module area. Of course it'd be better to test only
		 * for the .text subset... */
		if (mod_bound(addr, 0, mod)) {
			retval = 1;
			break;
		}
	}

	return retval;
}

#else

static inline int kernel_text_address(unsigned long addr)
{
	return (addr >= (unsigned long) &_stext &&
		addr <= (unsigned long) &_etext);
}

#endif

void show_trace(unsigned long * stack)
{
	unsigned long backchain, low_addr, high_addr, ret_addr;
	int i;

	if (!stack)
		stack = (unsigned long*)&stack;

	printk("Call Trace: ");
	low_addr = ((unsigned long) stack) & PSW_ADDR_MASK;
	high_addr = (low_addr & (-THREAD_SIZE)) + THREAD_SIZE;
	/* Skip the first frame (biased stack) */
	backchain = *((unsigned long *) low_addr) & PSW_ADDR_MASK;
	/* Print up to 8 lines */
	for (i = 0; i < 8; i++) {
		if (backchain < low_addr || backchain >= high_addr)
			break;
		ret_addr = *((unsigned long *) (backchain+112)) & PSW_ADDR_MASK;
		if (!kernel_text_address(ret_addr))
			break;
		if (i && ((i % 3) == 0))
			printk("\n   ");
		printk("[<%016lx>] ", ret_addr);
		low_addr = backchain;
		backchain = *((unsigned long *) backchain) & PSW_ADDR_MASK;
	}
	printk("\n");
}

void show_trace_task(struct task_struct *tsk)
{
	/*
	 * We can't print the backtrace of a running process. It is
	 * unreliable at best and can cause kernel oopses.
	 */
	if (task_has_cpu(tsk))
		return;
	show_trace((unsigned long *) tsk->thread.ksp);
}

void show_stack(unsigned long *sp)
{
	unsigned long *stack;
	int i;

	// debugging aid: "show_stack(NULL);" prints the
	// back trace for this cpu.

	if (sp == NULL)
		sp = (unsigned long*) &sp;

	stack = sp;
	for (i = 0; i < kstack_depth_to_print; i++) {
		if (((addr_t) stack & (THREAD_SIZE-1)) == 0)
			break;
		if (i && ((i % 4) == 0))
			printk("\n       ");
		printk("%016lx ", *stack++);
	}
	printk("\n");
	show_trace(sp);
}

void show_registers(struct pt_regs *regs)
{
	mm_segment_t old_fs;
	char *mode;
	int i;

	mode = (regs->psw.mask & PSW_PROBLEM_STATE) ? "User" : "Krnl";
	printk("%s PSW : %016lx %016lx\n",
	       mode, (unsigned long) regs->psw.mask,
	       (unsigned long) regs->psw.addr);
	printk("%s GPRS: %016lx %016lx %016lx %016lx\n", mode,
	       regs->gprs[0], regs->gprs[1], regs->gprs[2], regs->gprs[3]);
	printk("           %016lx %016lx %016lx %016lx\n",
	       regs->gprs[4], regs->gprs[5], regs->gprs[6], regs->gprs[7]);
	printk("           %016lx %016lx %016lx %016lx\n",
	       regs->gprs[8], regs->gprs[9], regs->gprs[10], regs->gprs[11]);
	printk("           %016lx %016lx %016lx %016lx\n",
	       regs->gprs[12], regs->gprs[13], regs->gprs[14], regs->gprs[15]);
	printk("%s ACRS: %08x %08x %08x %08x\n", mode,
	       regs->acrs[0], regs->acrs[1], regs->acrs[2], regs->acrs[3]);
	printk("           %08x %08x %08x %08x\n",
	       regs->acrs[4], regs->acrs[5], regs->acrs[6], regs->acrs[7]);
	printk("           %08x %08x %08x %08x\n",
	       regs->acrs[8], regs->acrs[9], regs->acrs[10], regs->acrs[11]);
	printk("           %08x %08x %08x %08x\n",
	       regs->acrs[12], regs->acrs[13], regs->acrs[14], regs->acrs[15]);

	/*
	 * Print the first 20 byte of the instruction stream at the
	 * time of the fault.
	 */
	old_fs = get_fs();
	if (regs->psw.mask & PSW_PROBLEM_STATE)
		set_fs(USER_DS);
	else
		set_fs(KERNEL_DS);
	printk("%s Code: ", mode);
	for (i = 0; i < 20; i++) {
		unsigned char c;
		if (__get_user(c, (char *)(regs->psw.addr + i))) {
			printk(" Bad PSW.");
			break;
		}
		printk("%02x ", c);
	}
	set_fs(old_fs);

	printk("\n");
}	

/* This is called from fs/proc/array.c */
char *task_show_regs(struct task_struct *task, char *buf)
{
	struct pt_regs *regs;

	regs = __KSTK_PTREGS(task);
	buf += sprintf(buf, "task: %016lx, ksp: %016lx\n",
		       (unsigned long) task, task->thread.ksp);
	buf += sprintf(buf, "User PSW : %016lx %016lx\n",
		       (unsigned long) regs->psw.mask, 
		       (unsigned long) regs->psw.addr);
	buf += sprintf(buf, "User GPRS: %016lx %016lx %016lx %016lx\n",
		       regs->gprs[0], regs->gprs[1],
		       regs->gprs[2], regs->gprs[3]);
	buf += sprintf(buf, "           %016lx %016lx %016lx %016lx\n",
		       regs->gprs[4], regs->gprs[5], 
		       regs->gprs[6], regs->gprs[7]);
	buf += sprintf(buf, "           %016lx %016lx %016lx %016lx\n",
		       regs->gprs[8], regs->gprs[9],
		       regs->gprs[10], regs->gprs[11]);
	buf += sprintf(buf, "           %016lx %016lx %016lx %016lx\n",
		       regs->gprs[12], regs->gprs[13],
		       regs->gprs[14], regs->gprs[15]);
	buf += sprintf(buf, "User ACRS: %08x %08x %08x %08x\n",
		       regs->acrs[0], regs->acrs[1],
		       regs->acrs[2], regs->acrs[3]);
	buf += sprintf(buf, "           %08x %08x %08x %08x\n",
		       regs->acrs[4], regs->acrs[5],
		       regs->acrs[6], regs->acrs[7]);
	buf += sprintf(buf, "           %08x %08x %08x %08x\n",
		       regs->acrs[8], regs->acrs[9],
		       regs->acrs[10], regs->acrs[11]);
	buf += sprintf(buf, "           %08x %08x %08x %08x\n",
		       regs->acrs[12], regs->acrs[13],
		       regs->acrs[14], regs->acrs[15]);
	return buf;
}

spinlock_t die_lock = SPIN_LOCK_UNLOCKED;

void die(const char * str, struct pt_regs * regs, long err)
{
        console_verbose();
        spin_lock_irq(&die_lock);
	bust_spinlocks(1);
        printk("%s: %04lx\n", str, err & 0xffff);
        show_regs(regs);
	bust_spinlocks(0);
        spin_unlock_irq(&die_lock);
        do_exit(SIGSEGV);
}

static void inline do_trap(long interruption_code, int signr, char *str,
                           struct pt_regs *regs, siginfo_t *info)
{
	/*
	 * We got all needed information from the lowcore and can
	 * now safely switch on interrupts.
	 */
	if (regs->psw.mask & PSW_PROBLEM_STATE)
		__sti();

        if (regs->psw.mask & PSW_PROBLEM_STATE) {
                struct task_struct *tsk = current;
                tsk->thread.trap_no = interruption_code & 0xffff;
		if (info)
			force_sig_info(signr, info, tsk);
		else
                	force_sig(signr, tsk);
#ifndef CONFIG_SYSCTL
#ifdef CONFIG_PROCESS_DEBUG
                printk("User process fault: interruption code 0x%lX\n",
                       interruption_code);
                show_regs(regs);
#endif
#else
		if (sysctl_userprocess_debug) {
			printk("User process fault: interruption code 0x%lX\n",
			       interruption_code);
			show_regs(regs);
		}
#endif
        } else {
                unsigned long fixup = search_exception_table(regs->psw.addr);
                if (fixup)
                        regs->psw.addr = fixup;
                else
                        die(str, regs, interruption_code);
        }
}

static inline void *get_check_address(struct pt_regs *regs)
{
	return (void *) ADDR_BITS_REMOVE(regs->psw.addr-S390_lowcore.pgm_ilc);
}

int do_debugger_trap(struct pt_regs *regs,int signal)
{
	if(regs->psw.mask&PSW_PROBLEM_STATE)
	{
		if(current->ptrace & PT_PTRACED)
			force_sig(signal,current);
		else
			return 1;
	}
	else
	{
#if CONFIG_REMOTE_DEBUG
		if(gdb_stub_initialised)
		{
			gdb_stub_handle_exception(regs, signal);
			return 0;
		}
#endif
		return 1;
	}
	return 0;
}

#define DO_ERROR(signr, str, name) \
asmlinkage void name(struct pt_regs * regs, long interruption_code) \
{ \
	do_trap(interruption_code, signr, str, regs, NULL); \
}

#define DO_ERROR_INFO(signr, str, name, sicode, siaddr) \
asmlinkage void name(struct pt_regs * regs, long interruption_code) \
{ \
        siginfo_t info; \
        info.si_signo = signr; \
        info.si_errno = 0; \
        info.si_code = sicode; \
        info.si_addr = (void *)siaddr; \
        do_trap(interruption_code, signr, str, regs, &info); \
}

DO_ERROR(SIGSEGV, "Unknown program exception", default_trap_handler)

DO_ERROR_INFO(SIGBUS, "addressing exception", addressing_exception,
	      BUS_ADRERR, get_check_address(regs))
DO_ERROR_INFO(SIGILL,  "execute exception", execute_exception,
	      ILL_ILLOPN, get_check_address(regs))
DO_ERROR_INFO(SIGFPE,  "fixpoint divide exception", divide_exception,
	      FPE_INTDIV, get_check_address(regs))
DO_ERROR_INFO(SIGILL,  "operand exception", operand_exception,
	      ILL_ILLOPN, get_check_address(regs))
DO_ERROR_INFO(SIGILL,  "privileged operation", privileged_op,
	      ILL_PRVOPC, get_check_address(regs))
DO_ERROR_INFO(SIGILL,  "special operation exception", special_op_exception,
	      ILL_ILLOPN, get_check_address(regs))
DO_ERROR_INFO(SIGILL,  "specification exception", specification_exception,
	      ILL_ILLOPN, get_check_address(regs))
DO_ERROR_INFO(SIGILL,  "translation exception", translation_exception,
	      ILL_ILLOPN, get_check_address(regs))

static inline void
do_fp_trap(struct pt_regs *regs, void *location,
           int fpc, long interruption_code)
{
	siginfo_t si;

	si.si_signo = SIGFPE;
	si.si_errno = 0;
	si.si_addr = location;
	si.si_code = 0;
	/* FPC[2] is Data Exception Code */
	if ((fpc & 0x00000300) == 0) {
		/* bits 6 and 7 of DXC are 0 iff IEEE exception */
		if (fpc & 0x8000) /* invalid fp operation */
			si.si_code = FPE_FLTINV;
		else if (fpc & 0x4000) /* div by 0 */
			si.si_code = FPE_FLTDIV;
		else if (fpc & 0x2000) /* overflow */
			si.si_code = FPE_FLTOVF;
		else if (fpc & 0x1000) /* underflow */
			si.si_code = FPE_FLTUND;
		else if (fpc & 0x0800) /* inexact */
			si.si_code = FPE_FLTRES;
	}
	current->thread.ieee_instruction_pointer = (addr_t) location;
	do_trap(interruption_code, SIGFPE,
		"floating point exception", regs, &si);
}

asmlinkage void illegal_op(struct pt_regs * regs, long interruption_code)
{
        __u8 opcode[6];
	__u16 *location;
	int do_sig = 0;

	location = (__u16 *) get_check_address(regs);

	/*
	 * We got all needed information from the lowcore and can
	 * now safely switch on interrupts.
	 */
	if (regs->psw.mask & PSW_PROBLEM_STATE)
		__sti();

	/* WARNING don't change this check back to */
	/* int problem_state=(regs->psw.mask & PSW_PROBLEM_STATE); */
	/* & then doing if(problem_state) an int is too small for this */
	/* check on 64 bit. */
	if(regs->psw.mask & PSW_PROBLEM_STATE)
		get_user(*((__u16 *) opcode), location);
	else
		*((__u16 *)opcode)=*((__u16 *)location);
	if(*((__u16 *)opcode)==S390_BREAKPOINT_U16)
        {
		if(do_debugger_trap(regs,SIGTRAP))
			do_sig=1;
	}
	else
		do_sig = 1;
	if (do_sig)
		do_trap(interruption_code, SIGILL,
			"illegal operation", regs, NULL);
}

asmlinkage void data_exception(struct pt_regs * regs, long interruption_code)
{
	__u16 *location;

	location = (__u16 *) get_check_address(regs);

	/*
	 * We got all needed information from the lowcore and can
	 * now safely switch on interrupts.
	 */
	if (regs->psw.mask & PSW_PROBLEM_STATE)
		__sti();

	__asm__ volatile ("stfpc %0\n\t" 
			  : "=m" (current->thread.fp_regs.fpc));

	if (current->thread.fp_regs.fpc & FPC_DXC_MASK)
		do_fp_trap(regs, location,
                           current->thread.fp_regs.fpc, interruption_code);
        else {
		siginfo_t info;
		info.si_signo = SIGILL;
		info.si_errno = 0;
		info.si_code = ILL_ILLOPN;
		info.si_addr = location;
		do_trap(interruption_code, SIGILL, 
			"data exception", regs, &info);
	}
}



/* init is done in lowcore.S and head.S */

void __init trap_init(void)
{
        int i;

        for (i = 0; i < 128; i++)
          pgm_check_table[i] = &default_trap_handler;
        pgm_check_table[1] = &illegal_op;
        pgm_check_table[2] = &privileged_op;
        pgm_check_table[3] = &execute_exception;
        pgm_check_table[4] = &do_protection_exception;
        pgm_check_table[5] = &addressing_exception;
        pgm_check_table[6] = &specification_exception;
        pgm_check_table[7] = &data_exception;
        pgm_check_table[9] = &divide_exception;
        pgm_check_table[0x12] = &translation_exception;
        pgm_check_table[0x13] = &special_op_exception;
        pgm_check_table[0x15] = &operand_exception;
        pgm_check_table[0x10] = &do_segment_exception;
        pgm_check_table[0x11] = &do_page_exception;
        pgm_check_table[0x1C] = &privileged_op;
        pgm_check_table[0x38] = &addressing_exception;
        pgm_check_table[0x3B] = &do_region_exception;
#ifdef CONFIG_PFAULT
	if (MACHINE_IS_VM) {
		/* request the 0x2603 external interrupt */
		if (register_early_external_interrupt(0x2603, pfault_interrupt,
						      &ext_int_pfault) != 0)
			panic("Couldn't request external interrupt 0x2603");
		/*
		 * Try to get pfault pseudo page faults going.
		 */
		if (pfault_init() != 0) {
			/* Tough luck, no pfault. */
			unregister_early_external_interrupt(0x2603,
							    pfault_interrupt,
							    &ext_int_pfault);
		}
	}
#endif
}


void handle_per_exception(struct pt_regs *regs)
{
	if(regs->psw.mask&PSW_PROBLEM_STATE)
	{
		per_struct *per_info=&current->thread.per_info;
		per_info->lowcore.words.perc_atmid=S390_lowcore.per_perc_atmid;
		per_info->lowcore.words.address=S390_lowcore.per_address;
		per_info->lowcore.words.access_id=S390_lowcore.per_access_id;
	}
	if(do_debugger_trap(regs,SIGTRAP))
	{
		/* I've seen this possibly a task structure being reused ? */
		printk("Spurious per exception detected\n");
		printk("switching off per tracing for this task.\n");
		show_regs(regs);
		/* Hopefully switching off per tracing will help us survive */
		regs->psw.mask &= ~PSW_PER_MASK;
	}
}

