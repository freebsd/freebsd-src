/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: trap.c,v 1.26 2000/05/27 00:40:40 sommerfeld Exp $
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "opt_ddb.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pioctl.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/user.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/trap.h>

/* These definitions should probably be somewhere else				XXX */
#define	FIRSTARG	3		/* first argument is in reg 3 */
#define	NARGREG		8		/* 8 args are in registers */
#define	MOREARGS(sp)	((caddr_t)((int)(sp) + 8)) /* more args go here */

#ifdef WITNESS
extern char *syscallnames[];
#endif

#if 0 /* XXX: not used yet */
static int fix_unaligned __P((struct proc *p, struct trapframe *frame));
#endif
static void trap_fatal __P((struct trapframe *frame));
static void printtrap __P((int vector, struct trapframe *frame, int isfatal,
			      int user));
static int trap_pfault __P((struct trapframe *frame, int user));
static int handle_onfault (struct trapframe *frame);

static const char *ppc_exception_names[] = {
	"Reserved 0",				/* 0 */
	"Reset",				/* 1 */
	"Machine Check",			/* 2 */
	"Data Storage Interrupt",		/* 3 */
	"Instruction Storage Interrupt",	/* 4 */
	"External Interrupt",			/* 5 */
	"Alignment Interrupt",			/* 6 */
	"Program Interrupt",			/* 7 */
	"Floating Point Unavailable",		/* 8 */
	"Decrementer Interrupt",		/* 9 */
	"Reserved",				/* 10 */
	"Reserved",				/* 11 */
	"System Call",				/* 12 */
	"Trace",				/* 13 */
	"Floating Point Assist",		/* 14 */
	"Performance Monitoring",		/* 15 */
	"Instruction TLB Miss",			/* 16 */
	"Data Load TLB Miss",			/* 17 */
	"Data Store TLB Miss",			/* 18 */
	"Instruction Breakpoint",		/* 19 */
	"System Management Interrupt",		/* 20 */
	"Reserved 21",				/* 21 */
	"Reserved 22",				/* 22 */
	"Reserved 23",				/* 23 */
	"Reserved 24",				/* 24 */
	"Reserved 25",				/* 25 */
	"Reserved 26",				/* 26 */
	"Reserved 27",				/* 27 */
	"Reserved 28",				/* 28 */
	"Reserved 29",				/* 29 */
	"Reserved 30",				/* 30 */
	"Reserved 31",				/* 31 */
	"Reserved 32",				/* 32 */
	"Reserved 33",				/* 33 */
	"Reserved 34",				/* 34 */
	"Reserved 35",				/* 35 */
	"Reserved 36",				/* 36 */
	"Reserved 37",				/* 37 */
	"Reserved 38",				/* 38 */
	"Reserved 39",				/* 39 */
	"Reserved 40",				/* 40 */
	"Reserved 41",				/* 41 */
	"Reserved 42",				/* 42 */
	"Reserved 43",				/* 43 */
	"Reserved 44",				/* 44 */
	"Reserved 45",				/* 45 */
	"Reserved 46",				/* 46 */
	"Reserved 47",				/* 47 */
};

static void
printtrap __P((int vector, struct trapframe *frame, int isfatal, int user))
{

	printf("\n");
	printf("%s %s trap:\n", isfatal ? "fatal" : "handled",
	    user ? "user" : "kernel");
	printf("\n");
	printf("   exception       = 0x%x (%s)\n", vector >> 8,
	    ppc_exception_names[vector >> 8]);
	switch (vector) {
	case EXC_DSI:
		printf("   virtual address = 0x%x\n", frame->dar);
		break;
	case EXC_ISI:
		printf("   virtual address = 0x%x\n", frame->srr0);
		break;
	}
	printf("   srr0            = 0x%x", frame->srr0);
	printf("   curthread       = %p\n", curthread);
	if (curthread != NULL)
		printf("          pid = %d, comm = %s\n",
		    curthread->td_proc->p_pid, curthread->td_proc->p_comm);
	printf("\n");
}

