/*-
 * Copyright (C) 1994, David Greenman
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the University of Utah, and William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)trap.c	7.4 (Berkeley) 5/13/91
 *	$Id: trap.c,v 1.60 1995/10/04 07:07:44 julian Exp $
 */

/*
 * 386 Trap and System call handling
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/../isa/isa_device.h>

#ifdef POWERFAIL_NMI
# include <syslog.h>
# include <machine/clock.h>
#endif

#include "isa.h"
#include "npx.h"

extern void trap __P((struct trapframe frame));
extern int trapwrite __P((unsigned addr));
extern void syscall __P((struct trapframe frame));
extern void linux_syscall __P((struct trapframe frame));

int	trap_pfault	__P((struct trapframe *, int));
void	trap_fatal	__P((struct trapframe *));

extern inthand_t IDTVEC(syscall);

#define MAX_TRAP_MSG		27
char *trap_msg[] = {
	"",					/*  0 unused */
	"privileged instruction fault",		/*  1 T_PRIVINFLT */
	"",					/*  2 unused */
	"breakpoint instruction fault",		/*  3 T_BPTFLT */
	"",					/*  4 unused */
	"",					/*  5 unused */
	"arithmetic trap",			/*  6 T_ARITHTRAP */
	"system forced exception",		/*  7 T_ASTFLT */
	"",					/*  8 unused */
	"general protection fault",		/*  9 T_PROTFLT */
	"trace trap",				/* 10 T_TRCTRAP */
	"",					/* 11 unused */
	"page fault",				/* 12 T_PAGEFLT */
	"",					/* 13 unused */
	"alignment fault",			/* 14 T_ALIGNFLT */
	"",					/* 15 unused */
	"",					/* 16 unused */
	"",					/* 17 unused */
	"integer divide fault",			/* 18 T_DIVIDE */
	"non-maskable interrupt trap",		/* 19 T_NMI */
	"overflow trap",			/* 20 T_OFLOW */
	"FPU bounds check fault",		/* 21 T_BOUND */
	"FPU device not available",		/* 22 T_DNA */
	"double fault",				/* 23 T_DOUBLEFLT */
	"FPU operand fetch fault",		/* 24 T_FPOPFLT */
	"invalid TSS fault",			/* 25 T_TSSFLT */
	"segment not present fault",		/* 26 T_SEGNPFLT */
	"stack fault",				/* 27 T_STKFLT */
};

static void userret __P((struct proc *p, struct trapframe *frame,
			 u_quad_t oticks));

static inline void
userret(p, frame, oticks)
	struct proc *p;
	struct trapframe *frame;
	u_quad_t oticks;
{
	int sig, s;

	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	p->p_priority = p->p_usrpri;
	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrunqueue ourselves but before we
		 * mi_switch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		s = splclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		splx(s);
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}
	/*
	 * Charge system time if profiling.
	 */
	if (p->p_flag & P_PROFIL) {
		u_quad_t ticks = p->p_sticks - oticks;

		if (ticks) {
#ifdef PROFTIMER
			extern int profscale;
			addupc(frame->tf_eip, &p->p_stats->p_prof,
			    ticks * profscale);
#else
			addupc(frame->tf_eip, &p->p_stats->p_prof, ticks);
#endif
		}
	}
	curpriority = p->p_priority;
}

/*
 * Exception, fault, and trap interface to the FreeBSD kernel.
 * This common code is called from assembly language IDT gate entry
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed.
 */

void
trap(frame)
	struct trapframe frame;
{
	struct proc *p = curproc;
	u_quad_t sticks = 0;
	int i = 0, ucode = 0, type, code;
#ifdef DIAGNOSTIC
	u_long eva;
#endif

	type = frame.tf_trapno;
	code = frame.tf_err;

