/*-
 * Copyright (c) 2014, 2015 The FreeBSD Foundation.
 * Copyright (c) 2014, 2017 Andrew Turner.
 * Copyright (c) 2018 Olivier Houchard
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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
#define	__ELF_WORD_SIZE 32

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/linker.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/imgact_elf.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>

#include <machine/elf.h>
#include <machine/pcb.h>
#ifdef VFP
#include <machine/vfp.h>
#endif

#include <compat/freebsd32/freebsd32_util.h>

#define	FREEBSD32_MINUSER	0x00001000
#define	FREEBSD32_MAXUSER	((1ul << 32) - PAGE_SIZE)
#define	FREEBSD32_SHAREDPAGE	(FREEBSD32_MAXUSER - PAGE_SIZE)
#define	FREEBSD32_USRSTACK	FREEBSD32_SHAREDPAGE
#define	AARCH32_MAXDSIZ		(512 * 1024 * 1024)
#define	AARCH32_MAXSSIZ		(64 * 1024 * 1024)
#define	AARCH32_MAXVMEM		0

extern const char *freebsd32_syscallnames[];

extern char aarch32_sigcode[];
extern int sz_aarch32_sigcode;

static int freebsd32_fetch_syscall_args(struct thread *td);
static void freebsd32_setregs(struct thread *td, struct image_params *imgp,
    u_long stack);
static void freebsd32_set_syscall_retval(struct thread *, int);

static bool elf32_arm_abi_supported(const struct image_params *,
    const int32_t *, const uint32_t *);
static void elf32_fixlimit(struct rlimit *rl, int which);

extern void freebsd32_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask);

u_long __read_frequently elf32_hwcap;
u_long __read_frequently elf32_hwcap2;

static SYSCTL_NODE(_compat, OID_AUTO, aarch32, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "aarch32 mode");

static u_long aarch32_maxdsiz = AARCH32_MAXDSIZ;
SYSCTL_ULONG(_compat_aarch32, OID_AUTO, maxdsiz, CTLFLAG_RWTUN,
    &aarch32_maxdsiz, 0, "");
u_long aarch32_maxssiz = AARCH32_MAXSSIZ;
SYSCTL_ULONG(_compat_aarch32, OID_AUTO, maxssiz, CTLFLAG_RWTUN,
    &aarch32_maxssiz, 0, "");
static u_long aarch32_maxvmem = AARCH32_MAXVMEM;
SYSCTL_ULONG(_compat_aarch32, OID_AUTO, maxvmem, CTLFLAG_RWTUN,
    &aarch32_maxvmem, 0, "");

static struct sysentvec elf32_freebsd_sysvec = {
	.sv_size	= SYS_MAXSYSCALL,
	.sv_table	= freebsd32_sysent,
	.sv_fixup	= elf32_freebsd_fixup,
	.sv_sendsig	= freebsd32_sendsig,
	.sv_sigcode	= aarch32_sigcode,
	.sv_szsigcode	= &sz_aarch32_sigcode,
	.sv_name	= "FreeBSD ELF32",
	.sv_coredump	= elf32_coredump,
	.sv_elf_core_osabi = ELFOSABI_FREEBSD,
	.sv_elf_core_abi_vendor = FREEBSD_ABI_VENDOR,
	.sv_elf_core_prepare_notes = elf32_prepare_notes,
	.sv_minsigstksz	= MINSIGSTKSZ,
	.sv_minuser	= FREEBSD32_MINUSER,
	.sv_maxuser	= FREEBSD32_MAXUSER,
	.sv_usrstack	= FREEBSD32_USRSTACK,
	.sv_psstrings	= FREEBSD32_PS_STRINGS,
	.sv_psstringssz	= sizeof(struct freebsd32_ps_strings),
	.sv_stackprot	= VM_PROT_READ | VM_PROT_WRITE,
	.sv_copyout_auxargs = elf32_freebsd_copyout_auxargs,
	.sv_copyout_strings = freebsd32_copyout_strings,
	.sv_setregs	= freebsd32_setregs,
	.sv_fixlimit	= elf32_fixlimit,
	.sv_maxssiz	= &aarch32_maxssiz,
	.sv_flags	= SV_ABI_FREEBSD | SV_ILP32 | SV_SHP | SV_TIMEKEEP |
	    SV_RNG_SEED_VER | SV_SIGSYS,
	.sv_set_syscall_retval = freebsd32_set_syscall_retval,
	.sv_fetch_syscall_args = freebsd32_fetch_syscall_args,
	.sv_syscallnames = freebsd32_syscallnames,
	.sv_shared_page_base = FREEBSD32_SHAREDPAGE,
	.sv_shared_page_len = PAGE_SIZE,
	.sv_schedtail	= NULL,
	.sv_thread_detach = NULL,
	.sv_trap	= NULL,
	.sv_hwcap	= &elf32_hwcap,
	.sv_hwcap2	= &elf32_hwcap2,
	.sv_hwcap3	= NULL,
	.sv_hwcap4	= NULL,
	.sv_onexec_old	= exec_onexec_old,
	.sv_onexit	= exit_onexit,
	.sv_regset_begin = SET_BEGIN(__elfN(regset)),
	.sv_regset_end	= SET_LIMIT(__elfN(regset)),
};
INIT_SYSENTVEC(elf32_sysvec, &elf32_freebsd_sysvec);

static Elf32_Brandinfo freebsd32_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_ARM,
	.compat_3_brand	= "FreeBSD",
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf32_freebsd_sysvec,
	.interp_newpath	= "/libexec/ld-elf32.so.1",
	.brand_note	= &elf32_freebsd_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE,
	.header_supported= elf32_arm_abi_supported,
};

static void
register_elf32_brand(void *arg)
{
	/* Check if we support AArch32 */
	if (ID_AA64PFR0_EL0_VAL(READ_SPECIALREG(id_aa64pfr0_el1)) ==
	    ID_AA64PFR0_EL0_64_32) {
		elf32_insert_brand_entry(&freebsd32_brand_info);
	} else {
		compat_freebsd_32bit = 0;
	}
}
SYSINIT(elf32, SI_SUB_EXEC, SI_ORDER_FIRST, register_elf32_brand, NULL);

