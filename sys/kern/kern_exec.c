/*
 * Copyright (c) 1993, David Greenman
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

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/filedesc.h>
#include <sys/fcntl.h>
#include <sys/acct.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/wait.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/namei.h>
#include <sys/sysent.h>
#include <sys/shm.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/vnode.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#include <machine/reg.h>

MALLOC_DEFINE(M_PARGS, "proc-args", "Process arguments");

static MALLOC_DEFINE(M_ATEXEC, "atexec", "atexec callback");

/*
 * callout list for things to do at exec time
 */
struct execlist {
	execlist_fn function;
	TAILQ_ENTRY(execlist) next;
};

TAILQ_HEAD(exec_list_head, execlist);
static struct exec_list_head exec_list = TAILQ_HEAD_INITIALIZER(exec_list);

static register_t *exec_copyout_strings(struct image_params *);

/* XXX This should be vm_size_t. */
static u_long ps_strings = PS_STRINGS;
SYSCTL_ULONG(_kern, KERN_PS_STRINGS, ps_strings, CTLFLAG_RD, &ps_strings, 0, "");

/* XXX This should be vm_size_t. */
static u_long usrstack = USRSTACK;
SYSCTL_ULONG(_kern, KERN_USRSTACK, usrstack, CTLFLAG_RD, &usrstack, 0, "");

u_long ps_arg_cache_limit = PAGE_SIZE / 16;
SYSCTL_ULONG(_kern, OID_AUTO, ps_arg_cache_limit, CTLFLAG_RW, 
    &ps_arg_cache_limit, 0, "");

int ps_argsopen = 1;
SYSCTL_INT(_kern, OID_AUTO, ps_argsopen, CTLFLAG_RW, &ps_argsopen, 0, "");

#ifdef __ia64__
/* XXX HACK */
static int regstkpages = 256;
SYSCTL_INT(_machdep, OID_AUTO, regstkpages, CTLFLAG_RW, &regstkpages, 0, "");
#endif

/*
 * Each of the items is a pointer to a `const struct execsw', hence the
 * double pointer here.
 */
static const struct execsw **execsw;

#ifndef _SYS_SYSPROTO_H_
struct execve_args {
        char    *fname; 
        char    **argv;
        char    **envv; 
};
#endif

/*
 * execve() system call.
 *
 * MPSAFE
 */
