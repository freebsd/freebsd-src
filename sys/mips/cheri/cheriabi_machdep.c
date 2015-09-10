/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989, 1990 William Jolitz
 * Copyright (c) 1994 John Dyson
 * Copyright (c) 2015 SRI International
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/imgact_elf.h>
#include <sys/imgact.h>
#include <sys/mman.h>

#include <machine/cheri.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

#include <compat/cheriabi/cheriabi_proto.h>
#include <compat/cheriabi/cheriabi_syscall.h>
#include <compat/cheriabi/cheriabi_sysargmap.h>
#include <compat/cheriabi/cheriabi_util.h>

#define	DELAYBRANCH(x)	((int)(x) < 0)

static int	cheriabi_fetch_syscall_args(struct thread *td,
		    struct syscall_args *sa);
static void	cheriabi_set_syscall_retval(struct thread *td, int error);

extern const char *cheriabi_syscallnames[];

struct sysentvec elf_freebsd_cheriabi_sysvec = {
	.sv_size	= CHERIABI_SYS_MAXSYSCALL,
	.sv_table	= cheriabi_sysent,
	.sv_mask	= 0,
	.sv_sigsize	= 0,
	.sv_sigtbl	= NULL,
	.sv_errsize	= 0,
	.sv_errtbl	= NULL,
	.sv_fixup	= __elfN(freebsd_fixup),
	.sv_sendsig	= sendsig,
	.sv_sigcode	= sigcode,	/* XXXBD: is the MIPS64 code enough? */
	.sv_szsigcode	= &szsigcode,
	.sv_prepsyscall	= NULL,
	.sv_name	= "FreeBSD-CHERI ELF64",
	.sv_coredump	= __elfN(coredump),
	.sv_imgact_try	= NULL,
	.sv_minsigstksz	= MINSIGSTKSZ,	/* XXXBD: or something bigger? */
	.sv_pagesize	= PAGE_SIZE,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= USRSTACK,
	.sv_psstrings	= CHERIABI_PS_STRINGS,
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_strings = cheriabi_copyout_strings,
	.sv_setregs	= exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_FREEBSD | SV_LP64 | SV_CHERI,
	.sv_set_syscall_retval = cheriabi_set_syscall_retval,
	.sv_fetch_syscall_args = cheriabi_fetch_syscall_args,
	.sv_syscallnames = cheriabi_syscallnames,
	.sv_schedtail	= NULL,
};
INIT_SYSENTVEC(cheriabi_sysent, &elf_freebsd_cheriabi_sysvec);

static Elf64_Brandinfo freebsd_cheriabi_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
#ifndef CPU_CHERI128
	.machine	= EM_MIPS_CHERI256,
#else
	.machine	= EM_MIPS_CHERI128,
#endif
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf_freebsd_cheriabi_sysvec,
	.interp_newpath = NULL,
	.flags		= 0
};

SYSINIT(cheriabi, SI_SUB_EXEC, SI_ORDER_ANY,
    (sysinit_cfunc_t) elf64_insert_brand_entry,
    &freebsd_cheriabi_brand_info);

static int
cheriabi_fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{
	struct trapframe *locr0 = td->td_frame;	 /* aka td->td_pcb->pcv_regs */
	struct cheri_frame *capreg = &td->td_pcb->pcb_cheriframe;
	register_t intargs[8];
	uintptr_t ptrargs[8];
	struct sysentvec *se;
	int error, i, isaved, psaved, curint, curptr, nintargs, nptrargs;

	error = 0;

	bzero(sa->args, sizeof(sa->args));

	/* compute next PC after syscall instruction */
	td->td_pcb->pcb_tpc = sa->trapframe->pc; /* Remember if restart */
	if (DELAYBRANCH(sa->trapframe->cause))	 /* Check BD bit */
		locr0->pc = MipsEmulateBranch(locr0, sa->trapframe->pc, 0, 0);
	else
		locr0->pc += sizeof(int);
	sa->code = locr0->v0;

	switch (sa->code) {
	case CHERIABI_SYS___syscall:
	case CHERIABI_SYS_syscall:
		/*
		 * This is an indirect syscall, in which the code is the first
		 * argument.
		 */
		sa->code = locr0->a0;
		intargs[0] = locr0->a1;
		intargs[1] = locr0->a2;
		intargs[2] = locr0->a3;
		intargs[3] = locr0->a4;
		intargs[4] = locr0->a5;
		intargs[5] = locr0->a6;
		intargs[6] = locr0->a7;
		isaved = 7;
		break;
	default:
		/*
		 * A direct syscall, arguments are just parameters to the syscall.
		 */
		intargs[0] = locr0->a0;
		intargs[1] = locr0->a1;
		intargs[2] = locr0->a2;
		intargs[3] = locr0->a3;
		intargs[4] = locr0->a4;
		intargs[5] = locr0->a5;
		intargs[6] = locr0->a6;
		intargs[7] = locr0->a7;
		isaved = 8;
		break;
	}

#if defined(CPU_CHERI_CHERI0) || defined (CPU_CHERI_CHERI8) || defined(CPU_CHERI_CHERI16)
#error	CHERIABI does not support fewer than 8 argument registers
#endif
	/*
	 * XXXBD: we should idealy use a user capability rather than KDC
	 * to generate the pointers, but then we have to answer: which one?
	 */
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c3, 0);
	CHERI_CTOPTR(ptrargs[0], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c4, 0);
	CHERI_CTOPTR(ptrargs[1], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c5, 0);
	CHERI_CTOPTR(ptrargs[2], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c6, 0);
	CHERI_CTOPTR(ptrargs[3], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c7, 0);
	CHERI_CTOPTR(ptrargs[4], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c8, 0);
	CHERI_CTOPTR(ptrargs[5], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c9, 0);
	CHERI_CTOPTR(ptrargs[6], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &capreg->cf_c10, 0);
	CHERI_CTOPTR(ptrargs[7], CHERI_CR_CTEMP0, CHERI_CR_KDC);
	psaved = 8;

