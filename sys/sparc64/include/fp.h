/*-
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_MACHINE_FP_H_
#define	_MACHINE_FP_H_

#define	FPRS_DL		(1 << 0)
#define	FPRS_DU		(1 << 1)
#define	FPRS_FEF	(1 << 2)

#define	FSR_CEXC_NX	(1 << 0)
#define	FSR_CEXC_DZ	(1 << 1)
#define	FSR_CEXC_UF	(1 << 2)
#define	FSR_CEXC_OF	(1 << 3)
#define	FSR_CEXC_NV	(1 << 4)
#define	FSR_AEXC_NX	(1 << 5)
#define	FSR_AEXC_DZ	(1 << 6)
#define	FSR_AEXC_UF	(1 << 7)
#define	FSR_AEXC_OF	(1 << 8)
#define	FSR_AEXC_NV	(1 << 9)
#define	FSR_QNE		(1 << 13)
#define	FSR_NS		(1 << 22)
#define	FSR_TEM_NX	(1 << 23)
#define	FSR_TEM_DZ	(1 << 24)
#define	FSR_TEM_UF	(1 << 25)
#define	FSR_TEM_OF	(1 << 26)
#define	FSR_TEM_NV	(1 << 27)

#define	FSR_FCC0_SHIFT	10
#define	FSR_FCC0(x)	(((x) >> FSR_FCC0_SHIFT) & 3)
#define	FSR_FTT_SHIFT	14
#define	FSR_FTT(x)	(((x) >> FSR_FTT_SHIFT) & 7)
#define	FSR_VER_SHIFT	17
#define	FSR_VER(x)	(((x) >> FSR_VER_SHIFT) & 7)
#define	FSR_RD_SHIFT	30
#define	FSR_RD(x)	(((x) >> FSR_RD_SHIFT) & 3)
#define	FSR_FCC1_SHIFT	32
#define	FSR_FCC1(x)	(((x) >> FSR_FCC1_SHIFT) & 3)
#define	FSR_FCC2_SHIFT	34
#define	FSR_FCC2(x)	(((x) >> FSR_FCC2_SHIFT) & 3)
#define	FSR_FCC3_SHIFT	36
#define	FSR_FCC3(x)	(((x) >> FSR_FCC3_SHIFT) & 3)

/* A block of 8 double-precision (16 single-precision) FP registers. */
struct fpblock {
	u_long	fpq_l[8];
};

struct fpstate {
	struct	fpblock fp_fb[4];
	u_long	fp_fsr;
	u_long	fp_fprs;
};

void	fp_init_thread(struct pcb *);
int	fp_enable_thread(struct thread *);
/*
 * Note: The pointers passed to the next two functions must be aligned on
 * 64 byte boundaries.
 */
void	savefpctx(struct fpstate *);
void	restorefpctx(struct fpstate *);

#endif /* !_MACHINE_FP_H_ */