	if (ISPL(frame.tf_cs) == SEL_UPL) {
		/* user trap */

		sticks = p->p_sticks;
		p->p_md.md_regs = (int *)&frame;

		switch (type) {
		case T_PRIVINFLT:	/* privileged instruction fault */
			ucode = type;
			i = SIGILL;
			break;

		case T_BPTFLT:		/* bpt instruction fault */
		case T_TRCTRAP:		/* trace trap */
			frame.tf_eflags &= ~PSL_T;
			i = SIGTRAP;
			break;

		case T_ARITHTRAP:	/* arithmetic trap */
			ucode = code;
			i = SIGFPE;
			break;

		case T_ASTFLT:		/* Allow process switch */
			astoff();
			cnt.v_soft++;
			if (p->p_flag & P_OWEUPC) {
				addupc(frame.tf_eip, &p->p_stats->p_prof, 1);
				p->p_flag &= ~P_OWEUPC;
			}
			goto out;

		case T_PROTFLT:		/* general protection fault */
		case T_SEGNPFLT:	/* segment not present fault */
		case T_STKFLT:		/* stack fault */
		case T_TSSFLT:		/* invalid TSS fault */
		case T_DOUBLEFLT:	/* double fault */
		default:
			ucode = code + BUS_SEGM_FAULT ;
			i = SIGBUS;
			break;

		case T_PAGEFLT:		/* page fault */
			i = trap_pfault(&frame, TRUE);
			if (i == -1)
				return;
			if (i == 0)
				goto out;

			ucode = T_PAGEFLT;
			break;

		case T_DIVIDE:		/* integer divide fault */
			ucode = FPE_INTDIV_TRAP;
			i = SIGFPE;
			break;

#if NISA > 0
		case T_NMI:
#ifdef POWERFAIL_NMI
			goto handle_powerfail;
#else /* !POWERFAIL_NMI */
#ifdef DDB
			/* NMI can be hooked up to a pushbutton for debugging */
			printf ("NMI ... going to debugger\n");
			if (kdb_trap (type, 0, &frame))
				return;
#endif /* DDB */
			/* machine/parity/power fail/"kitchen sink" faults */
			if (isa_nmi(code) == 0) return;
			panic("NMI indicates hardware failure");
#endif /* POWERFAIL_NMI */
#endif /* NISA > 0 */

		case T_OFLOW:		/* integer overflow fault */
			ucode = FPE_INTOVF_TRAP;
			i = SIGFPE;
			break;

		case T_BOUND:		/* bounds check fault */
			ucode = FPE_SUBRNG_TRAP;
			i = SIGFPE;
			break;

		case T_DNA:
#if NNPX > 0
			/* if a transparent fault (due to context switch "late") */
			if (npxdna())
				return;
#endif	/* NNPX > 0 */

#if defined(MATH_EMULATE) || defined(GPL_MATH_EMULATE)
			i = math_emulate(&frame);
			if (i == 0) {
				if (!(frame.tf_eflags & PSL_T))
					return;
				frame.tf_eflags &= ~PSL_T;
				i = SIGTRAP;
			}
			/* else ucode = emulator_only_knows() XXX */
#else	/* MATH_EMULATE || GPL_MATH_EMULATE */
			i = SIGFPE;
			ucode = FPE_FPU_NP_TRAP;
#endif	/* MATH_EMULATE || GPL_MATH_EMULATE */
			break;

		case T_FPOPFLT:		/* FPU operand fetch fault */
			ucode = T_FPOPFLT;
			i = SIGILL;
			break;
		}
	} else {
		/* kernel trap */

		switch (type) {
		case T_PAGEFLT:			/* page fault */
			(void) trap_pfault(&frame, FALSE);
			return;

		case T_PROTFLT:		/* general protection fault */
		case T_SEGNPFLT:	/* segment not present fault */
			/*
			 * Invalid segment selectors and out of bounds
			 * %eip's and %esp's can be set up in user mode.
			 * This causes a fault in kernel mode when the
			 * kernel tries to return to user mode.  We want
			 * to get this fault so that we can fix the
			 * problem here and not have to check all the
			 * selectors and pointers when the user changes
			 * them.
			 */
#define	MAYBE_DORETI_FAULT(where, whereto)				\
	do {								\
		if (frame.tf_eip == (int)where) {			\
			frame.tf_eip = (int)whereto;			\
			return;						\
		}							\
	} while (0)

			if (intr_nesting_level == 0) {
				MAYBE_DORETI_FAULT(doreti_iret,
						   doreti_iret_fault);
				MAYBE_DORETI_FAULT(doreti_popl_ds,
						   doreti_popl_ds_fault);
				MAYBE_DORETI_FAULT(doreti_popl_es,
						   doreti_popl_es_fault);
			}
			if (curpcb && curpcb->pcb_onfault) {
				frame.tf_eip = (int)curpcb->pcb_onfault;
				return;
			}
			break;

		case T_TSSFLT:
			/*
			 * PSL_NT can be set in user mode and isn't cleared
			 * automatically when the kernel is entered.  This
			 * causes a TSS fault when the kernel attempts to
			 * `iret' because the TSS link is uninitialized.  We
			 * want to get this fault so that we can fix the
			 * problem here and not every time the kernel is
			 * entered.
			 */
			if (frame.tf_eflags & PSL_NT) {
				frame.tf_eflags &= ~PSL_NT;
				return;
			}
			break;

		case T_TRCTRAP:	 /* trace trap */
			if (frame.tf_eip == (int)IDTVEC(syscall)) {
				/*
				 * We've just entered system mode via the
				 * syscall lcall.  Continue single stepping
				 * silently until the syscall handler has
				 * saved the flags.
				 */
				return;
			}
			if (frame.tf_eip == (int)IDTVEC(syscall) + 1) {
				/*
				 * The syscall handler has now saved the
				 * flags.  Stop single stepping it.
				 */
				frame.tf_eflags &= ~PSL_T;
				return;
			}
			/*
			 * Fall through.
			 */
		case T_BPTFLT:
			/*
			 * If DDB is enabled, let it handle the debugger trap.
			 * Otherwise, debugger traps "can't happen".
			 */
#ifdef DDB
			if (kdb_trap (type, 0, &frame))
				return;
#endif
			break;

#if NISA > 0
		case T_NMI:
#ifdef POWERFAIL_NMI
#ifndef TIMER_FREQ
#  define TIMER_FREQ 1193182
#endif
	handle_powerfail:
		{
		  static unsigned lastalert = 0;

		  if(time.tv_sec - lastalert > 10)
		    {
		      log(LOG_WARNING, "NMI: power fail\n");
		      sysbeep(TIMER_FREQ/880, hz);
		      lastalert = time.tv_sec;
		    }
		  return;
		}
#else /* !POWERFAIL_NMI */
#ifdef DDB
			/* NMI can be hooked up to a pushbutton for debugging */
			printf ("NMI ... going to debugger\n");
			if (kdb_trap (type, 0, &frame))
				return;
#endif /* DDB */
			/* machine/parity/power fail/"kitchen sink" faults */
			if (isa_nmi(code) == 0) return;
			/* FALL THROUGH */
#endif /* POWERFAIL_NMI */
#endif /* NISA > 0 */
		}

		trap_fatal(&frame);
		return;
	}

