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
 * $NetBSD: trap.c,v 1.58 2002/03/04 04:07:35 dbj Exp $
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "opt_ddb.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/user.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>
#include <machine/db_machdep.h>
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/spr.h>
#include <machine/sr.h>

/* These definitions should probably be somewhere else			XXX */
#define	FIRSTARG	3		/* first argument is in reg 3 */
#define	NARGREG		8		/* 8 args are in registers */
#define	MOREARGS(sp)	((caddr_t)((int)(sp) + 8)) /* more args go here */

#ifndef MULTIPROCESSOR
volatile int astpending;
volatile int want_resched;
extern int intr_depth;
#endif

void *syscall = NULL;	/* XXX dummy symbol for emul_netbsd */

static int fix_unaligned(struct proc *p, struct trapframe *frame);
static __inline void setusr(int);

void trap(struct trapframe *);	/* Called from locore / trap_subr */
int setfault(faultbuf);	/* defined in locore.S */
/* Why are these not defined in a header? */
int badaddr(void *, size_t);
int badaddr_read(void *, size_t, int *);

void
trap(frame)
	struct trapframe *frame;
{
	struct thread *td = PCPU_GET(curthread);
	struct proc *p = td->td_proc;
	int type = frame->exc;
	int ftype, rv;

#if 0
	curcpu()->ci_ev_traps.ev_count++;
#endif

	if (frame->srr1 & PSL_PR)
		type |= EXC_USER;

#ifdef DIAGNOSTIC
	if (curpcb->pcb_pmreal != curpm)
		panic("trap: curpm (%p) != curpcb->pcb_pmreal (%p)",
		    curpm, curpcb->pcb_pmreal);
#endif

#if 0
	uvmexp.traps++;
#endif

