/*-
 * Copyright (c) 1994, Sean Eric Fagan
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
 *	This product includes software developed by Sean Eric Fagan.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ptrace.h>
#include <sys/sx.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>

#include <machine/reg.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>

/*
 * Functions implemented using PROC_ACTION():
 *
 * proc_read_regs(proc, regs)
 *	Get the current user-visible register set from the process
 *	and copy it into the regs structure (<machine/reg.h>).
 *	The process is stopped at the time read_regs is called.
 *
 * proc_write_regs(proc, regs)
 *	Update the current register set from the passed in regs
 *	structure.  Take care to avoid clobbering special CPU
 *	registers or privileged bits in the PSL.
 *	Depending on the architecture this may have fix-up work to do,
 *	especially if the IAR or PCW are modified.
 *	The process is stopped at the time write_regs is called.
 *
 * proc_read_fpregs, proc_write_fpregs
 *	deal with the floating point register set, otherwise as above.
 *
 * proc_read_dbregs, proc_write_dbregs
 *	deal with the processor debug register set, otherwise as above.
 *
 * proc_sstep(proc)
 *	Arrange for the process to trap after executing a single instruction.
 */

#define	PROC_ACTION(action) do {					\
	int error;							\
									\
	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);			\
	if ((td->td_proc->p_sflag & PS_INMEM) == 0)			\
		error = EIO;						\
	else								\
		error = (action);					\
	return (error);							\
} while(0)

int
proc_read_regs(struct thread *td, struct reg *regs)
{

	PROC_ACTION(fill_regs(td, regs));
}

int
proc_write_regs(struct thread *td, struct reg *regs)
{

	PROC_ACTION(set_regs(td, regs));
}

int
proc_read_dbregs(struct thread *td, struct dbreg *dbregs)
{

	PROC_ACTION(fill_dbregs(td, dbregs));
}

int
proc_write_dbregs(struct thread *td, struct dbreg *dbregs)
{

	PROC_ACTION(set_dbregs(td, dbregs));
}

/*
 * Ptrace doesn't support fpregs at all, and there are no security holes
 * or translations for fpregs, so we can just copy them.
 */
int
proc_read_fpregs(struct thread *td, struct fpreg *fpregs)
{

	PROC_ACTION(fill_fpregs(td, fpregs));
}

int
proc_write_fpregs(struct thread *td, struct fpreg *fpregs)
{

	PROC_ACTION(set_fpregs(td, fpregs));
}

int
proc_sstep(struct thread *td)
{

	PROC_ACTION(ptrace_single_step(td));
}