static void
trap_fatal(struct trapframe *frame)
{

	printtrap(frame->exc, frame, 1, (frame->srr1 & PSL_PR));
#ifdef DDB
	if ((debugger_on_panic || db_active) && kdb_trap(frame->exc, 0, frame))
		return;
#endif
	panic("%s Trap", ppc_exception_names[frame->exc >> 8]);
}

/*
 * Handles a fatal fault when we have onfault state to recover.  Returns
 * non-zero if there was onfault recovery state available.
 */
static int
handle_onfault (struct trapframe *frame)
{
	struct thread *td;
	faultbuf *fb;

	td = curthread;
	fb = td->td_pcb->pcb_onfault;
	if (fb != NULL) {
		frame->srr0 = (*fb)[0];
		frame->fixreg[1] = (*fb)[1];
		frame->fixreg[2] = (*fb)[2];
		frame->cr = (*fb)[3];
		bcopy(&(*fb)[4], &frame->fixreg[13],
		    19 * sizeof(register_t));
		return (1);
	}
	return (0);
}

void
trap(struct trapframe *frame)
{
	struct thread *td;
	struct proc *p;
	int sig, type, user;
	u_int sticks, ucode;

	atomic_add_int(&cnt.v_trap, 1);

	td = curthread;
	p = td->td_proc;

	type = frame->exc;
	ucode = type;
	sig = 0;
	user = (frame->srr1 & PSL_PR);
	sticks = 0;

	CTR3(KTR_TRAP, "trap: %s type=%s (%s)", p->p_comm,
	    ppc_exception_names[type >> 8],
	    user ? "user" : "kernel");

	if (user) {
		sticks = td->td_kse->ke_sticks;
		td->td_frame = frame;
		KASSERT(td->td_ucred == NULL, ("already have a ucred"));
		PROC_LOCK(p);
		td->td_ucred = crhold(p->p_ucred);
		PROC_UNLOCK(p);

		/* User Mode Traps */
		switch (type) {
		case EXC_TRC:
			frame->srr1 &= ~PSL_SE;
			sig = SIGTRAP;
			break;
		case EXC_DSI:
		case EXC_ISI:
			sig = trap_pfault(frame, 1);
			break;
		case EXC_SC:
			syscall(frame);
			break;
		case EXC_FPU:
			enable_fpu(PCPU_GET(curpcb));
			frame->srr1 |= PSL_FP;
			break;

		case EXC_ALI:
#if 0			
		if (fix_unaligned(p, frame) != 0)
#endif	
			sig = SIGBUS;
#if 0		
		else
			frame->srr0 += 4;
#endif		
		break;

		case EXC_PGM:
			/* XXX temporarily */
			/* XXX: Magic Number? */
			if (frame->srr1 & 0x0002000)
				sig = SIGTRAP;
			else
				sig = SIGILL;
			break;

		default:
			trap_fatal(frame);
		}
	} else {
		/* Kernel Mode Traps */

		KASSERT(cold || td->td_ucred != NULL,
		    ("kernel trap doesn't have ucred"));
		switch (type) {
		case EXC_DSI:
			if (trap_pfault(frame, 0) == 0)
				return;
			break;
		case EXC_MCHK:
			if (handle_onfault(frame))
				return;
			break;
		default:
			trap_fatal(frame);
		}
	}
	if (sig != 0) {
		if (p->p_sysent->sv_transtrap != NULL)
			sig = (p->p_sysent->sv_transtrap)(sig, type);
		trapsignal(p, sig, ucode);
	}
	userret(td, frame, sticks);
	mtx_assert(&Giant, MA_NOTOWNED);
	mtx_lock(&Giant);
	crfree(td->td_ucred);
	mtx_unlock(&Giant);
	td->td_ucred = NULL;
}

