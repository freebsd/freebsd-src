/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 *	from: @(#)sys_process.c	7.22 (Berkeley) 5/11/91
 *	$Id: sys_process.c,v 1.5 1993/10/16 15:24:48 rgrimes Exp $
 */

#include <stddef.h>

#define IPCREG
#include "param.h"
#include "proc.h"
#include "vnode.h"
#include "buf.h"
#include "ptrace.h"

#include "machine/eflags.h"
#include "machine/reg.h"
#include "machine/psl.h"
#include "vm/vm.h"
#include "vm/vm_page.h"

#include "user.h"

/*
 * NOTES.
 *
 * The following ptrace calls have been defined in addition to
 * the standard ones found in original <sys/ptrace.h>:
 *
 * PT_ATTACH		-	attach to running process
 * PT_DETACH		-	detach from running process
 * PT_SYSCALL		-	trace system calls
 * PT_GETREG		-	get register file
 * PT_SETREG		-	set register file
 * PT_BREAD_[IDU]	-	block read from process (not yet implemented)
 * PT_BWRITE_[IDU]	-	block write	"		"
 * PT_INHERIT		-	make forked processes inherit trace flags
 *
 */

/* Define to prevent extraneous clutter in source */
#ifndef SSTRC
#define	SSTRC	0
#endif
#ifndef SFTRC
#define	SFTRC	0
#endif

/*
 * `ipcreg' defined in <machine/reg.h>
 * Should we define a structure with all regs?
 */
int sipcreg[NIPCREG] =
  { 0,0,sEDI,sESI,sEBP,sEBX,sEDX,sECX,sEAX,sEIP,sCS,sEFLAGS,sESP,sSS };

struct {
	int	flag;
#define IPC_BUSY	1
#define IPC_WANT	2
#define IPC_DONE	4
	int	req;			/* copy of ptrace request */
	int	*addr;			/* copy of ptrace address */
	int	data;			/* copy of ptrace data */
	int	error;			/* errno from `procxmt' */
	int	regs[NIPCREG];		/* PT_[GS]ETREG */
	caddr_t	buf;			/* PT_BREAD/WRITE */
	int	buflen;			/* 	"	*/
} ipc;

/*
 * Process debugging system call.
 */

struct ptrace_args {
	int	req;
	int	pid;
	int	*addr;
	int	data;
};

ptrace(curp, uap, retval)
	struct proc *curp;
	register struct ptrace_args *uap;
	int *retval;
{
	struct proc *p;
	int s, error = 0;

	*retval = 0;
	if (uap->req == PT_TRACE_ME) {
		curp->p_flag |= STRC;
		/*p->p_tptr = p->p_pptr; * What shall we do here ? */
		return 0;
	}
	if ((p = pfind(uap->pid)) == NULL) {
		return ESRCH;
	}

#ifdef notyet
	if (uap->req != PT_ATTACH && (
			(p->p_flag & STRC) == 0 ||
			(p->p_tptr && curp != p->p_tptr) ||
			(!p->p_tptr && curp != p->p_pptr)))

		return ESRCH;
#endif


#ifdef PT_ATTACH
	switch (uap->req) {
	case PT_ATTACH:
		if (curp->p_ucred->cr_uid != 0 && (
			curp->p_ucred->cr_uid != p->p_ucred->cr_uid ||
			curp->p_ucred->cr_uid != p->p_cred->p_svuid))
			return EACCES;

		p->p_tptr = curp;
		p->p_flag |= STRC;
		psignal(p, SIGTRAP);
		return 0;

	case PT_DETACH:
		if ((unsigned)uap->data >= NSIG)
			return EINVAL;
		p->p_flag &= ~(STRC|SSTRC|SFTRC);
		p->p_tptr = NULL;
		psignal(p->p_pptr, SIGCHLD);
		wakeup((caddr_t)p->p_pptr);
		s = splhigh();
		if (p->p_stat == SSTOP) {
			p->p_xstat = uap->data;
			setrun(p);
		} else if (uap->data) {
			psignal(p, uap->data);
		}
		splx(s);
		return 0;

#ifdef PT_INHERIT
	case PT_INHERIT:
		if ((p->p_flag & STRC) == 0)
			return ESRCH;
		p->p_flag |= SFTRC;
		return 0;
#endif

	default:
		break;
	}
#endif

	/* Other ptrace calls require target process to be in stopped state */
	if ((p->p_flag & STRC) == 0 || p->p_stat != SSTOP) {
		return ESRCH;
	}

	/* Acquire the ipc structure */
	while (ipc.flag & IPC_BUSY) {
		ipc.flag |= IPC_WANT;
		error = tsleep((caddr_t)&ipc, PWAIT|PCATCH, "ipc", 0);
		if (error)
			goto out;
	}

	/* Got it, fill it */
	ipc.flag = IPC_BUSY;
	ipc.error = 0;
	ipc.req = uap->req;
	ipc.addr = uap->addr;
	ipc.data = uap->data;

#ifdef PT_GETREGS
	switch (uap->req) {
	case PT_SETREGS:
		error = copyin((char *)ipc.addr, (char *)ipc.regs, sizeof(ipc.regs));
		if (error)
			goto out;
		break;

#ifdef notyet	/* requires change in number of args to ptrace syscall */
	case PT_BWRITE_I:
	case PT_BWRITE_D:
		ipc.buflen = uap->data;
		ipc.buf = kmem_alloc_wait(kernelmap, uap->data);
		error = copyin((char *)ipc.addr, (char *)ipc.buf, ipc.buflen);
		if (error) {
			kmem_free_wakeup(kernelmap, ipc.buf, ipc.buflen);
			goto out;
		}
#endif
	default:
		break;
	}
#endif

	setrun(p);
	while ((ipc.flag & IPC_DONE) == 0) {
		error = tsleep((caddr_t)&ipc, PWAIT|PCATCH, "ipc", 0);
		if (error)
			goto out;
	}

	*retval = ipc.data;
	if (error = ipc.error)
		goto out;

#ifdef PT_GETREGS
	switch (uap->req) {
	case PT_GETREGS:
		error = copyout((char *)ipc.regs, (char *)ipc.addr, sizeof(ipc.regs));
		break;

	case PT_BREAD_I:
	case PT_BREAD_D:
		/* Not yet */
	default:
		break;
	}
#endif

out:
	/* Release ipc structure */
	ipc.flag &= ~IPC_BUSY;
	if (ipc.flag & IPC_WANT) {
		ipc.flag &= ~IPC_WANT;
		wakeup((caddr_t)&ipc);
	}
	return error;
}

procxmt(p)
	register struct proc *p;
{
	int i, *xreg, rv = 0;
#ifdef i386
	int new_eflags, old_cs, old_ds, old_es, old_ss, old_eflags;
	int *regs;
#endif

	/* Are we still being traced? */
	if ((p->p_flag & STRC) == 0)
		return 1;

	p->p_addr->u_kproc.kp_proc = *p;
	fill_eproc(p, &p->p_addr->u_kproc.kp_eproc);

	switch (ipc.req) {
	case PT_READ_I:
	case PT_READ_D:
		if (!useracc(ipc.addr, sizeof(ipc.data), B_READ)) {
			ipc.error = EFAULT;
			break;
		}
		ipc.error = copyin((char *)ipc.addr, (char *)&ipc.data, sizeof(ipc.data));
		break;

	case PT_READ_U:
		if ((u_int)ipc.addr > UPAGES * NBPG - sizeof(int)) {
			ipc.error = EFAULT;
			break;
		}
		ipc.data = *(int *)((u_int)p->p_addr + (u_int)ipc.addr);
		break;

	case PT_WRITE_I:
	case PT_WRITE_D: {				/* 04 Sep 92*/
		vm_prot_t prot;		/* current protection of region */
		int cow;		/* ensure copy-on-write happens */

		if (cow = (useracc(ipc.addr, sizeof(ipc.data), B_WRITE) == 0)) {
			vm_offset_t	addr = (vm_offset_t)ipc.addr;
			vm_size_t	size;
			vm_prot_t	max_prot;
			vm_inherit_t	inh;
			boolean_t	shared;
			vm_object_t	object;
			vm_offset_t	objoff;

			/*
			 * XXX - the useracc check is stronger than the vm
			 * checks because the user page tables are in the map.
			 * Anyway, most of this can be removed now that COW
			 * works.
			 */
			if (!useracc(ipc.addr, sizeof(ipc.data), B_READ) ||
			    vm_region(&p->p_vmspace->vm_map, &addr, &size,
					&prot, &max_prot, &inh, &shared,
					&object, &objoff) != KERN_SUCCESS ||
			    vm_protect(&p->p_vmspace->vm_map, ipc.addr,
					sizeof(ipc.data), FALSE,
					prot|VM_PROT_WRITE) != KERN_SUCCESS ||
			    vm_fault(&p->p_vmspace->vm_map,trunc_page(ipc.addr),
					VM_PROT_WRITE, FALSE) != KERN_SUCCESS) {

				ipc.error = EFAULT;
				break;
			}
		}
		ipc.error = copyout((char *)&ipc.data,
					(char *)ipc.addr, sizeof(ipc.data));
		if (cow)
			if (vm_protect(&p->p_vmspace->vm_map, ipc.addr,
					sizeof(ipc.data), FALSE,
					prot) != KERN_SUCCESS)
				printf("ptrace: oops\n");
		break;
	}

	case PT_WRITE_U:
#ifdef i386
		regs = p->p_regs;
		/*
		 * XXX - privileged kernel state is scattered all over the
		 * user area.  Only allow write access to areas known to
		 * be safe.
		 */
#define	GO_IF_SAFE(min, size) \
		if ((u_int)ipc.addr >= (min) \
		    && (u_int)ipc.addr <= (min) + (size) - sizeof(int)) \
			goto pt_write_u
		/*
		 * Allow writing entire FPU state.
		 */
		GO_IF_SAFE(offsetof(struct user, u_pcb)
			   + offsetof(struct pcb, pcb_savefpu),
			   sizeof(struct save87));
		/*
		 * Allow writing ordinary registers.  Changes to segment
		 * registers and to some bits in %eflags will be silently
		 * ignored.  Such changes ought to be an error.
		 */
/*
 * XXX - there is no define for the base of the user area except USRSTACK.
 * XXX - USRSTACK is not the base of the user stack.  It is the base of the
 * user area.
 */
#define	USER_OFF(va)	((u_int)(va) - USRSTACK)
		GO_IF_SAFE(USER_OFF(regs),
			   (curpcb->pcb_flags & FM_TRAP ? tSS + 1 : sSS + 1)
			   * sizeof *regs);
		ipc.error = EFAULT;
		break;
#else
		if ((u_int)ipc.addr > UPAGES * NBPG - sizeof(int)) {
			ipc.error = EFAULT;
			break;
		}
#endif
	pt_write_u:
#ifdef i386
		if (curpcb->pcb_flags & FM_TRAP) {
			old_cs = regs[tCS];
			old_ds = regs[tES];
			old_es = regs[tES];
			old_ss = regs[tSS];
			old_eflags = regs[tEFLAGS];
		} else {
			old_cs = regs[sCS];
			old_ss = regs[sSS];
			old_eflags = regs[sEFLAGS];
		}
#endif
		*(int *)((u_int)p->p_addr + (u_int)ipc.addr) = ipc.data;
#ifdef i386
		/*
		 * Don't allow segment registers to change (although they can
		 * be changed directly to certain values).
		 * Don't allow privileged bits in %eflags to change.  Users
		 * have privilege to change TF and NT although although they
		 * usually shouldn't.
		 * XXX - fix PT_SETREGS.
		 * XXX - simplify.  Maybe copy through a temporary struct.
		 * Watch out for problems when ipc.addr is not a multiple
		 * of the register size.
		 */
#define EFL_UNPRIVILEGED (EFL_CF | EFL_PF | EFL_AF | EFL_ZF | EFL_SF \
			  | EFL_TF | EFL_DF | EFL_OF | EFL_NT)
		if (curpcb->pcb_flags & FM_TRAP) {
			regs[tCS] = old_cs;
			regs[tDS] = old_ds;
			regs[tES] = old_es;
			regs[tSS] = old_es;
			new_eflags = regs[tEFLAGS];
			regs[tEFLAGS]
				= (new_eflags & EFL_UNPRIVILEGED)
				  | (old_eflags & ~EFL_UNPRIVILEGED);
		} else {
			regs[sCS] = old_cs;
			regs[sSS] = old_ss;
			new_eflags = regs[sEFLAGS];
			regs[sEFLAGS]
				= (new_eflags & EFL_UNPRIVILEGED)
				  | (old_eflags & ~EFL_UNPRIVILEGED);
		}
#endif
		break;

	case PT_CONTINUE:
		if (ipc.addr != (int *)1) {
#ifdef i386
			p->p_regs[(curpcb->pcb_flags&FM_TRAP)?tEIP:sEIP] = (int)ipc.addr;
#endif
		}
		p->p_flag &= ~SSTRC;	/* Only set by PT_SYSCALL */
		if ((unsigned)ipc.data >= NSIG) {
			ipc.error = EINVAL;
		} else {
			p->p_xstat = ipc.data;
			rv = 1;
		}
		break;

	case PT_KILL:
		p->p_flag &= ~SSTRC;	/* Only set by PT_SYSCALL */
		rv = 2;
		break;

	case PT_STEP:
#ifdef i386
		if (ipc.addr != (int *)1) {
			p->p_regs[(curpcb->pcb_flags&FM_TRAP)?tEIP:sEIP] = (int)ipc.addr;
		}
		p->p_regs[(curpcb->pcb_flags&FM_TRAP)?tEFLAGS:sEFLAGS] |= PSL_T;
#endif
		p->p_flag &= ~SSTRC;	/* Only set by PT_SYSCALL */
		p->p_xstat = 0;
		rv = 1;
		break;

#ifdef PT_SYSCALL
	case PT_SYSCALL:
		if (ipc.addr != (int *)1) {
#ifdef i386
			p->p_regs[(curpcb->pcb_flags&FM_TRAP)?tEIP:sEIP] = (int)ipc.addr;
#endif
		}
		p->p_flag |= SSTRC;
		p->p_xstat = 0;
		rv = 1;
		break;
#endif
#ifdef PT_GETREGS
	case PT_GETREGS:
#ifdef i386
		xreg = (curpcb->pcb_flags&FM_TRAP)?ipcreg:sipcreg;
#endif

		for (i = 0; i < NIPCREG; i++)
			ipc.regs[i] = p->p_regs[xreg[i]];
		break;

	case PT_SETREGS:
#ifdef i386
		xreg = (curpcb->pcb_flags&FM_TRAP)?ipcreg:sipcreg;
#endif

		for (i = 0; i < NIPCREG; i++)
			p->p_regs[xreg[i]] = ipc.regs[i];
		break;
#endif

#ifdef PT_DUMP
	case PT_DUMP:
		/* Should be able to specify core file name */
		ipc.error = coredump(p);
		break;
#endif

	default:
		ipc.error = EINVAL;
	}
	ipc.flag |= IPC_DONE;
	wakeup((caddr_t)&ipc);

	if (rv == 2)
		kexit(p, 0); 	/*???*/

	return rv;
}