	switch (type) {
	case EXC_RUNMODETRC|EXC_USER:
		/* FALLTHROUGH */
	case EXC_TRC|EXC_USER:
		PROC_LOCK(p);
		frame->srr1 &= ~PSL_SE;
		trapsignal(p, SIGTRAP, EXC_TRC);
		PROC_UNLOCK(p);
		break;
	case EXC_DSI: {
		faultbuf *fb;
		/*
		 * Only query UVM if no interrupts are active (this applies
		 * "on-fault" as well.
		 */
#if 0
		curcpu()->ci_ev_kdsi.ev_count++;
#endif
		if (intr_depth < 0) {
			struct vm_map *map;
			vm_offset_t va;

#if 0
			KERNEL_LOCK(LK_CANRECURSE|LK_EXCLUSIVE);
#endif
			map = kernel_map;
			va = frame->dar;
			if ((va >> ADDR_SR_SHFT) == USER_SR) {
				register_t user_sr;

				__asm ("mfsr %0, %1"
				     : "=r"(user_sr) : "K"(USER_SR));
				va &= ADDR_PIDX | ADDR_POFF;
				va |= user_sr << ADDR_SR_SHFT;
				/* KERNEL_PROC_LOCK(p); XXX */
				map = &p->p_vmspace->vm_map;
			}
			if (frame->dsisr & DSISR_STORE)
				ftype = VM_PROT_WRITE;
			else
				ftype = VM_PROT_READ;
			rv = vm_fault(map, trunc_page(va), ftype,
			    VM_FAULT_NORMAL);
#if 0
			KERNEL_UNLOCK();
#endif
			if (rv == 0)
				return;
			if (rv == EACCES)
				rv = EFAULT;
		} else {
			rv = EFAULT;
		}
		if ((fb = td->td_pcb->pcb_onfault) != NULL) {
			frame->srr0 = (*fb)[0];
			frame->fixreg[1] = (*fb)[1];
			frame->fixreg[2] = (*fb)[2];
			frame->fixreg[3] = rv;
			frame->cr = (*fb)[3];
			memcpy(&frame->fixreg[13], &(*fb)[4],
				      19 * sizeof(register_t));
			return;
		}
		printf("trap: kernel %s DSI @ %#x by %#x (DSISR %#x, err=%d)\n",
		    (frame->dsisr & DSISR_STORE) ? "write" : "read",
		    frame->dar, frame->srr0, frame->dsisr, rv);
		goto brain_damage2;
	}
	case EXC_DSI|EXC_USER:
		PROC_LOCK(p);
#if 0
		curcpu()->ci_ev_udsi.ev_count++;
#endif
		if (frame->dsisr & DSISR_STORE)
			ftype = VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;
		rv = vm_fault(&p->p_vmspace->vm_map, trunc_page(frame->dar),
		    ftype, VM_FAULT_NORMAL);
#if 0
		curcpu()->ci_ev_udsi_fatal.ev_count++;
#endif
		printf("trap: pid %d (%s): user %s DSI @ %#x "
		    "by %#x (DSISR %#x, err=%d)\n",
		    p->p_pid, p->p_comm,
		    (frame->dsisr & DSISR_STORE) ? "write" : "read",
		    frame->dar, frame->srr0, frame->dsisr, rv);
		if (rv == ENOMEM) {
			printf("UVM: pid %d (%s), uid %d killed: "
			       "out of swap\n",
			       p->p_pid, p->p_comm,
			       td->td_ucred ?  td->td_ucred->cr_uid : -1);
			trapsignal(p, SIGKILL, EXC_DSI);
		} else {
			trapsignal(p, SIGSEGV, EXC_DSI);
		}
		PROC_UNLOCK(p);
		break;
	case EXC_ISI:
		printf("trap: kernel ISI by %#x (SRR1 %#x)\n",
		    frame->srr0, frame->srr1);
		goto brain_damage2;
	case EXC_ISI|EXC_USER:
		PROC_LOCK(p);
#if 0
		curcpu()->ci_ev_isi.ev_count++;
#endif
		ftype = VM_PROT_READ | VM_PROT_EXECUTE;
		rv = vm_fault(&p->p_vmspace->vm_map, trunc_page(frame->srr0),
		    ftype, VM_FAULT_NORMAL);
		printf("vm_fault said %d\n", rv);
		if (rv == 0) {
			PROC_UNLOCK(p);
			break;
		}
#if 0
		curcpu()->ci_ev_isi_fatal.ev_count++;
#endif
		printf("trap: pid %d (%s): user ISI trap @ %#x "
		    "(SSR1=%#x)\n",
		    p->p_pid, p->p_comm, frame->srr0, frame->srr1);
		trapsignal(p, SIGSEGV, EXC_ISI);
		PROC_UNLOCK(p);
		break;
	case EXC_SC|EXC_USER:
#if 0
		curcpu()->ci_ev_scalls.ev_count++;
#endif
		{
			const struct sysent *callp;
			size_t argsize;
			register_t code, error;
			register_t *params, rval[2];
			int n;
			register_t args[10];

			PROC_LOCK(p);

#if 0
			uvmexp.syscalls++;
#endif

			code = frame->fixreg[0];
			callp = &p->p_sysent->sv_table[0];
			params = frame->fixreg + FIRSTARG;
			n = NARGREG;

			switch (code) {
			case SYS_syscall:
				/*
				 * code is first argument,
				 * followed by actual args.
				 */
				code = *params++;
				n -= 1;
				break;
			case SYS___syscall:
				params++;
				code = *params++;
				n -= 2;
				break;
			default:
				break;
			}

			if (p->p_sysent->sv_mask)
				code &= p->p_sysent->sv_mask;
			callp += code;
			argsize = callp->sy_narg & SYF_ARGMASK;

			if (argsize > n * sizeof(register_t)) {
				memcpy(args, params, n * sizeof(register_t));
				error = copyin(MOREARGS(frame->fixreg[1]),
					       args + n,
					       argsize - n * sizeof(register_t));
				if (error)
					goto syscall_bad;
				params = args;
			}

#ifdef	KTRACE
			if (KTRPOINT(p, KTR_SYSCALL))
				ktrsyscall(p, code, argsize, params);
#endif

			rval[0] = 0;
			rval[1] = 0;

			error = (*callp->sy_call)(td, params);
			switch (error) {
			case 0:
				frame->fixreg[FIRSTARG] = rval[0];
				frame->fixreg[FIRSTARG + 1] = rval[1];
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
syscall_bad:
#if 0
				if (p->p_emul->e_errno)
					error = p->p_emul->e_errno[error];
#endif
				frame->fixreg[FIRSTARG] = error;
				frame->cr |= 0x10000000;
				break;
			}

#ifdef	KTRACE
			if (KTRPOINT(p, KTR_SYSRET))
				ktrsysret(p, code, error, rval[0]);
#endif
		}
		PROC_UNLOCK(p);
		break;

	case EXC_FPU|EXC_USER:
#if 0
		curcpu()->ci_ev_fpu.ev_count++;
#endif
#if 0
		if (fpuproc) {
			curcpu()->ci_ev_fpusw.ev_count++;
			save_fpu(fpuproc);
		}
#endif
#if defined(MULTIPROCESSOR)
		if (p->p_addr->u_pcb.pcb_fpcpu)
			save_fpu_proc(p);
#endif
#if 0
		fpuproc = p;
		p->p_addr->u_pcb.pcb_fpcpu = curcpu();
		enable_fpu(p);
#endif
		break;

#ifdef ALTIVEC
	case EXC_VEC|EXC_USER:
#if 0
		curcpu()->ci_ev_vec.ev_count++;
#endif
		if (vecproc) {
#if 0
			curcpu()->ci_ev_vecsw.ev_count++;
#endif
			save_vec(vecproc);
		}
		vecproc = p;
		enable_vec(p);
		break;
#endif

	case EXC_AST|EXC_USER:
		astpending = 0;		/* we are about to do it */
		PROC_LOCK(p);
#if 0
		uvmexp.softs++;
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
#endif
		/* Check whether we are being preempted. */
		if (want_resched)
			mi_switch();
		PROC_UNLOCK(p);
		break;

	case EXC_ALI|EXC_USER:
		PROC_LOCK(p);
#if 0
		curcpu()->ci_ev_ali.ev_count++;
#endif
		if (fix_unaligned(p, frame) != 0) {
#if 0
			curcpu()->ci_ev_ali_fatal.ev_count++;
#endif
			printf("trap: pid %d (%s): user ALI trap @ %#x "
			    "(SSR1=%#x)\n",
			    p->p_pid, p->p_comm, frame->srr0,
			    frame->srr1);
			trapsignal(p, SIGBUS, EXC_ALI);
		} else
			frame->srr0 += 4;
		PROC_UNLOCK(p);
		break;

	case EXC_PGM|EXC_USER:
/* XXX temporarily */
		PROC_LOCK(p);
#if 0
		curcpu()->ci_ev_pgm.ev_count++;
#endif
		printf("trap: pid %d (%s): user PGM trap @ %#x "
		    "(SSR1=%#x)\n",
		    p->p_pid, p->p_comm, frame->srr0, frame->srr1);
		if (frame->srr1 & 0x00020000)	/* Bit 14 is set if trap */
			trapsignal(p, SIGTRAP, EXC_PGM);
		else
			trapsignal(p, SIGILL, EXC_PGM);
		PROC_UNLOCK(p);
		break;

	case EXC_MCHK: {
		faultbuf *fb;

		if ((fb = td->td_pcb->pcb_onfault) != NULL) {
			frame->srr0 = (*fb)[0];
			frame->fixreg[1] = (*fb)[1];
			frame->fixreg[2] = (*fb)[2];
			frame->fixreg[3] = EFAULT;
			frame->cr = (*fb)[3];
			memcpy(&frame->fixreg[13], &(*fb)[4],
			      19 * sizeof(register_t));
			return;
		}
		goto brain_damage;
	}

	default:
brain_damage:
		printf("trap type %x at %x\n", type, frame->srr0);
brain_damage2:
#ifdef DDBX
		if (kdb_trap(type, frame))
			return;
#endif
#ifdef TRAP_PANICWAIT
		printf("Press a key to panic.\n");
		cnpollc(1);
		cngetc();
		cnpollc(0);
#endif
		panic("trap");
	}

