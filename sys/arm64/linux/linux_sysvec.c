/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1994-1996 Søren Schmidt
 * Copyright (c) 2018 Turing Robotic Industries Inc.
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

#define	__ELF_WORD_SIZE	64

#include <sys/param.h>
#include <sys/elf.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/stddef.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <arm64/linux/linux.h>
#include <arm64/linux/linux_proto.h>
#include <compat/linux/linux_elf.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_fork.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_vdso.h>

#include <arm64/linux/linux_sigframe.h>

#include <machine/md_var.h>
#include <machine/pcb.h>
#ifdef VFP
#include <machine/vfp.h>
#endif

MODULE_VERSION(linux64elf, 1);

#define	LINUX_VDSOPAGE_SIZE	PAGE_SIZE * 2
#define	LINUX_VDSOPAGE		(VM_MAXUSER_ADDRESS - \
				    LINUX_VDSOPAGE_SIZE)
#define	LINUX_SHAREDPAGE	(LINUX_VDSOPAGE - PAGE_SIZE)
				/*
				 * PAGE_SIZE - the size
				 * of the native SHAREDPAGE
				 */
#define	LINUX_USRSTACK		LINUX_SHAREDPAGE
#define	LINUX_PS_STRINGS	(LINUX_USRSTACK - \
				    sizeof(struct ps_strings))

static int linux_szsigcode;
static vm_object_t linux_vdso_obj;
static char *linux_vdso_mapping;
extern char _binary_linux_vdso_so_o_start;
extern char _binary_linux_vdso_so_o_end;
static vm_offset_t linux_vdso_base;

extern struct sysent linux_sysent[LINUX_SYS_MAXSYSCALL];
extern const char *linux_syscallnames[];

SET_DECLARE(linux_ioctl_handler_set, struct linux_ioctl_handler);

static void	linux_vdso_install(const void *param);
static void	linux_vdso_deinstall(const void *param);
static void	linux_vdso_reloc(char *mapping, Elf_Addr offset);
static void	linux_set_syscall_retval(struct thread *td, int error);
static int	linux_fetch_syscall_args(struct thread *td);
static void	linux_exec_setregs(struct thread *td, struct image_params *imgp,
		    uintptr_t stack);
static void	linux_exec_sysvec_init(void *param);
static int	linux_on_exec_vmspace(struct proc *p,
		    struct image_params *imgp);

LINUX_VDSO_SYM_CHAR(linux_platform);
LINUX_VDSO_SYM_INTPTR(kern_timekeep_base);
LINUX_VDSO_SYM_INTPTR(__user_rt_sigreturn);

static int
linux_fetch_syscall_args(struct thread *td)
{
	struct proc *p;
	struct syscall_args *sa;
	register_t *ap;

	p = td->td_proc;
	ap = td->td_frame->tf_x;
	sa = &td->td_sa;

	sa->code = td->td_frame->tf_x[8];
	sa->original_code = sa->code;

	if (sa->code >= p->p_sysent->sv_size)
		sa->callp = &nosys_sysent;
	else
		sa->callp = &p->p_sysent->sv_table[sa->code];

	if (sa->callp->sy_narg > nitems(sa->args))
		panic("ARM64TODO: Could we have more than %zu args?",
		    nitems(sa->args));
	memcpy(sa->args, ap, nitems(sa->args) * sizeof(register_t));

	td->td_retval[0] = 0;
	return (0);
}

static void
linux_set_syscall_retval(struct thread *td, int error)
{

	td->td_retval[1] = td->td_frame->tf_x[1];
	cpu_set_syscall_retval(td, error);

	if (__predict_false(error != 0)) {
		if (error != ERESTART && error != EJUSTRETURN)
			td->td_frame->tf_x[0] = bsd_to_linux_errno(error);
	}
}

void
linux64_arch_copyout_auxargs(struct image_params *imgp, Elf_Auxinfo **pos)
{

	AUXARGS_ENTRY((*pos), LINUX_AT_SYSINFO_EHDR, linux_vdso_base);
	AUXARGS_ENTRY((*pos), LINUX_AT_HWCAP, *imgp->sysent->sv_hwcap);
	AUXARGS_ENTRY((*pos), LINUX_AT_HWCAP2, *imgp->sysent->sv_hwcap2);
	AUXARGS_ENTRY((*pos), LINUX_AT_HWCAP3, *imgp->sysent->sv_hwcap3);
	AUXARGS_ENTRY((*pos), LINUX_AT_HWCAP4, *imgp->sysent->sv_hwcap4);
	AUXARGS_ENTRY((*pos), LINUX_AT_PLATFORM, PTROUT(linux_platform));
}