	trapsignal(p, i, ucode);

#ifdef DEBUG
	eva = rcr2();
	if (type <= MAX_TRAP_MSG) {
		uprintf("fatal process exception: %s",
			trap_msg[type]);
		if ((type == T_PAGEFLT) || (type == T_PROTFLT))
			uprintf(", fault VA = 0x%x", eva);
		uprintf("\n");
	}
#endif

out:
	userret(p, &frame, sticks);
}

#ifdef notyet
/*
 * This version doesn't allow a page fault to user space while
 * in the kernel. The rest of the kernel needs to be made "safe"
 * before this can be used. I think the only things remaining
 * to be made safe are the iBCS2 code and the process tracing/
 * debugging code.
 */
int
trap_pfault(frame, usermode)
	struct trapframe *frame;
	int usermode;
{
	vm_offset_t va;
	struct vmspace *vm = NULL;
	vm_map_t map = 0;
	int rv = 0;
	vm_prot_t ftype;
	int eva;
	struct proc *p = curproc;

	if (frame->tf_err & PGEX_W)
		ftype = VM_PROT_READ | VM_PROT_WRITE;
	else
		ftype = VM_PROT_READ;

	eva = rcr2();
	va = trunc_page((vm_offset_t)eva);

	if (va < VM_MIN_KERNEL_ADDRESS) {
		vm_offset_t v;
		vm_page_t ptepg;

		if (p == NULL ||
		    (!usermode && va < VM_MAXUSER_ADDRESS &&
		    (curpcb == NULL || curpcb->pcb_onfault == NULL))) {
			trap_fatal(frame);
			return (-1);
		}

		/*
		 * This is a fault on non-kernel virtual memory.
		 * vm is initialized above to NULL. If curproc is NULL
		 * or curproc->p_vmspace is NULL the fault is fatal.
		 */
		vm = p->p_vmspace;
		if (vm == NULL)
			goto nogo;

		map = &vm->vm_map;

		/*
		 * Keep swapout from messing with us during this
		 *	critical time.
		 */
		++p->p_lock;

		/*
		 * Grow the stack if necessary
		 */
		if ((caddr_t)va > vm->vm_maxsaddr
		    && (caddr_t)va < (caddr_t)USRSTACK) {
			if (!grow(p, va)) {
				rv = KERN_FAILURE;
				--p->p_lock;
				goto nogo;
			}
		}

		/*
		 * Check if page table is mapped, if not,
		 *	fault it first
		 */
		v = (vm_offset_t) vtopte(va);

		/* Fault the pte only if needed: */
		if (*((int *)vtopte(v)) == 0)
			(void) vm_fault(map, trunc_page(v), VM_PROT_WRITE, FALSE);

		pmap_use_pt( vm_map_pmap(map), va);

		/* Fault in the user page: */
		rv = vm_fault(map, va, ftype, FALSE);

		pmap_unuse_pt( vm_map_pmap(map), va);

		--p->p_lock;
	} else {
		/*
		 * Don't allow user-mode faults in kernel address space.
		 */
		if (usermode)
			goto nogo;

		/*
		 * Since we know that kernel virtual address addresses
		 * always have pte pages mapped, we just have to fault
		 * the page.
		 */
		rv = vm_fault(kernel_map, va, ftype, FALSE);
	}

	if (rv == KERN_SUCCESS)
		return (0);
nogo:
	if (!usermode) {
		if (curpcb && curpcb->pcb_onfault) {
			frame->tf_eip = (int)curpcb->pcb_onfault;
			return (0);
		}
		trap_fatal(frame);
		return (-1);
	}

	/* kludge to pass faulting virtual address to sendsig */
	frame->tf_err = eva;

	return((rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV);
}
#endif

int
trap_pfault(frame, usermode)
	struct trapframe *frame;
	int usermode;
{
	vm_offset_t va;
	struct vmspace *vm = NULL;
	vm_map_t map = 0;
	int rv = 0;
	vm_prot_t ftype;
	int eva;
	struct proc *p = curproc;

	eva = rcr2();
	va = trunc_page((vm_offset_t)eva);

	if (va >= KERNBASE) {
		/*
		 * Don't allow user-mode faults in kernel address space.
		 */
		if (usermode)
			goto nogo;

		map = kernel_map;
	} else {
		/*
		 * This is a fault on non-kernel virtual memory.
		 * vm is initialized above to NULL. If curproc is NULL
		 * or curproc->p_vmspace is NULL the fault is fatal.
		 */
		if (p != NULL)
			vm = p->p_vmspace;

		if (vm == NULL)
			goto nogo;

		map = &vm->vm_map;
	}

	if (frame->tf_err & PGEX_W)
		ftype = VM_PROT_READ | VM_PROT_WRITE;
	else
		ftype = VM_PROT_READ;

	if (map != kernel_map) {
		vm_offset_t v;
		vm_page_t ptepg;

		/*
		 * Keep swapout from messing with us during this
		 *	critical time.
		 */
		++p->p_lock;

		/*
		 * Grow the stack if necessary
		 */
		if ((caddr_t)va > vm->vm_maxsaddr
		    && (caddr_t)va < (caddr_t)USRSTACK) {
			if (!grow(p, va)) {
				rv = KERN_FAILURE;
				--p->p_lock;
				goto nogo;
			}
		}

		/*
		 * Check if page table is mapped, if not,
		 *	fault it first
		 */
		v = (vm_offset_t) vtopte(va);

		/* Fault the pte only if needed: */
		if (*((int *)vtopte(v)) == 0)
			(void) vm_fault(map, trunc_page(v), VM_PROT_WRITE, FALSE);

		pmap_use_pt( vm_map_pmap(map), va);

		/* Fault in the user page: */
		rv = vm_fault(map, va, ftype, FALSE);

		pmap_unuse_pt( vm_map_pmap(map), va);

		--p->p_lock;
	} else {
		/*
		 * Since we know that kernel virtual address addresses
		 * always have pte pages mapped, we just have to fault
		 * the page.
		 */
		rv = vm_fault(map, va, ftype, FALSE);
	}

	if (rv == KERN_SUCCESS)
		return (0);
nogo:
	if (!usermode) {
		if (curpcb && curpcb->pcb_onfault) {
			frame->tf_eip = (int)curpcb->pcb_onfault;
			return (0);
		}
		trap_fatal(frame);
		return (-1);
	}

	/* kludge to pass faulting virtual address to sendsig */
	frame->tf_err = eva;

	return((rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV);
}

void
trap_fatal(frame)
	struct trapframe *frame;
{
	int code, type, eva;
	struct soft_segment_descriptor softseg;

	code = frame->tf_err;
	type = frame->tf_trapno;
	eva = rcr2();
	sdtossd(&gdt[IDXSEL(frame->tf_cs & 0xffff)].sd, &softseg);

	if (type <= MAX_TRAP_MSG)
		printf("\n\nFatal trap %d: %s while in %s mode\n",
			type, trap_msg[type],
			ISPL(frame->tf_cs) == SEL_UPL ? "user" : "kernel");
	if (type == T_PAGEFLT) {
		printf("fault virtual address	= 0x%x\n", eva);
		printf("fault code		= %s %s, %s\n",
			code & PGEX_U ? "user" : "supervisor",
			code & PGEX_W ? "write" : "read",
			code & PGEX_P ? "protection violation" : "page not present");
	}
	printf("instruction pointer	= 0x%x:0x%x\n", frame->tf_cs & 0xffff, frame->tf_eip);
	printf("code segment		= base 0x%x, limit 0x%x, type 0x%x\n",
	    softseg.ssd_base, softseg.ssd_limit, softseg.ssd_type);
	printf("			= DPL %d, pres %d, def32 %d, gran %d\n",
	    softseg.ssd_dpl, softseg.ssd_p, softseg.ssd_def32, softseg.ssd_gran);
	printf("processor eflags	= ");
	if (frame->tf_eflags & PSL_T)
		printf("trace/trap, ");
	if (frame->tf_eflags & PSL_I)
		printf("interrupt enabled, ");
	if (frame->tf_eflags & PSL_NT)
		printf("nested task, ");
	if (frame->tf_eflags & PSL_RF)
		printf("resume, ");
	if (frame->tf_eflags & PSL_VM)
		printf("vm86, ");
	printf("IOPL = %d\n", (frame->tf_eflags & PSL_IOPL) >> 12);
	printf("current process		= ");
	if (curproc) {
		printf("%lu (%s)\n",
		    (u_long)curproc->p_pid, curproc->p_comm ?
		    curproc->p_comm : "");
	} else {
		printf("Idle\n");
	}
	printf("interrupt mask		= ");
	if ((cpl & net_imask) == net_imask)
		printf("net ");
	if ((cpl & tty_imask) == tty_imask)
		printf("tty ");
	if ((cpl & bio_imask) == bio_imask)
		printf("bio ");
	if (cpl == 0)
		printf("none");
	printf("\n");

#ifdef KDB
	if (kdb_trap(&psl))
		return;
#endif
#ifdef DDB
	if (kdb_trap (type, 0, frame))
		return;
#endif
	if (type <= MAX_TRAP_MSG)
		panic(trap_msg[type]);
	else
		panic("unknown/reserved trap");
}

/*
 * Compensate for 386 brain damage (missing URKR).
 * This is a little simpler than the pagefault handler in trap() because
 * it the page tables have already been faulted in and high addresses
 * are thrown out early for other reasons.
 */
int trapwrite(addr)
	unsigned addr;
{
	struct proc *p;
	vm_offset_t va, v;
	struct vmspace *vm;
	int rv;

	va = trunc_page((vm_offset_t)addr);
	/*
	 * XXX - MAX is END.  Changed > to >= for temp. fix.
	 */
	if (va >= VM_MAXUSER_ADDRESS)
		return (1);

	p = curproc;
	vm = p->p_vmspace;

	++p->p_lock;

	if ((caddr_t)va >= vm->vm_maxsaddr
	    && (caddr_t)va < (caddr_t)USRSTACK) {
		if (!grow(p, va)) {
			--p->p_lock;
			return (1);
		}
	}

	v = trunc_page(vtopte(va));

	/*
	 * wire the pte page
	 */
	if (va < USRSTACK) {
		vm_map_pageable(&vm->vm_map, v, round_page(v+1), FALSE);
	}

	/*
	 * fault the data page
	 */
	rv = vm_fault(&vm->vm_map, va, VM_PROT_READ|VM_PROT_WRITE, FALSE);

	/*
	 * unwire the pte page
	 */
	if (va < USRSTACK) {
		vm_map_pageable(&vm->vm_map, v, round_page(v+1), TRUE);
	}

	--p->p_lock;

	if (rv != KERN_SUCCESS)
		return 1;

	return (0);
}

/*
 * System call request from POSIX system call gate interface to kernel.
 * Like trap(), argument is call by reference.
 */
void
syscall(frame)
	struct trapframe frame;
{
	caddr_t params;
	int i;
	struct sysent *callp;
	struct proc *p = curproc;
	u_quad_t sticks;
	int error;
	int args[8], rval[2];
	u_int code;

	sticks = p->p_sticks;
	if (ISPL(frame.tf_cs) != SEL_UPL)
		panic("syscall");

	p->p_md.md_regs = (int *)&frame;
	params = (caddr_t)frame.tf_esp + sizeof(int);
	code = frame.tf_eax;
	/*
	 * Need to check if this is a 32 bit or 64 bit syscall.
	 */
	if (code == SYS_syscall) {
		/*
		 * Code is first argument, followed by actual args.
		 */
		code = fuword(params);
		params += sizeof(int);
	} else if (code == SYS___syscall) {
		/*
		 * Like syscall, but code is a quad, so as to maintain
		 * quad alignment for the rest of the arguments.
		 */
		code = fuword(params);
		params += sizeof(quad_t);
	}

 	if (p->p_sysent->sv_mask)
 		code &= p->p_sysent->sv_mask;

 	if (code >= p->p_sysent->sv_size)
 		callp = &p->p_sysent->sv_table[0];
  	else
 		callp = &p->p_sysent->sv_table[code];

	if ((i = callp->sy_narg * sizeof(int)) &&
	    (error = copyin(params, (caddr_t)args, (u_int)i))) {
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSCALL))
			ktrsyscall(p->p_tracep, code, callp->sy_narg, args);
#endif
		goto bad;
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, callp->sy_narg, args);
#endif
	rval[0] = 0;
	rval[1] = frame.tf_edx;

	error = (*callp->sy_call)(p, args, rval);

	switch (error) {

	case 0:
		/*
		 * Reinitialize proc pointer `p' as it may be different
		 * if this is a child returning from fork syscall.
		 */
		p = curproc;
		frame.tf_eax = rval[0];
		frame.tf_edx = rval[1];
		frame.tf_eflags &= ~PSL_C;
		break;

	case ERESTART:
		/*
		 * Reconstruct pc, assuming lcall $X,y is 7 bytes.
		 */
		frame.tf_eip -= 7;
		break;

	case EJUSTRETURN:
		break;

	default:
bad:
 		if (p->p_sysent->sv_errsize)
 			if (error >= p->p_sysent->sv_errsize)
  				error = -1;	/* XXX */
   			else
  				error = p->p_sysent->sv_errtbl[error];
		frame.tf_eax = error;
		frame.tf_eflags |= PSL_C;
		break;
	}

	if (frame.tf_eflags & PSL_T) {
		/* Traced syscall. */
		frame.tf_eflags &= ~PSL_T;
		trapsignal(p, SIGTRAP, 0);
	}

	userret(p, &frame, sticks);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}

