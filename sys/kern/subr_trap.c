/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 *	$Id: trap.c,v 1.18 1994/03/07 11:38:35 davidg Exp $
 */

/*
 * 386 Trap and System call handleing
 */

#include "isa.h"
#include "npx.h"
#include "ddb.h"
#include "machine/cpu.h"
#include "machine/psl.h"
#include "machine/reg.h"
#include "machine/eflags.h"

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "user.h"
#include "acct.h"
#include "kernel.h"
#ifdef KTRACE
#include "ktrace.h"
#endif

#include "vm/vm_param.h"
#include "vm/pmap.h"
#include "vm/vm_map.h"
#include "vm/vm_user.h"
#include "vm/vm_page.h"
#include "sys/vmmeter.h"

#include "machine/trap.h"

#ifdef	__GNUC__

/*
 * The "r" contraint could be "rm" except for fatal bugs in gas.  As usual,
 * we omit the size from the mov instruction to avoid nonfatal bugs in gas.
 */
#define	read_gs()	({ u_short gs; __asm("mov %%gs,%0" : "=r" (gs)); gs; })
#define	write_gs(newgs)	__asm("mov %0,%%gs" : : "r" ((u_short) newgs))

#else	/* not __GNUC__ */

u_short	read_gs		__P((void));
void	write_gs	__P((/* promoted u_short */ int gs));

#endif	/* __GNUC__ */

extern int grow(struct proc *,int);

struct	sysent sysent[];
int	nsysent;
extern short cpl;
extern short netmask, ttymask, biomask;

