/*
 *  linux/arch/ppc64/kernel/traps.c
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
#include <asm/iSeries/HvCall.h>
#include <asm/iSeries/HvCallCfg.h>

#ifdef CONFIG_KDB
#include <linux/kdb.h>
#endif

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/ppcdebug.h>
#include <asm/machdep.h> /* for ppc_attention_msg */

extern int fix_alignment(struct pt_regs *);
extern void bad_page_fault(struct pt_regs *, unsigned long);

/* This is true if we are using the firmware NMI handler (typically LPAR) */
extern int fwnmi_active;
/* This is true if we are using a check-exception based handler */
extern int check_exception_flag;

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
#ifdef CONFIG_KDB
void (*debugger)(struct pt_regs *regs);
int (*debugger_bpt)(struct pt_regs *regs);
int (*debugger_sstep)(struct pt_regs *regs);
int (*debugger_iabr_match)(struct pt_regs *regs);
int (*debugger_dabr_match)(struct pt_regs *regs);
void (*debugger_fault_handler)(struct pt_regs *regs);
#endif /* kdb */
#endif /* kgdb */
#endif /* xmon */

void set_local_DABR(void *valp);

/* do not want to kmalloc or wait on lock during machine check */
char mce_data_buf[RTAS_ERROR_LOG_MAX]__page_aligned;

/*
 * Trap & Exception support
 */

static void
_exception(int signr, siginfo_t *info, struct pt_regs *regs)
{
	if (!user_mode(regs))
	{
		show_regs(regs);
#if defined(CONFIG_XMON) || defined(CONFIG_KGDB) || defined(CONFIG_KDB)
		debugger(regs);
#endif
		print_backtrace((unsigned long *)regs->gpr[1]);
		panic("Exception in kernel pc %lx signal %d",regs->nip,signr);
#if defined(CONFIG_PPCDBG) && (defined(CONFIG_XMON) || defined(CONFIG_KGDB))
	/* Allow us to catch SIGILLs for 64-bit app/glibc debugging. -Peter */
	} else if (signr == SIGILL) {
		ifppcdebug(PPCDBG_SIGNALXMON)
			debugger(regs);
#endif
	}
	force_sig_info(signr, info, current);
}

/* Get the error information for errors coming through the
 * FWNMI vectors.  The pt_regs' r3 will be updated to reflect
 * the actual r3 if possible, and a ptr to the error log entry
 * will be returned if found.
 */
static struct rtas_error_log *FWNMI_get_errinfo(struct pt_regs *regs)
{
	unsigned long errdata = regs->gpr[3];
	struct rtas_error_log *errhdr = NULL;
	unsigned long *savep;

	if ((errdata >= 0x7000 && errdata < 0x7fff0) ||
	    (errdata >= rtas.base && errdata < rtas.base + rtas.size - 16)) {
		savep = __va(errdata);
		regs->gpr[3] = savep[0];	/* restore original r3 */
		memset(mce_data_buf, 0, RTAS_ERROR_LOG_MAX);
		memcpy(mce_data_buf, (char *)(savep + 1), RTAS_ERROR_LOG_MAX);
		errhdr = (struct rtas_error_log *)mce_data_buf;
	} else {
		printk("FWNMI: corrupt r3\n");
	}
	return errhdr;
}

/* Call this when done with the data returned by FWNMI_get_errinfo.
 * It will release the saved data area for other CPUs in the
 * partition to receive FWNMI errors.
 */
static void FWNMI_release_errinfo(void)
{
	unsigned long ret = rtas_call(rtas_token("ibm,nmi-interlock"), 0, 1, NULL);
	if (ret != 0)
		printk("FWNMI: nmi-interlock failed: %ld\n", ret);
}

