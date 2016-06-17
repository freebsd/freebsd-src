/*
 *  arch/ppc/kernel/traps.c
 *
 *  Copyright (C) 1995-1996  Gary Thomas (gdt@linuxppc.org)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 *  Modified by Cort Dougan (cort@cs.nmt.edu)
 *  and Paul Mackerras (paulus@cs.anu.edu.au)
 */

/*
 * This file handles the architecture-dependent parts of hardware exceptions
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/interrupt.h>
#include <linux/config.h>
#include <linux/init.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#ifdef CONFIG_PMAC_BACKLIGHT
#include <asm/backlight.h>
#endif

extern int fix_alignment(struct pt_regs *);
extern void bad_page_fault(struct pt_regs *, unsigned long, int sig);

#ifdef CONFIG_XMON
extern void xmon(struct pt_regs *regs);
extern int xmon_bpt(struct pt_regs *regs);
extern int xmon_sstep(struct pt_regs *regs);
extern int xmon_iabr_match(struct pt_regs *regs);
extern int xmon_dabr_match(struct pt_regs *regs);
extern void (*xmon_fault_handler)(struct pt_regs *regs);
#endif

#ifdef CONFIG_XMON
void (*debugger)(struct pt_regs *regs) = xmon;
int (*debugger_bpt)(struct pt_regs *regs) = xmon_bpt;
int (*debugger_sstep)(struct pt_regs *regs) = xmon_sstep;
int (*debugger_iabr_match)(struct pt_regs *regs) = xmon_iabr_match;
int (*debugger_dabr_match)(struct pt_regs *regs) = xmon_dabr_match;
void (*debugger_fault_handler)(struct pt_regs *regs);
#else
#ifdef CONFIG_KGDB
void (*debugger)(struct pt_regs *regs);
int (*debugger_bpt)(struct pt_regs *regs);
int (*debugger_sstep)(struct pt_regs *regs);
int (*debugger_iabr_match)(struct pt_regs *regs);
int (*debugger_dabr_match)(struct pt_regs *regs);
void (*debugger_fault_handler)(struct pt_regs *regs);
#else
#define debugger(regs)			do { } while (0)
#define debugger_bpt(regs)		0
#define debugger_sstep(regs)		0
#define debugger_iabr_match(regs)	0
#define debugger_dabr_match(regs)	0
#define debugger_fault_handler		((void (*)(struct pt_regs *))0)
#endif
#endif

/*
 * Trap & Exception support
 */


spinlock_t die_lock = SPIN_LOCK_UNLOCKED;

void die(const char * str, struct pt_regs * fp, long err)
{
	console_verbose();
	spin_lock_irq(&die_lock);
#ifdef CONFIG_PMAC_BACKLIGHT
	set_backlight_enable(1);
	set_backlight_level(BACKLIGHT_MAX);
#endif
	printk("Oops: %s, sig: %ld\n", str, err);
	show_regs(fp);
	spin_unlock_irq(&die_lock);
	/* do_exit() should take care of panic'ing from an interrupt
	 * context so we don't handle it here
	 */
	do_exit(err);
}

void
_exception(int signr, struct pt_regs *regs, int code, unsigned long addr)
{
	siginfo_t info;

	if (!user_mode(regs)) {
		debugger(regs);
		die("Exception in kernel mode", regs, signr);
	}
	info.si_signo = signr;
	info.si_errno = 0;
	info.si_code = code;
	info.si_addr = (void *) addr;
	force_sig_info(signr, &info, current);
}

/*
 * I/O accesses can cause machine checks on powermacs.
 * Check if the NIP corresponds to the address of a sync
 * instruction for which there is an entry in the exception
 * table.
 * Note that the 601 only takes a machine check on TEA
 * (transfer error ack) signal assertion, and does not
 * set any of the top 16 bits of SRR1.
 *  -- paulus.
 */
static inline int check_io_access(struct pt_regs *regs)
{
#ifdef CONFIG_ALL_PPC
	unsigned long fixup;
	unsigned long msr = regs->msr;

	if (((msr & 0xffff0000) == 0 || (msr & (0x80000 | 0x40000)))
	    && (fixup = search_exception_table(regs->nip)) != 0) {
		/*
		 * Check that it's a sync instruction, or somewhere
		 * in the twi; isync; nop sequence that inb/inw/inl uses.
		 * As the address is in the exception table
		 * we should be able to read the instr there.
		 * For the debug message, we look at the preceding
		 * load or store.
		 */
		unsigned int *nip = (unsigned int *)regs->nip;
		if (*nip == 0x60000000)		/* nop */
			nip -= 2;
		else if (*nip == 0x4c00012c)	/* isync */
			--nip;
		if (*nip == 0x7c0004ac || (*nip >> 26) == 3) {
			/* sync or twi */
			unsigned int rb;

			--nip;
			rb = (*nip >> 11) & 0x1f;
			printk(KERN_DEBUG "%s bad port %lx at %p\n",
			       (*nip & 0x100)? "OUT to": "IN from",
			       regs->gpr[rb] - _IO_BASE, nip);
			regs->nip = fixup;
			return 1;
		}
	}
#endif /* CONFIG_ALL_PPC */
	return 0;
}