int
execve(td, uap)
	struct thread *td;
	register struct execve_args *uap;
{
	struct proc *p = td->td_proc;
	struct nameidata nd, *ndp;
	struct ucred *newcred = NULL, *oldcred;
	struct uidinfo *euip;
	register_t *stack_base;
	int error, len, i;
	struct image_params image_params, *imgp;
	struct vattr attr;
	int (*img_first)(struct image_params *);
	struct pargs *oldargs = NULL, *newargs = NULL;
	struct procsig *oldprocsig, *newprocsig;
#ifdef KTRACE
	struct vnode *tracevp = NULL;
#endif
	struct vnode *textvp = NULL;
	int credential_changing;

	imgp = &image_params;

	/*
	 * Lock the process and set the P_INEXEC flag to indicate that
	 * it should be left alone until we're done here.  This is
	 * necessary to avoid race conditions - e.g. in ptrace() -
	 * that might allow a local user to illicitly obtain elevated
	 * privileges.
	 */
	PROC_LOCK(p);
	KASSERT((p->p_flag & P_INEXEC) == 0,
	    ("%s(): process already has P_INEXEC flag", __func__));
	if ((p->p_flag & P_KSES) && thread_single(SNGLE_EXIT)) {
		PROC_UNLOCK(p);
		return (ERESTART);	/* Try again later. */
	}
	/* If we get here all other threads are dead. */
	p->p_flag |= P_INEXEC;
	PROC_UNLOCK(p);

	/*
	 * Initialize part of the common data
	 */
	imgp->proc = p;
	imgp->uap = uap;
	imgp->attr = &attr;
	imgp->argc = imgp->envc = 0;
	imgp->argv0 = NULL;
	imgp->entry_addr = 0;
	imgp->vmspace_destroyed = 0;
	imgp->interpreted = 0;
	imgp->interpreter_name[0] = '\0';
	imgp->auxargs = NULL;
	imgp->vp = NULL;
	imgp->object = NULL;
	imgp->firstpage = NULL;
	imgp->ps_strings = 0;
	imgp->auxarg_size = 0;

	/*
	 * Allocate temporary demand zeroed space for argument and
	 *	environment strings
	 */
	imgp->stringbase = (char *)kmem_alloc_wait(exec_map, ARG_MAX + PAGE_SIZE);
	if (imgp->stringbase == NULL) {
		error = ENOMEM;
		mtx_lock(&Giant);
		goto exec_fail;
	}
	imgp->stringp = imgp->stringbase;
	imgp->stringspace = ARG_MAX;
	imgp->image_header = imgp->stringbase + ARG_MAX;

	/*
	 * Translate the file name. namei() returns a vnode pointer
	 *	in ni_vp amoung other things.
	 */
	ndp = &nd;
	NDINIT(ndp, LOOKUP, LOCKLEAF | FOLLOW | SAVENAME,
	    UIO_USERSPACE, uap->fname, td);

	mtx_lock(&Giant);
interpret:

	error = namei(ndp);
	if (error) {
		kmem_free_wakeup(exec_map, (vm_offset_t)imgp->stringbase,
			ARG_MAX + PAGE_SIZE);
		goto exec_fail;
	}

	imgp->vp = ndp->ni_vp;
	imgp->fname = uap->fname;

	/*
	 * Check file permissions (also 'opens' file)
	 */
	error = exec_check_permissions(imgp);
	if (error) {
		VOP_UNLOCK(imgp->vp, 0, td);
		goto exec_fail_dealloc;
	}
	VOP_GETVOBJECT(imgp->vp, &imgp->object);
	vm_object_reference(imgp->object);

	error = exec_map_first_page(imgp);
	VOP_UNLOCK(imgp->vp, 0, td);
	if (error)
		goto exec_fail_dealloc;

	/*
	 *	If the current process has a special image activator it
	 *	wants to try first, call it.   For example, emulating shell 
	 *	scripts differently.
	 */
	error = -1;
	if ((img_first = imgp->proc->p_sysent->sv_imgact_try) != NULL)
		error = img_first(imgp);

	/*
	 *	Loop through the list of image activators, calling each one.
	 *	An activator returns -1 if there is no match, 0 on success,
	 *	and an error otherwise.
	 */
	for (i = 0; error == -1 && execsw[i]; ++i) {
		if (execsw[i]->ex_imgact == NULL ||
		    execsw[i]->ex_imgact == img_first) {
			continue;
		}
		error = (*execsw[i]->ex_imgact)(imgp);
	}

	if (error) {
		if (error == -1)
			error = ENOEXEC;
		goto exec_fail_dealloc;
	}

	/*
	 * Special interpreter operation, cleanup and loop up to try to
	 * activate the interpreter.
	 */
	if (imgp->interpreted) {
		exec_unmap_first_page(imgp);
		/* free name buffer and old vnode */
		NDFREE(ndp, NDF_ONLY_PNBUF);
		vrele(ndp->ni_vp);
		vm_object_deallocate(imgp->object);
		imgp->object = NULL;
		/* set new name to that of the interpreter */
		NDINIT(ndp, LOOKUP, LOCKLEAF | FOLLOW | SAVENAME,
		    UIO_SYSSPACE, imgp->interpreter_name, td);
		goto interpret;
	}

	/*
	 * Copy out strings (args and env) and initialize stack base
	 */
	if (p->p_sysent->sv_copyout_strings)
		stack_base = (*p->p_sysent->sv_copyout_strings)(imgp);
	else
		stack_base = exec_copyout_strings(imgp);

	/*
	 * If custom stack fixup routine present for this process
	 * let it do the stack setup.
	 * Else stuff argument count as first item on stack
	 */
	if (p->p_sysent->sv_fixup)
		(*p->p_sysent->sv_fixup)(&stack_base, imgp);
	else
		suword(--stack_base, imgp->argc);

	/*
	 * For security and other reasons, the file descriptor table cannot
	 * be shared after an exec.
	 */
	FILEDESC_LOCK(p->p_fd);
	if (p->p_fd->fd_refcnt > 1) {
		struct filedesc *tmp;

		tmp = fdcopy(td);
		FILEDESC_UNLOCK(p->p_fd);
		fdfree(td);
		p->p_fd = tmp;
	} else
		FILEDESC_UNLOCK(p->p_fd);

	/*
	 * Malloc things before we need locks.
	 */
	newcred = crget();
	euip = uifind(attr.va_uid);
	i = imgp->endargs - imgp->stringbase;
	if (ps_arg_cache_limit >= i + sizeof(struct pargs))
		newargs = pargs_alloc(i);

	/* close files on exec */
	fdcloseexec(td);

	/*
	 * For security and other reasons, signal handlers cannot
	 * be shared after an exec. The new process gets a copy of the old
	 * handlers. In execsigs(), the new process will have its signals
	 * reset.
	 */
	PROC_LOCK(p);
	mp_fixme("procsig needs a lock");
	if (p->p_procsig->ps_refcnt > 1) {
		oldprocsig = p->p_procsig;
		PROC_UNLOCK(p);
		MALLOC(newprocsig, struct procsig *, sizeof(struct procsig),
		    M_SUBPROC, M_WAITOK);
		bcopy(oldprocsig, newprocsig, sizeof(*newprocsig));
		newprocsig->ps_refcnt = 1;
		oldprocsig->ps_refcnt--;
		PROC_LOCK(p);
		p->p_procsig = newprocsig;
		if (p->p_sigacts == &p->p_uarea->u_sigacts)
			panic("shared procsig but private sigacts?");

		p->p_uarea->u_sigacts = *p->p_sigacts;
		p->p_sigacts = &p->p_uarea->u_sigacts;
	}
	/* Stop profiling */
	stopprofclock(p);

	/* reset caught signals */
	execsigs(p);

	/* name this process - nameiexec(p, ndp) */
	len = min(ndp->ni_cnd.cn_namelen,MAXCOMLEN);
	bcopy(ndp->ni_cnd.cn_nameptr, p->p_comm, len);
	p->p_comm[len] = 0;

	/*
	 * mark as execed, wakeup the process that vforked (if any) and tell
	 * it that it now has its own resources back
	 */
	p->p_flag |= P_EXEC;
	if (p->p_pptr && (p->p_flag & P_PPWAIT)) {
		p->p_flag &= ~P_PPWAIT;
		wakeup(p->p_pptr);
	}

	/*
	 * Implement image setuid/setgid.
	 *
	 * Don't honor setuid/setgid if the filesystem prohibits it or if
	 * the process is being traced.
	 */
	oldcred = p->p_ucred;
	credential_changing = 0;
	credential_changing |= (attr.va_mode & VSUID) && oldcred->cr_uid !=
	    attr.va_uid;
	credential_changing |= (attr.va_mode & VSGID) && oldcred->cr_gid !=
	    attr.va_gid;

	if (credential_changing &&
	    (imgp->vp->v_mount->mnt_flag & MNT_NOSUID) == 0 &&
	    (p->p_flag & P_TRACED) == 0) {
		/*
		 * Turn off syscall tracing for set-id programs, except for
		 * root.  Record any set-id flags first to make sure that
		 * we do not regain any tracing during a possible block.
		 */
		setsugid(p);
#ifdef KTRACE
		if (p->p_tracep && suser_cred(oldcred, PRISON_ROOT)) {
			mtx_lock(&ktrace_mtx);
			p->p_traceflag = 0;
			tracevp = p->p_tracep;
			p->p_tracep = NULL;
			mtx_unlock(&ktrace_mtx);
		}
#endif
		/* Close any file descriptors 0..2 that reference procfs */
		setugidsafety(td);
		/* Make sure file descriptors 0..2 are in use.  */
		error = fdcheckstd(td);
		if (error != 0)
			goto done1;
		/*
		 * Set the new credentials.
		 */
		crcopy(newcred, oldcred);
		if (attr.va_mode & VSUID)
			change_euid(newcred, euip);
		if (attr.va_mode & VSGID)
			change_egid(newcred, attr.va_gid);
		/*
		 * Implement correct POSIX saved-id behavior.
		 */
		change_svuid(newcred, newcred->cr_uid);
		change_svgid(newcred, newcred->cr_gid);
		p->p_ucred = newcred;
		newcred = NULL;
	} else {
		if (oldcred->cr_uid == oldcred->cr_ruid &&
		    oldcred->cr_gid == oldcred->cr_rgid)
			p->p_flag &= ~P_SUGID;
		/*
		 * Implement correct POSIX saved-id behavior.
		 *
		 * XXX: It's not clear that the existing behavior is
		 * POSIX-compliant.  A number of sources indicate that the
		 * saved uid/gid should only be updated if the new ruid is
		 * not equal to the old ruid, or the new euid is not equal
		 * to the old euid and the new euid is not equal to the old
		 * ruid.  The FreeBSD code always updates the saved uid/gid.
		 * Also, this code uses the new (replaced) euid and egid as
		 * the source, which may or may not be the right ones to use.
		 */
		if (oldcred->cr_svuid != oldcred->cr_uid ||
		    oldcred->cr_svgid != oldcred->cr_gid) {
			crcopy(newcred, oldcred);
			change_svuid(newcred, newcred->cr_uid);
			change_svgid(newcred, newcred->cr_gid);
			p->p_ucred = newcred;
			newcred = NULL;
		}
	}

	/*
	 * Store the vp for use in procfs
	 */
	textvp = p->p_textvp;
	VREF(ndp->ni_vp);
	p->p_textvp = ndp->ni_vp;

	/*
	 * Notify others that we exec'd, and clear the P_INEXEC flag
	 * as we're now a bona fide freshly-execed process.
	 */
	KNOTE(&p->p_klist, NOTE_EXEC);
	p->p_flag &= ~P_INEXEC;

	/*
	 * If tracing the process, trap to debugger so breakpoints
	 * can be set before the program executes.
	 */
	_STOPEVENT(p, S_EXEC, 0);

	if (p->p_flag & P_TRACED)
		psignal(p, SIGTRAP);

	/* clear "fork but no exec" flag, as we _are_ execing */
	p->p_acflag &= ~AFORK;

	/* Free any previous argument cache */
	oldargs = p->p_args;
	p->p_args = NULL;

	/* Set values passed into the program in registers. */
	if (p->p_sysent->sv_setregs)
		(*p->p_sysent->sv_setregs)(td, imgp->entry_addr,
		    (u_long)(uintptr_t)stack_base, imgp->ps_strings);
	else
		setregs(td, imgp->entry_addr, (u_long)(uintptr_t)stack_base,
		    imgp->ps_strings);

	/* Cache arguments if they fit inside our allowance */
	if (ps_arg_cache_limit >= i + sizeof(struct pargs)) {
		bcopy(imgp->stringbase, newargs->ar_args, i);
		p->p_args = newargs;
		newargs = NULL;
	}
done1:
	PROC_UNLOCK(p);

	/*
	 * Free any resources malloc'd earlier that we didn't use.
	 */
	uifree(euip);
	if (newcred == NULL)
		crfree(oldcred);
	else
		crfree(newcred);
	/*
	 * Handle deferred decrement of ref counts.
	 */
	if (textvp != NULL)
		vrele(textvp);
#ifdef KTRACE
	if (tracevp != NULL)
		vrele(tracevp);
#endif
	if (oldargs != NULL)
		pargs_drop(oldargs);
	if (newargs != NULL)
		pargs_drop(newargs);

exec_fail_dealloc:

	/*
	 * free various allocated resources
	 */
	if (imgp->firstpage)
		exec_unmap_first_page(imgp);

	if (imgp->stringbase != NULL)
		kmem_free_wakeup(exec_map, (vm_offset_t)imgp->stringbase,
			ARG_MAX + PAGE_SIZE);

	if (imgp->vp) {
		NDFREE(ndp, NDF_ONLY_PNBUF);
		vrele(imgp->vp);
	}

	if (imgp->object)
		vm_object_deallocate(imgp->object);

	if (error == 0)
		goto done2;

exec_fail:
	/* we're done here, clear P_INEXEC */
	PROC_LOCK(p);
	p->p_flag &= ~P_INEXEC;
	PROC_UNLOCK(p);
	
	if (imgp->vmspace_destroyed) {
		/* sorry, no more process anymore. exit gracefully */
		exit1(td, W_EXITCODE(0, SIGABRT));
		/* NOT REACHED */
		error = 0;
	}
done2:
	mtx_unlock(&Giant);
	return (error);
}