int
proc_rwmem(struct proc *p, struct uio *uio)
{
	struct vmspace *vm;
	vm_map_t map;
	vm_object_t backing_object, object = NULL;
	vm_offset_t pageno = 0;		/* page number */
	vm_prot_t reqprot;
	int error, refcnt, writing;

	/*
	 * if the vmspace is in the midst of being deallocated or the
	 * process is exiting, don't try to grab anything.  The page table
	 * usage in that process can be messed up.
	 */
	vm = p->p_vmspace;
	if ((p->p_flag & P_WEXIT))
		return (EFAULT);
	do {
		if ((refcnt = vm->vm_refcnt) < 1)
			return (EFAULT);
	} while (!atomic_cmpset_int(&vm->vm_refcnt, refcnt, refcnt + 1));

	/*
	 * The map we want...
	 */
	map = &vm->vm_map;

	writing = uio->uio_rw == UIO_WRITE;
	reqprot = writing ? (VM_PROT_WRITE | VM_PROT_OVERRIDE_WRITE) :
	    VM_PROT_READ;

	/*
	 * Only map in one page at a time.  We don't have to, but it
	 * makes things easier.  This way is trivial - right?
	 */
	do {
		vm_map_t tmap;
		vm_offset_t uva;
		int page_offset;		/* offset into page */
		vm_map_entry_t out_entry;
		vm_prot_t out_prot;
		boolean_t wired;
		vm_pindex_t pindex;
		u_int len;
		vm_page_t m;

		object = NULL;

		uva = (vm_offset_t)uio->uio_offset;

		/*
		 * Get the page number of this segment.
		 */
		pageno = trunc_page(uva);
		page_offset = uva - pageno;

		/*
		 * How many bytes to copy
		 */
		len = min(PAGE_SIZE - page_offset, uio->uio_resid);

		/*
		 * Fault the page on behalf of the process
		 */
		error = vm_fault(map, pageno, reqprot, VM_FAULT_NORMAL);
		if (error) {
			error = EFAULT;
			break;
		}

		/*
		 * Now we need to get the page.  out_entry, out_prot, wired,
		 * and single_use aren't used.  One would think the vm code
		 * would be a *bit* nicer...  We use tmap because
		 * vm_map_lookup() can change the map argument.
		 */
		tmap = map;
		error = vm_map_lookup(&tmap, pageno, reqprot, &out_entry,
		    &object, &pindex, &out_prot, &wired);
		if (error) {
			error = EFAULT;
			break;
		}
		VM_OBJECT_LOCK(object);
		while ((m = vm_page_lookup(object, pindex)) == NULL &&
		    !writing &&
		    (backing_object = object->backing_object) != NULL) {
			/*
			 * Allow fallback to backing objects if we are reading.
			 */
			VM_OBJECT_LOCK(backing_object);
			pindex += OFF_TO_IDX(object->backing_object_offset);
			VM_OBJECT_UNLOCK(object);
			object = backing_object;
		}
		VM_OBJECT_UNLOCK(object);
		if (m == NULL) {
			vm_map_lookup_done(tmap, out_entry);
			error = EFAULT;
			break;
		}

		/*
		 * Hold the page in memory.
		 */
		vm_page_lock_queues();
		vm_page_hold(m);
		vm_page_unlock_queues();

		/*
		 * We're done with tmap now.
		 */
		vm_map_lookup_done(tmap, out_entry);

		/*
		 * Now do the i/o move.
		 */
		error = uiomove_fromphys(&m, page_offset, len, uio);

		/*
		 * Release the page.
		 */
		vm_page_lock_queues();
		vm_page_unhold(m);
		vm_page_unlock_queues();

	} while (error == 0 && uio->uio_resid > 0);

	vmspace_free(vm);
	return (error);
}

/*
 * Process debugging system call.
 */
#ifndef _SYS_SYSPROTO_H_
struct ptrace_args {
	int	req;
	pid_t	pid;
	caddr_t	addr;
	int	data;
};
#endif

/*
 * MPSAFE
 */
int
ptrace(struct thread *td, struct ptrace_args *uap)
{
	/*
	 * XXX this obfuscation is to reduce stack usage, but the register
	 * structs may be too large to put on the stack anyway.
	 */
	union {
		struct ptrace_io_desc piod;
		struct ptrace_lwpinfo pl;
		struct dbreg dbreg;
		struct fpreg fpreg;
		struct reg reg;
	} r;
	void *addr;
	int error = 0;

	addr = &r;
	switch (uap->req) {
	case PT_GETREGS:
	case PT_GETFPREGS:
	case PT_GETDBREGS:
	case PT_LWPINFO:
		break;
	case PT_SETREGS:
		error = copyin(uap->addr, &r.reg, sizeof r.reg);
		break;
	case PT_SETFPREGS:
		error = copyin(uap->addr, &r.fpreg, sizeof r.fpreg);
		break;
	case PT_SETDBREGS:
		error = copyin(uap->addr, &r.dbreg, sizeof r.dbreg);
		break;
	case PT_IO:
		error = copyin(uap->addr, &r.piod, sizeof r.piod);
		break;
	default:
		addr = uap->addr;
		break;
	}
	if (error)
		return (error);

	error = kern_ptrace(td, uap->req, uap->pid, addr, uap->data);
	if (error)
		return (error);

	switch (uap->req) {
	case PT_IO:
		(void)copyout(&r.piod, uap->addr, sizeof r.piod);
		break;
	case PT_GETREGS:
		error = copyout(&r.reg, uap->addr, sizeof r.reg);
		break;
	case PT_GETFPREGS:
		error = copyout(&r.fpreg, uap->addr, sizeof r.fpreg);
		break;
	case PT_GETDBREGS:
		error = copyout(&r.dbreg, uap->addr, sizeof r.dbreg);
		break;
	case PT_LWPINFO:
		error = copyout(&r.pl, uap->addr, uap->data);
		break;
	}

	return (error);
}

