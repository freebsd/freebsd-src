/* $FreeBSD$ */
/* From: NetBSD: ieeefp.h,v 1.2 1997/04/06 08:47:28 cgd Exp */

/* 
 * Written by J.T. Conklin, Apr 28, 1995
 * Public domain.
 */

#ifndef _ALPHA_IEEEFP_H_
#define _ALPHA_IEEEFP_H_

typedef int fp_except_t;
#define	FP_X_INV	(1LL << 1)	/* invalid operation exception */
#define	FP_X_DZ		(1LL << 2)	/* divide-by-zero exception */
#define	FP_X_OFL	(1LL << 3)	/* overflow exception */
#define	FP_X_UFL	(1LL << 4)	/* underflow exception */
#define	FP_X_IMP	(1LL << 5)	/* imprecise(inexact) exception */
#if 0
#define	FP_X_IOV	(1LL << 6)	/* integer overflow XXX? */
#endif

typedef enum {
    FP_RZ=0,			/* round to zero (truncate) */
    FP_RM=1,			/* round toward negative infinity */
    FP_RN=2,			/* round to nearest representable number */
    FP_RP=3			/* round toward positive infinity */
} fp_rnd_t;

#endif /* _ALPHA_IEEEFP_H_ */