#ifdef TRAP_DEBUG
	if (trap_debug)
		printf("SYSCALL #%d pid:%u\n", sa->code, td->td_proc->p_pid);
#endif

	se = td->td_proc->p_sysent;
	/*
	 * XXX
	 * Shouldn't this go before switching on the code?
	 */
	if (se->sv_mask)
		sa->code &= se->sv_mask;

	if (sa->code >= se->sv_size)
		sa->callp = &se->sv_table[0];
	else
		sa->callp = &se->sv_table[sa->code];

	sa->narg = sa->callp->sy_narg;

	nptrargs = bitcount(CHERIABI_SYS_argmap[sa->code].sam_ptrmask);
	nintargs = sa->narg - nintargs;
	KASSERT(nintargs <= isaved,
	    ("SYSCALL #%u pid:%u, nintargs (%u) > isaved (%u).\n",
	     sa->code, td->td_proc->p_pid, nintargs, isaved));
	KASSERT(nptrargs <= psaved,
	    ("SYSCALL #%u pid:%u, nptrargs (%u) > psaved (%u).\n",
	     sa->code, td->td_proc->p_pid, nptrargs, psaved));

	/*
	 * Check each argument to see if it is a pointer and pop an argument
	 * off the appropriate list.
	 */
	curint = curptr = 0;
	for (i = 0; i < sa->narg; i++)
		sa->args[i] =
		    (CHERIABI_SYS_argmap[sa->code].sam_ptrmask & 1 << i) ?
		    ptrargs[curptr++] : intargs[curint++];

	td->td_retval[0] = 0;
	td->td_retval[1] = locr0->v1;

	return (error);
}

static void
cheriabi_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *locr0 = td->td_frame;
	struct cheri_frame *capreg = &td->td_pcb->pcb_cheriframe;
	register_t a0;
	unsigned int code;
	struct sysentvec *se;

	code = locr0->v0;
	a0 = locr0->a0;
	if (code == CHERIABI_SYS_syscall || code == CHERIABI_SYS___syscall) {
		code = locr0->a0;
		a0 = locr0->a1;
	}

	se = td->td_proc->p_sysent;
	/*
	 * When programs start up, they pass through the return path
	 * (maybe via execve?).  When this happens, code is an absurd
	 * and out of range value.
	 */
	if (code > se->sv_size)
		code = 0;

	switch (error) {
	case 0:
		locr0->v0 = td->td_retval[0];
		locr0->v1 = td->td_retval[1];
		locr0->a3 = 0;

		if (!CHERIABI_SYS_argmap[code].sam_return_ptr)
			break;

		switch (code) {
		case CHERIABI_SYS_mmap:
			/*
			 * Assuming no one has stomped on it, a0 is the length
			 * requested.
			 *
			 * XXX: In a compressed capability world, we will need
			 * to round up out allocations to a representable size,
			 * not just the end of the page and return that
			 * capability instead.  Note well: this will violate
			 * POSIX which assumes fixed page sizes and page
			 * granularity allocations and probably will break
			 * existing code.
			 */
			if ((void *)td->td_retval[0] == MAP_FAILED)
				/* XXXBD: is this really what we want? */
				cheri_capability_set(&capreg->cf_c3,
				    CHERI_CAP_USER_PERMS, NULL,
				    0, 0, -1);
			else
				cheri_capability_set(&capreg->cf_c3,
				    CHERI_CAP_USER_PERMS, NULL,
				    (void *)td->td_retval[0],
				    roundup2((size_t)a0, PAGE_SIZE), 0);
			break;

		default:
			panic("%s: unsupported syscall (%u) returning pointer",
			    __func__, code);
		}
		break;
	case ERESTART:
		locr0->pc = td->td_pcb->pcb_tpc;
		break;

	case EJUSTRETURN:
		break;	/* nothing to do */

	default:
		locr0->v0 = error;
		locr0->a3 = 1;
	}
}
