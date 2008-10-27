/*-
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
 * $NetBSD: trap.c,v 1.58 2002/03/04 04:07:35 dbj Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_fpu_emu.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/proc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pioctl.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/signalvar.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <sys/vmmeter.h>

#include <security/audit/audit.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>
#include <machine/db_machdep.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/spr.h>

#ifdef FPU_EMU
#include <powerpc/fpu/fpu_extern.h>
#endif

#define	FAULTBUF_LR	0
#define	FAULTBUF_R1	1
#define	FAULTBUF_R2	2
#define	FAULTBUF_CR	3
#define	FAULTBUF_CTR	4
#define	FAULTBUF_XER	5
#define	FAULTBUF_R13	6

static void	trap_fatal(struct trapframe *frame);
static void	printtrap(u_int vector, struct trapframe *frame, int isfatal,
    int user);
static int	trap_pfault(struct trapframe *frame, int user);
static int	fix_unaligned(struct thread *td, struct trapframe *frame);
static int	handle_onfault(struct trapframe *frame);
static void	syscall(struct trapframe *frame);

int	setfault(faultbuf);		/* defined in locore.S */

/* Why are these not defined in a header? */
int	badaddr(void *, size_t);
int	badaddr_read(void *, size_t, int *);

extern char	*syscallnames[];

struct powerpc_exception {
	u_int	vector;
	char	*name;
};

static struct powerpc_exception powerpc_exceptions[] = {
	{ EXC_CRIT,	"critical input" },
	{ EXC_MCHK,	"machine check" },
	{ EXC_DSI,	"data storage interrupt" },
	{ EXC_ISI,	"instruction storage interrupt" },
	{ EXC_EXI,	"external interrupt" },
	{ EXC_ALI,	"alignment" },
	{ EXC_PGM,	"program" },
	{ EXC_SC,	"system call" },
	{ EXC_APU,	"auxiliary proc unavailable" },
	{ EXC_DECR,	"decrementer" },
	{ EXC_FIT,	"fixed-interval timer" },
	{ EXC_WDOG,	"watchdog timer" },
	{ EXC_DTMISS,	"data tlb miss" },
	{ EXC_ITMISS,	"instruction tlb miss" },
	{ EXC_DEBUG,	"debug" },
	{ EXC_PERF,	"performance monitoring" },
	{ EXC_LAST,	NULL }
};

static const char *
trapname(u_int vector)
{
	struct	powerpc_exception *pe;

	for (pe = powerpc_exceptions; pe->vector != EXC_LAST; pe++) {
		if (pe->vector == vector)
			return (pe->name);
	}

	return ("unknown");
}