/*
 * Reset registers to default values on exec.
 */
static void
linux_exec_setregs(struct thread *td, struct image_params *imgp,
    uintptr_t stack)
{
	struct trapframe *regs = td->td_frame;
	struct pcb *pcb = td->td_pcb;

	memset(regs, 0, sizeof(*regs));
	regs->tf_sp = stack;
	regs->tf_elr = imgp->entry_addr;
	pcb->pcb_tpidr_el0 = 0;
	pcb->pcb_tpidrro_el0 = 0;
	WRITE_SPECIALREG(tpidrro_el0, 0);
	WRITE_SPECIALREG(tpidr_el0, 0);

#ifdef VFP
	vfp_reset_state(td, pcb);
#endif

	/*
	 * Clear debug register state. It is not applicable to the new process.
	 */
	bzero(&pcb->pcb_dbg_regs, sizeof(pcb->pcb_dbg_regs));
}

static bool
linux_parse_sigreturn_ctx(struct thread *td, struct l_sigcontext *sc)
{
	struct l_fpsimd_context *fpsimd;
	struct _l_aarch64_ctx *ctx;
	int offset;

	offset = 0;
	while (1) {
		/* The offset must be 16 byte aligned */
		if ((offset & 15) != 0)
			return (false);

		/* Check for buffer overflow of the ctx */
		if ((offset + sizeof(*ctx)) >
		    sizeof(sc->__reserved))
			return (false);

		ctx = (struct _l_aarch64_ctx *)&sc->__reserved[offset];

		/* Check for buffer overflow of the data */
		if ((offset + ctx->size) > sizeof(sc->__reserved))
			return (false);

		switch(ctx->magic) {
		case 0:
			if (ctx->size != 0)
				return (false);
			return (true);
		case L_ESR_MAGIC:
			/* Ignore */
			break;
#ifdef VFP
		case L_FPSIMD_MAGIC:
			fpsimd = (struct l_fpsimd_context *)ctx;

			/*
			 * Discard any vfp state for the current thread, we
			 * are about to override it.
			 */
			critical_enter();
			vfp_discard(td);
			critical_exit();

			td->td_pcb->pcb_fpustate.vfp_fpcr = fpsimd->fpcr;
			td->td_pcb->pcb_fpustate.vfp_fpsr = fpsimd->fpsr;
			memcpy(td->td_pcb->pcb_fpustate.vfp_regs,
			    fpsimd->vregs, sizeof(fpsimd->vregs));

			break;
#endif
		default:
			return (false);
		}

		offset += ctx->size;
	}

}

int
linux_rt_sigreturn(struct thread *td, struct linux_rt_sigreturn_args *args)
{
	struct l_rt_sigframe *sf;
	struct l_sigframe *frame;
	struct trapframe *tf;
	sigset_t bmask;
	int error;

	sf = malloc(sizeof(*sf), M_LINUX, M_WAITOK | M_ZERO);

	tf = td->td_frame;
	frame = (struct l_sigframe *)tf->tf_sp;
	error = copyin((void *)&frame->sf, sf, sizeof(*sf));
	if (error != 0) {
		free(sf, M_LINUX);
		return (error);
	}

	memcpy(tf->tf_x, sf->sf_uc.uc_sc.regs, sizeof(tf->tf_x));
	tf->tf_lr = sf->sf_uc.uc_sc.regs[30];
	tf->tf_sp = sf->sf_uc.uc_sc.sp;
	tf->tf_elr = sf->sf_uc.uc_sc.pc;

	if ((sf->sf_uc.uc_sc.pstate & PSR_M_MASK) != PSR_M_EL0t ||
	    (sf->sf_uc.uc_sc.pstate & PSR_AARCH32) != 0 ||
	    (sf->sf_uc.uc_sc.pstate & PSR_DAIF) !=
	    (td->td_frame->tf_spsr & PSR_DAIF))
		goto einval;
	tf->tf_spsr = sf->sf_uc.uc_sc.pstate;

	if (!linux_parse_sigreturn_ctx(td, &sf->sf_uc.uc_sc))
		goto einval;

	/* Restore signal mask. */
	linux_to_bsd_sigset(&sf->sf_uc.uc_sigmask, &bmask);
	kern_sigprocmask(td, SIG_SETMASK, &bmask, NULL, 0);
	free(sf, M_LINUX);

	return (EJUSTRETURN);
einval:
	free(sf, M_LINUX);
	return (EINVAL);
}