#if defined(CONFIG_4xx) || defined(CONFIG_BOOKE)
/* On 4xx, the reason for the machine check or program exception
   is in the ESR. */
#define get_reason(regs)	((regs)->dsisr)
#define REASON_FP		0
#define REASON_ILLEGAL		ESR_PIL
#define REASON_PRIVILEGED	ESR_PPR
#define REASON_TRAP		ESR_PTR

/* single-step stuff */
#define single_stepping(regs)	(current->thread.dbcr0 & DBCR0_IC)
#define clear_single_step(regs)	(current->thread.dbcr0 &= ~DBCR0_IC)

#else
/* On non-4xx, the reason for the machine check or program
   exception is in the MSR. */
#define get_reason(regs)	((regs)->msr)
#define REASON_FP		0x100000
#define REASON_ILLEGAL		0x80000
#define REASON_PRIVILEGED	0x40000
#define REASON_TRAP		0x20000

#define single_stepping(regs)	((regs)->msr & MSR_SE)
#define clear_single_step(regs)	((regs)->msr &= ~MSR_SE)
#endif

void
MachineCheckException(struct pt_regs *regs)
{
	unsigned long reason = get_reason(regs);

	if (user_mode(regs)) {
		_exception(SIGBUS, regs, BUS_ADRERR, regs->nip);
		return;
	}

#if defined(CONFIG_8xx) && defined(CONFIG_PCI)
	/* the qspan pci read routines can cause machine checks -- Cort */
	bad_page_fault(regs, regs->dar, SIGBUS);
	return;
#endif
	if (debugger_fault_handler) {
		debugger_fault_handler(regs);
		return;
	}
	if (check_io_access(regs))
		return;

#if defined(CONFIG_4xx) && !defined(CONFIG_440A)
	if (reason & ESR_IMCP) {
		printk("Instruction");
		mtspr(SPRN_ESR, reason & ~ESR_IMCP);
	} else
		printk("Data");
	printk(" machine check in kernel mode.\n");
#elif defined(CONFIG_440A)
	printk("Machine check in kernel mode.\n");
	if (reason & ESR_IMCP){
		printk("Instruction Synchronous Machine Check exception\n");
		mtspr(SPRN_ESR, reason & ~ESR_IMCP);
	}
	else {
		u32 mcsr = mfspr(SPRN_MCSR);
		if (mcsr & MCSR_IB)
			printk("Instruction Read PLB Error\n");
		if (mcsr & MCSR_DRB)
			printk("Data Read PLB Error\n");
		if (mcsr & MCSR_DWB)
			printk("Data Write PLB Error\n");
		if (mcsr & MCSR_TLBP)
			printk("TLB Parity Error\n");
		if (mcsr & MCSR_ICP){
			flush_instruction_cache();
			printk("I-Cache Parity Error\n");
		}	
		if (mcsr & MCSR_DCSP)
			printk("D-Cache Search Parity Error\n");
		if (mcsr & MCSR_DCFP)
			printk("D-Cache Flush Parity Error\n");
		if (mcsr & MCSR_IMPE)
			printk("Machine Check exception is imprecise\n");
		
		/* Clear MCSR */
		mtspr(SPRN_MCSR, mcsr);
	}
#else /* !CONFIG_4xx && !CONFIG_E500 */
	printk("Machine check in kernel mode.\n");
	printk("Caused by (from SRR1=%lx): ", reason);
	switch (reason & 0x601F0000) {
	case 0x80000:
		printk("Machine check signal\n");
		break;
	case 0:		/* for 601 */
	case 0x40000:
	case 0x140000:	/* 7450 MSS error and TEA */
		printk("Transfer error ack signal\n");
		break;
	case 0x20000:
		printk("Data parity error signal\n");
		break;
	case 0x10000:
		printk("Address parity error signal\n");
		break;
	case 0x20000000:
		printk("L1 Data Cache error\n");
		break;
	case 0x40000000:
		printk("L1 Instruction Cache error\n");
		break;
	case 0x00100000:
		printk("L2 data cache parity error\n");
		break;
	default:
		printk("Unknown values in msr\n");
	}
#endif /* CONFIG_4xx */

	debugger(regs);
	die("machine check", regs, SIGBUS);
}

