/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD$
 */

#ifndef _MACHINE_FRAME_H_
#define	_MACHINE_FRAME_H_

#include <machine/reg.h>

/*
 * Software trap, exception, and syscall frame.
 *
 * This is loosely based on the Linux pt_regs structure. When I
 * understand things better, I might change it.
 */
struct trapframe {
	u_int64_t		tf_cr_iip;
	u_int64_t		tf_cr_ipsr;
	u_int64_t		tf_cr_isr;
	u_int64_t		tf_cr_ifa;
	u_int64_t		tf_pr;
	u_int64_t		tf_ar_rsc;
	u_int64_t		tf_ar_pfs;
	u_int64_t		tf_cr_ifs;
	u_int64_t		tf_ar_bspstore;
	u_int64_t		tf_ar_rnat;
	u_int64_t		tf_ar_bsp;
	u_int64_t		tf_ar_unat;
	u_int64_t		tf_ar_ccv;
	u_int64_t		tf_ar_fpsr;

	u_int64_t		tf_b[8];

	u_int64_t		tf_r[31];	/* don't need to save r0 */
#define FRAME_R1		0
#define FRAME_GP		0
#define FRAME_R2		1
#define FRAME_R3		2
#define FRAME_R4		3
#define FRAME_R5		4
#define FRAME_R6		5
#define FRAME_R7		6
#define FRAME_R8		7
#define FRAME_R9		8
#define FRAME_R10		9
#define FRAME_R11		10
#define FRAME_R12		11
#define FRAME_SP		11
#define FRAME_R13		12
#define FRAME_TP		12
#define FRAME_R14		13
#define FRAME_R15		14
#define FRAME_R16		15
#define FRAME_R17		16
#define FRAME_R18		17
#define FRAME_R19		18
#define FRAME_R20		19
#define FRAME_R21		20
#define FRAME_R22		21
#define FRAME_R23		22
#define FRAME_R24		23
#define FRAME_R25		24
#define FRAME_R26		25
#define FRAME_R27		26
#define FRAME_R28		27
#define FRAME_R29		28
#define FRAME_R30		29
#define FRAME_R31		30

	u_int64_t		tf_pad1;

	/*
	 * We rely on the compiler to save/restore f2-f5 and
	 * f16-f31. We also tell the compiler to avoid f32-f127
	 * completely so we don't worry about them at all.
	 */
	struct ia64_fpreg	tf_f[10];
#define FRAME_F6		0
#define FRAME_F7		1
#define FRAME_F8		2
#define FRAME_F9		3
#define FRAME_F10		3
#define FRAME_F11		3
#define FRAME_F12		3
#define FRAME_F13		3
#define FRAME_F14		3
#define FRAME_F15		3
};

#endif /* _MACHINE_FRAME_H_ */