void
SystemResetException(struct pt_regs *regs)
{
	char *msg = "System Reset in kernel mode.\n";
	printk(msg);
	if (fwnmi_active) {
		unsigned long *r3 = __va(regs->gpr[3]); /* for FWNMI debug */
		printk("FWNMI is active with save area at %p\n", r3);
		FWNMI_release_errinfo();
	}
#if defined(CONFIG_XMON)
	xmon(regs);
	if (smp_processor_id() == 0)
		udbg_printf("leaving xmon...\n");
#endif
#if defined(CONFIG_KDB)
	kdb_reset_debugger(regs);
#endif
}

/*
 * See if we can recover from a machine check exception.
 * This is only called on power4 (or above) and only via
 * the Firmware Non-Maskable Interrupts (fwnmi) handler
 * which provides the error analysis for us.
 *
 * Return 1 if corrected (or delivered a signal).
 * Return 0 if there is nothing we can do.
 */
static int recover_mce(struct pt_regs *regs, struct rtas_error_log *errp)
{
	siginfo_t info;
	int nonfatal = 0;


	if (errp->disposition == DISP_FULLY_RECOVERED) {
		/* Platform corrected itself */
		nonfatal = 1;
	} else if ((regs->msr & MSR_RI) &&
		   user_mode(regs) &&
		   errp->severity == SEVERITY_ERROR_SYNC &&
		   errp->disposition == DISP_NOT_RECOVERED &&
		   errp->target == TARGET_MEMORY &&
		   errp->type == TYPE_ECC_UNCORR &&
		   !(current->pid == 0 || current->pid == 1)) {

		/* Kill off a user process with an ECC error */
		printk(KERN_ERR "MCE: uncorrectable ecc error killed process %d (%s).\n", current->pid, current->comm);

		info.si_signo = SIGBUS;
		info.si_errno = 0;
		/* XXX better si_code for ECC error? */
		info.si_code = BUS_ADRERR;
		info.si_addr = (void *)regs->nip;
		_exception(SIGBUS, &info, regs);
		nonfatal = 1;
	}

	log_error((char *)errp, ERR_TYPE_RTAS_LOG, !nonfatal);

	return nonfatal;
}

/*
 * Handle a machine check.
 *
 * Note that on Power 4 and beyond Firmware Non-Maskable Interrupts (fwnmi)
 * should be present.  If so the handler which called us tells us if the
 * error was recovered (never true if RI=0).
 *
 * On hardware prior to Power 4 these exceptions were asynchronous which
 * means we can't tell exactly where it occurred and so we can't recover.
 *
 * Note that the debugger should test RI=0 and warn the user that system
 * state has been corrupted.
 */
void
MachineCheckException(struct pt_regs *regs)
{
	struct rtas_error_log *errp;

	if (fwnmi_active) {
		errp = FWNMI_get_errinfo(regs);
		FWNMI_release_errinfo();
		if (errp && recover_mce(regs, errp))
			return;
	} else if (check_exception_flag) {
		int status;
		unsigned long long srr1 = regs->msr;

		memset(mce_data_buf, 0, RTAS_ERROR_LOG_MAX);
		/* XXX
		 * We only pass the low 32 bits of SRR1, this could
		 * be changed to 7 input params and the high 32 bits
		 * of SRR1 could be passed as the extended info argument.
		 */
		status = rtas_call(rtas_token("check-exception"), 6, 1, NULL,
				   0x200, (uint)srr1, RTAS_INTERNAL_ERROR, 0,
				   __pa(mce_data_buf), RTAS_ERROR_LOG_MAX);
		if (status == 0)
			log_error((char *)mce_data_buf, ERR_TYPE_RTAS_LOG, 1);
	}

#if defined(CONFIG_XMON) || defined(CONFIG_KGDB)
	if (debugger_fault_handler) {
		debugger_fault_handler(regs);
		return;
	}
#endif
	printk(KERN_EMERG "Unrecoverable Machine check.\n");
	printk(KERN_EMERG "Caused by (from SRR1=%lx): ", regs->msr);
	show_regs(regs);
#if defined(CONFIG_XMON) || defined(CONFIG_KGDB) || defined(CONFIG_KDB)
	debugger(regs);
#endif
	print_backtrace((unsigned long *)regs->gpr[1]);
	panic("machine check");
}