void
syscall(struct trapframe *frame)
{
	caddr_t params;
	struct sysent *callp;
	struct thread *td;
	struct proc *p;
	int error, n;
	size_t narg;
	register_t args[10];
	u_int code;

	td = curthread;
	p = td->td_proc;

	atomic_add_int(&cnt.v_syscall, 1);
			
	code = frame->fixreg[0];
	params = (caddr_t) (frame->fixreg + FIRSTARG);
			
	if (p->p_sysent->sv_prepsyscall)
		/*
		 * The prep code is MP aware.
		 */
		(*p->p_sysent->sv_prepsyscall)(frame, args, &code, &params);
	else if (code == SYS_syscall)
		/*
		 * code is first argument,
		 * followed by actual args.
		 */
		code = *params++;
	else if (code == SYS___syscall) {
		/*
		 * Like syscall, but code is a quad,
		 * so as to maintain quad alignment
		 * for the rest of the args.
		 */
		params++;
		code = *params++;
	}

 	if (p->p_sysent->sv_mask)
 		code &= p->p_sysent->sv_mask;

 	if (code >= p->p_sysent->sv_size)
 		callp = &p->p_sysent->sv_table[0];
  	else
 		callp = &p->p_sysent->sv_table[code];

	narg = callp->sy_narg & SYF_ARGMASK;

	n = NARGREG - (params - (caddr_t)(frame->fixreg + FIRSTARG));
	if (narg > n * sizeof(register_t)) {
		bcopy(params, args, n * sizeof(register_t));
		if (error = copyin(MOREARGS(frame->fixreg[1]), args + n,
			narg - n * sizeof(register_t))) {
#ifdef	KTRACE
			/* Can't get all the arguments! */
			if (KTRPOINT(p, KTR_SYSCALL))
				ktrsyscall(p->p_tracep, code, narg, args);
#endif
			goto bad;
		}
		params = (caddr_t) args;
	}

	/*
	 * Try to run the syscall without Giant if the syscall is MP safe.
	 */
	if ((callp->sy_narg & SYF_MPSAFE) == 0)
		mtx_lock(&Giant);

#ifdef	KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, narg, params);
#endif
	td->td_retval[0] = 0;
	td->td_retval[1] = frame->fixreg[FIRSTARG + 1];

	STOPEVENT(p, S_SCE, narg);

	error = (*callp->sy_call)(td, args);
	switch (error) {
	case 0:
		frame->fixreg[FIRSTARG] = td->td_retval[0];
		frame->fixreg[FIRSTARG + 1] = td->td_retval[1];
		/* XXX: Magic number */
		frame->cr &= ~0x10000000;
		break;
	case ERESTART:
		/*
		 * Set user's pc back to redo the system call.
		 */
		frame->srr0 -= 4;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
bad:
		if (p->p_sysent->sv_errsize) {
			if (error >= p->p_sysent->sv_errsize)
				error = -1;	/* XXX */
			else
				error = p->p_sysent->sv_errtbl[error];
		}
		frame->fixreg[FIRSTARG] = error;
		/* XXX: Magic number: Carry Flag Equivalent? */
		frame->cr |= 0x10000000;
		break;
	}

	
#ifdef	KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, td->td_retval[0]);
#endif

	if ((callp->sy_narg & SYF_MPSAFE) == 0)
		mtx_unlock(&Giant);

	/*
	 * Does the comment in the i386 code about errno apply here?
	 */
	STOPEVENT(p, S_SCX, code);

#ifdef WITNESS
	if (witness_list(td)) {
		panic("system call %s returning with mutex(s) held\n",
		    syscallnames[code]);
	}
#endif
	mtx_assert(&sched_lock, MA_NOTOWNED);
	mtx_assert(&Giant, MA_NOTOWNED);	
}

