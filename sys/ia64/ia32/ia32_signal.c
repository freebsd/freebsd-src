/*-
 * Copyright (c) 2002 Doug Rabson
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
__FBSDID("$FreeBSD$");

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
#include <sys/sysproto.h>

#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

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
#include <compat/ia32/ia32_signal.h>
#include <x86/include/psl.h>
#include <x86/include/segments.h>
#include <x86/include/specialreg.h>

char ia32_sigcode[] = {
	0xff, 0x54, 0x24, 0x10,		/* call *SIGF_HANDLER(%esp) */
	0x8d, 0x44, 0x24, 0x14,		/* lea SIGF_UC(%esp),%eax */
	0x50,				/* pushl %eax */
	0xf7, 0x40, 0x54, 0x00, 0x00, 0x02, 0x02, /* testl $PSL_VM,UC_EFLAGS(%ea
x) */
	0x75, 0x03,			/* jne 9f */
	0x8e, 0x68, 0x14,		/* movl UC_GS(%eax),%gs */
	0xb8, 0x57, 0x01, 0x00, 0x00,	/* 9: movl $SYS_sigreturn,%eax */
	0x50,				/* pushl %eax */
	0xcd, 0x80,			/* int $0x80 */
	0xeb, 0xfe,			/* 0: jmp 0b */
	0
};
int sz_ia32_sigcode = sizeof(ia32_sigcode);

#ifdef COMPAT_43
int
ofreebsd32_sigreturn(struct thread *td, struct ofreebsd32_sigreturn_args *uap)
{

	return (EOPNOTSUPP);
}
#endif

/*
 * Signal sending has not been implemented on ia64.  This causes
 * the sigtramp code to not understand the arguments and the application
 * will generally crash if it tries to handle a signal.  Calling
 * sendsig() means that at least untrapped signals will work.
 */
void
ia32_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	sendsig(catcher, ksi, mask);
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_freebsd32_sigreturn(struct thread *td, struct freebsd4_freebsd32_sigreturn_args *uap)
{
	return (sys_sigreturn(td, (struct sigreturn_args *)uap));
}
#endif

int
freebsd32_sigreturn(struct thread *td, struct freebsd32_sigreturn_args *uap)
{
	return (sys_sigreturn(td, (struct sigreturn_args *)uap));
}


