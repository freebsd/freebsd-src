/*-
 * Copyright (c) 2003 Peter Wemm.
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
 * $FreeBSD: src/sys/amd64/include/ieeefp.h,v 1.14 2005/04/12 23:12:00 jhb Exp $
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
 * SSE mxcsr register bit field masks
 */
#define	SSE_STKY_FLD	0x3f	/* exception flags */
#define	SSE_DAZ_FLD	0x40	/* Denormals are zero */
#define	SSE_MSKS_FLD	0x1f80	/* exception masks field */
#define	SSE_RND_FLD	0x6000	/* rounding control */
#define	SSE_FZ_FLD	0x8000	/* flush to zero on underflow */

/*
 * FP register bit field offsets
 */
#define FP_MSKS_OFF	0	/* exception masks offset */
#define FP_PRC_OFF	8	/* precision control offset */
#define FP_RND_OFF	10	/* round control offset */
#define FP_STKY_OFF	0	/* sticky flags offset */

/*
 * SSE mxcsr register bit field offsets
 */
#define	SSE_STKY_OFF	0	/* exception flags offset */
#define	SSE_DAZ_OFF	6	/* DAZ exception mask offset */
#define	SSE_MSKS_OFF	7	/* other exception masks offset */
#define	SSE_RND_OFF	13	/* rounding control offset */
#define	SSE_FZ_OFF	15	/* flush to zero offset */

#if defined(__GNUCLIKE_ASM) && defined(__CC_SUPPORTS___INLINE__) \
    && !defined(__cplusplus)

#define	__fldenv(addr)	__asm __volatile("fldenv %0" : : "m" (*(addr)))
#define	__fnstenv(addr)	__asm __volatile("fnstenv %0" : "=m" (*(addr)))
#define	__fldcw(addr)	__asm __volatile("fldcw %0" : : "m" (*(addr)))
#define	__fnstcw(addr)	__asm __volatile("fnstcw %0" : "=m" (*(addr)))
#define	__fnstsw(addr)	__asm __volatile("fnstsw %0" : "=m" (*(addr)))
#define	__ldmxcsr(addr)	__asm __volatile("ldmxcsr %0" : : "m" (*(addr)))
#define	__stmxcsr(addr)	__asm __volatile("stmxcsr %0" : "=m" (*(addr)))

/*
 * General notes about conflicting SSE vs FP status bits.
 * This code assumes that software will not fiddle with the control
 * bits of the SSE and x87 in such a way to get them out of sync and
 * still expect this to work.  Break this at your peril.
 * Because I based this on the i386 port, the x87 state is used for
 * the fpget*() functions, and is shadowed into the SSE state for
 * the fpset*() functions.  For dual source fpget*() functions, I
 * merge the two together.  I think.
 */

/* Set rounding control */
static __inline__ fp_rnd_t
__fpgetround(void)
{
	unsigned short _cw;

	__fnstcw(&_cw);
	return ((_cw & FP_RND_FLD) >> FP_RND_OFF);
}

static __inline__ fp_rnd_t
__fpsetround(fp_rnd_t _m)
{
	unsigned short _cw;
	unsigned int _mxcsr;
	fp_rnd_t _p;

	__fnstcw(&_cw);
	_p = (_cw & FP_RND_FLD) >> FP_RND_OFF;
	_cw &= ~FP_RND_FLD;
	_cw |= (_m << FP_RND_OFF) & FP_RND_FLD;
	__fldcw(&_cw);
	__stmxcsr(&_mxcsr);
	_mxcsr &= ~SSE_RND_FLD;
	_mxcsr |= (_m << SSE_RND_OFF) & SSE_RND_FLD;
	__ldmxcsr(&_mxcsr);
	return (_p);
}

/*
 * Set precision for fadd/fsub/fsqrt etc x87 instructions
 * There is no equivalent SSE mode or control.
 */
static __inline__ fp_prec_t
__fpgetprec(void)
{
	unsigned short _cw;

	__fnstcw(&_cw);
	return ((_cw & FP_PRC_FLD) >> FP_PRC_OFF);
}

static __inline__ fp_prec_t
__fpsetprec(fp_rnd_t _m)
{
	unsigned short _cw;
	fp_prec_t _p;

	__fnstcw(&_cw);
	_p = (_cw & FP_PRC_FLD) >> FP_PRC_OFF;
	_cw &= ~FP_PRC_FLD;
	_cw |= (_m << FP_PRC_OFF) & FP_PRC_FLD;
	__fldcw(&_cw);
	return (_p);
}

/*
 * Look at the exception masks
 * Note that x87 masks are inverse of the fp*() functions
 * API.  ie: mask = 1 means disable for x87 and SSE, but
 * for the fp*() api, mask = 1 means enabled.
 */
static __inline__ fp_except_t
__fpgetmask(void)
{
	unsigned short _cw;

	__fnstcw(&_cw);
	return ((~_cw) & FP_MSKS_FLD);
}

static __inline__ fp_except_t
__fpsetmask(fp_except_t _m)
{
	unsigned short _cw;
	unsigned int _mxcsr;
	fp_except_t _p;

	__fnstcw(&_cw);
	_p = (~_cw) & FP_MSKS_FLD;
	_cw &= ~FP_MSKS_FLD;
	_cw |= (~_m) & FP_MSKS_FLD;
	__fldcw(&_cw);
	__stmxcsr(&_mxcsr);
	/* XXX should we clear non-ieee SSE_DAZ_FLD and SSE_FZ_FLD ? */
	_mxcsr &= ~SSE_MSKS_FLD;
	_mxcsr |= ((~_m) << SSE_MSKS_OFF) & SSE_MSKS_FLD;
	__ldmxcsr(&_mxcsr);
	return (_p);
}

/* See which sticky exceptions are pending, and reset them */
static __inline__ fp_except_t
__fpgetsticky(void)
{
	unsigned short _sw;
	unsigned int _mxcsr;
	fp_except_t _ex;

	__fnstsw(&_sw);
	_ex = _sw & FP_STKY_FLD;
	__stmxcsr(&_mxcsr);
	_ex |= _mxcsr & SSE_STKY_FLD;
	return (_ex);
}

#endif /* __GNUCLIKE_ASM && __CC_SUPPORTS___INLINE__ && !__cplusplus */

#if !defined(__IEEEFP_NOINLINES__) && !defined(__cplusplus) \
    && defined(__GNUCLIKE_ASM) && defined(__CC_SUPPORTS___INLINE__)

#define	fpgetround()	__fpgetround()
#define	fpsetround(_m)	__fpsetround(_m)
#define	fpgetprec()	__fpgetprec()
#define	fpsetprec(_m)	__fpsetprec(_m)
#define	fpgetmask()	__fpgetmask()
#define	fpsetmask(_m)	__fpsetmask(_m)
#define	fpgetsticky()	__fpgetsticky()

/* Suppress prototypes in the MI header. */
#define	_IEEEFP_INLINED_	1

#else /* !__IEEEFP_NOINLINES__ && !__cplusplus && __GNUCLIKE_ASM
         && __CC_SUPPORTS___INLINE__ */

/* Augment the userland declarations */
__BEGIN_DECLS
extern fp_prec_t fpgetprec(void);
extern fp_prec_t fpsetprec(fp_prec_t);
__END_DECLS

#endif /* !__IEEEFP_NOINLINES__ && !__cplusplus && __GNUCLIKE_ASM
          && __CC_SUPPORTS___INLINE__ */

#endif /* !_MACHINE_IEEEFP_H_ */