	/* Take pending signals. */
	{
		int sig;

		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}

	/*
	 * If someone stole the fp or vector unit while we were away,
	 * disable it
	 */
#if 0
	if (p != fpuproc || p->p_addr->u_pcb.pcb_fpcpu != curcpu())
		frame->srr1 &= ~PSL_FP;
#endif
#ifdef ALTIVEC
	if (p != vecproc)
		frame->srr1 &= ~PSL_VEC;
#endif

#if 0
	curcpu()->ci_schedstate.spc_curpriority = p->p_priority = p->p_usrpri;
	p->p_priority = p->p_usrpri;
#endif
}

void child_return(void *);

void
child_return(void *arg)
{
	struct thread *td = arg;
	struct proc *p = td->td_proc;
	struct trapframe *tf = trapframe(td);

	PROC_UNLOCK(p);

	tf->fixreg[FIRSTARG] = 0;
	tf->fixreg[FIRSTARG + 1] = 1;
	tf->cr &= ~0x10000000;
#if 0
	tf->srr1 &= ~(PSL_FP|PSL_VEC);	/* Disable FP & AltiVec, as we can't
					   be them. */
	td->td_pcb->pcb_fpcpu = NULL;
#endif
#ifdef	KTRACE
	if (KTRPOINT(p, KTR_SYSRET)) {
		PROC_LOCK(p);
		ktrsysret(p, SYS_fork, 0, 0);
		PROC_UNLOCK(p);
	}
#endif
	/* Profiling?							XXX */
#if 0
	curcpu()->ci_schedstate.spc_curpriority = p->p_priority;
#endif
}

