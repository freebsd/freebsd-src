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
 *    derived from this software withough specific prior written permission
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
 *
 * $FreeBSD$
 */

/* XXX we use functions that might not exist. */
#include "opt_compat.h"

#ifndef COMPAT_43
#error "Unable to compile Linux-emulator due to missing COMPAT_43 option!"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/imgact.h>
#include <sys/imgact_aout.h>
#include <sys/imgact_elf.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>
#include <sys/exec.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/cpu.h>
#include <machine/md_var.h>

#include <alpha/linux/linux.h>
#include <linux_proto.h>
#include <compat/linux/linux_util.h>
#undef szsigcode

MODULE_VERSION(linux, 1);
MODULE_DEPEND(linux, osf1, 1, 1, 1);

MALLOC_DEFINE(M_LINUX, "linux", "Linux mode structures");

#if BYTE_ORDER == LITTLE_ENDIAN
#define	SHELLMAGIC	0x2123 /* #! */
#else
#define	SHELLMAGIC	0x2321
#endif


extern struct linker_set linux_ioctl_handler_set;

static int	elf_linux_fixup __P((long **stack_base,
    struct image_params *iparams));

static int
elf_linux_fixup(long **stack_base, struct image_params *imgp)
{
	long *pos;
	Elf64_Auxargs *args;

	args = (Elf64_Auxargs *)imgp->auxargs;
	pos = *stack_base + (imgp->argc + imgp->envc + 2);

	if (args->trace) {
		AUXARGS_ENTRY(pos, AT_DEBUG, 1);
	}
	if (args->execfd != -1) {
		AUXARGS_ENTRY(pos, AT_EXECFD, args->execfd);
	}       
	AUXARGS_ENTRY(pos, AT_PHDR, args->phdr);
	AUXARGS_ENTRY(pos, AT_PHENT, args->phent);
	AUXARGS_ENTRY(pos, AT_PHNUM, args->phnum);
	AUXARGS_ENTRY(pos, AT_PAGESZ, args->pagesz);
	AUXARGS_ENTRY(pos, AT_FLAGS, args->flags);
	AUXARGS_ENTRY(pos, AT_ENTRY, args->entry);
	AUXARGS_ENTRY(pos, AT_BASE, args->base);
	AUXARGS_ENTRY(pos, AT_UID, imgp->proc->p_cred->p_ruid);
	AUXARGS_ENTRY(pos, AT_EUID, imgp->proc->p_cred->p_svuid);
	AUXARGS_ENTRY(pos, AT_GID, imgp->proc->p_cred->p_rgid);
	AUXARGS_ENTRY(pos, AT_EGID, imgp->proc->p_cred->p_svgid);
	AUXARGS_ENTRY(pos, AT_NULL, 0);
	
	free(imgp->auxargs, M_TEMP);      
	imgp->auxargs = NULL;

	(*stack_base)--;
	**stack_base = (long)imgp->argc;
	return 0;
}

extern int _ucodesel, _udatasel;

void osf1_sendsig __P((sig_t, int , sigset_t *, u_long ));
void osendsig(sig_t catcher, int sig, sigset_t *mask, u_long code);

/*
 * If a linux binary is exec'ing something, try this image activator 
 * first.  We override standard shell script execution in order to
 * be able to modify the interpreter path.  We only do this if a linux
 * binary is doing the exec, so we do not create an EXEC module for it.
 */
static int	exec_linux_imgact_try __P((struct image_params *iparams));

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

			linux_emul_find(imgp->proc, NULL, linux_emul_path, 
			    imgp->interpreter_name, &rpath, 0);
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
	0,
	0,
	0,
	0,
	elf_linux_fixup,
	osendsig,
	linux_sigcode,
	&linux_szsigcode,
	0,
	"Linux ELF",
	elf_coredump,
	exec_linux_imgact_try
};

static Elf64_Brandinfo linux_brand = {
					ELFOSABI_LINUX,
					"Linux",
					"/compat/linux",
					"/lib/ld-linux.so.1",
					&elf_linux_sysvec
				 };

static Elf64_Brandinfo linux_glibc2brand = {
					ELFOSABI_LINUX,
					"Linux",
					"/compat/linux",
					"/lib/ld-linux.so.2",
					&elf_linux_sysvec
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

	error = 0;

	switch(type) {
	case MOD_LOAD:
		for (brandinfo = &linux_brandlist[0]; *brandinfo != NULL;
		    ++brandinfo)
			if (elf_insert_brand_entry(*brandinfo) < 0)
				error = EINVAL;
		if (error)
			printf("cannot insert Linux elf brand handler\n");
		else {
			linux_ioctl_register_handlers(&linux_ioctl_handler_set);
			if (bootverbose)
				printf("Linux-ELF exec handler installed\n");
		}
		break;
	case MOD_UNLOAD:
		linux_ioctl_unregister_handlers(&linux_ioctl_handler_set);
		for (brandinfo = &linux_brandlist[0]; *brandinfo != NULL;
		    ++brandinfo)
			if (elf_brand_inuse(*brandinfo))
				error = EBUSY;

		if (error == 0) {
			for (brandinfo = &linux_brandlist[0];
			    *brandinfo != NULL; ++brandinfo)
				if (elf_remove_brand_entry(*brandinfo) < 0)
					error = EINVAL;
		}
		if (error)
			printf("Could not deinstall ELF interpreter entry\n");
		else if (bootverbose)
			printf("Linux-elf exec handler removed\n");
		break;
	default:
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
