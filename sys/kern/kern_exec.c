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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by David Greenman
 * 4. The name of the developer may not be used to endorse or promote products
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
 *	$Id: kern_exec.c,v 1.22 1995/08/24 10:32:37 davidg Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/acct.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/wait.h>
#include <sys/malloc.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/shm.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/reg.h>

int *exec_copyout_strings __P((struct image_params *));

static int exec_check_permissions(struct image_params *);

/*
 * execsw_set is constructed for us by the linker.  Each of the items
 * is a pointer to a `const struct execsw', hence the double pointer here.
 */
const struct execsw **execsw = (const struct execsw **)&execsw_set.ls_items[0];

struct execve_args {
        char    *fname; 
        char    **argv;
        char    **envv; 
};

/*
 * execve() system call.
 */
int
execve(p, uap, retval)
	struct proc *p;
	register struct execve_args *uap;
	int *retval;
{
	struct nameidata nd, *ndp;
	int *stack_base;
	int error, len, i;
	struct image_params image_params, *iparams;
	struct vnode *vnodep;
	struct vattr attr;
	char *image_header;

	iparams = &image_params;
	bzero((caddr_t)iparams, sizeof(struct image_params));
	image_header = (char *)0;

	/*
	 * Initialize a few constants in the common area
	 */
	iparams->proc = p;
	iparams->uap = uap;
	iparams->attr = &attr;

	/*
	 * Allocate temporary demand zeroed space for argument and
	 *	environment strings
	 */
	iparams->stringbase = (char *)vm_map_min(exec_map);
	error = vm_map_find(exec_map, NULL, 0, (vm_offset_t *)&iparams->stringbase,
	    ARG_MAX, TRUE);
	if (error) {
		log(LOG_WARNING, "execve: failed to allocate string space\n");
		return (error);
	}

	if (!iparams->stringbase) {
		error = ENOMEM;
		goto exec_fail;
	}
	iparams->stringp = iparams->stringbase;
	iparams->stringspace = ARG_MAX;

	/*
	 * Translate the file name. namei() returns a vnode pointer
	 *	in ni_vp amoung other things.
	 */
	ndp = &nd;
	NDINIT(ndp, LOOKUP, LOCKLEAF | FOLLOW | SAVENAME,
	    UIO_USERSPACE, uap->fname, p);

interpret:

	error = namei(ndp);
	if (error) {
		vm_map_remove(exec_map, (vm_offset_t)iparams->stringbase,
		    (vm_offset_t)iparams->stringbase + ARG_MAX);
		goto exec_fail;
	}

	iparams->vnodep = vnodep = ndp->ni_vp;

	if (vnodep == NULL) {
		error = ENOEXEC;
		goto exec_fail_dealloc;
	}

	/*
	 * Check file permissions (also 'opens' file)
	 */
	error = exec_check_permissions(iparams);

	/*
	 * Lose the lock on the vnode. It's no longer needed, and must not
	 * exist for the pagefault paging to work below.
	 */
	VOP_UNLOCK(vnodep);

	if (error)
		goto exec_fail_dealloc;

	/*
	 * Map the image header (first page) of the file into
	 *	kernel address space
	 */
	error = vm_mmap(kernel_map,			/* map */
			(vm_offset_t *)&image_header,	/* address */
			PAGE_SIZE,			/* size */
			VM_PROT_READ, 			/* protection */
			VM_PROT_READ, 			/* max protection */
			0,	 			/* flags */
			(caddr_t)vnodep,		/* vnode */
			0);				/* offset */
	if (error) {
		uprintf("mmap failed: %d\n",error);
		goto exec_fail_dealloc;
	}
	iparams->image_header = image_header;

	/*
	 * Loop through list of image activators, calling each one.
	 *	If there is no match, the activator returns -1. If there
	 *	is a match, but there was an error during the activation,
	 *	the error is returned. Otherwise 0 means success. If the
	 *	image is interpreted, loop back up and try activating
	 *	the interpreter.
	 */
	for (i = 0; execsw[i]; ++i) {
		if (execsw[i]->ex_imgact)
			error = (*execsw[i]->ex_imgact)(iparams);
		else
			continue;

		if (error == -1)
			continue;
		if (error)
			goto exec_fail_dealloc;
		if (iparams->interpreted) {
			/* free old vnode and name buffer */
			vrele(ndp->ni_vp);
			FREE(ndp->ni_cnd.cn_pnbuf, M_NAMEI);
			if (vm_map_remove(kernel_map, (vm_offset_t)image_header,
			    (vm_offset_t)image_header + PAGE_SIZE))
				panic("execve: header dealloc failed (1)");

			/* set new name to that of the interpreter */
			NDINIT(ndp, LOOKUP, LOCKLEAF | FOLLOW | SAVENAME,
			    UIO_SYSSPACE, iparams->interpreter_name, p);
			goto interpret;
		}
		break;
	}
	/* If we made it through all the activators and none matched, exit. */
	if (error == -1) {
		error = ENOEXEC;
		goto exec_fail_dealloc;
	}

	/*
	 * Copy out strings (args and env) and initialize stack base
	 */
	stack_base = exec_copyout_strings(iparams);
	p->p_vmspace->vm_minsaddr = (char *)stack_base;

	/*
	 * If custom stack fixup routine present for this process
	 * let it do the stack setup.
	 * Else stuff argument count as first item on stack
	 */
	if (p->p_sysent->sv_fixup)
		(*p->p_sysent->sv_fixup)(&stack_base, iparams);
	else
		suword(--stack_base, iparams->argc);

	/* close files on exec */
	fdcloseexec(p);

	/* reset caught signals */
	execsigs(p);

	/* name this process - nameiexec(p, ndp) */
	len = min(ndp->ni_cnd.cn_namelen,MAXCOMLEN);
	bcopy(ndp->ni_cnd.cn_nameptr, p->p_comm, len);
	p->p_comm[len] = 0;

	/*
	 * mark as executable, wakeup any process that was vforked and tell
	 * it that it now has it's own resources back
	 */
	p->p_flag |= P_EXEC;
	if (p->p_pptr && (p->p_flag & P_PPWAIT)) {
		p->p_flag &= ~P_PPWAIT;
		wakeup((caddr_t)p->p_pptr);
	}

	/* implement set userid/groupid */
	p->p_flag &= ~P_SUGID;

	/*
	 * Turn off kernel tracing for set-id programs, except for
	 * root.
	 */
	if (p->p_tracep && (attr.va_mode & (VSUID | VSGID)) &&
	    suser(p->p_ucred, &p->p_acflag)) {
		p->p_traceflag = 0;
		vrele(p->p_tracep);
		p->p_tracep = 0;
	}
	if ((attr.va_mode & VSUID) && (p->p_flag & P_TRACED) == 0) {
		p->p_ucred = crcopy(p->p_ucred);
		p->p_ucred->cr_uid = attr.va_uid;
		p->p_flag |= P_SUGID;
	}
	if ((attr.va_mode & VSGID) && (p->p_flag & P_TRACED) == 0) {
		p->p_ucred = crcopy(p->p_ucred);
		p->p_ucred->cr_groups[0] = attr.va_gid;
		p->p_flag |= P_SUGID;
	}

	/*
	 * Implement correct POSIX saved uid behavior.
	 */
	p->p_cred->p_svuid = p->p_ucred->cr_uid;
	p->p_cred->p_svgid = p->p_ucred->cr_gid;

	/*
	 * Store the vp for use in procfs
	 */
	if (p->p_textvp)		/* release old reference */
		vrele(p->p_textvp);
	VREF(ndp->ni_vp);
	p->p_textvp = ndp->ni_vp;

	/*
	 * If tracing the process, trap to debugger so breakpoints
	 * 	can be set before the program executes.
	 */
	if (p->p_flag & P_TRACED)
		psignal(p, SIGTRAP);

	/* clear "fork but no exec" flag, as we _are_ execing */
	p->p_acflag &= ~AFORK;

	/* Set entry address */
	setregs(p, iparams->entry_addr, (u_long)stack_base);

	/*
	 * free various allocated resources
	 */
	if (vm_map_remove(exec_map, (vm_offset_t)iparams->stringbase,
	    (vm_offset_t)iparams->stringbase + ARG_MAX))
		panic("execve: string buffer dealloc failed (1)");
	if (vm_map_remove(kernel_map, (vm_offset_t)image_header,
	    (vm_offset_t)image_header + PAGE_SIZE))
		panic("execve: header dealloc failed (2)");
	vrele(ndp->ni_vp);
	FREE(ndp->ni_cnd.cn_pnbuf, M_NAMEI);

	return (0);

exec_fail_dealloc:
	if (iparams->stringbase && iparams->stringbase != (char *)-1)
		if (vm_map_remove(exec_map, (vm_offset_t)iparams->stringbase,
		    (vm_offset_t)iparams->stringbase + ARG_MAX))
			panic("execve: string buffer dealloc failed (2)");
	if (iparams->image_header && iparams->image_header != (char *)-1)
		if (vm_map_remove(kernel_map, (vm_offset_t)image_header,
		    (vm_offset_t)image_header + PAGE_SIZE))
			panic("execve: header dealloc failed (3)");
	if (ndp->ni_vp)
		vrele(ndp->ni_vp);
	FREE(ndp->ni_cnd.cn_pnbuf, M_NAMEI);

exec_fail:
	if (iparams->vmspace_destroyed) {
		/* sorry, no more process anymore. exit gracefully */
		exit1(p, W_EXITCODE(0, SIGABRT));
		/* NOT REACHED */
		return(0);
	} else {
		return(error);
	}
}