void
trap(struct trapframe *frame)
{
	struct thread	*td;
	struct proc	*p;
	int		sig, type, user;
	ksiginfo_t	ksi;

	PCPU_INC(cnt.v_trap);

	td = PCPU_GET(curthread);
	p = td->td_proc;

	type = frame->exc;
	sig = 0;
	user = (frame->srr1 & PSL_PR) ? 1 : 0;

	CTR3(KTR_TRAP, "trap: %s type=%s (%s)", p->p_comm,
	    trapname(type), user ? "user" : "kernel");

	if (user) {
		td->td_frame = frame;
		if (td->td_ucred != p->p_ucred)
			cred_update_thread(td);

		/* User Mode Traps */
		switch (type) {
		case EXC_DSI:
		case EXC_ISI:
			sig = trap_pfault(frame, 1);
			break;

		case EXC_SC:
			syscall(frame);
			break;

		case EXC_ALI:
			if (fix_unaligned(td, frame) != 0)
				sig = SIGBUS;
			else
				frame->srr0 += 4;
			break;

		case EXC_DEBUG:	/* Single stepping */
			mtspr(SPR_DBSR, mfspr(SPR_DBSR));
			frame->srr1 &= ~PSL_DE;
			sig = SIGTRAP;
			break;

		case EXC_PGM:	/* Program exception */
#ifdef FPU_EMU
			sig = fpu_emulate(frame,
			    (struct fpreg *)&td->td_pcb->pcb_fpu);
#else
			/* XXX SIGILL for non-trap instructions. */
			sig = SIGTRAP;
#endif
			break;

		default:
			trap_fatal(frame);
		}
	} else {
		/* Kernel Mode Traps */
		KASSERT(cold || td->td_ucred != NULL,
		    ("kernel trap doesn't have ucred"));

		switch (type) {
		case EXC_DEBUG:
			mtspr(SPR_DBSR, mfspr(SPR_DBSR));
			kdb_trap(frame->exc, 0, frame);
			return;

		case EXC_DSI:
			if (trap_pfault(frame, 0) == 0)
 				return;
			break;

		case EXC_MCHK:
			if (handle_onfault(frame))
 				return;
			break;
#ifdef KDB
		case EXC_PGM:
			if (frame->cpu.booke.esr & ESR_PTR)
				kdb_trap(EXC_PGM, 0, frame);
			return;
#endif
		default:
			break;
		}
		trap_fatal(frame);
	}

	if (sig != 0) {
		if (p->p_sysent->sv_transtrap != NULL)
			sig = (p->p_sysent->sv_transtrap)(sig, type);
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = sig;
		ksi.ksi_code = type; /* XXX, not POSIX */
		/* ksi.ksi_addr = ? */
		ksi.ksi_trapno = type;
		trapsignal(td, &ksi);
	}

	userret(td, frame);
	mtx_assert(&Giant, MA_NOTOWNED);
}

static void
trap_fatal(struct trapframe *frame)
{

	printtrap(frame->exc, frame, 1, (frame->srr1 & PSL_PR));
#ifdef KDB
	if ((debugger_on_panic || kdb_active) &&
	    kdb_trap(frame->exc, 0, frame))
		return;
#endif
	panic("%s trap", trapname(frame->exc));
}

static void
printtrap(u_int vector, struct trapframe *frame, int isfatal, int user)
{
	register_t va = 0;

	printf("\n");
	printf("%s %s trap:\n", isfatal ? "fatal" : "handled",
	    user ? "user" : "kernel");
	printf("\n");
	printf("   exception       = 0x%x (%s)\n", vector, trapname(vector));
	
	switch (vector) {
	case EXC_DTMISS:
	case EXC_DSI:
		va = frame->cpu.booke.dear;
		break;

	case EXC_ITMISS:
	case EXC_ISI:
		va = frame->srr0;
		break;
	}

	printf("   virtual address = 0x%08x\n", va);
	printf("   srr0            = 0x%08x\n", frame->srr0);
	printf("   srr1            = 0x%08x\n", frame->srr1);
	printf("   curthread       = %p\n", curthread);
	if (curthread != NULL)
		printf("          pid = %d, comm = %s\n",
		    curthread->td_proc->p_pid, curthread->td_proc->p_comm);
	printf("\n");
}

/*
 * Handles a fatal fault when we have onfault state to recover.  Returns
 * non-zero if there was onfault recovery state available.
 */
static int
handle_onfault(struct trapframe *frame)
{
	struct		thread *td;
	faultbuf	*fb;

	td = curthread;
	fb = td->td_pcb->pcb_onfault;
	if (fb != NULL) {
		frame->srr0 = (*fb)[FAULTBUF_LR];
		frame->fixreg[1] = (*fb)[FAULTBUF_R1];
		frame->fixreg[2] = (*fb)[FAULTBUF_R2];
		frame->fixreg[3] = 1;
		frame->cr = (*fb)[FAULTBUF_CR];
		frame->ctr = (*fb)[FAULTBUF_CTR];
		frame->xer = (*fb)[FAULTBUF_XER];
		bcopy(&(*fb)[FAULTBUF_R13], &frame->fixreg[13],
		    19 * sizeof(register_t));
		return (1);
	}
	return (0);
}