int
kern_ptrace(struct thread *td, int req, pid_t pid, void *addr, int data)
{
	struct iovec iov;
	struct uio uio;
	struct proc *curp, *p, *pp;
	struct thread *td2 = NULL;
	struct ptrace_io_desc *piod;
	struct ptrace_lwpinfo *pl;
	int error, write, tmp, num;
	int proctree_locked = 0;
	lwpid_t tid = 0, *buf;
	pid_t saved_pid = pid;

	curp = td->td_proc;

	/* Lock proctree before locking the process. */
	switch (req) {
	case PT_TRACE_ME:
	case PT_ATTACH:
	case PT_STEP:
	case PT_CONTINUE:
	case PT_TO_SCE:
	case PT_TO_SCX:
	case PT_SYSCALL:
	case PT_DETACH:
		sx_xlock(&proctree_lock);
		proctree_locked = 1;
		break;
	default:
		break;
	}

	write = 0;
	if (req == PT_TRACE_ME) {
		p = td->td_proc;
		PROC_LOCK(p);
	} else {
		if (pid <= PID_MAX) {
			if ((p = pfind(pid)) == NULL) {
				if (proctree_locked)
					sx_xunlock(&proctree_lock);
				return (ESRCH);
			}
		} else {
			/* this is slow, should be optimized */
			sx_slock(&allproc_lock);
			FOREACH_PROC_IN_SYSTEM(p) {
				PROC_LOCK(p);
				mtx_lock_spin(&sched_lock);
				FOREACH_THREAD_IN_PROC(p, td2) {
					if (td2->td_tid == pid)
						break;
				}
				mtx_unlock_spin(&sched_lock);
				if (td2 != NULL)
					break; /* proc lock held */
				PROC_UNLOCK(p);
			}
			sx_sunlock(&allproc_lock);
			if (p == NULL) {
				if (proctree_locked)
					sx_xunlock(&proctree_lock);
				return (ESRCH);
			}
			tid = pid;
			pid = p->p_pid;
		}
	}
	if ((error = p_cansee(td, p)) != 0)
		goto fail;

	if ((error = p_candebug(td, p)) != 0)
		goto fail;

	/*
	 * System processes can't be debugged.
	 */
	if ((p->p_flag & P_SYSTEM) != 0) {
		error = EINVAL;
		goto fail;
	}

	if (tid == 0) {
		td2 = FIRST_THREAD_IN_PROC(p);
		tid = td2->td_tid;
	}

	/*
	 * Permissions check
	 */
	switch (req) {
	case PT_TRACE_ME:
		/* Always legal. */
		break;

	case PT_ATTACH:
		/* Self */
		if (p->p_pid == td->td_proc->p_pid) {
			error = EINVAL;
			goto fail;
		}

		/* Already traced */
		if (p->p_flag & P_TRACED) {
			error = EBUSY;
			goto fail;
		}

		/* Can't trace an ancestor if you're being traced. */
		if (curp->p_flag & P_TRACED) {
			for (pp = curp->p_pptr; pp != NULL; pp = pp->p_pptr) {
				if (pp == p) {
					error = EINVAL;
					goto fail;
				}
			}
		}


		/* OK */
		break;

	case PT_CLEARSTEP:
		/* Allow thread to clear single step for itself */
		if (td->td_tid == tid)
			break;

		/* FALLTHROUGH */
	default:
		/* not being traced... */
		if ((p->p_flag & P_TRACED) == 0) {
			error = EPERM;
			goto fail;
		}

		/* not being traced by YOU */
		if (p->p_pptr != td->td_proc) {
			error = EBUSY;
			goto fail;
		}

		/* not currently stopped */
		if (!P_SHOULDSTOP(p) || p->p_suspcount != p->p_numthreads ||
		    (p->p_flag & P_WAITED) == 0) {
			error = EBUSY;
			goto fail;
		}

		/* OK */
		break;
	}

#ifdef FIX_SSTEP
	/*
	 * Single step fixup ala procfs
	 */
	FIX_SSTEP(td2);			/* XXXKSE */
#endif

	/*
	 * Actually do the requests
	 */

	td->td_retval[0] = 0;

	switch (req) {
	case PT_TRACE_ME:
		/* set my trace flag and "owner" so it can read/write me */
		p->p_flag |= P_TRACED;
		p->p_oppid = p->p_pptr->p_pid;
		PROC_UNLOCK(p);
		sx_xunlock(&proctree_lock);
		return (0);

	case PT_ATTACH:
		/* security check done above */
		p->p_flag |= P_TRACED;
		p->p_oppid = p->p_pptr->p_pid;
		if (p->p_pptr != td->td_proc)
			proc_reparent(p, td->td_proc);
		data = SIGSTOP;
		goto sendsig;	/* in PT_CONTINUE below */

	case PT_CLEARSTEP:
		_PHOLD(p);
		error = ptrace_clear_single_step(td2);
		_PRELE(p);
		if (error)
			goto fail;
		PROC_UNLOCK(p);
		return (0);

	case PT_SETSTEP:
		_PHOLD(p);
		error = ptrace_single_step(td2);
		_PRELE(p);
		if (error)
			goto fail;
		PROC_UNLOCK(p);
		return (0);

	case PT_SUSPEND:
		_PHOLD(p);
		mtx_lock_spin(&sched_lock);
		td2->td_flags |= TDF_DBSUSPEND;
		mtx_unlock_spin(&sched_lock);
		_PRELE(p);
		PROC_UNLOCK(p);
		return (0);

	case PT_RESUME:
		_PHOLD(p);
		mtx_lock_spin(&sched_lock);
		td2->td_flags &= ~TDF_DBSUSPEND;
		mtx_unlock_spin(&sched_lock);
		_PRELE(p);
		PROC_UNLOCK(p);
		return (0);

	case PT_STEP:
	case PT_CONTINUE:
	case PT_TO_SCE:
	case PT_TO_SCX:
	case PT_SYSCALL:
	case PT_DETACH:
		/* Zero means do not send any signal */
		if (data < 0 || data > _SIG_MAXSIG) {
			error = EINVAL;
			goto fail;
		}

		_PHOLD(p);

		switch (req) {
		case PT_STEP:
			PROC_UNLOCK(p);
			error = ptrace_single_step(td2);
			if (error) {
				PRELE(p);
				goto fail_noproc;
			}
			PROC_LOCK(p);
			break;
		case PT_TO_SCE:
			p->p_stops |= S_PT_SCE;
			break;
		case PT_TO_SCX:
			p->p_stops |= S_PT_SCX;
			break;
		case PT_SYSCALL:
			p->p_stops |= S_PT_SCE | S_PT_SCX;
			break;
		}

		if (addr != (void *)1) {
			PROC_UNLOCK(p);
			error = ptrace_set_pc(td2, (u_long)(uintfptr_t)addr);
			if (error) {
				PRELE(p);
				goto fail_noproc;
			}
			PROC_LOCK(p);
		}
		_PRELE(p);

		if (req == PT_DETACH) {
			/* reset process parent */
			if (p->p_oppid != p->p_pptr->p_pid) {
				struct proc *pp;

				PROC_UNLOCK(p);
				pp = pfind(p->p_oppid);
				if (pp == NULL)
					pp = initproc;
				else
					PROC_UNLOCK(pp);
				PROC_LOCK(p);
				proc_reparent(p, pp);
				if (pp == initproc)
					p->p_sigparent = SIGCHLD;
			}
			p->p_flag &= ~(P_TRACED | P_WAITED);
			p->p_oppid = 0;

			/* should we send SIGCHLD? */
		}

	sendsig:
		if (proctree_locked)
			sx_xunlock(&proctree_lock);
		/* deliver or queue signal */
		if (P_SHOULDSTOP(p)) {
			p->p_xstat = data;
			p->p_flag &= ~(P_STOPPED_TRACE|P_STOPPED_SIG);
			mtx_lock_spin(&sched_lock);
			if (saved_pid <= PID_MAX) {
				p->p_xthread->td_flags &= ~TDF_XSIG;
				p->p_xthread->td_xsig = data;
			} else {
				td2->td_flags &= ~TDF_XSIG;
				td2->td_xsig = data;
			}
			p->p_xthread = NULL;
			if (req == PT_DETACH) {
				struct thread *td3;
				FOREACH_THREAD_IN_PROC(p, td3)
					td3->td_flags &= ~TDF_DBSUSPEND; 
			}
			/*
			 * unsuspend all threads, to not let a thread run,
			 * you should use PT_SUSPEND to suspend it before
			 * continuing process.
			 */
			thread_unsuspend(p);
			thread_continued(p);
			mtx_unlock_spin(&sched_lock);
		} else if (data) {
			psignal(p, data);
		}
		PROC_UNLOCK(p);

		return (0);

	case PT_WRITE_I:
	case PT_WRITE_D:
		write = 1;
		/* FALLTHROUGH */
	case PT_READ_I:
	case PT_READ_D:
		PROC_UNLOCK(p);
		tmp = 0;
		/* write = 0 set above */
		iov.iov_base = write ? (caddr_t)&data : (caddr_t)&tmp;
		iov.iov_len = sizeof(int);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = (off_t)(uintptr_t)addr;
		uio.uio_resid = sizeof(int);
		uio.uio_segflg = UIO_SYSSPACE;	/* i.e.: the uap */
		uio.uio_rw = write ? UIO_WRITE : UIO_READ;
		uio.uio_td = td;
		error = proc_rwmem(p, &uio);
		if (uio.uio_resid != 0) {
			/*
			 * XXX proc_rwmem() doesn't currently return ENOSPC,
			 * so I think write() can bogusly return 0.
			 * XXX what happens for short writes?  We don't want
			 * to write partial data.
			 * XXX proc_rwmem() returns EPERM for other invalid
			 * addresses.  Convert this to EINVAL.  Does this
			 * clobber returns of EPERM for other reasons?
			 */
			if (error == 0 || error == ENOSPC || error == EPERM)
				error = EINVAL;	/* EOF */
		}
		if (!write)
			td->td_retval[0] = tmp;
		return (error);

	case PT_IO:
		PROC_UNLOCK(p);
		piod = addr;
		iov.iov_base = piod->piod_addr;
		iov.iov_len = piod->piod_len;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = (off_t)(uintptr_t)piod->piod_offs;
		uio.uio_resid = piod->piod_len;
		uio.uio_segflg = UIO_USERSPACE;
		uio.uio_td = td;
		switch (piod->piod_op) {
		case PIOD_READ_D:
		case PIOD_READ_I:
			uio.uio_rw = UIO_READ;
			break;
		case PIOD_WRITE_D:
		case PIOD_WRITE_I:
			uio.uio_rw = UIO_WRITE;
			break;
		default:
			return (EINVAL);
		}
		error = proc_rwmem(p, &uio);
		piod->piod_len -= uio.uio_resid;
		return (error);

	case PT_KILL:
		data = SIGKILL;
		goto sendsig;	/* in PT_CONTINUE above */

	case PT_SETREGS:
		_PHOLD(p);
		error = proc_write_regs(td2, addr);
		_PRELE(p);
		PROC_UNLOCK(p);
		return (error);

	case PT_GETREGS:
		_PHOLD(p);
		error = proc_read_regs(td2, addr);
		_PRELE(p);
		PROC_UNLOCK(p);
		return (error);

	case PT_SETFPREGS:
		_PHOLD(p);
		error = proc_write_fpregs(td2, addr);
		_PRELE(p);
		PROC_UNLOCK(p);
		return (error);

	case PT_GETFPREGS:
		_PHOLD(p);
		error = proc_read_fpregs(td2, addr);
		_PRELE(p);
		PROC_UNLOCK(p);
		return (error);

	case PT_SETDBREGS:
		_PHOLD(p);
		error = proc_write_dbregs(td2, addr);
		_PRELE(p);
		PROC_UNLOCK(p);
		return (error);

	case PT_GETDBREGS:
		_PHOLD(p);
		error = proc_read_dbregs(td2, addr);
		_PRELE(p);
		PROC_UNLOCK(p);
		return (error);

	case PT_LWPINFO:
		if (data == 0 || data > sizeof(*pl))
			return (EINVAL);
		pl = addr;
		_PHOLD(p);
		if (saved_pid <= PID_MAX) {
			pl->pl_lwpid = p->p_xthread->td_tid;
			pl->pl_event = PL_EVENT_SIGNAL;
		} else {
			pl->pl_lwpid = td2->td_tid;
			if (td2->td_flags & TDF_XSIG)
				pl->pl_event = PL_EVENT_SIGNAL;
			else
				pl->pl_event = 0;
		}
		if (td2->td_pflags & TDP_SA) {
			pl->pl_flags = PL_FLAG_SA;
			if (td2->td_upcall && !TD_CAN_UNBIND(td2))
				pl->pl_flags |= PL_FLAG_BOUND;
		} else {
			pl->pl_flags = 0;
		}
		_PRELE(p);
		PROC_UNLOCK(p);
		return (0);

	case PT_GETNUMLWPS:
		td->td_retval[0] = p->p_numthreads;
		PROC_UNLOCK(p);
		return (0);

	case PT_GETLWPLIST:
		if (data <= 0) {
			PROC_UNLOCK(p);
			return (EINVAL);
		}
		num = imin(p->p_numthreads, data);
		PROC_UNLOCK(p);
		buf = malloc(num * sizeof(lwpid_t), M_TEMP, M_WAITOK);
		tmp = 0;
		PROC_LOCK(p);
		mtx_lock_spin(&sched_lock);
		FOREACH_THREAD_IN_PROC(p, td2) {
			if (tmp >= num)
				break;
			buf[tmp++] = td2->td_tid;
		}
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
		error = copyout(buf, addr, tmp * sizeof(lwpid_t));
		free(buf, M_TEMP);
		if (!error)
			td->td_retval[0] = num;
 		return (error);

	default:
#ifdef __HAVE_PTRACE_MACHDEP
		if (req >= PT_FIRSTMACH) {
			_PHOLD(p);
			PROC_UNLOCK(p);
			error = cpu_ptrace(td2, req, addr, data);
			PRELE(p);
			return (error);
		}
#endif
		break;
	}

	/* Unknown request. */
	error = EINVAL;

fail:
	PROC_UNLOCK(p);
fail_noproc:
	if (proctree_locked)
		sx_xunlock(&proctree_lock);
	return (error);
}

/*
 * Stop a process because of a debugging event;
 * stay stopped until p->p_step is cleared
 * (cleared by PIOCCONT in procfs).
 */
void
stopevent(struct proc *p, unsigned int event, unsigned int val)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	p->p_step = 1;
	do {
		p->p_xstat = val;
		p->p_xthread = NULL;
		p->p_stype = event;	/* Which event caused the stop? */
		wakeup(&p->p_stype);	/* Wake up any PIOCWAIT'ing procs */
		msleep(&p->p_step, &p->p_mtx, PWAIT, "stopevent", 0);
	} while (p->p_step);
}