int
exec_map_first_page(imgp)
	struct image_params *imgp;
{
	int rv, i;
	int initial_pagein;
	vm_page_t ma[VM_INITIAL_PAGEIN];
	vm_object_t object;

	GIANT_REQUIRED;

	if (imgp->firstpage) {
		exec_unmap_first_page(imgp);
	}

	VOP_GETVOBJECT(imgp->vp, &object);

	ma[0] = vm_page_grab(object, 0, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

	if ((ma[0]->valid & VM_PAGE_BITS_ALL) != VM_PAGE_BITS_ALL) {
		initial_pagein = VM_INITIAL_PAGEIN;
		if (initial_pagein > object->size)
			initial_pagein = object->size;
		for (i = 1; i < initial_pagein; i++) {
			if ((ma[i] = vm_page_lookup(object, i)) != NULL) {
				if ((ma[i]->flags & PG_BUSY) || ma[i]->busy)
					break;
				if (ma[i]->valid)
					break;
				vm_page_busy(ma[i]);
			} else {
				ma[i] = vm_page_alloc(object, i, VM_ALLOC_NORMAL);
				if (ma[i] == NULL)
					break;
			}
		}
		initial_pagein = i;

		rv = vm_pager_get_pages(object, ma, initial_pagein, 0);
		ma[0] = vm_page_lookup(object, 0);

		if ((rv != VM_PAGER_OK) || (ma[0] == NULL) || (ma[0]->valid == 0)) {
			if (ma[0]) {
				vm_page_lock_queues();
				vm_page_protect(ma[0], VM_PROT_NONE);
				vm_page_free(ma[0]);
				vm_page_unlock_queues();
			}
			return EIO;
		}
	}
	vm_page_lock_queues();
	vm_page_wire(ma[0]);
	vm_page_wakeup(ma[0]);
	vm_page_unlock_queues();

	pmap_qenter((vm_offset_t)imgp->image_header, ma, 1);
	imgp->firstpage = ma[0];

	return 0;
}

void
exec_unmap_first_page(imgp)
	struct image_params *imgp;
{
	GIANT_REQUIRED;

