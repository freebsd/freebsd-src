/* $Id$ */
/* From: NetBSD: ieeefp.h,v 1.2 1997/04/06 08:47:28 cgd Exp */

/* 
 * Written by J.T. Conklin, Apr 28, 1995
 * Public domain.
 */

#ifndef _ALPHA_IEEEFP_H_
#define _ALPHA_IEEEFP_H_

typedef int fp_except;
#define	FP_X_INV	0x01	/* invalid operation exception */
#define	FP_X_DZ		0x02	/* divide-by-zero exception */
#define	FP_X_OFL	0x04	/* overflow exception */
#define	FP_X_UFL	0x08	/* underflow exception */
#define	FP_X_IMP	0x10	/* imprecise (loss of precision; "inexact") */
#define	FP_X_IOV	0x20    /* integer overflow XXX? */

typedef enum {
    FP_RZ=0,			/* round to zero (truncate) */
    FP_RM=1,			/* round toward negative infinity */
    FP_RN=2,			/* round to nearest representable number */
    FP_RP=3			/* round toward positive infinity */
} fp_rnd;

#endif /* _ALPHA_IEEEFP_H_ */
