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
 *
 * $FreeBSD$
 */

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

#include <ia64/ia32/ia32_util.h>
#include <i386/include/psl.h>
#include <i386/include/segments.h>
#include <i386/include/specialreg.h>

static register_t *ia32_copyout_strings(struct image_params *imgp);
static void ia32_setregs(struct thread *td, u_long entry, u_long stack,
    u_long ps_strings);

extern struct sysent ia32_sysent[];

static char ia32_sigcode[] = {
	0xff, 0x54, 0x24, 0x10,		/* call *SIGF_HANDLER(%esp) */
	0x8d, 0x44, 0x24, 0x14,		/* lea SIGF_UC(%esp),%eax */
	0x50,				/* pushl %eax */
	0xf7, 0x40, 0x54, 0x00, 0x00, 0x02, 0x02, /* testl $PSL_VM,UC_EFLAGS(%eax) */
	0x75, 0x03,			/* jne 9f */
	0x8e, 0x68, 0x14,		/* movl UC_GS(%eax),%gs */
	0xb8, 0x57, 0x01, 0x00, 0x00,	/* 9: movl $SYS_sigreturn,%eax */
	0x50,				/* pushl %eax */
	0xcd, 0x80,			/* int $0x80 */
	0xeb, 0xfe,			/* 0: jmp 0b */
	0
};
static int ia32_szsigcode = sizeof(ia32_sigcode);

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
	sendsig,
	ia32_sigcode,
	&ia32_szsigcode,
	NULL,
	"FreeBSD ELF",
	elf32_coredump,
	NULL,
	IA32_MINSIGSTKSZ,
	IA32_PAGE_SIZE,
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
						"/lib/ld-elf.so.1",
						&ia32_freebsd_sysvec
					  };

SYSINIT(ia32, SI_SUB_EXEC, SI_ORDER_ANY,
	(sysinit_cfunc_t) elf32_insert_brand_entry,
	&ia32_brand_info);

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
	destp =	(caddr_t)arginfo - szsigcode - IA32_USRSPACE -
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
	vectp = (void*)((uintptr_t)vectp & ~15);
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

static void
ia32_setregs(struct thread *td, u_long entry, u_long stack, u_long ps_strings)
{
	struct trapframe *tf = td->td_frame;
	vm_offset_t gdt, ldt;
	u_int64_t codesel, datasel, ldtsel;
	u_int64_t codeseg, dataseg, gdtseg, ldtseg;
	struct segment_descriptor desc;
	struct vmspace *vmspace = td->td_proc->p_vmspace;

	exec_setregs(td, entry, stack, ps_strings);

	/* Non-syscall frames are cleared by exec_setregs() */
	if (tf->tf_flags & FRAME_SYSCALL) {
		bzero(&tf->tf_scratch, sizeof(tf->tf_scratch));
		bzero(&tf->tf_scratch_fp, sizeof(tf->tf_scratch_fp));
	} else
		tf->tf_special.ndirty = 0;

	tf->tf_special.psr |= IA64_PSR_IS;
	tf->tf_special.sp = stack;

	/* Point the RSE backstore to something harmless. */
	tf->tf_special.bspstore = (IA32_PS_STRINGS - ia32_szsigcode -
	    IA32_USRSPACE + 15) & ~15;

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
	gdt = IA32_USRSTACK;
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

	desc.sd_lolimit = ((IA32_USRSTACK >> 12) - 1) & 0xffff;
	desc.sd_lobase = 0;
	desc.sd_type = SDT_MEMERA;
	desc.sd_dpl = SEL_UPL;
	desc.sd_p = 1;
	desc.sd_hilimit = ((IA32_USRSTACK >> 12) - 1) >> 16;
	desc.sd_def32 = 1;
	desc.sd_gran = 1;
	desc.sd_hibase = 0;
	copyout(&desc, (caddr_t) ldt + 8*LUCODE_SEL, sizeof(desc));
	desc.sd_type = SDT_MEMRWA;
	copyout(&desc, (caddr_t) ldt + 8*LUDATA_SEL, sizeof(desc));

	codeseg = 0		/* base */
		+ (((IA32_USRSTACK >> 12) - 1) << 32) /* limit */
		+ ((long)SDT_MEMERA << 52)
		+ ((long)SEL_UPL << 57)
		+ (1L << 59) /* present */
		+ (1L << 62) /* 32 bits */
		+ (1L << 63); /* page granularity */
	dataseg = 0		/* base */
		+ (((IA32_USRSTACK >> 12) - 1) << 32) /* limit */
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
	tf->tf_scratch.gr11 = IA32_PS_STRINGS;

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
