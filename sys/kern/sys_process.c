/*
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ptrace.h>
#include <sys/sx.h>
#include <sys/user.h>

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
	mtx_lock_spin(&sched_lock);					\
	if ((td->td_proc->p_sflag & PS_INMEM) == 0)			\
		error = EIO;						\
	else								\
		error = (action);					\
	mtx_unlock_spin(&sched_lock);					\
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
	vm_object_t object = NULL;
	vm_offset_t pageno = 0;		/* page number */
	vm_prot_t reqprot;
	vm_offset_t kva;
	int error, writing;

	GIANT_REQUIRED;

	/*
	 * if the vmspace is in the midst of being deallocated or the
	 * process is exiting, don't try to grab anything.  The page table
	 * usage in that process can be messed up.
	 */
	vm = p->p_vmspace;
	if ((p->p_flag & P_WEXIT))
		return (EFAULT);
	if (vm->vm_refcnt < 1)
		return (EFAULT);
	++vm->vm_refcnt;
	/*
	 * The map we want...
	 */
	map = &vm->vm_map;

	writing = uio->uio_rw == UIO_WRITE;
	reqprot = writing ? (VM_PROT_WRITE | VM_PROT_OVERRIDE_WRITE) :
	    VM_PROT_READ;

	kva = kmem_alloc_pageable(kernel_map, PAGE_SIZE);

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

			/*
			 * Make sure that there is no residue in 'object' from
			 * an error return on vm_map_lookup.
			 */
			object = NULL;

			break;
		}

		m = vm_page_lookup(object, pindex);

		/* Allow fallback to backing objects if we are reading */

		while (m == NULL && !writing && object->backing_object) {

			pindex += OFF_TO_IDX(object->backing_object_offset);
			object = object->backing_object;
			
			m = vm_page_lookup(object, pindex);
		}

		if (m == NULL) {
			error = EFAULT;

			/*
			 * Make sure that there is no residue in 'object' from
			 * an error return on vm_map_lookup.
			 */
			object = NULL;

			vm_map_lookup_done(tmap, out_entry);

			break;
		}

		/*
		 * Wire the page into memory
		 */
		vm_page_wire(m);

		/*
		 * We're done with tmap now.
		 * But reference the object first, so that we won't loose
		 * it.
		 */
		vm_object_reference(object);
		vm_map_lookup_done(tmap, out_entry);

		pmap_qenter(kva, &m, 1);

		/*
		 * Now do the i/o move.
		 */
		error = uiomove((caddr_t)(kva + page_offset), len, uio);

		pmap_qremove(kva, 1);

		/*
		 * release the page and the object
		 */
		vm_page_unwire(m, 1);
		vm_object_deallocate(object);

		object = NULL;

	} while (error == 0 && uio->uio_resid > 0);

	if (object)
		vm_object_deallocate(object);

	kmem_free(kernel_map, kva, PAGE_SIZE);
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