	if (imgp->firstpage) {
		pmap_qremove((vm_offset_t)imgp->image_header, 1);
		vm_page_lock_queues();
		vm_page_unwire(imgp->firstpage, 1);
		vm_page_unlock_queues();
		imgp->firstpage = NULL;
	}
}

/*
 * Destroy old address space, and allocate a new stack
 *	The new stack is only SGROWSIZ large because it is grown
 *	automatically in trap.c.
 */
int
exec_new_vmspace(imgp, minuser, maxuser, stack_addr)
	struct image_params *imgp;
	vm_offset_t minuser, maxuser, stack_addr;
{
	int error;
	struct execlist *ep;
	struct proc *p = imgp->proc;
	struct vmspace *vmspace = p->p_vmspace;

	GIANT_REQUIRED;

	stack_addr = stack_addr - maxssiz;

	imgp->vmspace_destroyed = 1;

	/*
	 * Perform functions registered with at_exec().
	 */
	TAILQ_FOREACH(ep, &exec_list, next)
		(*ep->function)(p);

	/*
	 * Blow away entire process VM, if address space not shared,
	 * otherwise, create a new VM space so that other threads are
	 * not disrupted
	 */
	if (vmspace->vm_refcnt == 1
	    && vm_map_min(&vmspace->vm_map) == minuser
	    && vm_map_max(&vmspace->vm_map) == maxuser) {
		if (vmspace->vm_shm)
			shmexit(p);
		pmap_remove_pages(vmspace_pmap(vmspace), minuser, maxuser);
		vm_map_remove(&vmspace->vm_map, minuser, maxuser);
	} else {
		vmspace_exec(p, minuser, maxuser);
		vmspace = p->p_vmspace;
	}

	/* Allocate a new stack */
	error = vm_map_stack(&vmspace->vm_map, stack_addr, (vm_size_t)maxssiz,
	    VM_PROT_ALL, VM_PROT_ALL, 0);
	if (error)
		return (error);

#ifdef __ia64__
	{
		/*
		 * Allocate backing store. We really need something
		 * similar to vm_map_stack which can allow the backing 
		 * store to grow upwards. This will do for now.
		 */
		vm_offset_t bsaddr;
		bsaddr = USRSTACK - 2*maxssiz;
		error = vm_map_find(&vmspace->vm_map, 0, 0, &bsaddr,
				    regstkpages * PAGE_SIZE, 0,
				    VM_PROT_ALL, VM_PROT_ALL, 0);
		FIRST_THREAD_IN_PROC(p)->td_md.md_bspstore = bsaddr;
	}
#endif

	/* vm_ssize and vm_maxsaddr are somewhat antiquated concepts in the
	 * VM_STACK case, but they are still used to monitor the size of the
	 * process stack so we can check the stack rlimit.
	 */
	vmspace->vm_ssize = sgrowsiz >> PAGE_SHIFT;
	vmspace->vm_maxsaddr = (char *)USRSTACK - maxssiz;

	return(0);
}

/*
 * Copy out argument and environment strings from the old process
 *	address space into the temporary string buffer.
 */
int
exec_extract_strings(imgp)
	struct image_params *imgp;
{
	char	**argv, **envv;
	char	*argp, *envp;
	int	error;
	size_t	length;

