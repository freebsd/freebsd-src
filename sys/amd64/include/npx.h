/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *
 *	from: @(#)npx.h	5.3 (Berkeley) 1/18/91
 * $FreeBSD$
 */

/*
 * 287/387 NPX Coprocessor Data Structures and Constants
 * W. Jolitz 1/90
 */

#ifndef _MACHINE_NPX_H_
#define	_MACHINE_NPX_H_

/* Contents of each x87 floating point accumulator */
struct fpacc87 {
	u_char	fp_bytes[10];
};

/* Contents of each SSE extended accumulator */
struct  xmmacc {
	u_char	xmm_bytes[16];
};

struct  envxmm {
	u_int16_t	en_cw;		/* control word (16bits) */
	u_int16_t	en_sw;		/* status word (16bits) */
	u_int8_t	en_tw;		/* tag word (8bits) */
	u_int8_t	en_zero;
	u_int16_t	en_opcode;	/* opcode last executed (11 bits ) */
	u_int64_t	en_rip;		/* floating point instruction pointer */
	u_int64_t	en_rdp;		/* floating operand pointer */
	u_int32_t	en_mxcsr;	/* SSE sontorol/status register */
	u_int32_t	en_mxcsr_mask;	/* valid bits in mxcsr */
};

struct  savefpu {
	struct	envxmm	sv_env;
	struct {
		struct fpacc87	fp_acc;
		u_char		fp_pad[6];      /* padding */
	} sv_fp[8];
	struct xmmacc	sv_xmm[16];
	u_char sv_pad[96];
} __aligned(16);

/*
 * The hardware default control word for i387's and later coprocessors is
 * 0x37F, giving:
 *
 *	round to nearest
 *	64-bit precision
 *	all exceptions masked.
 *
 * We modify the affine mode bit and precision bits in this to give:
 *
 *	affine mode for 287's (if they work at all) (1 in bitfield 1<<12)
 *	53-bit precision (2 in bitfield 3<<8)
 *
 * 64-bit precision often gives bad results with high level languages
 * because it makes the results of calculations depend on whether
 * intermediate values are stored in memory or in FPU registers.
 */
#define	__INITIAL_NPXCW__	0x127F

#ifdef _KERNEL
int	npxdna(void);
void	npxdrop(void);
void	npxexit(struct thread *td);
int	npxformat(void);
int	npxgetregs(struct thread *td, struct savefpu *addr);
void	npxinit(u_short control);
void	npxsave(struct savefpu *addr);
void	npxsetregs(struct thread *td, struct savefpu *addr);
int	npxtrap(void);
#endif

#endif /* !_MACHINE_NPX_H_ */