int
ptrace(struct thread *td, struct ptrace_args *uap)
{
	struct iovec iov;
	struct uio uio;
	/*
	 * XXX this obfuscation is to reduce stack usage, but the register
	 * structs may be too large to put on the stack anyway.
	 */
	union {
		struct ptrace_io_desc piod;
		struct dbreg dbreg;
		struct fpreg fpreg;
		struct reg reg;
	} r;
	struct proc *curp, *p, *pp;
	struct thread *td2;
	int error, write;
	int proctree_locked = 0;

	curp = td->td_proc;

	/*
	 * Do copyin() early before getting locks and lock proctree before
	 * locking the process.
	 */
	switch (uap->req) {
	case PT_TRACE_ME:
	case PT_ATTACH:
	case PT_STEP:
	case PT_CONTINUE:
	case PT_DETACH:
		sx_xlock(&proctree_lock);
		proctree_locked = 1;
		break;
#ifdef PT_SETREGS
	case PT_SETREGS:
		error = copyin(uap->addr, &r.reg, sizeof r.reg);
		if (error)
			return (error);
		break;
#endif /* PT_SETREGS */
#ifdef PT_SETFPREGS
	case PT_SETFPREGS:
		error = copyin(uap->addr, &r.fpreg, sizeof r.fpreg);
		if (error)
			return (error);
		break;
#endif /* PT_SETFPREGS */
#ifdef PT_SETDBREGS
	case PT_SETDBREGS:
		error = copyin(uap->addr, &r.dbreg, sizeof r.dbreg);
		if (error)
			return (error);
		break;
#endif /* PT_SETDBREGS */
	default:
	}
		
	write = 0;
	if (uap->req == PT_TRACE_ME) {
		p = td->td_proc;
		PROC_LOCK(p);
	} else {
		if ((p = pfind(uap->pid)) == NULL) {
			if (proctree_locked)
				sx_xunlock(&proctree_lock);
			return (ESRCH);
		}
	}
	if (p_cansee(td->td_proc, p)) {
		error = ESRCH;
		goto fail;
	}

	if ((error = p_candebug(td->td_proc, p)) != 0)
		goto fail;

	/*
	 * System processes can't be debugged.
	 */
	if ((p->p_flag & P_SYSTEM) != 0) {
		error = EINVAL;
		goto fail;
	}
	
	/*
	 * Permissions check
	 */
	switch (uap->req) {
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

	case PT_READ_I:
	case PT_READ_D:
	case PT_WRITE_I:
	case PT_WRITE_D:
	case PT_IO:
	case PT_CONTINUE:
	case PT_KILL:
	case PT_STEP:
	case PT_DETACH:
	case PT_GETREGS:
	case PT_SETREGS:
	case PT_GETFPREGS:
	case PT_SETFPREGS:
	case PT_GETDBREGS:
	case PT_SETDBREGS:
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
		if (p->p_stat != SSTOP || (p->p_flag & P_WAITED) == 0) {
			error = EBUSY;
			goto fail;
		}

		/* OK */
		break;

	default:
		error = EINVAL;
		goto fail;
	}

	td2 = FIRST_THREAD_IN_PROC(p);
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

	switch (uap->req) {
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
		uap->data = SIGSTOP;
		goto sendsig;	/* in PT_CONTINUE below */

	case PT_STEP:
	case PT_CONTINUE:
	case PT_DETACH:
		/* XXX uap->data is used even in the PT_STEP case. */
		if ((uap->req != PT_STEP) && ((unsigned)uap->data >= NSIG)) {
			error = EINVAL;
			goto fail;
		}

		_PHOLD(p);

		if (uap->req == PT_STEP) {
			error = ptrace_single_step(td2);
			if (error) {
				_PRELE(p);
				goto fail;
			}
		}

		if (uap->addr != (caddr_t)1) {
			fill_kinfo_proc(p, &p->p_uarea->u_kproc);
			error = ptrace_set_pc(td2,
			    (u_long)(uintfptr_t)uap->addr);
			if (error) {
				_PRELE(p);
				goto fail;
			}
		}
		_PRELE(p);

		if (uap->req == PT_DETACH) {
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
			}
			p->p_flag &= ~(P_TRACED | P_WAITED);
			p->p_oppid = 0;

			/* should we send SIGCHLD? */
		}

	sendsig:
		if (proctree_locked)
			sx_xunlock(&proctree_lock);
		/* deliver or queue signal */
		if (p->p_stat == SSTOP) {
			p->p_xstat = uap->data;
			mtx_lock_spin(&sched_lock);
			setrunnable(td2);	/* XXXKSE */
			mtx_unlock_spin(&sched_lock);
		} else if (uap->data)		      
			psignal(p, uap->data);
		PROC_UNLOCK(p);
		
		return (0);

	case PT_WRITE_I:
	case PT_WRITE_D:
		write = 1;
		/* fallthrough */
	case PT_READ_I:
	case PT_READ_D:
		PROC_UNLOCK(p);
		/* write = 0 set above */
		iov.iov_base = write ? (caddr_t)&uap->data :
		    (caddr_t)td->td_retval;
		iov.iov_len = sizeof(int);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = (off_t)(uintptr_t)uap->addr;
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
		return (error);

	case PT_IO:
		error = copyin(uap->addr, &r.piod, sizeof r.piod);
		if (error)
			return (error);
		iov.iov_base = r.piod.piod_addr;
		iov.iov_len = r.piod.piod_len;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = (off_t)(uintptr_t)r.piod.piod_offs;
		uio.uio_resid = r.piod.piod_len;
		uio.uio_segflg = UIO_USERSPACE;
		uio.uio_td = td;
		switch (r.piod.piod_op) {
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
		r.piod.piod_len -= uio.uio_resid;
		(void)copyout(&r.piod, uap->addr, sizeof r.piod);
		return (error);

	case PT_KILL:
		uap->data = SIGKILL;
		goto sendsig;	/* in PT_CONTINUE above */

	case PT_SETREGS:
		_PHOLD(p);
		error = proc_write_regs(td2, &r.reg);
		_PRELE(p);
		PROC_UNLOCK(p);
		return (error);

	case PT_GETREGS:
		_PHOLD(p);
		error = proc_read_regs(td2, &r.reg);
		_PRELE(p);
		PROC_UNLOCK(p);
		if (error == 0)
			error = copyout(&r.reg, uap->addr, sizeof r.reg);
		return (error);

	case PT_SETFPREGS:
		_PHOLD(p);
		error = proc_write_fpregs(td2, &r.fpreg);
		_PRELE(p);
		PROC_UNLOCK(p);
		return (error);

	case PT_GETFPREGS:
		_PHOLD(p);
		error = proc_read_fpregs(td2, &r.fpreg);
		_PRELE(p);
		PROC_UNLOCK(p);
		if (error == 0)
			error = copyout(&r.fpreg, uap->addr, sizeof r.fpreg);
		return (error);

	case PT_SETDBREGS:
		_PHOLD(p);
		error = proc_write_dbregs(td2, &r.dbreg);
		_PRELE(p);
		PROC_UNLOCK(p);
		return (error);

	case PT_GETDBREGS:
		_PHOLD(p);
		error = proc_read_dbregs(td2, &r.dbreg);
		_PRELE(p);
		PROC_UNLOCK(p);
		if (error == 0)
			error = copyout(&r.dbreg, uap->addr, sizeof r.dbreg);
		return (error);

	default:
		KASSERT(0, ("unreachable code\n"));
		break;
	}

	KASSERT(0, ("unreachable code\n"));
	return (0);

fail:
	PROC_UNLOCK(p);
	if (proctree_locked)
		sx_xunlock(&proctree_lock);
	return (error);
}

int
trace_req(struct proc *p)
{

	return (1);
}

/*
 * Stop a process because of a debugging event;
 * stay stopped until p->p_step is cleared
 * (cleared by PIOCCONT in procfs).
 */
void
stopevent(struct proc *p, unsigned int event, unsigned int val)
{

	PROC_LOCK_ASSERT(p, MA_OWNED | MA_NOTRECURSED);
	p->p_step = 1;

	do {
		p->p_xstat = val;
		p->p_stype = event;	/* Which event caused the stop? */
		wakeup(&p->p_stype);	/* Wake up any PIOCWAIT'ing procs */
		msleep(&p->p_step, &p->p_mtx, PWAIT, "stopevent", 0);
	} while (p->p_step);
}
