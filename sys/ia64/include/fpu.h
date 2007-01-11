/*-
 * Copyright (c) 1998 Doug Rabson
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
 * $FreeBSD: src/sys/ia64/include/fpu.h,v 1.3 2004/12/11 06:15:12 marcel Exp $
 */

#ifndef _MACHINE_FPU_H_
#define _MACHINE_FPU_H_

/*
 * Floating point status register bits.
 */

#define IA64_FPSR_TRAP_VD	0x0000000000000001L
#define IA64_FPSR_TRAP_DD	0x0000000000000002L
#define IA64_FPSR_TRAP_ZD	0x0000000000000004L
#define IA64_FPSR_TRAP_OD	0x0000000000000008L
#define IA64_FPSR_TRAP_UD	0x0000000000000010L
#define IA64_FPSR_TRAP_ID	0x0000000000000020L
#define IA64_FPSR_SF(i,v)	((v) << ((i)*13+6))

#define IA64_SF_FTZ		0x0001L
#define IA64_SF_WRE		0x0002L
#define IA64_SF_PC		0x000cL
#define IA64_SF_PC_0		0x0000L
#define IA64_SF_PC_1		0x0004L
#define IA64_SF_PC_2		0x0008L
#define IA64_SF_PC_3		0x000cL
#define IA64_SF_RC		0x0030L
#define IA64_SF_RC_NEAREST	0x0000L
#define IA64_SF_RC_NEGINF	0x0010L
#define IA64_SF_RC_POSINF	0x0020L
#define IA64_SF_RC_TRUNC	0x0030L
#define IA64_SF_TD		0x0040L
#define IA64_SF_V		0x0080L
#define IA64_SF_D		0x0100L
#define IA64_SF_Z		0x0200L
#define IA64_SF_O		0x0400L
#define IA64_SF_U		0x0800L
#define IA64_SF_I		0x1000L

#define IA64_SF_DEFAULT		(IA64_SF_PC_3 | IA64_SF_RC_NEAREST)

#define IA64_FPSR_DEFAULT	(IA64_FPSR_TRAP_VD			\
				 | IA64_FPSR_TRAP_DD			\
				 | IA64_FPSR_TRAP_ZD			\
				 | IA64_FPSR_TRAP_OD			\
				 | IA64_FPSR_TRAP_UD			\
				 | IA64_FPSR_TRAP_ID			\
				 | IA64_FPSR_SF(0, IA64_SF_DEFAULT)	\
				 | IA64_FPSR_SF(1, (IA64_SF_DEFAULT	\
						    | IA64_SF_TD	\
						    | IA64_SF_WRE))	\
				 | IA64_FPSR_SF(2, (IA64_SF_DEFAULT	\
						    | IA64_SF_TD))	\
				 | IA64_FPSR_SF(3, (IA64_SF_DEFAULT	\
						    | IA64_SF_TD)))

struct fpswa_ret {
	unsigned long	status;
	unsigned long	err1;
	unsigned long	err2;
	unsigned long	err3;
};

struct fpswa_bundle {
	long double	bits;		/* Force 16-byte alignment. */
};

struct fpswa_fpctx {
	unsigned long	mask_low;			/* f63 - f2 */
	unsigned long	mask_high;			/* f127 - f64 */
	union _ia64_fpreg *fp_low_preserved;		/* f2 - f5 */
	union _ia64_fpreg *fp_low_volatile;		/* f6 - f15 */
	union _ia64_fpreg *fp_high_preserved;		/* f16 - f31 */
	union _ia64_fpreg *fp_high_volatile;		/* f32 - f127 */
};

struct fpswa_iface {
	unsigned int	if_rev;
	unsigned int	__res;
	struct fpswa_ret (*if_fpswa)(unsigned long, struct fpswa_bundle *,
	    unsigned long *, unsigned long *, unsigned long *, unsigned long *,
	    unsigned long *, struct fpswa_fpctx *);
};

#endif /* ! _MACHINE_FPU_H_ */