void
ia32_setregs(struct thread *td, struct image_params *imgp, u_long stack)
{
	struct trapframe *tf = td->td_frame;
	vm_offset_t gdt, ldt;
	u_int64_t codesel, datasel, ldtsel;
	u_int64_t codeseg, dataseg, gdtseg, ldtseg;
	struct segment_descriptor desc;
	struct vmspace *vmspace = td->td_proc->p_vmspace;
	struct sysentvec *sv;

	sv = td->td_proc->p_sysent;
	exec_setregs(td, imgp, stack);

	/* Non-syscall frames are cleared by exec_setregs() */
	if (tf->tf_flags & FRAME_SYSCALL) {
		bzero(&tf->tf_scratch, sizeof(tf->tf_scratch));
		bzero(&tf->tf_scratch_fp, sizeof(tf->tf_scratch_fp));
	} else
		tf->tf_special.ndirty = 0;

	tf->tf_special.psr |= IA64_PSR_IS;
	tf->tf_special.sp = stack;

	/* Point the RSE backstore to something harmless. */
	tf->tf_special.bspstore = (sv->sv_psstrings - sz_ia32_sigcode -
	    SPARE_USRSPACE + 15) & ~15;

	codesel = LSEL(LUCODE_SEL, SEL_UPL);
	datasel = LSEL(LUDATA_SEL, SEL_UPL);
	ldtsel = GSEL(GLDT_SEL, SEL_UPL);

	/* Setup ia32 segment registers. */
	tf->tf_scratch.gr16 = (datasel << 48) | (datasel << 32) |
	    (datasel << 16) | datasel;
	tf->tf_scratch.gr17 = (ldtsel << 32) | (datasel << 16) | codesel;

	/*
	 * Build the GDT and LDT.
	 */
	gdt = sv->sv_usrstack;
	vm_map_find(&vmspace->vm_map, 0, 0, &gdt, IA32_PAGE_SIZE << 1, 0,
	    VM_PROT_ALL, VM_PROT_ALL, 0);
	ldt = gdt + IA32_PAGE_SIZE;

	desc.sd_lolimit = 8*NLDT-1;
	desc.sd_lobase = ldt & 0xffffff;
	desc.sd_type = SDT_SYSLDT;
	desc.sd_dpl = SEL_UPL;
	desc.sd_p = 1;
	desc.sd_hilimit = 0;
	desc.sd_def32 = 0;
	desc.sd_gran = 0;
	desc.sd_hibase = ldt >> 24;
	copyout(&desc, (caddr_t) gdt + 8*GLDT_SEL, sizeof(desc));

	desc.sd_lolimit = ((sv->sv_usrstack >> 12) - 1) & 0xffff;
	desc.sd_lobase = 0;
	desc.sd_type = SDT_MEMERA;
	desc.sd_dpl = SEL_UPL;
	desc.sd_p = 1;
	desc.sd_hilimit = ((sv->sv_usrstack >> 12) - 1) >> 16;
	desc.sd_def32 = 1;
	desc.sd_gran = 1;
	desc.sd_hibase = 0;
	copyout(&desc, (caddr_t) ldt + 8*LUCODE_SEL, sizeof(desc));
	desc.sd_type = SDT_MEMRWA;
	copyout(&desc, (caddr_t) ldt + 8*LUDATA_SEL, sizeof(desc));

	codeseg = 0		/* base */
		+ (((sv->sv_usrstack >> 12) - 1) << 32) /* limit */
		+ ((long)SDT_MEMERA << 52)
		+ ((long)SEL_UPL << 57)
		+ (1L << 59) /* present */
		+ (1L << 62) /* 32 bits */
		+ (1L << 63); /* page granularity */
	dataseg = 0		/* base */
		+ (((sv->sv_usrstack >> 12) - 1) << 32) /* limit */
		+ ((long)SDT_MEMRWA << 52)
		+ ((long)SEL_UPL << 57)
		+ (1L << 59) /* present */
		+ (1L << 62) /* 32 bits */
		+ (1L << 63); /* page granularity */

	tf->tf_scratch.csd = codeseg;
	tf->tf_scratch.ssd = dataseg;
	tf->tf_scratch.gr24 = dataseg; /* ESD */
	tf->tf_scratch.gr27 = dataseg; /* DSD */
	tf->tf_scratch.gr28 = dataseg; /* FSD */
	tf->tf_scratch.gr29 = dataseg; /* GSD */

	gdtseg = gdt		/* base */
		+ ((8L*NGDT - 1) << 32) /* limit */
		+ ((long)SDT_SYSNULL << 52)
		+ ((long)SEL_UPL << 57)
		+ (1L << 59) /* present */
		+ (0L << 62) /* 16 bits */
		+ (0L << 63); /* byte granularity */
	ldtseg = ldt		/* base */
		+ ((8L*NLDT - 1) << 32) /* limit */
		+ ((long)SDT_SYSLDT << 52)
		+ ((long)SEL_UPL << 57)
		+ (1L << 59) /* present */
		+ (0L << 62) /* 16 bits */
		+ (0L << 63); /* byte granularity */

	tf->tf_scratch.gr30 = ldtseg; /* LDTD */
	tf->tf_scratch.gr31 = gdtseg; /* GDTD */

	/* Set ia32 control registers on this processor. */
	ia64_set_cflg(CR0_PE | CR0_PG | ((long)(CR4_XMM | CR4_FXSR) << 32));
	ia64_set_eflag(PSL_USER);

	/* PS_STRINGS value for BSD/OS binaries.  It is 0 for non-BSD/OS. */
	tf->tf_scratch.gr11 = td->td_proc->p_sysent->sv_psstrings;

	/*
	 * XXX - Linux emulator
	 * Make sure sure edx is 0x0 on entry. Linux binaries depend
	 * on it.
	 */
	td->td_retval[1] = 0;
}

void
ia32_restorectx(struct pcb *pcb)
{

	ia64_set_cflg(pcb->pcb_ia32_cflg);
	ia64_set_eflag(pcb->pcb_ia32_eflag);
	ia64_set_fcr(pcb->pcb_ia32_fcr);
	ia64_set_fdr(pcb->pcb_ia32_fdr);
	ia64_set_fir(pcb->pcb_ia32_fir);
	ia64_set_fsr(pcb->pcb_ia32_fsr);
}

void
ia32_savectx(struct pcb *pcb)
{

	pcb->pcb_ia32_cflg = ia64_get_cflg();
	pcb->pcb_ia32_eflag = ia64_get_eflag();
	pcb->pcb_ia32_fcr = ia64_get_fcr();
	pcb->pcb_ia32_fdr = ia64_get_fdr();
	pcb->pcb_ia32_fir = ia64_get_fir();
	pcb->pcb_ia32_fsr = ia64_get_fsr();
}

int
freebsd32_getcontext(struct thread *td, struct freebsd32_getcontext_args *uap)
{

	return (nosys(td, NULL));
}

int
freebsd32_setcontext(struct thread *td, struct freebsd32_setcontext_args *uap)
{

	return (nosys(td, NULL));
}

int
freebsd32_swapcontext(struct thread *td, struct freebsd32_swapcontext_args *uap)
{

	return (nosys(td, NULL));
}