static int
trap_pfault(struct trapframe *frame, int user)
{
	vm_offset_t eva, va;
	struct thread *td;
	struct proc *p;
	vm_map_t map;
	vm_prot_t ftype;
	int rv;

	td = curthread;
	p = td->td_proc;
	if (frame->exc == EXC_ISI) {
		eva = frame->srr0;
		ftype = VM_PROT_READ | VM_PROT_EXECUTE;
	} else {
		eva = frame->dar;
		if (frame->dsisr & DSISR_STORE)
			ftype = VM_PROT_READ | VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;
	}

	if ((eva >> ADDR_SR_SHFT) != USER_SR) {
		if (user)
			return (SIGSEGV);
		map = kernel_map;
	} else {
		u_int user_sr;

		if (p->p_vmspace == NULL)
			return (SIGSEGV);
				
		__asm ("mfsr %0, %1"
		    : "=r"(user_sr)
		    : "K"(USER_SR));
		eva &= ADDR_PIDX | ADDR_POFF;
		eva |= user_sr << ADDR_SR_SHFT;
		map = &p->p_vmspace->vm_map;
	}
	va = trunc_page(eva);

	mtx_lock(&Giant);
	if (map != kernel_map) {
		/*
		 * Keep swapout from messing with us during this
		 *	critical time.
		 */
		PROC_LOCK(p);
		++p->p_lock;
		PROC_UNLOCK(p);

		/*
		 * Grow the stack if necessary
		 */
		/* grow_stack returns false only if va falls into
		 * a growable stack region and the stack growth
		 * fails.  It returns true if va was not within
		 * a growable stack region, or if the stack 
		 * growth succeeded.
		 */
		if (!grow_stack (p, va))
			rv = KERN_FAILURE;
		else
			/* Fault in the user page: */
			rv = vm_fault(map, va, ftype,
			      (ftype & VM_PROT_WRITE) ? VM_FAULT_DIRTY
						      : VM_FAULT_NORMAL);

		PROC_LOCK(p);
		--p->p_lock;
		PROC_UNLOCK(p);
	} else {
		/*
		 * Don't have to worry about process locking or stacks in the
		 * kernel.
		 */
		rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);
	}
	mtx_unlock(&Giant);

	if (rv == KERN_SUCCESS)
		return (0);

	if (!user && handle_onfault(frame))
		return (0);

	return (SIGSEGV);
}

#if 0 /* XXX: child_return not used */
/*
 * XXX: the trapframe return values should be setup in vm_machdep.c in
 * cpu_fork().
 */
void
child_return(void *arg)
{
	struct proc *p;
	struct trapframe *tf;

	p = arg;
	tf = trapframe(p);

	tf->fixreg[FIRSTARG] = 0;
	tf->fixreg[FIRSTARG + 1] = 1;
	tf->cr &= ~0x10000000;
	tf->srr1 &= ~PSL_FP;	/* Disable FPU, as we can't be fpuproc */
#ifdef	KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, SYS_fork, 0, 0);
#endif
	/* Profiling?							XXX */
	curcpu()->ci_schedstate.spc_curpriority = p->p_priority;
}
#endif

static __inline void
setusr(int content)
{

	__asm __volatile ("isync; mtsr %0,%1; isync"
		          :: "n"(USER_SR), "r"(content));
}

