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
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>

#define	PROC_REG_ACTION(name, action, type)				\
int									\
proc_##name##_##type##s(struct thread *td, struct type *regs)		\
{									\
	int error;							\
									\
	mtx_lock_spin(&sched_lock);					\
	error = (action##_##type##s(td, regs));				\
	mtx_unlock_spin(&sched_lock);					\
	return (error);							\
}

PROC_REG_ACTION(read, fill, reg);
PROC_REG_ACTION(write, set, reg);
PROC_REG_ACTION(read, fill, dbreg);
PROC_REG_ACTION(write, set, dbreg);
PROC_REG_ACTION(read, fill, fpreg);
PROC_REG_ACTION(write, set, fpreg);

int
proc_sstep(struct thread *td)
{
	int error;

	mtx_lock_spin(&sched_lock);
	error = ptrace_single_step(td);
	mtx_unlock_spin(&sched_lock);
	return (error);
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
	int error;
	int writing;

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

		pmap_kenter(kva, VM_PAGE_TO_PHYS(m));

		/*
		 * Now do the i/o move.
		 */
		error = uiomove((caddr_t)(kva + page_offset), len, uio);

		pmap_kremove(kva);

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
	struct proc *curp = td->td_proc;
	struct proc *p;
	struct iovec iov;
	struct uio uio;
	union {
		struct reg	reg;
		struct dbreg	dbreg;
		struct fpreg	fpreg;
	} r;
	int error = 0;
	int write;

	write = 0;
	if (uap->req == PT_TRACE_ME) {
		p = curp;
		PROC_LOCK(p);
	} else {
		if ((p = pfind(uap->pid)) == NULL)
			return (ESRCH);
	}
	if (p_cansee(curp, p)) {
		PROC_UNLOCK(p);
		return (ESRCH);
	}

	if ((error = p_candebug(curp, p)) != 0) {
		PROC_UNLOCK(p);
		return (error);
	}

	/*
	 * Don't debug system processes!
	 */
	if ((p->p_flag & P_SYSTEM) != 0) {
		PROC_UNLOCK(p);
		return (EINVAL);
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
		if (p->p_pid == curp->p_pid) {
			PROC_UNLOCK(p);
			return (EINVAL);
		}

		/* Already traced */
		if (p->p_flag & P_TRACED) {
			PROC_UNLOCK(p);
			return (EBUSY);
		}

		/* OK */
		break;

	case PT_READ_I:
	case PT_READ_D:
	case PT_WRITE_I:
	case PT_WRITE_D:
	case PT_CONTINUE:
	case PT_KILL:
	case PT_STEP:
	case PT_DETACH:
#ifdef PT_GETREGS
	case PT_GETREGS:
#endif
#ifdef PT_SETREGS
	case PT_SETREGS:
#endif
#ifdef PT_GETFPREGS
	case PT_GETFPREGS:
#endif
#ifdef PT_SETFPREGS
	case PT_SETFPREGS:
#endif
#ifdef PT_GETDBREGS
	case PT_GETDBREGS:
#endif
#ifdef PT_SETDBREGS
	case PT_SETDBREGS:
#endif
		/* not being traced... */
		if ((p->p_flag & P_TRACED) == 0) {
			PROC_UNLOCK(p);
			return (EPERM);
		}

		/* not being traced by YOU */
		if (p->p_pptr != curp) {
			PROC_UNLOCK(p);
			return (EBUSY);
		}

		/* not currently stopped */
		mtx_lock_spin(&sched_lock);
		if (p->p_stat != SSTOP || (p->p_flag & P_WAITED) == 0) {
			mtx_unlock_spin(&sched_lock);
			PROC_UNLOCK(p);
			return (EBUSY);
		}
		mtx_unlock_spin(&sched_lock);

		/* OK */
		break;

	default:
		PROC_UNLOCK(p);
		return (EINVAL);
	}

	PROC_UNLOCK(p);
#ifdef FIX_SSTEP
	/*
	 * Single step fixup ala procfs
	 */
	FIX_SSTEP(&p->p_thread);	/* XXXKSE */
#endif

	/*
	 * Actually do the requests
	 */

	td->td_retval[0] = 0;

	switch (uap->req) {
	case PT_TRACE_ME:
		/* set my trace flag and "owner" so it can read/write me */
		sx_xlock(&proctree_lock);
		PROC_LOCK(p);
		p->p_flag |= P_TRACED;
		p->p_oppid = p->p_pptr->p_pid;
		PROC_UNLOCK(p);
		sx_xunlock(&proctree_lock);
		return (0);

	case PT_ATTACH:
		/* security check done above */
		sx_xlock(&proctree_lock);
		PROC_LOCK(p);
		p->p_flag |= P_TRACED;
		p->p_oppid = p->p_pptr->p_pid;
		if (p->p_pptr != curp)
			proc_reparent(p, curp);
		PROC_UNLOCK(p);
		sx_xunlock(&proctree_lock);
		uap->data = SIGSTOP;
		goto sendsig;	/* in PT_CONTINUE below */

	case PT_STEP:
	case PT_CONTINUE:
	case PT_DETACH:
		if ((uap->req != PT_STEP) && ((unsigned)uap->data >= NSIG))
			return (EINVAL);

		PHOLD(p);

		if (uap->req == PT_STEP) {
			if ((error = ptrace_single_step(&p->p_thread))) {
				PRELE(p);
				return (error);
			}
		}

		if (uap->addr != (caddr_t)1) {
			fill_kinfo_proc(p, &p->p_uarea->u_kproc);
			if ((error = ptrace_set_pc(&p->p_thread,
			    (u_long)(uintfptr_t)uap->addr))) {
				PRELE(p);
				return (error);
			}
		}
		PRELE(p);

		if (uap->req == PT_DETACH) {
			/* reset process parent */
			sx_xlock(&proctree_lock);
			if (p->p_oppid != p->p_pptr->p_pid) {
				struct proc *pp;

				pp = pfind(p->p_oppid);
				if (pp != NULL)
					PROC_UNLOCK(pp);
				else
					pp = initproc;
				PROC_LOCK(p);
				proc_reparent(p, pp);
			} else
				PROC_LOCK(p);
			p->p_flag &= ~(P_TRACED | P_WAITED);
			p->p_oppid = 0;

			PROC_UNLOCK(p);
			sx_xunlock(&proctree_lock);

			/* should we send SIGCHLD? */

		}

	sendsig:
		/* deliver or queue signal */
		PROC_LOCK(p);
		mtx_lock_spin(&sched_lock);
		if (p->p_stat == SSTOP) {
			p->p_xstat = uap->data;
			setrunnable(&p->p_thread); /* XXXKSE */
			mtx_unlock_spin(&sched_lock);
		} else {
			mtx_unlock_spin(&sched_lock);
			if (uap->data)		      
				psignal(p, uap->data);

		}
		PROC_UNLOCK(p);
		return (0);

	case PT_WRITE_I:
	case PT_WRITE_D:
		write = 1;
		/* fallthrough */
	case PT_READ_I:
	case PT_READ_D:
		/* write = 0 set above */
		iov.iov_base = write ? (caddr_t)&uap->data :
		    (caddr_t)td->td_retval;
		iov.iov_len = sizeof(int);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = (off_t)(uintptr_t)uap->addr;
		uio.uio_resid = sizeof(int);
		uio.uio_segflg = UIO_SYSSPACE;	/* ie: the uap */
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

	case PT_KILL:
		uap->data = SIGKILL;
		goto sendsig;	/* in PT_CONTINUE above */

#ifdef PT_SETREGS
	case PT_SETREGS:
		error = copyin(uap->addr, &r.reg, sizeof r.reg);
		if (error == 0) {
			PHOLD(p);
			error = proc_write_regs(&p->p_thread, &r.reg);
			PRELE(p);
		}
		return (error);
#endif /* PT_SETREGS */

#ifdef PT_GETREGS
	case PT_GETREGS:
		PHOLD(p);
		error = proc_read_regs(&p->p_thread, &r.reg);
		PRELE(p);
		if (error == 0)
			error = copyout(&r.reg, uap->addr, sizeof r.reg);
		return (error);
#endif /* PT_SETREGS */

#ifdef PT_SETFPREGS
	case PT_SETFPREGS:
		error = copyin(uap->addr, &r.fpreg, sizeof r.fpreg);
		if (error == 0) {
			PHOLD(p);
			error = proc_write_fpregs(&p->p_thread, &r.fpreg);
			PRELE(p);
		}
		return (error);
#endif /* PT_SETFPREGS */

#ifdef PT_GETFPREGS
	case PT_GETFPREGS:
		PHOLD(p);
		error = proc_read_fpregs(&p->p_thread, &r.fpreg);
		PRELE(p);
		if (error == 0)
			error = copyout(&r.fpreg, uap->addr, sizeof r.fpreg);
		return (error);
#endif /* PT_SETFPREGS */

#ifdef PT_SETDBREGS
	case PT_SETDBREGS:
		error = copyin(uap->addr, &r.dbreg, sizeof r.dbreg);
		if (error == 0) {
			PHOLD(p);
			error = proc_write_dbregs(&p->p_thread, &r.dbreg);
			PRELE(p);
		}
		return (error);
#endif /* PT_SETDBREGS */
		
#ifdef PT_GETDBREGS
	case PT_GETDBREGS:
		PHOLD(p);
		error = proc_read_dbregs(&p->p_thread, &r.dbreg);
		PRELE(p);
		if (error == 0)
			error = copyout(&r.dbreg, uap->addr, sizeof r.dbreg);
		return (error);
#endif /* PT_SETDBREGS */

	default:
		KASSERT(0, ("unreachable code\n"));
		break;
	}

	KASSERT(0, ("unreachable code\n"));
	return (0);
}

int
trace_req(struct proc *p)
{
	return (1);
}

/*
 * stopevent()
 * Stop a process because of a debugging event;
 * stay stopped until p->p_step is cleared
 * (cleared by PIOCCONT in procfs).
 *
 * Must be called with the proc struct mutex held.
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
