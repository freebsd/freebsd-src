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
 * 4. The name of the developer may be used to endorse or promote products
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
 *	$Id: kern_execve.c,v 1.20 1994/03/26 12:24:27 davidg Exp $
 */

#include "param.h"
#include "systm.h"
#include "signalvar.h"
#include "resourcevar.h"
#include "imgact.h"
#include "kernel.h"
#include "mount.h"
#include "file.h"
#include "acct.h"
#include "exec.h"
#include "stat.h"
#include "wait.h"
#include "mman.h"
#include "malloc.h"
#include "syslog.h"

#include "vm/vm.h"
#include "vm/vm_param.h"
#include "vm/vm_map.h"
#include "vm/vm_kern.h"
#include "vm/vm_user.h"

#include "machine/reg.h"

int exec_extract_strings __P((struct image_params *));
int *exec_copyout_strings __P((struct image_params *));

/*
 * execsw_set is constructed for us by the linker.  Each of the items
 * is a pointer to a `const struct execsw', hence the double pointer here.
 */
extern const struct linker_set execsw_set;
const struct execsw **execsw = (const struct execsw **)&execsw_set.ls_items[0];

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
	char *stringbase, *stringp;
	int *stack_base;
	int error, resid, len, i;
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
	error = vm_allocate(kernel_map, (vm_offset_t *)&iparams->stringbase, 
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
	ndp->ni_nameiop = LOOKUP | LOCKLEAF | FOLLOW | SAVENAME;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;

interpret:

	error = namei(ndp, p);
	if (error) {
		vm_deallocate(kernel_map, (vm_offset_t)iparams->stringbase, 
			      ARG_MAX);
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
			MAP_FILE, 			/* flags */
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
			vput(ndp->ni_vp);
			FREE(ndp->ni_pnbuf, M_NAMEI);
			if (vm_deallocate(kernel_map, 
					  (vm_offset_t)image_header, PAGE_SIZE))
				panic("execve: header dealloc failed (1)");

			/* set new name to that of the interpreter */
			ndp->ni_segflg = UIO_SYSSPACE;
			ndp->ni_dirp = iparams->interpreter_name;
			ndp->ni_nameiop = LOOKUP | LOCKLEAF | FOLLOW | SAVENAME;
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
	 * Stuff argument count as first item on stack
	 */
	*(--stack_base) = iparams->argc;

	/* close files on exec */
	fdcloseexec(p);

	/* reset caught signals */
	execsigs(p);

	/* name this process - nameiexec(p, ndp) */
	len = MIN(ndp->ni_namelen,MAXCOMLEN);
	bcopy(ndp->ni_ptr, p->p_comm, len);
	p->p_comm[len] = 0;
	
	/*
	 * mark as executable, wakeup any process that was vforked and tell
	 * it that it now has it's own resources back
	 */
	p->p_flag |= SEXEC;
	if (p->p_pptr && (p->p_flag & SPPWAIT)) {
		p->p_flag &= ~SPPWAIT;
		wakeup((caddr_t)p->p_pptr);
	}
	
	/* implement set userid/groupid */
	p->p_flag &= ~SUGID;

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
	if ((attr.va_mode & VSUID) && (p->p_flag & STRC) == 0) {
		p->p_ucred = crcopy(p->p_ucred);
		p->p_ucred->cr_uid = attr.va_uid;
		p->p_flag |= SUGID;
	}
	if ((attr.va_mode & VSGID) && (p->p_flag & STRC) == 0) {
		p->p_ucred = crcopy(p->p_ucred);
		p->p_ucred->cr_groups[0] = attr.va_gid;
		p->p_flag |= SUGID;
	}

	/*
	 * Implement correct POSIX saved uid behavior.
	 */
	p->p_cred->p_svuid = p->p_ucred->cr_uid;
	p->p_cred->p_svgid = p->p_ucred->cr_gid;

	/* mark vnode pure text */
 	ndp->ni_vp->v_flag |= VTEXT;

	/*
	 * If tracing the process, trap to debugger so breakpoints
	 * 	can be set before the program executes.
	 */
	if (p->p_flag & STRC)
		psignal(p, SIGTRAP);

	/* clear "fork but no exec" flag, as we _are_ execing */
	p->p_acflag &= ~AFORK;

	/* Set entry address */
	setregs(p, iparams->entry_addr, stack_base);

	/*
	 * free various allocated resources
	 */
	if (vm_deallocate(kernel_map, (vm_offset_t)iparams->stringbase, ARG_MAX))
		panic("execve: string buffer dealloc failed (1)");
	if (vm_deallocate(kernel_map, (vm_offset_t)image_header, PAGE_SIZE))
		panic("execve: header dealloc failed (2)");
	vput(ndp->ni_vp);
	FREE(ndp->ni_pnbuf, M_NAMEI);

	return (0);

exec_fail_dealloc:
	if (iparams->stringbase && iparams->stringbase != (char *)-1)
		if (vm_deallocate(kernel_map, (vm_offset_t)iparams->stringbase,
				  ARG_MAX))
			panic("execve: string buffer dealloc failed (2)");
	if (iparams->image_header && iparams->image_header != (char *)-1)
		if (vm_deallocate(kernel_map, 
				  (vm_offset_t)iparams->image_header, PAGE_SIZE))
			panic("execve: header dealloc failed (3)");
	vput(ndp->ni_vp);
	FREE(ndp->ni_pnbuf, M_NAMEI);

exec_fail:
	if (iparams->vmspace_destroyed) {
		/* sorry, no more process anymore. exit gracefully */
#if 0	/* XXX */
		vm_deallocate(&vs->vm_map, USRSTACK - MAXSSIZ, MAXSSIZ);
#endif
		kexit(p, W_EXITCODE(0, SIGABRT));
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
	vm_deallocate(&vmspace->vm_map, 0, USRSTACK);

	/* Allocate a new stack */
	error = vm_allocate(&vmspace->vm_map, (vm_offset_t *)&stack_addr,
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
	int	length;

	/*
	 * extract arguments first
	 */

	argv = iparams->uap->argv; 

	if (argv)
		while (argp = (caddr_t) fuword(argv++)) {
			if (argp == (caddr_t) -1)
				return (EFAULT);
			if (copyinstr(argp, iparams->stringp, iparams->stringspace,
				&length) == ENAMETOOLONG)
					return(E2BIG);
			iparams->stringspace -= length;
			iparams->stringp += length;
			iparams->argc++;
		}

	/*
	 * extract environment strings
	 */

	envv = iparams->uap->envv; 

	if (envv)
		while (envp = (caddr_t) fuword(envv++)) {
			if (envp == (caddr_t) -1)
				return (EFAULT);
			if (copyinstr(envp, iparams->stringp, iparams->stringspace,
				&length) == ENAMETOOLONG)
					return(E2BIG);
			iparams->stringspace -= length;
			iparams->stringp += length;
			iparams->envc++;
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
	int vect_table_size, string_table_size;

	/*
	 * Calculate string base and vector table pointers.
	 */
	destp = (caddr_t) ((caddr_t)USRSTACK -
		roundup((ARG_MAX - iparams->stringspace), sizeof(char *)));
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

	for (; argc > 0; --argc) {
		*(vectp++) = destp;
		while (*destp++ = *stringp++);
	}

	/* a null vector table pointer seperates the argp's from the envp's */
	*(vectp++) = NULL;

	for (; envc > 0; --envc) {
		*(vectp++) = destp;
		while (*destp++ = *stringp++);
	}

	/* end of vector table is a null pointer */
	*vectp = NULL;

	return (stack_base);
}

/*
 * Check permissions of file to execute.
 *	Return 0 for success or error code on failure.
 */
int
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
        if ((vnodep->v_mount->mnt_flag & MNT_NOSUID) || (p->p_flag & STRC))
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