/*
 * Enable process profiling system call.
 */

struct profil_args {
	short	*bufbase;	/* base of data buffer */
	unsigned bufsize;	/* size of data buffer */
	unsigned pcoffset;	/* pc offset (for subtraction) */
	unsigned pcscale;	/* scaling factor for offset pc */
};

/* ARGSUSED */
profil(p, uap, retval)
	struct proc *p;
	register struct profil_args *uap;
	int *retval;
{
	/* from looking at man pages, and include files, looks like
	 * this just sets up the fields of p->p_stats->p_prof...
	 * and those fields come straight from the args.
	 * only thing *we* have to do is check the args for validity...
	 *
	 * cgd
	 */

	/* check to make sure that the buffer is OK.  addupc (in locore)
	 * checks for faults, but would one be generated, say, writing to
	 * kernel space?  probably not -- it just uses "movl"...
	 *
	 * so we've gotta check to make sure that the info set up for
	 * addupc is set right... it's gotta be writable by the user...
	 */

	if (useracc(uap->bufbase,uap->bufsize*sizeof(short),B_WRITE) == 0)
		return EFAULT;

	p->p_stats->p_prof.pr_base = uap->bufbase;
	p->p_stats->p_prof.pr_size = uap->bufsize;
	p->p_stats->p_prof.pr_off = uap->pcoffset;
	p->p_stats->p_prof.pr_scale = uap->pcscale;

	return 0;
}