static void
linux_rt_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct thread *td;
	struct proc *p;
	struct trapframe *tf;
	struct l_sigframe *fp, *frame;
	struct l_fpsimd_context *fpsimd;
	struct l_esr_context *esr;
	l_stack_t uc_stack;
	ucontext_t uc;
	uint8_t *scr;
	struct sigacts *psp;
	int onstack, sig, issiginfo;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	sig = ksi->ksi_signo;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);

	tf = td->td_frame;
	onstack = sigonstack(tf->tf_sp);
	issiginfo = SIGISMEMBER(psp->ps_siginfo, sig);

	CTR4(KTR_SIG, "sendsig: td=%p (%s) catcher=%p sig=%d", td, p->p_comm,
	    catcher, sig);

	/* Allocate and validate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !onstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct l_sigframe *)((uintptr_t)td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size);
#if defined(COMPAT_43)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else {
		fp = (struct l_sigframe *)td->td_frame->tf_sp;
	}

	/* Make room, keeping the stack aligned */
	fp--;
	fp = (struct l_sigframe *)STACKALIGN(fp);

	get_mcontext(td, &uc.uc_mcontext, 0);
	uc.uc_sigmask = *mask;

	uc_stack.ss_sp = PTROUT(td->td_sigstk.ss_sp);
	uc_stack.ss_size = td->td_sigstk.ss_size;
	uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK) != 0 ?
	    (onstack ? LINUX_SS_ONSTACK : 0) : LINUX_SS_DISABLE;
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(td->td_proc);

	/* Fill in the frame to copy out */
	frame = malloc(sizeof(*frame), M_LINUX, M_WAITOK | M_ZERO);

	memcpy(&frame->sf.sf_uc.uc_sc.regs, tf->tf_x, sizeof(tf->tf_x));
	frame->sf.sf_uc.uc_sc.regs[30] = tf->tf_lr;
	frame->sf.sf_uc.uc_sc.sp = tf->tf_sp;
	frame->sf.sf_uc.uc_sc.pc = tf->tf_elr;
	frame->sf.sf_uc.uc_sc.pstate = tf->tf_spsr;
	frame->sf.sf_uc.uc_sc.fault_address = (register_t)ksi->ksi_addr;

	/* Stack frame for unwinding */
	frame->fp = tf->tf_x[29];
	frame->lr = tf->tf_elr;

	/* Translate the signal. */
	sig = bsd_to_linux_signal(sig);
	siginfo_to_lsiginfo(&ksi->ksi_info, &frame->sf.sf_si, sig);
	bsd_to_linux_sigset(mask, &frame->sf.sf_uc.uc_sigmask);

	/*
	 * Prepare fpsimd & esr. Does not check sizes, as
	 * __reserved is big enougth.
	 */
	scr = (uint8_t *)&frame->sf.sf_uc.uc_sc.__reserved;
#ifdef VFP
	fpsimd = (struct l_fpsimd_context *) scr;
	fpsimd->head.magic = L_FPSIMD_MAGIC;
	fpsimd->head.size = sizeof(struct l_fpsimd_context);
	fpsimd->fpsr = uc.uc_mcontext.mc_fpregs.fp_sr;
	fpsimd->fpcr = uc.uc_mcontext.mc_fpregs.fp_cr;

	memcpy(fpsimd->vregs, &uc.uc_mcontext.mc_fpregs.fp_q,
	    sizeof(uc.uc_mcontext.mc_fpregs.fp_q));
	scr += roundup(sizeof(struct l_fpsimd_context), 16);
#endif
	if (ksi->ksi_addr != 0) {
		esr = (struct l_esr_context *) scr;
		esr->head.magic = L_ESR_MAGIC;
		esr->head.size = sizeof(struct l_esr_context);
		esr->esr = tf->tf_esr;
	}

	memcpy(&frame->sf.sf_uc.uc_stack, &uc_stack, sizeof(uc_stack));

	/* Copy the sigframe out to the user's stack. */
	if (copyout(frame, fp, sizeof(*fp)) != 0) {
		/* Process has trashed its stack. Kill it. */
		free(frame, M_LINUX);
		CTR2(KTR_SIG, "sendsig: sigexit td=%p fp=%p", td, fp);
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}
	free(frame, M_LINUX);

	tf->tf_x[0]= sig;
	if (issiginfo) {
		tf->tf_x[1] = (register_t)&fp->sf.sf_si;
		tf->tf_x[2] = (register_t)&fp->sf.sf_uc;
	} else {
		tf->tf_x[1] = 0;
		tf->tf_x[2] = 0;
	}
	tf->tf_x[29] = (register_t)&fp->fp;
	tf->tf_elr = (register_t)catcher;
	tf->tf_sp = (register_t)fp;
	tf->tf_lr = (register_t)__user_rt_sigreturn;

	CTR3(KTR_SIG, "sendsig: return td=%p pc=%#x sp=%#x", td, tf->tf_elr,
	    tf->tf_sp);

	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