	/*
	 * extract arguments first
	 */

	argv = imgp->uap->argv;

	if (argv) {
		argp = (caddr_t) (intptr_t) fuword(argv);
		if (argp == (caddr_t) -1)
			return (EFAULT);
		if (argp)
			argv++;
		if (imgp->argv0)
			argp = imgp->argv0;
		if (argp) {
			do {
				if (argp == (caddr_t) -1)
					return (EFAULT);
				if ((error = copyinstr(argp, imgp->stringp,
				    imgp->stringspace, &length))) {
					if (error == ENAMETOOLONG)
						return(E2BIG);
					return (error);
				}
				imgp->stringspace -= length;
				imgp->stringp += length;
				imgp->argc++;
			} while ((argp = (caddr_t) (intptr_t) fuword(argv++)));
		}
	}	

	imgp->endargs = imgp->stringp;

	/*
	 * extract environment strings
	 */

	envv = imgp->uap->envv;

	if (envv) {
		while ((envp = (caddr_t) (intptr_t) fuword(envv++))) {
			if (envp == (caddr_t) -1)
				return (EFAULT);
			if ((error = copyinstr(envp, imgp->stringp,
			    imgp->stringspace, &length))) {
				if (error == ENAMETOOLONG)
					return(E2BIG);
				return (error);
			}
			imgp->stringspace -= length;
			imgp->stringp += length;
			imgp->envc++;
		}
	}

