/*-
 * Copyright (c) 1994-1996 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* XXX we use functions that might not exist. */
#include "opt_compat.h"

#ifndef COMPAT_43
#error "Unable to compile Linux-emulator due to missing COMPAT_43 option!"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_aout.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/md_var.h>

#include <alpha/linux/linux.h>
#include <alpha/linux/linux_proto.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_util.h>
#undef szsigcode

MODULE_VERSION(linux, 1);
MODULE_DEPEND(linux, osf1, 1, 1, 1);
MODULE_DEPEND(linux, sysvmsg, 1, 1, 1);
MODULE_DEPEND(linux, sysvsem, 1, 1, 1);
MODULE_DEPEND(linux, sysvshm, 1, 1, 1);

MALLOC_DEFINE(M_LINUX, "linux", "Linux mode structures");

#if BYTE_ORDER == LITTLE_ENDIAN
#define	SHELLMAGIC	0x2123 /* #! */
#else
#define	SHELLMAGIC	0x2321
#endif

SET_DECLARE(linux_ioctl_handler_set, struct linux_ioctl_handler);

void osendsig(sig_t catcher, int sig, sigset_t *mask, u_long code);

static int	elf_linux_fixup(register_t **stack_base,
    struct image_params *iparams);
static int	exec_linux_imgact_try(struct image_params *iparams);

static int
elf_linux_fixup(register_t **stack_base, struct image_params *imgp)
{
	Elf64_Auxargs *args;
	register_t *pos;

	KASSERT(curthread->td_proc == imgp->proc &&
	    (curthread->td_proc->p_flag & P_SA) == 0,
	    ("unsafe elf_linux_fixup(), should be curproc"));
	args = (Elf64_Auxargs *)imgp->auxargs;
	pos = *stack_base + (imgp->args->argc + imgp->args->envc + 2);

	if (args->trace)
		AUXARGS_ENTRY(pos, AT_DEBUG, 1);
	if (args->execfd != -1)
		AUXARGS_ENTRY(pos, AT_EXECFD, args->execfd);
	AUXARGS_ENTRY(pos, AT_PHDR, args->phdr);
	AUXARGS_ENTRY(pos, AT_PHENT, args->phent);
	AUXARGS_ENTRY(pos, AT_PHNUM, args->phnum);
	AUXARGS_ENTRY(pos, AT_PAGESZ, args->pagesz);
	AUXARGS_ENTRY(pos, AT_FLAGS, args->flags);
	AUXARGS_ENTRY(pos, AT_ENTRY, args->entry);
	AUXARGS_ENTRY(pos, AT_BASE, args->base);
	AUXARGS_ENTRY(pos, AT_UID, imgp->proc->p_ucred->cr_ruid);
	AUXARGS_ENTRY(pos, AT_EUID, imgp->proc->p_ucred->cr_svuid);
	AUXARGS_ENTRY(pos, AT_GID, imgp->proc->p_ucred->cr_rgid);
	AUXARGS_ENTRY(pos, AT_EGID, imgp->proc->p_ucred->cr_svgid);
	AUXARGS_ENTRY(pos, AT_NULL, 0);
	
	free(imgp->auxargs, M_TEMP);      
	imgp->auxargs = NULL;

	(*stack_base)--;
	**stack_base = (register_t)imgp->args->argc;
	return 0;
}

/*
 * If a linux binary is exec'ing something, try this image activator 
 * first.  We override standard shell script execution in order to
 * be able to modify the interpreter path.  We only do this if a linux
 * binary is doing the exec, so we do not create an EXEC module for it.
 */
static int
exec_linux_imgact_try(imgp)
	struct image_params *imgp;
{
	const char *head;
	int error;

	head = (const char *)imgp->image_header;
	error = -1;

