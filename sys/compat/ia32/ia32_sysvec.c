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
 *
 * $FreeBSD$
 */

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

#include <amd64/ia32/ia32_util.h>
#include <amd64/ia32/ia32_proto.h>
#include <amd64/ia32/ia32_signal.h>
#include <machine/psl.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/cpufunc.h>

static register_t *ia32_copyout_strings(struct image_params *imgp);
static void ia32_setregs(struct thread *td, u_long entry, u_long stack,
    u_long ps_strings);

extern struct sysent ia32_sysent[];

struct sysentvec ia32_freebsd_sysvec = {
	SYS_MAXSYSCALL,
	ia32_sysent,
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
	PAGE_SIZE,
	0,
	IA32_USRSTACK,
	IA32_USRSTACK,
	IA32_PS_STRINGS,
	VM_PROT_ALL,
	ia32_copyout_strings,
	ia32_setregs
};



static Elf32_Brandinfo ia32_brand_info = {
						ELFOSABI_FREEBSD,
						EM_386,
						"FreeBSD",
						"/compat/ia32",
						"/usr/libexec/ld-elf.so.1",
						&ia32_freebsd_sysvec
					  };

SYSINIT(ia32, SI_SUB_EXEC, SI_ORDER_ANY,
	(sysinit_cfunc_t) elf32_insert_brand_entry,
	&ia32_brand_info);

extern int _ucode32sel, _udatasel;

static register_t *
ia32_copyout_strings(struct image_params *imgp)
{
	int argc, envc;
	u_int32_t *vectp;
	char *stringp, *destp;
	u_int32_t *stack_base;
	struct ia32_ps_strings *arginfo;
	int szsigcode;

	/*
	 * Calculate string base and vector table pointers.
	 * Also deal with signal trampoline code for this exec type.
	 */
	arginfo = (struct ia32_ps_strings *)IA32_PS_STRINGS;
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
		vectp = (u_int32_t *) (destp - (imgp->argc + imgp->envc + 2 +
				       imgp->auxarg_size) * sizeof(u_int32_t));

	} else
		/*
		 * The '+ 2' is for the null pointers at the end of each of
		 * the arg and env vector sets
		 */
		vectp = (u_int32_t *)
			(destp - (imgp->argc + imgp->envc + 2) * sizeof(u_int32_t));

	/*
	 * vectp also becomes our initial stack base
	 */
	stack_base = vectp;

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

/*
 * Clear registers on exec
 */
void
ia32_setregs(td, entry, stack, ps_strings)
	struct thread *td;
	u_long entry;
	u_long stack;
	u_long ps_strings;
{
	struct trapframe *regs = td->td_frame;
	struct pcb *pcb = td->td_pcb;
	
	wrmsr(MSR_FSBASE, 0);
	wrmsr(MSR_KGSBASE, 0);	/* User value while we're in the kernel */
	pcb->pcb_fsbase = 0;
	pcb->pcb_gsbase = 0;
	load_ds(_udatasel);
	load_es(_udatasel);
	load_fs(_udatasel);
	load_gs(_udatasel);
	pcb->pcb_ds = _udatasel;
	pcb->pcb_es = _udatasel;
	pcb->pcb_fs = _udatasel;
	pcb->pcb_gs = _udatasel;

	bzero((char *)regs, sizeof(struct trapframe));
	regs->tf_rip = entry;
	regs->tf_rsp = stack;
	regs->tf_rflags = PSL_USER | (regs->tf_rflags & PSL_T);
	regs->tf_ss = _udatasel;
	regs->tf_cs = _ucode32sel;
	regs->tf_rbx = ps_strings;

	/*
	 * Arrange to trap the next npx or `fwait' instruction (see npx.c
	 * for why fwait must be trapped at least if there is an npx or an
	 * emulator).  This is mainly to handle the case where npx0 is not
	 * configured, since the npx routines normally set up the trap
	 * otherwise.  It should be done only at boot time, but doing it
	 * here allows modifying `npx_exists' for testing the emulator on
	 * systems with an npx.
	 */
	load_cr0(rcr0() | CR0_MP | CR0_TS);

	fpstate_drop(td);

	/* Return via doreti so that we can change to a different %cs */
	pcb->pcb_flags |= PCB_FULLCTX;
	td->td_retval[1] = 0;
}
