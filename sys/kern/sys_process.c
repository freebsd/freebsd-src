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
 *	$Id: sys_process.c,v 1.14 1995/05/30 08:05:58 rgrimes Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ptrace.h>
#include <sys/errno.h>

#include <machine/reg.h>
#include <machine/psl.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <sys/user.h>

static int
pread (struct proc *procp, unsigned int addr, unsigned int *retval) {
	int		rv;
	vm_map_t	map, tmap;
	vm_object_t	object;
	vm_offset_t	kva = 0;
	int		page_offset;	/* offset into page */
	vm_offset_t	pageno;		/* page number */
	vm_map_entry_t	out_entry;
	vm_prot_t	out_prot;
	boolean_t	wired, single_use;
	vm_offset_t	off;

	/* Map page into kernel space */

	map = &procp->p_vmspace->vm_map;

	page_offset = addr - trunc_page(addr);
	pageno = trunc_page(addr);

	tmap = map;
	rv = vm_map_lookup (&tmap, pageno, VM_PROT_READ, &out_entry,
		&object, &off, &out_prot, &wired, &single_use);

	if (rv != KERN_SUCCESS)
		return EINVAL;

	vm_map_lookup_done (tmap, out_entry);

	/* Find space in kernel_map for the page we're interested in */
	rv = vm_map_find (kernel_map, object, off, &kva, PAGE_SIZE, 1);

	if (!rv) {
		vm_object_reference (object);

		rv = vm_map_pageable (kernel_map, kva, kva + PAGE_SIZE, 0);
		if (!rv) {
			*retval = 0;
			bcopy ((caddr_t)kva + page_offset,
			       retval, sizeof *retval);
		}
		vm_map_remove (kernel_map, kva, kva + PAGE_SIZE);
	}

	return rv;
}