struct sysentvec elf_linux_sysvec = {
	.sv_size	= LINUX_SYS_MAXSYSCALL,
	.sv_table	= linux_sysent,
	.sv_fixup	= __elfN(freebsd_fixup),
	.sv_sendsig	= linux_rt_sendsig,
	.sv_sigcode	= &_binary_linux_vdso_so_o_start,
	.sv_szsigcode	= &linux_szsigcode,
	.sv_name	= "Linux ELF64",
	.sv_coredump	= elf64_coredump,
	.sv_elf_core_osabi = ELFOSABI_NONE,
	.sv_elf_core_abi_vendor = LINUX_ABI_VENDOR,
	.sv_elf_core_prepare_notes = linux64_prepare_notes,
	.sv_minsigstksz	= LINUX_MINSIGSTKSZ,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= LINUX_USRSTACK,
	.sv_psstrings	= LINUX_PS_STRINGS,
	.sv_psstringssz	= sizeof(struct ps_strings),
	.sv_stackprot	= VM_PROT_READ | VM_PROT_WRITE,
	.sv_copyout_auxargs = __linuxN(copyout_auxargs),
	.sv_copyout_strings = __linuxN(copyout_strings),
	.sv_setregs	= linux_exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_LINUX | SV_LP64 | SV_SHP | SV_SIG_DISCIGN |
	    SV_SIG_WAITNDQ | SV_TIMEKEEP,
	.sv_set_syscall_retval = linux_set_syscall_retval,
	.sv_fetch_syscall_args = linux_fetch_syscall_args,
	.sv_syscallnames = linux_syscallnames,
	.sv_shared_page_base = LINUX_SHAREDPAGE,
	.sv_shared_page_len = PAGE_SIZE,
	.sv_schedtail	= linux_schedtail,
	.sv_thread_detach = linux_thread_detach,
	.sv_trap	= NULL,
	.sv_hwcap	= &linux_elf_hwcap,
	.sv_hwcap2	= &linux_elf_hwcap2,
	.sv_hwcap3	= &linux_elf_hwcap3,
	.sv_hwcap4	= &linux_elf_hwcap4,
	.sv_onexec	= linux_on_exec_vmspace,
	.sv_onexit	= linux_on_exit,
	.sv_ontdexit	= linux_thread_dtor,
	.sv_setid_allowed = &linux_setid_allowed_query,
};

static int
linux_on_exec_vmspace(struct proc *p, struct image_params *imgp)
{
	int error;

	error = linux_map_vdso(p, linux_vdso_obj, linux_vdso_base,
	    LINUX_VDSOPAGE_SIZE, imgp);
	if (error == 0)
		error = linux_on_exec(p, imgp);
	return (error);
}

/*
 * linux_vdso_install() and linux_exec_sysvec_init() must be called
 * after exec_sysvec_init() which is SI_SUB_EXEC (SI_ORDER_ANY).
 */
static void
linux_exec_sysvec_init(void *param)
{
	l_uintptr_t *ktimekeep_base;
	struct sysentvec *sv;
	ptrdiff_t tkoff;

	sv = param;
	/* Fill timekeep_base */
	exec_sysvec_init(sv);

	tkoff = kern_timekeep_base - linux_vdso_base;
	ktimekeep_base = (l_uintptr_t *)(linux_vdso_mapping + tkoff);
	*ktimekeep_base = sv->sv_shared_page_base + sv->sv_timekeep_offset;
}
SYSINIT(elf_linux_exec_sysvec_init, SI_SUB_EXEC + 1, SI_ORDER_ANY,
    linux_exec_sysvec_init, &elf_linux_sysvec);