int
copyin(udaddr, kaddr, len)
	const void *udaddr;
	void *kaddr;
	size_t len;
{
	const char *up;
	char *kp;
	char *p;
	size_t l;
	faultbuf env;
	uint segment;

	up = udaddr;
	kp = kaddr;

#if 0
	if (setfault(env)) {
		PCPU_GET(curpcb)->pcb_onfault = 0;
		return EFAULT;
	}
#endif
	while (len > 0) {
		p = (char *)USER_ADDR + ((u_int)up & ~SEGMENT_MASK);
		l = ((char *)USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;
		segment = (uint)up >> ADDR_SR_SHFT;
		setusr(PCPU_GET(curpcb)->pcb_pm->pm_sr[segment]);
		bcopy(p, kp, l);
		up += l;
		kp += l;
		len -= l;
	}
	PCPU_GET(curpcb)->pcb_onfault = 0;
	return 0;
}

int
copyout(kaddr, udaddr, len)
	const void *kaddr;
	void *udaddr;
	size_t len;
{
	const char *kp;
	char *up;
	char *p;
	size_t l;
	faultbuf env;
	uint segment;

	kp = kaddr;
	up = udaddr;

#if 0
	if (setfault(env)) {
		PCPU_GET(curpcb)->pcb_onfault = 0;
		return EFAULT;
	}
#endif
	while (len > 0) {
		p = (char *)USER_ADDR + ((u_int)up & ~SEGMENT_MASK);
		l = ((char *)USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;
		segment = (u_int)up >> ADDR_SR_SHFT;
		setusr(PCPU_GET(curpcb)->pcb_pm->pm_sr[segment]);
		bcopy(kp, p, l);
		up += l;
		kp += l;
		len -= l;
	}
	PCPU_GET(curpcb)->pcb_onfault = 0;
	return 0;
}

#if 0 /* XXX: not used yet */
/*
 * kcopy(const void *src, void *dst, size_t len);
 *
 * Copy len bytes from src to dst, aborting if we encounter a fatal
 * page fault.
 *
 * kcopy() _must_ save and restore the old fault handler since it is
 * called by uiomove(), which may be in the path of servicing a non-fatal
 * page fault.
 */
int
kcopy(const void *src, void *dst, size_t len)
{
	faultbuf env, *oldfault;

	oldfault = PCPU_GET(curpcb)->pcb_onfault;
	if (setfault(env)) {
		PCPU_GET(curpcb)->pcb_onfault = oldfault;
		return EFAULT;
	}

	bcopy(src, dst, len);

	PCPU_GET(curpcb)->pcb_onfault = oldfault;
	return 0;
}

int
badaddr(void *addr, size_t size)
{

	return badaddr_read(addr, size, NULL);
}

int
badaddr_read(void *addr, size_t size, int *rptr)
{
	faultbuf env;
	int x;

	/* Get rid of any stale machine checks that have been waiting.  */
	__asm __volatile ("sync; isync");

	if (setfault(env)) {
		PCPU_GET(curpcb)->pcb_onfault = 0;
		__asm __volatile ("sync");
		return 1;
	}

	__asm __volatile ("sync");

	switch (size) {
	case 1:
		x = *(volatile int8_t *)addr;
		break;
	case 2:
		x = *(volatile int16_t *)addr;
		break;
	case 4:
		x = *(volatile int32_t *)addr;
		break;
	default:
		panic("badaddr: invalid size (%d)", size);
	}

	/* Make sure we took the machine check, if we caused one. */
	__asm __volatile ("sync; isync");

	PCPU_GET(curpcb)->pcb_onfault = 0;
	__asm __volatile ("sync");	/* To be sure. */

	/* Use the value to avoid reorder. */
	if (rptr)
		*rptr = x;

	return 0;
}
#endif

/*
 * For now, this only deals with the particular unaligned access case
 * that gcc tends to generate.  Eventually it should handle all of the
 * possibilities that can happen on a 32-bit PowerPC in big-endian mode.
 */

#if 0 /* XXX: Not used yet */
static int
fix_unaligned(p, frame)
	struct proc *p;
	struct trapframe *frame;
{
	int indicator;
	
	indicator = EXC_ALI_OPCODE_INDICATOR(frame->dsisr);

	switch (indicator) {
	case EXC_ALI_LFD:
	case EXC_ALI_STFD:
		{
			int reg = EXC_ALI_RST(frame->dsisr);
			double *fpr = &p->p_addr->u_pcb.pcb_fpu.fpr[reg];

			/* Juggle the FPU to ensure that we've initialized
			 * the FPRs, and that their current state is in
			 * the PCB.
			 */
			if (!(pcb->pcb_flags & PCB_FPU))
				enable_fpu(PCPU_GET(curpcb));
				frame->srr1 |= PSL_FP;
			}
			save_fpu(PCPU_GET(curpcb));

			if (indicator == EXC_ALI_LFD) {
				if (copyin((void *)frame->dar, fpr,
				    sizeof(double)) != 0)
					return -1;
				if (!(pcb->pcb_flags & PCB_FPU))
					enable_fpu(PCPU_GET(curpcb));
					frame->srr1 |= PSL_FP;
				}
			} else {
				if (copyout(fpr, (void *)frame->dar,
				    sizeof(double)) != 0)
					return -1;
			}
			return 0;
		}
		break;
	}

	return -1;
}
#endif