/*
 * Destroy old address space, and allocate a new stack
 *	The new stack is only SGROWSIZ large because it is grown
 *	automatically in trap.c.
 */
int
exec_new_vmspace(iparams)
	struct image_params *iparams;
{
	int error;
	struct vmspace *vmspace = iparams->proc->p_vmspace;
	caddr_t	stack_addr = (caddr_t) (USRSTACK - SGROWSIZ);

	iparams->vmspace_destroyed = 1;

	/* Blow away entire process VM */
#ifdef SYSVSHM
	if (vmspace->vm_shm)
		shmexit(iparams->proc);
#endif
	vm_map_remove(&vmspace->vm_map, 0, USRSTACK);

	/* Allocate a new stack */
	error = vm_map_find(&vmspace->vm_map, NULL, 0, (vm_offset_t *)&stack_addr,
	    SGROWSIZ, FALSE);
	if (error)
		return(error);

	vmspace->vm_ssize = SGROWSIZ >> PAGE_SHIFT;

	/* Initialize maximum stack address */
	vmspace->vm_maxsaddr = (char *)USRSTACK - MAXSSIZ;

	return(0);
}

/*
 * Copy out argument and environment strings from the old process
 *	address space into the temporary string buffer.
 */
int
exec_extract_strings(iparams)
	struct image_params *iparams;
{
	char	**argv, **envv;
	char	*argp, *envp;
	int	error, length;