void
syscall(struct trapframe *frame)
{
	caddr_t		params;
	struct		sysent *callp;
	struct		thread *td;
	struct		proc *p;
	int		error, n;
	size_t		narg;
	register_t	args[10];
	u_int		code;

	td = PCPU_GET(curthread);
	p = td->td_proc;

	PCPU_INC(cnt.v_syscall);

	code = frame->fixreg[0];
	params = (caddr_t)(frame->fixreg + FIRSTARG);
	n = NARGREG;

	if (p->p_sysent->sv_prepsyscall) {
		/*
		 * The prep code is MP aware.
		 */
		(*p->p_sysent->sv_prepsyscall)(frame, args, &code, &params);
	} else if (code == SYS_syscall) {
		/*
		 * code is first argument,
		 * followed by actual args.
		 */
		code = *(u_int *) params;
		params += sizeof(register_t);
		n -= 1;
	} else if (code == SYS___syscall) {
		/*
		 * Like syscall, but code is a quad,
		 * so as to maintain quad alignment
		 * for the rest of the args.
		 */
		params += sizeof(register_t);
		code = *(u_int *) params;
		params += sizeof(register_t);
		n -= 2;
	}

	if (p->p_sysent->sv_mask)
		code &= p->p_sysent->sv_mask;

	if (code >= p->p_sysent->sv_size)
		callp = &p->p_sysent->sv_table[0];
	else
		callp = &p->p_sysent->sv_table[code];

	narg = callp->sy_narg;

	if (narg > n) {
		bcopy(params, args, n * sizeof(register_t));
		error = copyin(MOREARGS(frame->fixreg[1]), args + n,
		    (narg - n) * sizeof(register_t));
		params = (caddr_t)args;
	} else
		error = 0;

	CTR5(KTR_SYSC, "syscall: p=%s %s(%x %x %x)", p->p_comm,
	     syscallnames[code],
	     frame->fixreg[FIRSTARG],
	     frame->fixreg[FIRSTARG+1],
	     frame->fixreg[FIRSTARG+2]);

#ifdef	KTRACE
	if (KTRPOINT(td, KTR_SYSCALL))
		ktrsyscall(code, narg, (register_t *)params);
#endif

	td->td_syscalls++;

	if (error == 0) {
		td->td_retval[0] = 0;
		td->td_retval[1] = frame->fixreg[FIRSTARG + 1];

		STOPEVENT(p, S_SCE, narg);

		PTRACESTOP_SC(p, td, S_PT_SCE);

		AUDIT_SYSCALL_ENTER(code, td);
		error = (*callp->sy_call)(td, params);
		AUDIT_SYSCALL_EXIT(error, td);

		CTR3(KTR_SYSC, "syscall: p=%s %s ret=%x", p->p_comm,
		     syscallnames[code], td->td_retval[0]);
	}

	switch (error) {
	case 0:
		if (frame->fixreg[0] == SYS___syscall && SYS_lseek) {
			/*
			 * 64-bit return, 32-bit syscall. Fixup byte order
			 */
			frame->fixreg[FIRSTARG] = 0;
			frame->fixreg[FIRSTARG + 1] = td->td_retval[0];
		} else {
			frame->fixreg[FIRSTARG] = td->td_retval[0];
			frame->fixreg[FIRSTARG + 1] = td->td_retval[1];
		}
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

	/*
	 * Check for misbehavior.
	 */
	WITNESS_WARN(WARN_PANIC, NULL, "System call %s returning",
	    (code >= 0 && code < SYS_MAXSYSCALL) ? syscallnames[code] : "???");
	KASSERT(td->td_critnest == 0,
	    ("System call %s returning in a critical section",
	    (code >= 0 && code < SYS_MAXSYSCALL) ? syscallnames[code] : "???"));
	KASSERT(td->td_locks == 0,
	    ("System call %s returning with %d locks held",
	    (code >= 0 && code < SYS_MAXSYSCALL) ? syscallnames[code] : "???",
	    td->td_locks));

#ifdef	KTRACE
	if (KTRPOINT(td, KTR_SYSRET))
		ktrsysret(code, error, td->td_retval[0]);
#endif

	/*
	 * Does the comment in the i386 code about errno apply here?
	 */
	STOPEVENT(p, S_SCX, code);

	PTRACESTOP_SC(p, td, S_PT_SCX);
}

static int
trap_pfault(struct trapframe *frame, int user)
{
	vm_offset_t	eva, va;
	struct		thread *td;
	struct		proc *p;
	vm_map_t	map;
	vm_prot_t	ftype;
	int		rv;

	td = curthread;
	p = td->td_proc;

	if (frame->exc == EXC_ISI) {
		eva = frame->srr0;
		ftype = VM_PROT_READ | VM_PROT_EXECUTE;

	} else {
		eva = frame->cpu.booke.dear;
		if (frame->cpu.booke.esr & ESR_ST)
			ftype = VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;
	}

	if (user) {
		KASSERT(p->p_vmspace != NULL, ("trap_pfault: vmspace  NULL"));
		map = &p->p_vmspace->vm_map;
	} else {
		if (eva < VM_MAXUSER_ADDRESS) {

			if (p->p_vmspace == NULL)
				return (SIGSEGV);

			map = &p->p_vmspace->vm_map;

		} else {
			map = kernel_map;
		}
	}
	va = trunc_page(eva);

	if (map != kernel_map) {
		/*
		 * Keep swapout from messing with us during this
		 *	critical time.
		 */
		PROC_LOCK(p);
		++p->p_lock;
		PROC_UNLOCK(p);

		/* Fault in the user page: */
		rv = vm_fault(map, va, ftype,
		    (ftype & VM_PROT_WRITE) ? VM_FAULT_DIRTY : VM_FAULT_NORMAL);

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

	if (rv == KERN_SUCCESS)
		return (0);

	if (!user && handle_onfault(frame))
		return (0);

	return ((rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV);
}

int
badaddr(void *addr, size_t size)
{

	return (badaddr_read(addr, size, NULL));
}

int
badaddr_read(void *addr, size_t size, int *rptr)
{
	struct thread	*td;
	faultbuf	env;
	int		x;

	/* Get rid of any stale machine checks that have been waiting.  */
	__asm __volatile ("sync; isync");

	td = PCPU_GET(curthread);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = 0;
		__asm __volatile ("sync");
		return (1);
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

	td->td_pcb->pcb_onfault = 0;
	__asm __volatile ("sync");	/* To be sure. */

	/* Use the value to avoid reorder. */
	if (rptr)
		*rptr = x;

	return (0);
}

/*
 * For now, this only deals with the particular unaligned access case
 * that gcc tends to generate.  Eventually it should handle all of the
 * possibilities that can happen on a 32-bit PowerPC in big-endian mode.
 */

static int
fix_unaligned(struct thread *td, struct trapframe *frame)
{
#if 0
	struct thread	*fputhread;
	int		indicator, reg;
	double		*fpr;

	indicator = EXC_ALI_OPCODE_INDICATOR(frame->dsisr);

	switch (indicator) {
	case EXC_ALI_LFD:
	case EXC_ALI_STFD:
		reg = EXC_ALI_RST(frame->dsisr);
		fpr = &td->td_pcb->pcb_fpu.fpr[reg];
		fputhread = PCPU_GET(fputhread);
		/* Juggle the FPU to ensure that we've initialized
		 * the FPRs, and that their current state is in
		 * the PCB.
		 */
		if (fputhread != td) {
			if (fputhread)
				save_fpu(fputhread);
			enable_fpu(td);
		}
		save_fpu(td);

		if (indicator == EXC_ALI_LFD) {
			if (copyin((void *)frame->dar, fpr,
			    sizeof(double)) != 0)
				return -1;
			enable_fpu(td);
		} else {
			if (copyout(fpr, (void *)frame->dar,
			    sizeof(double)) != 0)
				return -1;
		}
		return 0;
		break;
	}

#endif
	return (-1);
}

#ifdef KDB
int db_trap_glue(struct trapframe *);
int
db_trap_glue(struct trapframe *tf)
{
	if (!(tf->srr1 & PSL_PR))
		return (kdb_trap(tf->exc, 0, tf));
	return (0);
}
#endif