static bool
elf32_arm_abi_supported(const struct image_params *imgp,
    const int32_t *osrel __unused, const uint32_t *fctl0 __unused)
{
	const Elf32_Ehdr *hdr;

#define	EF_ARM_EABI_FREEBSD_MIN	EF_ARM_EABI_VER4
	hdr = (const Elf32_Ehdr *)imgp->image_header;
	if (EF_ARM_EABI_VERSION(hdr->e_flags) < EF_ARM_EABI_FREEBSD_MIN) {
		if (bootverbose)
			uprintf("Attempting to execute non EABI binary "
			    "(rev %d) image %s",
			    EF_ARM_EABI_VERSION(hdr->e_flags),
			    imgp->args->fname);
		return (false);
        }

	return (true);
}

static int
freebsd32_fetch_syscall_args(struct thread *td)
{
	struct proc *p;
	register_t *ap;
	struct syscall_args *sa;
	int error, i, nap, narg;
	unsigned int args[4];

	nap = 4;
	p = td->td_proc;
	ap = td->td_frame->tf_x;
	sa = &td->td_sa;

	/* r7 is the syscall id */
	sa->code = td->td_frame->tf_x[7];
	sa->original_code = sa->code;

	if (sa->code == SYS_syscall) {
		sa->code = *ap++;
		nap--;
	} else if (sa->code == SYS___syscall) {
		sa->code = ap[1];
		nap -= 2;
		ap += 2;
	}

	if (sa->code >= p->p_sysent->sv_size)
		sa->callp = &nosys_sysent;
	else
		sa->callp = &p->p_sysent->sv_table[sa->code];

	narg = sa->callp->sy_narg;
	for (i = 0; i < nap; i++)
		sa->args[i] = ap[i];
	if (narg > nap) {
		if (narg - nap > nitems(args))
			panic("Too many system call arguiments");
		error = copyin((void *)td->td_frame->tf_x[13], args,
		    (narg - nap) * sizeof(int));
		if (error != 0)
			return (error);
		for (i = 0; i < (narg - nap); i++)
			sa->args[i + nap] = args[i];
	}

	td->td_retval[0] = 0;
	td->td_retval[1] = 0;

	return (0);
}

static void
freebsd32_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *frame;

	frame = td->td_frame;
	switch (error) {
	case 0:
		frame->tf_x[0] = td->td_retval[0];
		frame->tf_x[1] = td->td_retval[1];
		frame->tf_spsr &= ~PSR_C;
		break;
	case ERESTART:
		/*
		 * Reconstruct the pc to point at the swi.
		 */
		if ((frame->tf_spsr & PSR_T) != 0)
			frame->tf_elr -= 2; //THUMB_INSN_SIZE;
		else
			frame->tf_elr -= 4; //INSN_SIZE;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
		frame->tf_x[0] = error;
		frame->tf_spsr |= PSR_C;
		break;
	}
}

static void
freebsd32_setregs(struct thread *td, struct image_params *imgp,
   uintptr_t stack)
{
	struct trapframe *tf = td->td_frame;
	struct pcb *pcb = td->td_pcb;

	memset(tf, 0, sizeof(struct trapframe));

	/*
	 * We need to set x0 for init as it doesn't call
	 * cpu_set_syscall_retval to copy the value. We also
	 * need to set td_retval for the cases where we do.
	 */
	tf->tf_x[0] = stack;
	/* SP_usr is mapped to x13 */
	tf->tf_x[13] = stack;
	/* LR_usr is mapped to x14 */
	tf->tf_x[14] = imgp->entry_addr;
	tf->tf_elr = imgp->entry_addr;
	tf->tf_spsr = PSR_M_32;
	if ((uint32_t)imgp->entry_addr & 1)
		tf->tf_spsr |= PSR_T;

#ifdef VFP
	vfp_reset_state(td, pcb);
#endif

	/*
	 * Clear debug register state. It is not applicable to the new process.
	 */
	bzero(&pcb->pcb_dbg_regs, sizeof(pcb->pcb_dbg_regs));
}

void
elf32_dump_thread(struct thread *td, void *dst, size_t *off)
{
}

static void
elf32_fixlimit(struct rlimit *rl, int which)
{

	switch (which) {
	case RLIMIT_DATA:
		if (aarch32_maxdsiz != 0) {
			if (rl->rlim_cur > aarch32_maxdsiz)
				rl->rlim_cur = aarch32_maxdsiz;
			if (rl->rlim_max > aarch32_maxdsiz)
				rl->rlim_max = aarch32_maxdsiz;
		}
		break;
	case RLIMIT_STACK:
		if (aarch32_maxssiz != 0) {
			if (rl->rlim_cur > aarch32_maxssiz)
				rl->rlim_cur = aarch32_maxssiz;
			if (rl->rlim_max > aarch32_maxssiz)
				rl->rlim_max = aarch32_maxssiz;
		}
		break;
	case RLIMIT_VMEM:
		if (aarch32_maxvmem != 0) {
			if (rl->rlim_cur > aarch32_maxvmem)
				rl->rlim_cur = aarch32_maxvmem;
			if (rl->rlim_max > aarch32_maxvmem)
				rl->rlim_max = aarch32_maxvmem;
		}
		break;
	}
}
