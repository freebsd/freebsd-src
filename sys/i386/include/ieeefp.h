/*-
 * Copyright (c) 1990 Andrew Moore, Talke Studio
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
 * 	from: @(#) ieeefp.h 	1.0 (Berkeley) 9/23/93
 * $FreeBSD: src/sys/i386/include/ieeefp.h,v 1.11.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

/*
 *	IEEE floating point type and constant definitions.
 */

#ifndef _MACHINE_IEEEFP_H_
#define _MACHINE_IEEEFP_H_

#ifndef _SYS_CDEFS_H_
#error this file needs sys/cdefs.h as a prerequisite
#endif

/*
 * FP rounding modes
 */
typedef enum {
	FP_RN=0,	/* round to nearest */
	FP_RM,		/* round down to minus infinity */
	FP_RP,		/* round up to plus infinity */
	FP_RZ		/* truncate */
} fp_rnd_t;

/*
 * FP precision modes
 */
typedef enum {
	FP_PS=0,	/* 24 bit (single-precision) */
	FP_PRS,		/* reserved */
	FP_PD,		/* 53 bit (double-precision) */
	FP_PE		/* 64 bit (extended-precision) */
} fp_prec_t;

#define fp_except_t	int

/*
 * FP exception masks
 */
#define FP_X_INV	0x01	/* invalid operation */
#define FP_X_DNML	0x02	/* denormal */
#define FP_X_DZ		0x04	/* zero divide */
#define FP_X_OFL	0x08	/* overflow */
#define FP_X_UFL	0x10	/* underflow */
#define FP_X_IMP	0x20	/* (im)precision */
#define FP_X_STK	0x40	/* stack fault */

/*
 * FP registers
 */
#define FP_MSKS_REG	0	/* exception masks */
#define FP_PRC_REG	0	/* precision */
#define FP_RND_REG	0	/* direction */
#define FP_STKY_REG	1	/* sticky flags */

/*
 * FP register bit field masks
 */
#define FP_MSKS_FLD	0x3f	/* exception masks field */
#define FP_PRC_FLD	0x300	/* precision control field */
#define FP_RND_FLD	0xc00	/* round control field */
#define FP_STKY_FLD	0x3f	/* sticky flags field */

/*
 * FP register bit field offsets
 */
#define FP_MSKS_OFF	0	/* exception masks offset */
#define FP_PRC_OFF	8	/* precision control offset */
#define FP_RND_OFF	10	/* round control offset */
#define FP_STKY_OFF	0	/* sticky flags offset */

#ifdef __GNUCLIKE_ASM

#define	__fldenv(addr)	__asm __volatile("fldenv %0" : : "m" (*(addr)))
#define	__fnstenv(addr)	__asm __volatile("fnstenv %0" : "=m" (*(addr)))
#define	__fnstcw(addr)	__asm __volatile("fnstcw %0" : "=m" (*(addr)))
#define	__fnstsw(addr)	__asm __volatile("fnstsw %0" : "=m" (*(addr)))

/*
 * return the contents of a FP register
 */
static __inline__ int
__fpgetreg(int _reg)
{
	unsigned short _mem;

	/*-
	 * This is more efficient than it looks.  The switch gets optimized
	 * away if _reg is constant.
	 *
	 * The default case only supports _reg == 0.  We could handle more
	 * registers (e.g., tags) using fnstenv, but the interface doesn't
	 * support more.
	 */
	switch(_reg) {
	default:
		__fnstcw(&_mem);
		break;
	case FP_STKY_REG:
		__fnstsw(&_mem);
		break;
	}
	return _mem;
}

/*
 * set a FP mode; return previous mode
 */
static __inline__ int
__fpsetreg(int _m, int _reg, int _fld, int _off)
{
	unsigned _env[7];
	unsigned _p;

	/*
	 * _reg == 0 could be handled better using fnstcw/fldcw.
	 */
	__fnstenv(_env);
	_p =  (_env[_reg] & _fld) >> _off;
	_env[_reg] = (_env[_reg] & ~_fld) | (_m << _off & _fld);
	__fldenv(_env);
	return _p;
}

#endif /* __GNUCLIKE_ASM */

/*
 * SysV/386 FP control interface
 */
#define	fpgetround()	((fp_rnd_t)					\
	((__fpgetreg(FP_RND_REG) & FP_RND_FLD) >> FP_RND_OFF))
#define	fpsetround(m)	((fp_rnd_t)					\
	__fpsetreg((m), FP_RND_REG, FP_RND_FLD, FP_RND_OFF))
#define	fpgetprec()	((fp_prec_t)					\
	((__fpgetreg(FP_PRC_REG) & FP_PRC_FLD) >> FP_PRC_OFF))
#define	fpsetprec(m)	((fp_prec_t)					\
	__fpsetreg((m), FP_PRC_REG, FP_PRC_FLD, FP_PRC_OFF))
#define	fpgetmask()	((fp_except_t)					\
	((~__fpgetreg(FP_MSKS_REG) & FP_MSKS_FLD) >> FP_MSKS_OFF))
#define	fpsetmask(m)	((fp_except_t)					\
	(~__fpsetreg(~(m), FP_MSKS_REG, FP_MSKS_FLD, FP_MSKS_OFF)) &	\
	    (FP_MSKS_FLD >> FP_MSKS_OFF))
#define	fpgetsticky()	((fp_except_t)					\
	((__fpgetreg(FP_STKY_REG) & FP_STKY_FLD) >> FP_STKY_OFF))
#define	fpresetsticky(m) ((fp_except_t)					\
	__fpsetreg(0, FP_STKY_REG, (m), FP_STKY_OFF))

/* Suppress prototypes in the MI header. */
#define	_IEEEFP_INLINED_	1

#endif /* !_MACHINE_IEEEFP_H_ */