void
SMIException(struct pt_regs *regs)
{
#if defined(CONFIG_XMON) || defined(CONFIG_KGDB) || defined(CONFIG_KDB)
	{
		debugger(regs);
		return;
	}
#endif
	show_regs(regs);
	print_backtrace((unsigned long *)regs->gpr[1]);
	panic("System Management Interrupt");
}

void
UnknownException(struct pt_regs *regs)
{
	siginfo_t info;

	printk("Bad trap at PC: %lx, SR: %lx, vector=%lx\n",
	       regs->nip, regs->msr, regs->trap);

	info.si_signo = SIGTRAP;
	info.si_errno = 0;
	info.si_code = 0;
	info.si_addr = 0;
	_exception(SIGTRAP, &info, regs);	
}

void
InstructionBreakpointException(struct pt_regs *regs)
{
	siginfo_t info;
	
#if defined(CONFIG_XMON) || defined(CONFIG_KGDB) || defined (CONFIG_KDB)
	if (debugger_iabr_match(regs))
		return;
#endif
	info.si_signo = SIGTRAP;
	info.si_errno = 0;
	info.si_code = TRAP_BRKPT;
	info.si_addr = (void *)regs->nip;
	_exception(SIGTRAP, &info, regs);
}

static void
parse_fpe(siginfo_t *info, struct pt_regs *regs)
{
	unsigned long fpscr;

	if (regs->msr & MSR_FP)
		giveup_fpu(current);

	fpscr = current->thread.fpscr;

	/* Invalid operation */
	if ((fpscr & FPSCR_VE) && (fpscr & FPSCR_VX))
		info->si_code = FPE_FLTINV;

	/* Overflow */
	else if ((fpscr & FPSCR_OE) && (fpscr & FPSCR_OX))
		info->si_code = FPE_FLTOVF;

	/* Underflow */
	else if ((fpscr & FPSCR_UE) && (fpscr & FPSCR_UX))
		info->si_code = FPE_FLTUND;

	/* Divide by zero */
	else if ((fpscr & FPSCR_ZE) && (fpscr & FPSCR_ZX))
		info->si_code = FPE_FLTDIV;

	/* Inexact result */
	else if ((fpscr & FPSCR_XE) && (fpscr & FPSCR_XX))
		info->si_code = FPE_FLTRES;

	else
		info->si_code = 0;

	info->si_signo = SIGFPE;
	info->si_errno = 0;
	info->si_addr = (void *)regs->nip;
	_exception(SIGFPE, info, regs);
}

#ifndef CONFIG_ALTIVEC
void IllegalAltiVecInstruction(struct pt_regs *regs)
{
	siginfo_t info;

	info.si_signo = SIGILL;
	info.si_errno = 0;
	info.si_code = ILL_ILLTRP;
	info.si_addr = (void *)regs->nip;
	_exception(SIGILL, &info, regs);
}
#endif

void
ProgramCheckException(struct pt_regs *regs)
{
	siginfo_t info;

	if (regs->msr & 0x100000) {
		/* IEEE FP exception */

		parse_fpe(&info, regs);
	} else if (regs->msr & 0x40000) {
		/* Privileged instruction */

		info.si_signo = SIGILL;
		info.si_errno = 0;
		info.si_code = ILL_PRVOPC;
		info.si_addr = (void *)regs->nip;
		_exception(SIGILL, &info, regs);
	} else if (regs->msr & 0x20000) {
		/* trap exception */

#if defined(CONFIG_XMON) || defined(CONFIG_KGDB) || defined(CONFIG_KDB)
		if (debugger_bpt(regs))
			return;
#endif
		info.si_signo = SIGTRAP;
		info.si_errno = 0;
		info.si_code = TRAP_BRKPT;
		info.si_addr = (void *)regs->nip;
		_exception(SIGTRAP, &info, regs);
	} else {
		/* Illegal instruction */

		info.si_signo = SIGILL;
		info.si_errno = 0;
		info.si_code = ILL_ILLTRP;
		info.si_addr = (void *)regs->nip;
		_exception(SIGILL, &info, regs);
	}
}

 void
