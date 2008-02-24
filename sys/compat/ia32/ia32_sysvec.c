/*-
 * Copyright (c) 2002 Doug Rabson
 * Copyright (c) 2003 Peter Wemm
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/compat/ia32/ia32_sysvec.c,v 1.27 2007/09/24 20:49:39 jhb Exp $");

#include "opt_compat.h"

#define __ELF_WORD_SIZE 32

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/namei.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/imgact_elf.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <compat/freebsd32/freebsd32_signal.h>
#include <compat/freebsd32/freebsd32_util.h>
#include <compat/freebsd32/freebsd32_proto.h>
#include <compat/freebsd32/freebsd32_syscall.h>
#include <compat/ia32/ia32_signal.h>
#ifdef __amd64__
#include <machine/psl.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#else
#include <i386/include/psl.h>
#include <i386/include/segments.h>
#include <i386/include/specialreg.h>
#endif
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/cpufunc.h>

CTASSERT(sizeof(struct ia32_mcontext) == 640);
CTASSERT(sizeof(struct ia32_ucontext) == 704);
CTASSERT(sizeof(struct ia32_sigframe) == 800);
CTASSERT(sizeof(struct siginfo32) == 64);
#ifdef COMPAT_FREEBSD4
CTASSERT(sizeof(struct ia32_mcontext4) == 260);
CTASSERT(sizeof(struct ia32_ucontext4) == 324);
CTASSERT(sizeof(struct ia32_sigframe4) == 408);
#endif

static register_t *ia32_copyout_strings(struct image_params *imgp);
static void ia32_fixlimit(struct rlimit *rl, int which);

extern struct sysent freebsd32_sysent[];

SYSCTL_NODE(_compat, OID_AUTO, ia32, CTLFLAG_RW, 0, "ia32 mode");

static u_long	ia32_maxdsiz = IA32_MAXDSIZ;
SYSCTL_ULONG(_compat_ia32, OID_AUTO, maxdsiz, CTLFLAG_RW, &ia32_maxdsiz, 0, "");
TUNABLE_ULONG("compat.ia32.maxdsiz", &ia32_maxdsiz);
static u_long	ia32_maxssiz = IA32_MAXSSIZ;
SYSCTL_ULONG(_compat_ia32, OID_AUTO, maxssiz, CTLFLAG_RW, &ia32_maxssiz, 0, "");
TUNABLE_ULONG("compat.ia32.maxssiz", &ia32_maxssiz);
static u_long	ia32_maxvmem = IA32_MAXVMEM;
SYSCTL_ULONG(_compat_ia32, OID_AUTO, maxvmem, CTLFLAG_RW, &ia32_maxvmem, 0, "");
TUNABLE_ULONG("compat.ia32.maxvmem", &ia32_maxvmem);

struct sysentvec ia32_freebsd_sysvec = {
	FREEBSD32_SYS_MAXSYSCALL,
	freebsd32_sysent,
	0,
	0,
	NULL,
	0,
	NULL,
	NULL,
	elf32_freebsd_fixup,
	ia32_sendsig,
	ia32_sigcode,
	&sz_ia32_sigcode,
	NULL,
	"FreeBSD ELF32",
	elf32_coredump,
	NULL,
	MINSIGSTKSZ,
	IA32_PAGE_SIZE,
	0,
	FREEBSD32_USRSTACK,
	FREEBSD32_USRSTACK,
	FREEBSD32_PS_STRINGS,
	VM_PROT_ALL,
	ia32_copyout_strings,
	ia32_setregs,
	ia32_fixlimit,
	&ia32_maxssiz
};


static Elf32_Brandinfo ia32_brand_info = {
						ELFOSABI_FREEBSD,
						EM_386,
						"FreeBSD",
						NULL,
						"/libexec/ld-elf.so.1",
						&ia32_freebsd_sysvec,
						"/libexec/ld-elf32.so.1",
						BI_CAN_EXEC_DYN,
					  };

SYSINIT(ia32, SI_SUB_EXEC, SI_ORDER_ANY,
	(sysinit_cfunc_t) elf32_insert_brand_entry,
	&ia32_brand_info);

static Elf32_Brandinfo ia32_brand_oinfo = {
						ELFOSABI_FREEBSD,
						EM_386,
						"FreeBSD",
						NULL,
						"/usr/libexec/ld-elf.so.1",
						&ia32_freebsd_sysvec,
						"/libexec/ld-elf32.so.1",
						BI_CAN_EXEC_DYN,
					  };

SYSINIT(oia32, SI_SUB_EXEC, SI_ORDER_ANY,
	(sysinit_cfunc_t) elf32_insert_brand_entry,
	&ia32_brand_oinfo);


void
elf32_dump_thread(struct thread *td __unused, void *dst __unused,
    size_t *off __unused)
{
}


/* XXX may be freebsd32 MI */
static register_t *
ia32_copyout_strings(struct image_params *imgp)
{
	int argc, envc;
	u_int32_t *vectp;
	char *stringp, *destp;
	u_int32_t *stack_base;
	struct freebsd32_ps_strings *arginfo;
	int szsigcode;

	/*
	 * Calculate string base and vector table pointers.
	 * Also deal with signal trampoline code for this exec type.
	 */
	arginfo = (struct freebsd32_ps_strings *)FREEBSD32_PS_STRINGS;
	szsigcode = *(imgp->proc->p_sysent->sv_szsigcode);
	destp =	(caddr_t)arginfo - szsigcode - SPARE_USRSPACE -
		roundup((ARG_MAX - imgp->args->stringspace), sizeof(char *));

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
		vectp = (u_int32_t *) (destp - (imgp->args->argc + imgp->args->envc + 2 +
				       imgp->auxarg_size) * sizeof(u_int32_t));

	} else
		/*
		 * The '+ 2' is for the null pointers at the end of each of
		 * the arg and env vector sets
		 */
		vectp = (u_int32_t *)
			(destp - (imgp->args->argc + imgp->args->envc + 2) * sizeof(u_int32_t));

	/*
	 * vectp also becomes our initial stack base
	 */
	stack_base = vectp;

	stringp = imgp->args->begin_argv;
	argc = imgp->args->argc;
	envc = imgp->args->envc;
	/*
	 * Copy out strings - arguments and environment.
	 */
	copyout(stringp, destp, ARG_MAX - imgp->args->stringspace);

	/*
	 * Fill in "ps_strings" struct for ps, w, etc.
	 */
	suword32(&arginfo->ps_argvstr, (u_int32_t)(intptr_t)vectp);
	suword32(&arginfo->ps_nargvstr, argc);

	/*
	 * Fill in argument portion of vector table.
	 */
	for (; argc > 0; --argc) {
		suword32(vectp++, (u_int32_t)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* a null vector table pointer separates the argp's from the envp's */
	suword32(vectp++, 0);

	suword32(&arginfo->ps_envstr, (u_int32_t)(intptr_t)vectp);
	suword32(&arginfo->ps_nenvstr, envc);

	/*
	 * Fill in environment portion of vector table.
	 */
	for (; envc > 0; --envc) {
		suword32(vectp++, (u_int32_t)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* end of vector table is a null pointer */
	suword32(vectp, 0);

	return ((register_t *)stack_base);
}

static void
ia32_fixlimit(struct rlimit *rl, int which)
{

	switch (which) {
	case RLIMIT_DATA:
		if (ia32_maxdsiz != 0) {
			if (rl->rlim_cur > ia32_maxdsiz)
				rl->rlim_cur = ia32_maxdsiz;
			if (rl->rlim_max > ia32_maxdsiz)
				rl->rlim_max = ia32_maxdsiz;
		}
		break;
	case RLIMIT_STACK:
		if (ia32_maxssiz != 0) {
			if (rl->rlim_cur > ia32_maxssiz)
				rl->rlim_cur = ia32_maxssiz;
			if (rl->rlim_max > ia32_maxssiz)
				rl->rlim_max = ia32_maxssiz;
		}
		break;
	case RLIMIT_VMEM:
		if (ia32_maxvmem != 0) {
			if (rl->rlim_cur > ia32_maxvmem)
				rl->rlim_cur = ia32_maxvmem;
			if (rl->rlim_max > ia32_maxvmem)
				rl->rlim_max = ia32_maxvmem;
		}
		break;
	}
}
