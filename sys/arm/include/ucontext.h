/*	$NetBSD: mcontext.h,v 1.4 2003/10/08 22:43:01 thorpej Exp $	*/

/*-
 * Copyright (c) 2001, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein and by Jason R. Thorpe of Wasabi Systems, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_MCONTEXT_H_
#define _MACHINE_MCONTEXT_H_
/*
 * General register state
 */
#define _NGREG		17
typedef unsigned int	__greg_t;
typedef __greg_t	__gregset_t[_NGREG];

#define _REG_R0		0
#define _REG_R1		1
#define _REG_R2		2
#define _REG_R3		3
#define _REG_R4		4
#define _REG_R5		5
#define _REG_R6		6
#define _REG_R7		7
#define _REG_R8		8
#define _REG_R9		9
#define _REG_R10	10
#define _REG_R11	11
#define _REG_R12	12
#define _REG_R13	13
#define _REG_R14	14
#define _REG_R15	15
#define _REG_CPSR	16
/* Convenience synonyms */
#define _REG_FP		_REG_R11
#define _REG_SP		_REG_R13
#define _REG_LR		_REG_R14
#define _REG_PC		_REG_R15

/*
 * Floating point register state
 */
/* Note: the storage layout of this structure must be identical to ARMFPE! */
typedef struct {
	unsigned int	__fp_fpsr;
	struct {
		unsigned int	__fp_exponent;
		unsigned int	__fp_mantissa_hi;
		unsigned int	__fp_mantissa_lo;
	}		__fp_fr[8];
} __fpregset_t;

typedef struct {
	unsigned int	__vfp_fpscr;
	unsigned int	__vfp_fstmx[33];
	unsigned int	__vfp_fpsid;
} __vfpregset_t;

typedef struct {
	__gregset_t	__gregs;
	union {
		__fpregset_t __fpregs;
		__vfpregset_t __vfpregs;
	} __fpu;
} mcontext_t;

/* Machine-dependent uc_flags */
#define	_UC_ARM_VFP	0x00010000	/* FPU field is VFP */

/* used by signal delivery to indicate status of signal stack */
#define _UC_SETSTACK	0x00020000
#define _UC_CLRSTACK	0x00040000

#define _UC_MACHINE_PAD	3		/* Padding appended to ucontext_t */

#define _UC_MACHINE_SP(uc)	((uc)->uc_mcontext.__gregs[_REG_SP])
#define _UC_MACHINE_PC(uc)	((uc)->uc_mcontext.__gregs[_REG_PC])
#define _UC_MACHINE_INTRV(uc)	((uc)->uc_mcontext.__gregs[_REG_R0])

#define	_UC_MACHINE_SET_PC(uc, pc)	_UC_MACHINE_PC(uc) = (pc)

#endif	/* !_MACHINE_MCONTEXT_H_ */