	/*
	 * extract arguments first
	 */

	argv = iparams->uap->argv;

	if (argv) {
		while ((argp = (caddr_t) fuword(argv++))) {
			if (argp == (caddr_t) -1)
				return (EFAULT);
			if ((error = copyinstr(argp, iparams->stringp,
			    iparams->stringspace, &length))) {
				if (error == ENAMETOOLONG)
					return(E2BIG);
				return (error);
			}
			iparams->stringspace -= length;
			iparams->stringp += length;
			iparams->argc++;
		}
	}

	/*
	 * extract environment strings
	 */

	envv = iparams->uap->envv;

	if (envv) {
		while ((envp = (caddr_t) fuword(envv++))) {
			if (envp == (caddr_t) -1)
				return (EFAULT);
			if ((error = copyinstr(envp, iparams->stringp,
			    iparams->stringspace, &length))) {
				if (error == ENAMETOOLONG)
					return(E2BIG);
				return (error);
			}
			iparams->stringspace -= length;
			iparams->stringp += length;
			iparams->envc++;
		}
	}

	return (0);
}

/*
 * Copy strings out to the new process address space, constructing
 *	new arg and env vector tables. Return a pointer to the base
 *	so that it can be used as the initial stack pointer.
 */
int *
exec_copyout_strings(iparams)
	struct image_params *iparams;
{
	int argc, envc;
	char **vectp;
	char *stringp, *destp;
	int *stack_base;
	struct ps_strings *arginfo;