void
SMIException(struct pt_regs *regs)
{
	debugger(regs);
#if !(defined(CONFIG_XMON) || defined(CONFIG_KGDB))
	show_regs(regs);
	panic("System Management Interrupt");
#endif
}

void
UnknownException(struct pt_regs *regs)
{
	printk("Bad trap at PC: %lx, SR: %lx, vector=%lx    %s\n",
	       regs->nip, regs->msr, regs->trap, print_tainted());
	_exception(SIGTRAP, regs, 0, 0);
}

void
InstructionBreakpoint(struct pt_regs *regs)
{
	if (debugger_iabr_match(regs))
		return;
	_exception(SIGTRAP, regs, TRAP_BRKPT, 0);
}

void
RunModeException(struct pt_regs *regs)
{
	_exception(SIGTRAP, regs, 0, 0);
}

/* Illegal instruction emulation support.  Originally written to
 * provide the PVR to user applications using the mfspr rd, PVR.
 * Return non-zero if we can't emulate, or EFAULT if the associated
 * memory access caused an access fault.  Return zero on success.
 *
 * There are a couple of ways to do this, either "decode" the instruction
 * or directly match lots of bits.  In this case, matching lots of
 * bits is faster and easier.
 *
 */
#define INST_MFSPR_PVR		0x7c1f42a6
#define INST_MFSPR_PVR_MASK	0xfc1fffff

static int
emulate_instruction(struct pt_regs *regs)
{
	u32 instword;
	u32 rd;
	int retval;

	retval = -EINVAL;

	if (!user_mode(regs))
		return retval;

	if (get_user(instword, (u32 *)(regs->nip)))
		return -EFAULT;

	/* Emulate the mfspr rD, PVR.
	 */
	if ((instword & INST_MFSPR_PVR_MASK) == INST_MFSPR_PVR) {
		rd = (instword >> 21) & 0x1f;
		regs->gpr[rd] = mfspr(PVR);
		retval = 0;
		regs->nip += 4;
	}
	return retval;
}

/*
 * After we have successfully emulated an instruction, we have to
 * check if the instruction was being single-stepped, and if so,
 * pretend we got a single-step exception.  This was pointed out
 * by Kumar Gala.  -- paulus
 */
static void emulate_single_step(struct pt_regs *regs)
{
	if (single_stepping(regs)) {
		clear_single_step(regs);
		if (debugger_sstep(regs))
			return;
		_exception(SIGTRAP, regs, TRAP_TRACE, 0);
	}
}

void
ProgramCheckException(struct pt_regs *regs)
{
	unsigned int reason = get_reason(regs);
	extern int do_mathemu(struct pt_regs *regs);

#ifdef CONFIG_MATH_EMULATION
	/* (reason & REASON_ILLEGAL) would be the obvious thing here,
	 * but there seems to be a hardware bug on the 405GP (RevD)
	 * that means ESR is sometimes set incorrectly - either to
	 * ESR_DST (!?) or 0.  In the process of chasing this with the
	 * hardware people - not sure if it can happen on any illegal
	 * instruction or only on FP instructions, whether there is a
	 * pattern to occurences etc. -dgibson 31/Mar/2003 */
	if (!(reason & REASON_TRAP) && do_mathemu(regs) == 0) {
		emulate_single_step(regs);
		return;
	}
#endif /* CONFIG_MATH_EMULATION */

	if (reason & REASON_FP) {
		/* IEEE FP exception */
		int code = 0;
		u32 fpscr;

		if (regs->msr & MSR_FP)
			giveup_fpu(current);
		fpscr = current->thread.fpscr;
		fpscr &= fpscr << 22;	/* mask summary bits with enables */
		if (fpscr & FPSCR_VX)
			code = FPE_FLTINV;
		else if (fpscr & FPSCR_OX)
			code = FPE_FLTOVF;
		else if (fpscr & FPSCR_UX)
			code = FPE_FLTUND;
		else if (fpscr & FPSCR_ZX)
			code = FPE_FLTDIV;
		else if (fpscr & FPSCR_XX)
			code = FPE_FLTRES;
		_exception(SIGFPE, regs, code, regs->nip);
		return;
	}

	if (reason & REASON_TRAP) {
		/* trap exception */
		if (debugger_bpt(regs))
			return;
		_exception(SIGTRAP, regs, TRAP_BRKPT, 0);
		return;
	}

	if (reason & REASON_PRIVILEGED) {
		/* Try to emulate it if we should. */
		if (emulate_instruction(regs) == 0) {
			emulate_single_step(regs);
			return;
		}
		_exception(SIGILL, regs, ILL_PRVOPC, regs->nip);
		return;
	}

	_exception(SIGILL, regs, ILL_ILLOPC, regs->nip);
}

