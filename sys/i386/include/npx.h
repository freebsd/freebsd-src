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
 *	$Id: npx.h,v 1.3 1994/04/29 21:44:23 gclarkii Exp $
 */

/*
 * 287/387 NPX Coprocessor Data Structures and Constants
 * W. Jolitz 1/90
 */

#ifndef	___NPX87___
#define	___NPX87___

/* Environment information of floating point unit */
struct	env87 {
	long	en_cw;		/* control word (16bits) */
	long	en_sw;		/* status word (16bits) */
	long	en_tw;		/* tag word (16bits) */
	long	en_fip;		/* floating point instruction pointer */
	u_short	en_fcs;		/* floating code segment selector */
	u_short	en_opcode;	/* opcode last executed (11 bits ) */
	long	en_foo;		/* floating operand offset */
	long	en_fos;		/* floating operand segment selector */
};

/* Contents of each floating point accumulator */
struct	fpacc87 {
#ifdef dontdef /* too unportable */
	u_long	fp_mantlo;	/* mantissa low (31:0) */
	u_long	fp_manthi;	/* mantissa high (63:32) */
	int	fp_exp:15;	/* exponent */
	int	fp_sgn:1;	/* mantissa sign */
#else
	u_char	fp_bytes[10];
#endif
};

/* Floating point context */
struct	save87 {
	struct	env87 sv_env;		/* floating point control/status */
	struct	fpacc87	sv_ac[8];	/* accumulator contents, 0-7 */
	u_long	sv_ex_sw;	/* status word for last exception (was pad) */
	u_long	sv_ex_tw;	/* tag word for last exception (was pad) */
#ifdef GPL_MATH_EMULATE
        u_char  sv_pad[60];
#else
        u_char	sv_pad[8 * 2 - 2 * 4];	/* bogus historical padding */
#endif /* GPL_MATH_EMULATE */
};

/* Cyrix EMC memory - mapped coprocessor context switch information */
struct	emcsts {
	long	em_msw;		/* memory mapped status register when swtched */
	long	em_tar;		/* memory mapped temp A register when swtched */
	long	em_dl;		/* memory mapped D low register when swtched */
};

/* Intel prefers long real (53 bit) precision */
#define	__iBCS_NPXCW__		0x262
/* wfj prefers temporary real (64 bit) precision */
#define	__386BSD_NPXCW__	0x362
/*
 * bde prefers 53 bit precision and all exceptions masked.
 *
 * The standard control word from finit is 0x37F, giving:
 *
 *	round to nearest
 *	64-bit precision
 *	all exceptions masked.
 *
 * Now I want:
 *
 *	affine mode for 287's (if they work at all) (1 in bitfield 1<<12)
 *	53-bit precision (2 in bitfield 3<<8)
 *	overflow exception unmasked (0 in bitfield 1<<3)
 *	zero divide exception unmasked (0 in bitfield 1<<2)
 *	invalid-operand exception unmasked (0 in bitfield 1<<0).
 *
 * 64-bit precision often gives bad results with high level languages
 * because it makes the results of calculations depend on whether
 * intermediate values are stored in memory or in FPU registers.
 *
 * The "Intel" and wfj control words have:
 *
 *	underflow exception unmasked (0 in bitfield 1<<4)
 *
 * but that causes an unexpected exception in the test program 'paranoia'
 * and makes denormals useless (DBL_MIN / 2 underflows).  It doesn't make
 * a lot of sense to trap underflow without trapping denormals.
 *
 * Later I will want the IEEE default of all exceptions masked.  See the
 * 0.0 math manpage for why this is better.  The 0.1 math manpage is empty.
 */
#define	__BDE_NPXCW__		0x1272
#define	__BETTER_BDE_NPXCW__	0x127f

#ifdef __BROKEN_NPXCW__
#ifdef __386BSD__
#define	__INITIAL_NPXCW__	__386BSD_NPXCW__
#else
#define	__INITIAL_NPXCW__	__iBCS_NPXCW__
#endif
#else
#define	__INITIAL_NPXCW__	__BDE_NPXCW__
#endif

#endif	___NPX87___