static void
linux_vdso_install(const void *param)
{
	char *vdso_start = &_binary_linux_vdso_so_o_start;
	char *vdso_end = &_binary_linux_vdso_so_o_end;

	linux_szsigcode = vdso_end - vdso_start;
	MPASS(linux_szsigcode <= LINUX_VDSOPAGE_SIZE);

	linux_vdso_base = LINUX_VDSOPAGE;

	__elfN(linux_vdso_fixup)(vdso_start, linux_vdso_base);

	linux_vdso_obj = __elfN(linux_shared_page_init)
	    (&linux_vdso_mapping, LINUX_VDSOPAGE_SIZE);
	bcopy(vdso_start, linux_vdso_mapping, linux_szsigcode);

	linux_vdso_reloc(linux_vdso_mapping, linux_vdso_base);
}
SYSINIT(elf_linux_vdso_init, SI_SUB_EXEC + 1, SI_ORDER_FIRST,
    linux_vdso_install, NULL);

static void
linux_vdso_deinstall(const void *param)
{

	__elfN(linux_shared_page_fini)(linux_vdso_obj,
	    linux_vdso_mapping, LINUX_VDSOPAGE_SIZE);
}
SYSUNINIT(elf_linux_vdso_uninit, SI_SUB_EXEC, SI_ORDER_FIRST,
    linux_vdso_deinstall, NULL);

static void
linux_vdso_reloc(char *mapping, Elf_Addr offset)
{
	Elf_Size rtype, symidx;
	const Elf_Rela *rela;
	const Elf_Shdr *shdr;
	const Elf_Ehdr *ehdr;
	Elf_Addr *where;
	Elf_Addr addr, addend;
	int i, relacnt;

	MPASS(offset != 0);

	relacnt = 0;
	ehdr = (const Elf_Ehdr *)mapping;
	shdr = (const Elf_Shdr *)(mapping + ehdr->e_shoff);
	for (i = 0; i < ehdr->e_shnum; i++)
	{
		switch (shdr[i].sh_type) {
		case SHT_REL:
			printf("Linux Aarch64 vDSO: unexpected Rel section\n");
			break;
		case SHT_RELA:
			rela = (const Elf_Rela *)(mapping + shdr[i].sh_offset);
			relacnt = shdr[i].sh_size / sizeof(*rela);
		}
	}

	for (i = 0; i < relacnt; i++, rela++) {
		where = (Elf_Addr *)(mapping + rela->r_offset);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);

		switch (rtype) {
		case R_AARCH64_NONE:	/* none */
			break;

		case R_AARCH64_RELATIVE:	/* B + A */
			addr = (Elf_Addr)(mapping + addend);
			if (*where != addr)
				*where = addr;
			break;
		default:
			printf("Linux Aarch64 vDSO: unexpected relocation type %ld, "
			    "symbol index %ld\n", rtype, symidx);
		}
	}
}

static Elf_Brandnote linux64_brandnote = {
	.hdr.n_namesz	= sizeof(GNU_ABI_VENDOR),
	.hdr.n_descsz	= 16,
	.hdr.n_type	= 1,
	.vendor		= GNU_ABI_VENDOR,
	.flags		= BN_TRANSLATE_OSREL,
	.trans_osrel	= linux_trans_osrel
};

static Elf64_Brandinfo linux_glibc2brand = {
	.brand		= ELFOSABI_LINUX,
	.machine	= EM_AARCH64,
	.compat_3_brand	= "Linux",
	.interp_path	= "/lib64/ld-linux-x86-64.so.2",
	.sysvec		= &elf_linux_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &linux64_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

Elf64_Brandinfo *linux_brandlist[] = {
	&linux_glibc2brand,
	NULL
};

static int
linux64_elf_modevent(module_t mod, int type, void *data)
{
	Elf64_Brandinfo **brandinfo;
	struct linux_ioctl_handler**lihp;
	int error;

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
			stclohz = (stathz ? stathz : hz);
			if (bootverbose)
				printf("Linux arm64 ELF exec handler installed\n");
		}
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
				printf("Linux arm64 ELF exec handler removed\n");
		} else
			printf("Could not deinstall Linux arm64 ELF interpreter entry\n");
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (error);
}

static moduledata_t linux64_elf_mod = {
	"linux64elf",
	linux64_elf_modevent,
	0
};

DECLARE_MODULE_TIED(linux64elf, linux64_elf_mod, SI_SUB_EXEC, SI_ORDER_ANY);
MODULE_DEPEND(linux64elf, linux_common, 1, 1, 1);
FEATURE(linux64, "AArch64 Linux 64bit support");