KernelFPUnavailableException(struct pt_regs *regs)
{
	printk("Illegal floating point used in kernel (task=0x%016lx, pc=0x%016lx, trap=0x%08x)\n",
		current, regs->nip, regs->trap);
	panic("Unrecoverable FP Unavailable Exception in Kernel");
}


void
KernelAltiVecUnavailableException(struct pt_regs *regs)
{
	printk("Illegal Altivec used in kernel (task=0x%016lx, pc=0x%016lx, trap=0x%08x)\n",
		(unsigned long)current, regs->nip, (unsigned int)regs->trap);
	panic("Unrecoverable Altivec Unavailable Exception in Kernel");
}

void
AltiVecAssistException(struct pt_regs *regs)
{
#ifdef CONFIG_ALTIVEC
	printk("Altivec assist called by %s, switching java mode off\n",
		current->comm);
	/* We do this the "hard" way, but that's ok for now, maybe one
	 * day, we'll have a proper implementation...
	 */
	if (regs->msr & MSR_VEC)
		giveup_altivec(current);
	current->thread.vscr.u[3] |= 0x00010000;
#else
	siginfo_t info;

	printk("Altivec assist called by %s;, no altivec support !\n",
		current->comm);

	info.si_signo = SIGTRAP;
	info.si_errno = 0;
	info.si_code = 0;
	info.si_addr = 0;
	_exception(SIGTRAP, &info, regs);
#endif /* CONFIG_ALTIVEC */
}

void
ThermalInterrupt(struct pt_regs *regs)
{
	panic("Thermal interrupt exception not handled !");
}

void
SingleStepException(struct pt_regs *regs)
{
	siginfo_t info;

	regs->msr &= ~MSR_SE;  /* Turn off 'trace' bit */

#if defined(CONFIG_XMON) || defined(CONFIG_KGDB) || defined(CONFIG_KDB)
	if (debugger_sstep(regs))
		return;
#endif
	info.si_signo = SIGTRAP;
	info.si_errno = 0;
	info.si_code = TRAP_TRACE;
	info.si_addr = (void *)regs->nip;
	_exception(SIGTRAP, &info, regs);	
}

void
AlignmentException(struct pt_regs *regs)
{
	int fixed;
	siginfo_t info;

	fixed = fix_alignment(regs);
	if (fixed == 1) {
		ifppcdebug(PPCDBG_ALIGNFIXUP)
			if (!user_mode(regs))
				PPCDBG(PPCDBG_ALIGNFIXUP, "fix alignment at %lx\n", regs->nip);
		regs->nip += 4;	/* skip over emulated instruction */
		return;
	}
	
	/* Operand address was bad */
	if (fixed == -EFAULT) {
		if (user_mode(regs)) {
			info.si_signo = SIGSEGV;
			info.si_errno = 0;
			info.si_code = SEGV_MAPERR;
			info.si_addr = (void *)regs->dar;
			force_sig_info(SIGSEGV, &info, current);
		} else {
			/* Search exception table */
			bad_page_fault(regs, regs->dar);
		}

		return;
	}

	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRALN;
	info.si_addr = (void *)regs->nip;
	_exception(SIGBUS, &info, regs);
}

void __init trap_init(void)
{
}

/*
 * Set the DABR on all processors in the system.  The value is defined as:
 * DAB(0:60), Break Translate(61), Write(62), Read(63)
 */
void
set_all_DABR(unsigned long val) {
	set_local_DABR(&val); 
	smp_call_function(set_local_DABR, &val, 0, 0);
}

void
set_local_DABR(void *valp) {
	unsigned long val = *((unsigned long *)valp); 

	HvCall_setDABR(val);
}