	return (0);
}

/*
 * Copy strings out to the new process address space, constructing
 *	new arg and env vector tables. Return a pointer to the base
 *	so that it can be used as the initial stack pointer.
 */
register_t *
exec_copyout_strings(imgp)
	struct image_params *imgp;
{
	int argc, envc;
	char **vectp;
	char *stringp, *destp;
	register_t *stack_base;
	struct ps_strings *arginfo;
	int szsigcode;

	/*
	 * Calculate string base and vector table pointers.
	 * Also deal with signal trampoline code for this exec type.
	 */
	arginfo = (struct ps_strings *)PS_STRINGS;
	szsigcode = *(imgp->proc->p_sysent->sv_szsigcode);
	destp =	(caddr_t)arginfo - szsigcode - SPARE_USRSPACE -
		roundup((ARG_MAX - imgp->stringspace), sizeof(char *));

	/*
	 * install sigcode
	 */
	if (szsigcode)
		copyout(imgp->proc->p_sysent->sv_sigcode,
			((caddr_t)arginfo - szsigcode), szsigcode);

	/*
	 * If we have a valid auxargs ptr, prepare some room
	 * on the stack.
	 */
	if (imgp->auxargs) {
		/*
		 * 'AT_COUNT*2' is size for the ELF Auxargs data. This is for
		 * lower compatibility.
		 */
		imgp->auxarg_size = (imgp->auxarg_size) ? imgp->auxarg_size
			: (AT_COUNT * 2);
		/*
		 * The '+ 2' is for the null pointers at the end of each of
		 * the arg and env vector sets,and imgp->auxarg_size is room
		 * for argument of Runtime loader.
		 */
		vectp = (char **) (destp - (imgp->argc + imgp->envc + 2 +
				       imgp->auxarg_size) * sizeof(char *));

	} else 
		/*
		 * The '+ 2' is for the null pointers at the end of each of
		 * the arg and env vector sets
		 */
		vectp = (char **)
			(destp - (imgp->argc + imgp->envc + 2) * sizeof(char *));