void
SingleStepException(struct pt_regs *regs)
{
	regs->msr &= ~MSR_SE;  /* Turn off 'trace' bit */
	if (debugger_sstep(regs))
		return;
	_exception(SIGTRAP, regs, TRAP_TRACE, 0);
}

void
AlignmentException(struct pt_regs *regs)
{
	int fixed;

	fixed = fix_alignment(regs);
	if (fixed == 1) {
		regs->nip += 4;	/* skip over emulated instruction */
		emulate_single_step(regs);
		return;
	}
	if (fixed == -EFAULT) {
		/* fixed == -EFAULT means the operand address was bad */
		if (user_mode(regs))
			_exception(SIGSEGV, regs, SEGV_ACCERR, regs->dar);
		else
			bad_page_fault(regs, regs->dar, SIGSEGV);
		return;
	}
	_exception(SIGBUS, regs, BUS_ADRALN, regs->dar);
}

void
StackOverflow(struct pt_regs *regs)
{
	printk(KERN_CRIT "Kernel stack overflow in process %p, r1=%lx\n",
	       current, regs->gpr[1]);
	debugger(regs);
	show_regs(regs);
	panic("kernel stack overflow");
}

void
trace_syscall(struct pt_regs *regs)
{
	printk("Task: %p(%d), PC: %08lX/%08lX, Syscall: %3ld, Result: %s%ld    %s\n",
	       current, current->pid, regs->nip, regs->link, regs->gpr[0],
	       regs->ccr&0x10000000?"Error=":"", regs->gpr[3], print_tainted());
}

#ifdef CONFIG_8xx
void
SoftwareEmulation(struct pt_regs *regs)
{
	extern int do_mathemu(struct pt_regs *);
	extern int Soft_emulate_8xx(struct pt_regs *);
	int errcode;

	if (!user_mode(regs)) {
		debugger(regs);
		die("Kernel Mode Software FPU Emulation", regs, SIGFPE);
	}

#ifdef CONFIG_MATH_EMULATION
	errcode = do_mathemu(regs);
#else
	errcode = Soft_emulate_8xx(regs);
#endif
	if (errcode) {
		if (errcode > 0)
			_exception(SIGFPE, regs, 0, 0);
		else if (errcode == -EFAULT)
			_exception(SIGSEGV, regs, 0, 0);
		else
			_exception(SIGILL, regs, ILL_ILLOPC, regs->nip);
	} else
		emulate_single_step(regs);
}
#endif /* CONFIG_8xx */

#if defined(CONFIG_4xx) || defined(CONFIG_BOOKE)

void DebugException(struct pt_regs *regs)
{
	unsigned long debug_status;

	debug_status = mfspr(SPRN_DBSR);

	regs->msr &= ~MSR_DE;  /* Turn off 'debug' bit */
	if (debug_status & DBSR_TIE) {		/* trap instruction*/

		mtspr(SPRN_DBSR, DBSR_TIE);

		if (!user_mode(regs) && debugger_bpt(regs))
			return;
		_exception(SIGTRAP, regs, 0, 0);

	} else if (debug_status & DBSR_IC) {	/* instruction completion */

		mtspr(SPRN_DBSR, DBSR_IC);
		current->thread.dbcr0 &=  ~DBCR0_IC;

		if (!user_mode(regs) && debugger_sstep(regs))
			return;
		_exception(SIGTRAP, regs, 0, 0);
	}
}
#endif /* CONFIG_4xx || CONFIG_BOOKE */

#if !defined(CONFIG_TAU_INT)
void
TAUException(struct pt_regs *regs)
{
	printk("Thermal trap at PC: %lx, SR: %lx, vector=%lx    %s\n",
	       regs->nip, regs->msr, regs->trap, print_tainted());
}
#endif /* CONFIG_INT_TAU */

#ifdef CONFIG_ALTIVEC
void
AltivecAssistException(struct pt_regs *regs)
{
	if (regs->msr & MSR_VEC)
		giveup_altivec(current);
	/* XXX quick hack for now: set the non-Java bit in the VSCR */
	current->thread.vscr.u[3] |= 0x10000;
}
#endif /* CONFIG_ALTIVEC */

void __init trap_init(void)
{
}
