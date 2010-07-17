/*-
 * Copyright (c) 2001 Doug Rabson
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
 * $FreeBSD: src/sys/ia64/include/ieeefp.h,v 1.5.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $
 */

#ifndef _MACHINE_IEEEFP_H_
#define _MACHINE_IEEEFP_H_

#include <machine/fpu.h>

typedef int fp_except_t;
#define	FP_X_INV	IA64_FPSR_TRAP_VD /* invalid operation exception */
#define	FP_X_DZ		IA64_FPSR_TRAP_ZD /* divide-by-zero exception */
#define	FP_X_OFL	IA64_FPSR_TRAP_OD /* overflow exception */
#define	FP_X_UFL	IA64_FPSR_TRAP_UD /* underflow exception */
#define	FP_X_IMP	IA64_FPSR_TRAP_ID /* imprecise(inexact) exception */

typedef enum {
	FP_RN = 0,		/* round to nearest */
	FP_RM,			/* round toward minus infinity */
	FP_RP,			/* round toward plus infinity */
	FP_RZ			/* round toward zero */
} fp_rnd_t;

#endif /* !_MACHINE_IEEEFP_H_ */