static __inline void
setusr(content)
	int content;
{
	__asm __volatile ("isync; mtsr %0,%1; isync"
		      :: "n"(USER_SR), "r"(content));
}

int kcopy(const void *, void *, size_t);

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
	struct thread *td;
	faultbuf env, *oldfault;
	int rv;

	td = PCPU_GET(curthread);
	oldfault = td->td_pcb->pcb_onfault;
	if ((rv = setfault(env)) != 0) {
		td->td_pcb->pcb_onfault = oldfault;
		return rv;
	}

	memcpy(dst, src, len);

	td->td_pcb->pcb_onfault = oldfault;
	return 0;
}

int
badaddr(addr, size)
	void *addr;
	size_t size;
{
	return badaddr_read(addr, size, NULL);
}

int
badaddr_read(addr, size, rptr)
	void *addr;
	size_t size;
	int *rptr;
{
	struct thread *td;
	faultbuf env;
	int x;

	/* Get rid of any stale machine checks that have been waiting.  */
	__asm __volatile ("sync; isync");

	td = PCPU_GET(curthread);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = 0;
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

	td->td_pcb->pcb_onfault = 0;
	__asm __volatile ("sync");	/* To be sure. */

	/* Use the value to avoid reorder. */
	if (rptr)
		*rptr = x;

	return 0;
}

/*
 * For now, this only deals with the particular unaligned access case
 * that gcc tends to generate.  Eventually it should handle all of the
 * possibilities that can happen on a 32-bit PowerPC in big-endian mode.
 */

static int
fix_unaligned(p, frame)
	struct proc *p;
	struct trapframe *frame;
{
	int indicator = EXC_ALI_OPCODE_INDICATOR(frame->dsisr);

	switch (indicator) {
	case EXC_ALI_LFD:
	case EXC_ALI_STFD:
#if 0
		{
			int reg = EXC_ALI_RST(frame->dsisr);
			double *fpr = &p->p_addr->u_pcb.pcb_fpu.fpr[reg];

			/* Juggle the FPU to ensure that we've initialized
			 * the FPRs, and that their current state is in
			 * the PCB.
			 */
			if (fpuproc != p) {
				if (fpuproc)
					save_fpu(fpuproc);
				enable_fpu(p);
			}
			save_fpu(p);

			if (indicator == EXC_ALI_LFD) {
				if (copyin((void *)frame->dar, fpr,
				    sizeof(double)) != 0)
					return -1;
				enable_fpu(p);
			} else {
				if (copyout(fpr, (void *)frame->dar,
				    sizeof(double)) != 0)
					return -1;
			}
			return 0;
		}
#endif
		break;
	}

	return -1;
}