#ifdef COMPAT_LINUX
void
linux_syscall(frame)
	struct trapframe frame;
{
	int i;
	struct proc *p = curproc;
	struct sysent *callp;
	u_quad_t sticks;
	int error;
	int rval[2];
	u_int code;
	struct linux_syscall_args {
		int arg1;
		int arg2;
		int arg3;
		int arg4;
		int arg5;
	} args;

	args.arg1 = frame.tf_ebx;
	args.arg2 = frame.tf_ecx;
	args.arg3 = frame.tf_edx;
	args.arg4 = frame.tf_esi;
	args.arg5 = frame.tf_edi;

	sticks = p->p_sticks;
	if (ISPL(frame.tf_cs) != SEL_UPL)
		panic("linux syscall");

	p->p_md.md_regs = (int *)&frame;
	code = frame.tf_eax;

	if (p->p_sysent->sv_mask)
		code &= p->p_sysent->sv_mask;

	if (code >= p->p_sysent->sv_size)
		callp = &p->p_sysent->sv_table[0];
	else
		callp = &p->p_sysent->sv_table[code];

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, callp->sy_narg, (int *)&args);
#endif

	rval[0] = 0;

	error = (*callp->sy_call)(p, &args, rval);

	switch (error) {

	case 0:
		/*
		 * Reinitialize proc pointer `p' as it may be different
		 * if this is a child returning from fork syscall.
		 */
		p = curproc;
		frame.tf_eax = rval[0];
		frame.tf_eflags &= ~PSL_C;
		break;

	case ERESTART:
		/* Reconstruct pc, subtract size of int 0x80 */
		frame.tf_eip -= 2;
		break;

	case EJUSTRETURN:
		break;

	default:
 		if (p->p_sysent->sv_errsize)
 			if (error >= p->p_sysent->sv_errsize)
  				error = -1;	/* XXX */
   			else
  				error = p->p_sysent->sv_errtbl[error];
		frame.tf_eax = -error;
		frame.tf_eflags |= PSL_C;
		break;
	}

	if (frame.tf_eflags & PSL_T) {
		/* Traced syscall. */
		frame.tf_eflags &= ~PSL_T;
		trapsignal(p, SIGTRAP, 0);
	}

	userret(p, &frame, sticks);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}
#endif /* COMPAT_LINUX */