	/*
	 * The interpreter for shell scripts run from a linux binary needs
	 * to be located in /compat/linux if possible in order to recursively
	 * maintain linux path emulation.
	 */
	if (((const short *)head)[0] == SHELLMAGIC) {
		/*
		 * Run our normal shell image activator.  If it succeeds
		 * attempt to use the alternate path for the interpreter.  If
		 * an alternate path is found, use our stringspace to store it.
		 */
		if ((error = exec_shell_imgact(imgp)) == 0) {
			char *rpath = NULL;

			linux_emul_convpath(FIRST_THREAD_IN_PROC(imgp->proc),
			    imgp->interpreter_name, UIO_SYSSPACE, &rpath, 0);
			if (rpath != imgp->interpreter_name) {
				int len = strlen(rpath) + 1;

				if (len <= MAXSHELLCMDLEN) {
					memcpy(imgp->interpreter_name, rpath,
					    len);
				}
				free(rpath, M_TEMP);
			}
		}
	}
	return(error);
}

/*
 * To maintain OSF/1 compat, linux uses BSD signals & errnos on their
 * alpha port.  This greatly simplfies things for us.
 */

struct sysentvec elf_linux_sysvec = {
	LINUX_SYS_MAXSYSCALL,
	linux_sysent,
	0,
	0,
	NULL,
	0,
	NULL,
	NULL,
	elf_linux_fixup,
	osendsig,
	linux_sigcode,
	&linux_szsigcode,
	NULL,
	"Linux ELF",
	elf64_coredump,
	exec_linux_imgact_try,
	LINUX_MINSIGSTKSZ,
	PAGE_SIZE,
	VM_MIN_ADDRESS,
	VM_MAXUSER_ADDRESS,
	USRSTACK,
	PS_STRINGS,
	VM_PROT_ALL,
	exec_copyout_strings,
	exec_setregs,
	NULL
};

static Elf64_Brandinfo linux_brand = {
					ELFOSABI_LINUX,
					EM_ALPHA,
					"Linux",
					"/compat/linux",
					"/lib/ld-linux.so.1",
					&elf_linux_sysvec,
					NULL,
				 };

static Elf64_Brandinfo linux_glibc2brand = {
					ELFOSABI_LINUX,
					EM_ALPHA,
					"Linux",
					"/compat/linux",
					"/lib/ld-linux.so.2",
					&elf_linux_sysvec,
					NULL,
				 };

Elf64_Brandinfo *linux_brandlist[] = {
					&linux_brand,
					&linux_glibc2brand,
					NULL
				};

static int
linux_elf_modevent(module_t mod, int type, void *data)
{
	Elf64_Brandinfo **brandinfo;
	int error;
	struct linux_ioctl_handler **lihp;

	error = 0;

	switch(type) {
	case MOD_LOAD:
		for (brandinfo = &linux_brandlist[0]; *brandinfo != NULL;
		    ++brandinfo)
			if (elf64_insert_brand_entry(*brandinfo) < 0)
				error = EINVAL;
		if (error == 0) {
			SET_FOREACH(lihp, linux_ioctl_handler_set)
				linux_ioctl_register_handler(*lihp);
			if (bootverbose)
				printf("Linux ELF exec handler installed\n");
		} else
			printf("cannot insert Linux ELF brand handler\n");
		break;
	case MOD_UNLOAD:
		for (brandinfo = &linux_brandlist[0]; *brandinfo != NULL;
		    ++brandinfo)
			if (elf64_brand_inuse(*brandinfo))
				error = EBUSY;
		if (error == 0) {
			for (brandinfo = &linux_brandlist[0];
			    *brandinfo != NULL; ++brandinfo)
				if (elf64_remove_brand_entry(*brandinfo) < 0)
					error = EINVAL;
		}
		if (error == 0) {
			SET_FOREACH(lihp, linux_ioctl_handler_set)
				linux_ioctl_unregister_handler(*lihp);
			if (bootverbose)
				printf("Linux ELF exec handler removed\n");
		} else
			printf("Could not deinstall ELF interpreter entry\n");
		break;
	default:
		return (EOPNOTSUPP);
		break;
	}
	return error;
}

static moduledata_t linux_elf_mod = {
	"linuxelf",
	linux_elf_modevent,
	0
};

DUMMY(rt_sigreturn);

DECLARE_MODULE(linuxelf, linux_elf_mod, SI_SUB_EXEC, SI_ORDER_ANY);