static int
pwrite (struct proc *procp, unsigned int addr, unsigned int datum) {
	int		rv;
	vm_map_t	map, tmap;
	vm_object_t	object;
	vm_offset_t	kva = 0;
	int		page_offset;	/* offset into page */
	vm_offset_t	pageno;		/* page number */
	vm_map_entry_t	out_entry;
	vm_prot_t	out_prot;
	boolean_t	wired, single_use;
	vm_offset_t	off;
	boolean_t	fix_prot = 0;

	/* Map page into kernel space */

	map = &procp->p_vmspace->vm_map;

	page_offset = addr - trunc_page(addr);
	pageno = trunc_page(addr);

	/*
	 * Check the permissions for the area we're interested in.
	 */

	if (vm_map_check_protection (map, pageno, pageno + PAGE_SIZE,
		VM_PROT_WRITE) == FALSE) {
		/*
		 * If the page was not writable, we make it so.
		 * XXX It is possible a page may *not* be read/executable,
		 * if a process changes that!
		 */
		fix_prot = 1;
		/* The page isn't writable, so let's try making it so... */
		if ((rv = vm_map_protect (map, pageno, pageno + PAGE_SIZE,
			VM_PROT_ALL, 0)) != KERN_SUCCESS)
		  return EFAULT;	/* I guess... */
	}

	/*
	 * Now we need to get the page.  out_entry, out_prot, wired, and
	 * single_use aren't used.  One would think the vm code would be
	 * a *bit* nicer...  We use tmap because vm_map_lookup() can
	 * change the map argument.
	 */

	tmap = map;
	rv = vm_map_lookup (&tmap, pageno, VM_PROT_WRITE, &out_entry,
		&object, &off, &out_prot, &wired, &single_use);
	if (rv != KERN_SUCCESS) {
		return EINVAL;
	}

	/*
	 * Okay, we've got the page.  Let's release tmap.
	 */

	vm_map_lookup_done (tmap, out_entry);

	/*
	 * Fault the page in...
	 */

	vm_map_pageable(map, trunc_page(vtopte(pageno)),
		trunc_page(vtopte(pageno)) + PAGE_SIZE, FALSE);
	rv = vm_fault(map, pageno, VM_PROT_WRITE|VM_PROT_READ, FALSE);
	vm_map_pageable(map, trunc_page(vtopte(pageno)),
		trunc_page(vtopte(pageno)) + PAGE_SIZE, TRUE);
	if (rv != KERN_SUCCESS)
		return EFAULT;

	/* Find space in kernel_map for the page we're interested in */
	rv = vm_map_find (kernel_map, object, off, &kva, PAGE_SIZE, 1);

	if (!rv) {
		vm_object_reference (object);

		rv = vm_map_pageable (kernel_map, kva, kva + PAGE_SIZE, 0);
		if (!rv) {
		  bcopy (&datum, (caddr_t)kva + page_offset, sizeof datum);
		}
		vm_map_remove (kernel_map, kva, kva + PAGE_SIZE);
	}

	if (fix_prot)
		vm_map_protect (map, pageno, pageno + PAGE_SIZE,
			VM_PROT_READ|VM_PROT_EXECUTE, 0);
	return rv;
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
ptrace(curp, uap, retval)
	struct proc *curp;
	struct ptrace_args *uap;
	int *retval;
{
	struct proc *p;
	int error = 0;

	*retval = 0;
	if (uap->req == PT_TRACE_ME) {
		curp->p_flag |= P_TRACED;
		return 0;
	}
	if ((p = pfind(uap->pid)) == NULL) {
		return ESRCH;
	}

#ifdef PT_ATTACH
	if (uap->req != PT_ATTACH && (
			(p->p_flag & P_TRACED) == 0 ||
			(p->p_tptr && curp != p->p_tptr) ||
			(!p->p_tptr && curp != p->p_pptr)))

		return ESRCH;
#endif
#ifdef PT_ATTACH
	if (uap->req != PT_ATTACH) {
#endif
		if ((p->p_flag & P_TRACED) == 0)
			return EPERM;
		if (p->p_stat != SSTOP || (p->p_flag & P_WAITED) == 0)
			return EBUSY;
#ifdef PT_ATTACH
	}
#endif
	/*
	 * XXX The PT_ATTACH code is completely broken.  It will
	 * be obsoleted by a /proc filesystem, so is it worth it
	 * to fix it?  (Answer, probably.  So that'll be next,
	 * I guess.)
	 */

	switch (uap->req) {
#ifdef PT_ATTACH
	case PT_ATTACH:
		if (curp->p_ucred->cr_uid != 0 && (
			curp->p_ucred->cr_uid != p->p_ucred->cr_uid ||
			curp->p_ucred->cr_uid != p->p_cred->p_svuid))
			return EACCES;

		p->p_tptr = curp;
		p->p_flag |= P_TRACED;
		psignal(p, SIGSTOP);
		return 0;

	case PT_DETACH:
		if ((unsigned)uap->data >= NSIG)
			return EINVAL;
		p->p_flag &= ~P_TRACED;
		p->p_tptr = NULL;
		psignal(p->p_pptr, SIGCHLD);
		wakeup((caddr_t)p->p_pptr);
		s = splhigh();
		if (p->p_stat == SSTOP) {
			p->p_xstat = uap->data;
			setrunnable(p);
		} else if (uap->data) {
			psignal(p, uap->data);
		}
		splx(s);
		return 0;

# ifdef PT_INHERIT
	case PT_INHERIT:
		if ((p->p_flag & P_TRACED) == 0)
			return ESRCH;
		return 0;
# endif	/* PT_INHERIT */
#endif	/* PT_ATTACH */

	case PT_READ_I:
	case PT_READ_D:
		if ((error = pread (p, (unsigned int)uap->addr, retval)))
			return error;
		return 0;
	case PT_WRITE_I:
	case PT_WRITE_D:
		if ((error = pwrite (p, (unsigned int)uap->addr,
				    (unsigned int)uap->data)))
			return error;
		return 0;
	case PT_STEP:
		if ((error = ptrace_single_step (p)))
			return error;
		/* fallthrough */
	case PT_CONTINUE:
		/*
		 * Continue at addr uap->addr with signal
		 * uap->data; if uap->addr is 1, then we just
		 * let the chips fall where they may.
		 *
		 * The only check I'll make right now is for
		 * uap->data to be larger than NSIG; if so, we return
		 * EINVAL.
		 */
		if (uap->data >= NSIG)
			return EINVAL;

		if (uap->addr != (caddr_t)1) {
			fill_eproc (p, &p->p_addr->u_kproc.kp_eproc);
			if ((error = ptrace_set_pc (p, (u_int)uap->addr)))
				return error;
		}

		p->p_xstat = uap->data;

/*		if (p->p_stat == SSTOP) */
		setrunnable (p);
		return 0;
	case PT_READ_U:
		if ((u_int)uap->addr > (UPAGES * NBPG - sizeof(int))) {
			return EFAULT;
		}
		p->p_addr->u_kproc.kp_proc = *p;
		fill_eproc (p, &p->p_addr->u_kproc.kp_eproc);
		*retval = *(int*)((u_int)p->p_addr + (u_int)uap->addr);
		return 0;
	case PT_WRITE_U:
		p->p_addr->u_kproc.kp_proc = *p;
		fill_eproc (p, &p->p_addr->u_kproc.kp_eproc);
		return ptrace_write_u(p, (vm_offset_t)uap->addr, uap->data);
	case PT_KILL:
		p->p_xstat = SIGKILL;
		setrunnable(p);
		return 0;
#ifdef PT_GETREGS
	case PT_GETREGS:
		/*
		 * copyout the registers into addr.  There's no
		 * size constraint!!! *GRRR*
		 */
		return ptrace_getregs(p, uap->addr);
	case PT_SETREGS:
		/*
		 * copyin the registers from addr.  Again, no
		 * size constraint!!! *GRRRR*
		 */
		return ptrace_setregs (p, uap->addr);
#endif /* PT_GETREGS */
	default:
		break;
	}

	return 0;
}

int
trace_req(p)
	struct proc *p;
{
	return 1;
}