	/*
	 * Calculate string base and vector table pointers.
	 */
	arginfo = PS_STRINGS;
	destp =	(caddr_t)arginfo - roundup((ARG_MAX - iparams->stringspace), sizeof(char *));
	/*
	 * The '+ 2' is for the null pointers at the end of each of the
	 *	arg and	env vector sets
	 */
	vectp = (char **) (destp -
		(iparams->argc + iparams->envc + 2) * sizeof(char *));

	/*
	 * vectp also becomes our initial stack base
	 */
	stack_base = (int *)vectp;

	stringp = iparams->stringbase;
	argc = iparams->argc;
	envc = iparams->envc;

	/*
	 * Copy out strings - arguments and environment.
	 */
	copyout(stringp, destp, ARG_MAX - iparams->stringspace);

	/*
	 * Fill in "ps_strings" struct for ps, w, etc.
	 */
	suword(&arginfo->ps_argvstr, (int)destp);
	suword(&arginfo->ps_nargvstr, argc);

	/*
	 * Fill in argument portion of vector table.
	 */
	for (; argc > 0; --argc) {
		suword(vectp++, (int)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* a null vector table pointer seperates the argp's from the envp's */
	suword(vectp++, NULL);

	suword(&arginfo->ps_envstr, (int)destp);
	suword(&arginfo->ps_nenvstr, envc);

	/*
	 * Fill in environment portion of vector table.
	 */
	for (; envc > 0; --envc) {
		suword(vectp++, (int)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* end of vector table is a null pointer */
	suword(vectp, NULL);

	return (stack_base);
}

/*
 * Check permissions of file to execute.
 *	Return 0 for success or error code on failure.
 */
static int
exec_check_permissions(iparams)
	struct image_params *iparams;
{
	struct proc *p = iparams->proc;
	struct vnode *vnodep = iparams->vnodep;
	struct vattr *attr = iparams->attr;
	int error;

	/*
	 * Check number of open-for-writes on the file and deny execution
	 *	if there are any.
	 */
	if (vnodep->v_writecount) {
		return (ETXTBSY);
	}

	/* Get file attributes */
	error = VOP_GETATTR(vnodep, attr, p->p_ucred, p);
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
	if ((vnodep->v_mount->mnt_flag & MNT_NOEXEC) ||
	    ((attr->va_mode & 0111) == 0) ||
	    (attr->va_type != VREG)) {
		return (EACCES);
	}

	/*
	 * Zero length files can't be exec'd
	 */
	if (attr->va_size == 0)
		return (ENOEXEC);

	/*
	 * Disable setuid/setgid if the filesystem prohibits it or if
	 *	the process is being traced.
	 */
        if ((vnodep->v_mount->mnt_flag & MNT_NOSUID) || (p->p_flag & P_TRACED))
		attr->va_mode &= ~(VSUID | VSGID);

	/*
	 *  Check for execute permission to file based on current credentials.
	 *	Then call filesystem specific open routine (which does nothing
	 *	in the general case).
	 */
	error = VOP_ACCESS(vnodep, VEXEC, p->p_ucred, p);
	if (error)
		return (error);

	error = VOP_OPEN(vnodep, FREAD, p->p_ucred, p);
	if (error)
		return (error);

	return (0);
}
