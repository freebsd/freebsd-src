/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 1999 by Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#ifndef _ASM_SIGCONTEXT_H
#define _ASM_SIGCONTEXT_H

/*
 * Keep this struct definition in sync with the sigcontext fragment
 * in arch/mips/tools/offset.c
 */
struct sigcontext {
	unsigned long long sc_regs[32];
	unsigned long long sc_fpregs[32];
	unsigned long long sc_mdhi;
	unsigned long long sc_mdlo;
	unsigned long long sc_pc;
	unsigned int       sc_status;
	unsigned int       sc_fpc_csr;
	unsigned int       sc_fpc_eir;
	unsigned int       sc_used_math;
	unsigned int       sc_cause;
	unsigned int       sc_badvaddr;
};

#ifdef __KERNEL__
struct sigcontext32 {
	u32 sc_regmask;		/* Unused */
	u32 sc_status;
	u64 sc_pc;
	u64 sc_regs[32];
	u64 sc_fpregs[32];
	u32 sc_ownedfp;		/* Unused */
	u32 sc_fpc_csr;
	u32 sc_fpc_eir;		/* Unused */
	u32 sc_used_math;
	u32 sc_ssflags;		/* Unused */
	u64 sc_mdhi;
	u64 sc_mdlo;

	u32 sc_cause;		/* Unused */
	u32 sc_badvaddr;	/* Unused */

	u32 sc_sigset[4];	/* kernel's sigset_t */
};
#endif /* __KERNEL__ */

#endif /* _ASM_SIGCONTEXT_H */