	/*
	 * vectp also becomes our initial stack base
	 */
	stack_base = (register_t *)vectp;

	stringp = imgp->stringbase;
	argc = imgp->argc;
	envc = imgp->envc;

	/*
	 * Copy out strings - arguments and environment.
	 */
	copyout(stringp, destp, ARG_MAX - imgp->stringspace);

	/*
	 * Fill in "ps_strings" struct for ps, w, etc.
	 */
	suword(&arginfo->ps_argvstr, (long)(intptr_t)vectp);
	suword(&arginfo->ps_nargvstr, argc);

	/*
	 * Fill in argument portion of vector table.
	 */
	for (; argc > 0; --argc) {
		suword(vectp++, (long)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* a null vector table pointer separates the argp's from the envp's */
	suword(vectp++, 0);

	suword(&arginfo->ps_envstr, (long)(intptr_t)vectp);
	suword(&arginfo->ps_nenvstr, envc);

	/*
	 * Fill in environment portion of vector table.
	 */
	for (; envc > 0; --envc) {
		suword(vectp++, (long)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* end of vector table is a null pointer */
	suword(vectp, 0);

	return (stack_base);
}

/*
 * Check permissions of file to execute.
 *	Called with imgp->vp locked.
 *	Return 0 for success or error code on failure.
 */
int
exec_check_permissions(imgp)
	struct image_params *imgp;
{
	struct vnode *vp = imgp->vp;
	struct vattr *attr = imgp->attr;
	struct thread *td;
	int error;

	td = curthread;			/* XXXKSE */
	/* Get file attributes */
	error = VOP_GETATTR(vp, attr, td->td_ucred, td);
	if (error)
		return (error);

	/*
	 * 1) Check if file execution is disabled for the filesystem that this
	 *	file resides on.
	 * 2) Insure that at least one execute bit is on - otherwise root
	 *	will always succeed, and we don't want to happen unless the
	 *	file really is executable.
	 * 3) Insure that the file is a regular file.
	 */
	if ((vp->v_mount->mnt_flag & MNT_NOEXEC) ||
	    ((attr->va_mode & 0111) == 0) ||
	    (attr->va_type != VREG))
		return (EACCES);

	/*
	 * Zero length files can't be exec'd
	 */
	if (attr->va_size == 0)
		return (ENOEXEC);

	/*
	 *  Check for execute permission to file based on current credentials.
	 */
	error = VOP_ACCESS(vp, VEXEC, td->td_ucred, td);
	if (error)
		return (error);

	/*
	 * Check number of open-for-writes on the file and deny execution
	 * if there are any.
	 */
	if (vp->v_writecount)
		return (ETXTBSY);

	/*
	 * Call filesystem specific open routine (which does nothing in the
	 * general case).
	 */
	error = VOP_OPEN(vp, FREAD, td->td_ucred, td);
	return (error);
}

/*
 * Exec handler registration
 */
int
exec_register(execsw_arg)
	const struct execsw *execsw_arg;
{
	const struct execsw **es, **xs, **newexecsw;
	int count = 2;	/* New slot and trailing NULL */

	if (execsw)
		for (es = execsw; *es; es++)
			count++;
	newexecsw = malloc(count * sizeof(*es), M_TEMP, M_WAITOK);
	if (newexecsw == NULL)
		return ENOMEM;
	xs = newexecsw;
	if (execsw)
		for (es = execsw; *es; es++)
			*xs++ = *es;
	*xs++ = execsw_arg;
	*xs = NULL;
	if (execsw)
		free(execsw, M_TEMP);
	execsw = newexecsw;
	return 0;
}

int
exec_unregister(execsw_arg)
	const struct execsw *execsw_arg;
{
	const struct execsw **es, **xs, **newexecsw;
	int count = 1;

	if (execsw == NULL)
		panic("unregister with no handlers left?\n");

	for (es = execsw; *es; es++) {
		if (*es == execsw_arg)
			break;
	}
	if (*es == NULL)
		return ENOENT;
	for (es = execsw; *es; es++)
		if (*es != execsw_arg)
			count++;
	newexecsw = malloc(count * sizeof(*es), M_TEMP, M_WAITOK);
	if (newexecsw == NULL)
		return ENOMEM;
	xs = newexecsw;
	for (es = execsw; *es; es++)
		if (*es != execsw_arg)
			*xs++ = *es;
	*xs = NULL;
	if (execsw)
		free(execsw, M_TEMP);
	execsw = newexecsw;
	return 0;
}

int
at_exec(function)
	execlist_fn function;
{
	struct execlist *ep;

#ifdef INVARIANTS
	/* Be noisy if the programmer has lost track of things */
	if (rm_at_exec(function)) 
		printf("WARNING: exec callout entry (%p) already present\n",
		    function);
#endif
	ep = malloc(sizeof(*ep), M_ATEXEC, M_NOWAIT);
	if (ep == NULL)
		return (ENOMEM);
	ep->function = function;
	TAILQ_INSERT_TAIL(&exec_list, ep, next);
	return (0);
}

/*
 * Scan the exec callout list for the given item and remove it.
 * Returns the number of items removed (0 or 1)
 */
int
rm_at_exec(function)
	execlist_fn function;
{
	struct execlist *ep;

	TAILQ_FOREACH(ep, &exec_list, next) {
		if (ep->function == function) {
			TAILQ_REMOVE(&exec_list, ep, next);
			free(ep, M_ATEXEC);
			return(1);
		}
	}	
	return (0);
}