#define MAX_TRAP_MSG		27
char *trap_msg[] = {
	"reserved addressing fault",		/*  0 T_RESADFLT */
	"privileged instruction fault",		/*  1 T_PRIVINFLT */
	"reserved operand fault",		/*  2 T_RESOPFLT */
	"breakpoint instruction fault",		/*  3 T_BPTFLT */
	"",					/*  4 unused */
	"system call trap",			/*  5 T_SYSCALL */
	"arithmetic trap",			/*  6 T_ARITHTRAP */
	"system forced exception",		/*  7 T_ASTFLT */
	"segmentation (limit) fault",		/*  8 T_SEGFLT */
	"protection fault",			/*  9 T_PROTFLT */
	"trace trap",				/* 10 T_TRCTRAP */
	"",					/* 11 unused */
	"page fault",				/* 12 T_PAGEFLT */
	"page table fault",			/* 13 T_TABLEFLT */
	"alignment fault",			/* 14 T_ALIGNFLT */
	"kernel stack pointer not valid",	/* 15 T_KSPNOTVAL */
	"bus error",				/* 16 T_BUSERR */
	"kernel debugger fault",		/* 17 T_KDBTRAP */
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

#define pde_v(v) (PTD[((v)>>PD_SHIFT)&1023].pd_v)

/*
 * trap(frame):
 *	Exception, fault, and trap interface to BSD kernel. This
 * common code is called from assembly language IDT gate entry
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed. Note that the
 * effect is as if the arguments were passed call by reference.
 */

/*ARGSUSED*/
void
trap(frame)
	struct trapframe frame;
{
	register int i;
	register struct proc *p = curproc;
	struct timeval syst;
	int ucode, type, code, eva, fault_type;

	frame.tf_eflags &= ~PSL_NT;	/* clear nested trap XXX */
	type = frame.tf_trapno;
#if NDDB > 0
	if (curpcb && curpcb->pcb_onfault) {
		if (frame.tf_trapno == T_BPTFLT
		    || frame.tf_trapno == T_TRCTRAP)
			if (kdb_trap (type, 0, &frame))
				return;
	}
#endif
	
	if (curpcb == 0 || curproc == 0)
		goto skiptoswitch;
	if (curpcb->pcb_onfault && frame.tf_trapno != T_PAGEFLT) {
		extern int _udatasel;

		if (read_gs() != (u_short) _udatasel)
			/*
			 * Some user has corrupted %gs but we depend on it in
			 * copyout() etc.  Fix it up and retry.
			 *
			 * (We don't preserve %fs or %gs, so users can change
			 * them to either _ucodesel, _udatasel or a not-present
			 * selector, possibly ORed with 0 to 3, making them
			 * volatile for other users.  Not preserving them saves
			 * time and doesn't lose functionality or open security
			 * holes.)
			 */
			write_gs(_udatasel);
		else
copyfault:
			frame.tf_eip = (int)curpcb->pcb_onfault;
		return;
	}

	syst = p->p_stime;
	if (ISPL(frame.tf_cs) == SEL_UPL) {
		type |= T_USER;
		p->p_regs = (int *)&frame;
	}

skiptoswitch:
	ucode=0;
	eva = rcr2();
	code = frame.tf_err;

	if ((type & ~T_USER) == T_PAGEFLT)
		goto pfault;

	switch (type) {
	case T_SEGNPFLT|T_USER:
	case T_STKFLT|T_USER:
	case T_PROTFLT|T_USER:		/* protection fault */
		ucode = code + BUS_SEGM_FAULT ;
		i = SIGBUS;
		break;

	case T_PRIVINFLT|T_USER:	/* privileged instruction fault */
	case T_RESADFLT|T_USER:		/* reserved addressing fault */
	case T_RESOPFLT|T_USER:		/* reserved operand fault */
	case T_FPOPFLT|T_USER:		/* coprocessor operand fault */
		ucode = type &~ T_USER;
		i = SIGILL;
		break;

	case T_ASTFLT|T_USER:		/* Allow process switch */
		astoff();
		cnt.v_soft++;
		if ((p->p_flag & SOWEUPC) && p->p_stats->p_prof.pr_scale) {
			addupc(frame.tf_eip, &p->p_stats->p_prof, 1);
			p->p_flag &= ~SOWEUPC;
		}
		goto out;

	case T_DNA|T_USER:
#if NNPX > 0
		/* if a transparent fault (due to context switch "late") */
		if (npxdna()) return;
#endif	/* NNPX > 0 */
#ifdef	MATH_EMULATE
		i = math_emulate(&frame);
		if (i == 0) return;
#else	/* MATH_EMULTATE */
		panic("trap: math emulation necessary!");
#endif	/* MATH_EMULTATE */
		ucode = FPE_FPU_NP_TRAP;
		break;

	case T_BOUND|T_USER:
		ucode = FPE_SUBRNG_TRAP;
		i = SIGFPE;
		break;

	case T_OFLOW|T_USER:
		ucode = FPE_INTOVF_TRAP;
		i = SIGFPE;
		break;

	case T_DIVIDE|T_USER:
		ucode = FPE_INTDIV_TRAP;
		i = SIGFPE;
		break;

	case T_ARITHTRAP|T_USER:
		ucode = code;
		i = SIGFPE;
		break;

	pfault:
	case T_PAGEFLT:			/* allow page faults in kernel mode */
	case T_PAGEFLT|T_USER:		/* page fault */
	    {
		vm_offset_t va;
		struct vmspace *vm;
		vm_map_t map = 0;
		int rv = 0, oldflags;
		vm_prot_t ftype;
		unsigned nss, v;
		extern vm_map_t kernel_map;

		va = trunc_page((vm_offset_t)eva);

		/*
		 * Don't allow user-mode faults in kernel address space
		 */
		if ((type == (T_PAGEFLT|T_USER)) && (va >= KERNBASE)) {
			goto nogo;
		}

		if ((p == 0) || (type == T_PAGEFLT && va >= KERNBASE)) {
			vm = 0;
			map = kernel_map;
		} else {
			vm = p->p_vmspace;
			map = &vm->vm_map;
		}

		if (code & PGEX_W)
			ftype = VM_PROT_READ | VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;

		oldflags = p->p_flag;
		if (map != kernel_map) {
			vm_offset_t pa;
			vm_offset_t v = (vm_offset_t) vtopte(va);

			/*
			 * Keep swapout from messing with us during this
			 *	critical time.
			 */
			p->p_flag |= SLOCK;

			/*
			 * Grow the stack if necessary
			 */
			if ((caddr_t)va > vm->vm_maxsaddr
			    && (caddr_t)va < (caddr_t)USRSTACK) {
				if (!grow(p, va)) {
					rv = KERN_FAILURE;
					p->p_flag &= ~SLOCK;
					p->p_flag |= (oldflags & SLOCK);
					goto nogo;
				}
			}

			/*
			 * Check if page table is mapped, if not,
			 *	fault it first
			 */

			/* Fault the pte only if needed: */
			*(volatile char *)v += 0;	

			vm_page_hold(pmap_pte_vm_page(vm_map_pmap(map),v));

			/* Fault in the user page: */
			rv = vm_fault(map, va, ftype, FALSE);

			vm_page_unhold(pmap_pte_vm_page(vm_map_pmap(map),v));

			p->p_flag &= ~SLOCK;
			p->p_flag |= (oldflags & SLOCK);
		} else {
			/*
			 * Since we know that kernel virtual address addresses
			 * always have pte pages mapped, we just have to fault
			 * the page.
			 */
			rv = vm_fault(map, va, ftype, FALSE);
		}

		if (rv == KERN_SUCCESS) {
			if (type == T_PAGEFLT)
				return;
			goto out;
		}
nogo:
		if (type == T_PAGEFLT) {
			if (curpcb->pcb_onfault)
				goto copyfault;

			goto we_re_toast;
		}
		i = (rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV;

		/* kludge to pass faulting virtual address to sendsig */
		ucode = type &~ T_USER;
		frame.tf_err = eva;

		break;
	    }

#if NDDB == 0
	case T_TRCTRAP:	 /* trace trap -- someone single stepping lcall's */
		frame.tf_eflags &= ~PSL_T;

			/* Q: how do we turn it on again? */
		return;
#endif
	
	case T_BPTFLT|T_USER:		/* bpt instruction fault */
	case T_TRCTRAP|T_USER:		/* trace trap */
		frame.tf_eflags &= ~PSL_T;
		i = SIGTRAP;
		break;

#if NISA > 0
	case T_NMI:
	case T_NMI|T_USER:
#if NDDB > 0
		/* NMI can be hooked up to a pushbutton for debugging */
		printf ("NMI ... going to debugger\n");
		if (kdb_trap (type, 0, &frame))
			return;
#endif
		/* machine/parity/power fail/"kitchen sink" faults */
		if (isa_nmi(code) == 0) return;
		/* FALL THROUGH */
#endif
	default:
	we_re_toast:

		fault_type = type & ~T_USER;
		if (fault_type <= MAX_TRAP_MSG)
			printf("\n\nFatal trap %d: %s while in %s mode\n",
				fault_type, trap_msg[fault_type],
				ISPL(frame.tf_cs) == SEL_UPL ? "user" : "kernel");
		if (fault_type == T_PAGEFLT) {
			printf("fault virtual address	= 0x%x\n", eva);
			printf("fault code		= %s %s, %s\n",
				code & PGEX_U ? "user" : "supervisor",
				code & PGEX_W ? "write" : "read",
				code & PGEX_P ? "protection violation" : "page not present");
		}
		printf("instruction pointer	= 0x%x\n", frame.tf_eip);
		printf("processor eflags	= ");
		if (frame.tf_eflags & EFL_TF)
			printf("trace/trap, ");
		if (frame.tf_eflags & EFL_IF)
			printf("interrupt enabled, ");
		if (frame.tf_eflags & EFL_NT)
			printf("nested task, ");
		if (frame.tf_eflags & EFL_RF)
			printf("resume, ");
		if (frame.tf_eflags & EFL_VM)
			printf("vm86, ");
		printf("IOPL = %d\n", (frame.tf_eflags & EFL_IOPL) >> 12);
		printf("current process		= ");
		if (curproc) {
			printf("%d (%s)\n",
			    curproc->p_pid, curproc->p_comm ?
			    curproc->p_comm : "");
		} else {
			printf("Idle\n");
		}
		printf("interrupt mask		= ");
		if ((cpl & netmask) == netmask)
			printf("net ");
		if ((cpl & ttymask) == ttymask)
			printf("tty ");
		if ((cpl & biomask) == biomask)
			printf("bio ");
		if (cpl == 0)
			printf("none");
		printf("\n");

#ifdef KDB
		if (kdb_trap(&psl))
			return;
#endif
#if NDDB > 0
		if (kdb_trap (type, 0, &frame))
			return;
#endif
		if (fault_type <= MAX_TRAP_MSG)
			panic(trap_msg[fault_type]);
		else
			panic("unknown/reserved trap");

		/* NOTREACHED */
	}

	trapsignal(p, i, ucode);
	if ((type & T_USER) == 0)
		return;
out:
	while (i = CURSIG(p))
		psig(i);
	p->p_pri = p->p_usrpri;
	if (want_resched) {
		int s;
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrq ourselves but before we
		 * swtch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		s = splclock();
		setrq(p);
		p->p_stats->p_ru.ru_nivcsw++;
		swtch();
		splx(s);
		while (i = CURSIG(p))
			psig(i);
	}
	if (p->p_stats->p_prof.pr_scale) {
		int ticks;
		struct timeval *tv = &p->p_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks) {
#ifdef PROFTIMER
			extern int profscale;
			addupc(frame.tf_eip, &p->p_stats->p_prof,
			    ticks * profscale);
#else
			addupc(frame.tf_eip, &p->p_stats->p_prof, ticks);
#endif
		}
	}
	curpri = p->p_pri;
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
	unsigned nss;
	struct proc *p;
	vm_offset_t va, v;
	struct vmspace *vm;
	int oldflags;
	int rv;

	va = trunc_page((vm_offset_t)addr);
	/*
	 * XXX - MAX is END.  Changed > to >= for temp. fix.
	 */
	if (va >= VM_MAXUSER_ADDRESS)
		return (1);

	p = curproc;
	vm = p->p_vmspace;

	oldflags = p->p_flag;
	p->p_flag |= SLOCK;

	if ((caddr_t)va >= vm->vm_maxsaddr
	    && (caddr_t)va < (caddr_t)USRSTACK) {
		if (!grow(p, va)) {
			p->p_flag &= ~SLOCK;
			p->p_flag |= (oldflags & SLOCK);
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

	p->p_flag &= ~SLOCK;
	p->p_flag |= (oldflags & SLOCK);

	if (rv != KERN_SUCCESS)
		return 1;

	return (0);
}

/*
 * syscall(frame):
 *	System call request from POSIX system call gate interface to kernel.
 * Like trap(), argument is call by reference.
 */
/*ARGSUSED*/
void
syscall(frame)
	volatile struct trapframe frame;
{
	register int *locr0 = ((int *)&frame);
	register caddr_t params;
	register int i;
	register struct sysent *callp;
	register struct proc *p = curproc;
	struct timeval syst;
	int error, opc;
	int args[8], rval[2];
	int code;

#ifdef lint
	r0 = 0; r0 = r0; r1 = 0; r1 = r1;
#endif
	syst = p->p_stime;
	if (ISPL(frame.tf_cs) != SEL_UPL)
		panic("syscall");

	code = frame.tf_eax;
	p->p_regs = (int *)&frame;
	params = (caddr_t)frame.tf_esp + sizeof (int) ;

	/*
	 * Reconstruct pc, assuming lcall $X,y is 7 bytes, as it is always.
	 */
	opc = frame.tf_eip - 7;
	if (code == 0) {
		code = fuword(params);
		params += sizeof (int);
	}
	if (code < 0 || code >= nsysent)
		callp = &sysent[0];
	else
		callp = &sysent[code];

	if ((i = callp->sy_narg * sizeof (int)) &&
	    (error = copyin(params, (caddr_t)args, (u_int)i))) {
		frame.tf_eax = error;
		frame.tf_eflags |= PSL_C;	/* carry bit */
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSCALL))
			ktrsyscall(p->p_tracep, code, callp->sy_narg, args);
#endif
		goto done;
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, callp->sy_narg, args);
#endif
	rval[0] = 0;
	rval[1] = frame.tf_edx;
/*pg("%d. s %d\n", p->p_pid, code);*/
	error = (*callp->sy_call)(p, args, rval);
	if (error == ERESTART)
		frame.tf_eip = opc;
	else if (error != EJUSTRETURN) {
		if (error) {
/*pg("error %d", error);*/
			frame.tf_eax = error;
			frame.tf_eflags |= PSL_C;	/* carry bit */
		} else {
			frame.tf_eax = rval[0];
			frame.tf_edx = rval[1];
			frame.tf_eflags &= ~PSL_C;	/* carry bit */
		}
	}
	/* else if (error == EJUSTRETURN) */
		/* nothing to do */
done:
	/*
	 * Reinitialize proc pointer `p' as it may be different
	 * if this is a child returning from fork syscall.
	 */
	p = curproc;
	while (i = CURSIG(p))
		psig(i);
	p->p_pri = p->p_usrpri;
	if (want_resched) {
		int s;
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrq ourselves but before we
		 * swtch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		s = splclock();
		setrq(p);
		p->p_stats->p_ru.ru_nivcsw++;
		swtch();
		splx(s);
		while (i = CURSIG(p))
			psig(i);
	}
	if (p->p_stats->p_prof.pr_scale) {
		int ticks;
		struct timeval *tv = &p->p_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks) {
#ifdef PROFTIMER
			extern int profscale;
			addupc(frame.tf_eip, &p->p_stats->p_prof,
			    ticks * profscale);
#else
			addupc(frame.tf_eip, &p->p_stats->p_prof, ticks);
#endif
		}
	}
	curpri = p->p_pri;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
#ifdef	DIAGNOSTICx
{ extern int _udatasel, _ucodesel;
	if (frame.tf_ss != _udatasel)
		printf("ss %x call %d\n", frame.tf_ss, code);
	if ((frame.tf_cs&0xffff) != _ucodesel)
		printf("cs %x call %d\n", frame.tf_cs, code);
	if (frame.tf_eip > VM_MAXUSER_ADDRESS) {
		printf("eip %x call %d\n", frame.tf_eip, code);
		frame.tf_eip = 0;
	}
}
#endif
}
