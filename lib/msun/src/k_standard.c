/* @(#)k_standard.c 5.1 93/09/24 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#include "math.h"
#include "math_private.h"
#include <errno.h>

#ifndef _USE_WRITE
#include <stdio.h>			/* fputs(), stderr */
#define	WRITE2(u,v)	fputs(u, stderr)
#else	/* !defined(_USE_WRITE) */
#include <unistd.h>			/* write */
#define	WRITE2(u,v)	write(2, u, v)
#undef fflush
#endif	/* !defined(_USE_WRITE) */

static const double zero = 0.0;	/* used as const */

/*
 * Standard conformance (non-IEEE) on exception cases.
 * Mapping:
 *	1 -- acos(|x|>1)
 *	2 -- asin(|x|>1)
 *	3 -- atan2(+-0,+-0)
 *	4 -- hypot overflow
 *	5 -- cosh overflow
 *	6 -- exp overflow
 *	7 -- exp underflow
 *	8 -- y0(0)
 *	9 -- y0(-ve)
 *	10-- y1(0)
 *	11-- y1(-ve)
 *	12-- yn(0)
 *	13-- yn(-ve)
 *	14-- lgamma(finite) overflow
 *	15-- lgamma(-integer)
 *	16-- log(0)
 *	17-- log(x<0)
 *	18-- log10(0)
 *	19-- log10(x<0)
 *	20-- pow(0.0,0.0)
 *	21-- pow(x,y) overflow
 *	22-- pow(x,y) underflow
 *	23-- pow(0,negative)
 *	24-- pow(neg,non-integral)
 *	25-- sinh(finite) overflow
 *	26-- sqrt(negative)
 *      27-- fmod(x,0)
 *      28-- remainder(x,0)
 *	29-- acosh(x<1)
 *	30-- atanh(|x|>1)
 *	31-- atanh(|x|=1)
 *	32-- scalb overflow
 *	33-- scalb underflow
 *	34-- j0(|x|>X_TLOSS)
 *	35-- y0(x>X_TLOSS)
 *	36-- j1(|x|>X_TLOSS)
 *	37-- y1(x>X_TLOSS)
 *	38-- jn(|x|>X_TLOSS, n)
 *	39-- yn(x>X_TLOSS, n)
 *	40-- gamma(finite) overflow
 *	41-- gamma(-integer)
 *	42-- pow(NaN,0.0)
 */


double
__kernel_standard(double x, double y, int type)
{
	struct exception exc;
#ifndef HUGE_VAL	/* this is the only routine that uses HUGE_VAL */
#define HUGE_VAL inf
	double inf = 0.0;

	SET_HIGH_WORD(inf,0x7ff00000);	/* set inf to infinite */
#endif

#ifdef _USE_WRITE
	(void) fflush(stdout);
#endif
	exc.arg1 = x;
	exc.arg2 = y;
	switch(type) {
	    case 1:
	    case 101:
		/* acos(|x|>1) */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "acos" : "acosf";
		exc.retval = zero;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if(_LIB_VERSION == _SVID_) {
		    (void) WRITE2("acos: DOMAIN error\n", 19);
		  }
		  errno = EDOM;
		}
		break;
	    case 2:
	    case 102:
		/* asin(|x|>1) */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "asin" : "asinf";
		exc.retval = zero;
		if(_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if(_LIB_VERSION == _SVID_) {
		    	(void) WRITE2("asin: DOMAIN error\n", 19);
		  }
		  errno = EDOM;
		}
		break;
	    case 3:
	    case 103:
		/* atan2(+-0,+-0) */
		exc.arg1 = y;
		exc.arg2 = x;
		exc.type = DOMAIN;
		exc.name = type < 100 ? "atan2" : "atan2f";
		exc.retval = zero;
		if(_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if(_LIB_VERSION == _SVID_) {
			(void) WRITE2("atan2: DOMAIN error\n", 20);
		      }
		  errno = EDOM;
		}
		break;
	    case 4:
	    case 104:
		/* hypot(finite,finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "hypot" : "hypotf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = HUGE;
		else
		  exc.retval = HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
		else if (!matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 5:
	    case 105:
		/* cosh(finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "cosh" : "coshf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = HUGE;
		else
		  exc.retval = HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
		else if (!matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 6:
	    case 106:
		/* exp(finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "exp" : "expf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = HUGE;
		else
		  exc.retval = HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
		else if (!matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 7:
	    case 107:
		/* exp(finite) underflow */
		exc.type = UNDERFLOW;
		exc.name = type < 100 ? "exp" : "expf";
		exc.retval = zero;
		if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
		else if (!matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 8:
	    case 108:
		/* y0(0) = -inf */
		exc.type = DOMAIN;	/* should be SING for IEEE */
		exc.name = type < 100 ? "y0" : "y0f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("y0: DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 9:
	    case 109:
		/* y0(x<0) = NaN */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "y0" : "y0f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("y0: DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 10:
	    case 110:
		/* y1(0) = -inf */
		exc.type = DOMAIN;	/* should be SING for IEEE */
		exc.name = type < 100 ? "y1" : "y1f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("y1: DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 11:
	    case 111:
		/* y1(x<0) = NaN */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "y1" : "y1f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("y1: DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 12:
	    case 112:
		/* yn(n,0) = -inf */
		exc.type = DOMAIN;	/* should be SING for IEEE */
		exc.name = type < 100 ? "yn" : "ynf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("yn: DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 13:
	    case 113:
		/* yn(x<0) = NaN */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "yn" : "ynf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("yn: DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 14:
	    case 114:
		/* lgamma(finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "lgamma" : "lgammaf";
                if (_LIB_VERSION == _SVID_)
                  exc.retval = HUGE;
                else
                  exc.retval = HUGE_VAL;
                if (_LIB_VERSION == _POSIX_)
			errno = ERANGE;
                else if (!matherr(&exc)) {
                        errno = ERANGE;
		}
		break;
	    case 15:
	    case 115:
		/* lgamma(-integer) or lgamma(0) */
		exc.type = SING;
		exc.name = type < 100 ? "lgamma" : "lgammaf";
                if (_LIB_VERSION == _SVID_)
                  exc.retval = HUGE;
                else
                  exc.retval = HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("lgamma: SING error\n", 19);
		      }
		  errno = EDOM;
		}
		break;
	    case 16:
	    case 116:
		/* log(0) */
		exc.type = SING;
		exc.name = type < 100 ? "log" : "logf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("log: SING error\n", 16);
		      }
		  errno = EDOM;
		}
		break;
	    case 17:
	    case 117:
		/* log(x<0) */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "log" : "logf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("log: DOMAIN error\n", 18);
		      }
		  errno = EDOM;
		}
		break;
	    case 18:
	    case 118:
		/* log10(0) */
		exc.type = SING;
		exc.name = type < 100 ? "log10" : "log10f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("log10: SING error\n", 18);
		      }
		  errno = EDOM;
		}
		break;
	    case 19:
	    case 119:
		/* log10(x<0) */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "log10" : "log10f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("log10: DOMAIN error\n", 20);
		      }
		  errno = EDOM;
		}
		break;
	    case 20:
	    case 120:
		/* pow(0.0,0.0) */
		/* error only if _LIB_VERSION == _SVID_ */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "pow" : "powf";
		exc.retval = zero;
		if (_LIB_VERSION != _SVID_) exc.retval = 1.0;
		else if (!matherr(&exc)) {
			(void) WRITE2("pow(0,0): DOMAIN error\n", 23);
			errno = EDOM;
		}
		break;
	    case 21:
	    case 121:
		/* pow(x,y) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "pow" : "powf";
		if (_LIB_VERSION == _SVID_) {
		  exc.retval = HUGE;
		  y *= 0.5;
		  if(x<zero&&rint(y)!=y) exc.retval = -HUGE;
		} else {
		  exc.retval = HUGE_VAL;
		  y *= 0.5;
		  if(x<zero&&rint(y)!=y) exc.retval = -HUGE_VAL;
		}
		if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
		else if (!matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 22:
	    case 122:
		/* pow(x,y) underflow */
		exc.type = UNDERFLOW;
		exc.name = type < 100 ? "pow" : "powf";
		exc.retval =  zero;
		if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
		else if (!matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 23:
	    case 123:
		/* 0**neg */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "pow" : "powf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = zero;
		else
		  exc.retval = -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("pow(0,neg): DOMAIN error\n", 25);
		      }
		  errno = EDOM;
		}
		break;
	    case 24:
	    case 124:
		/* neg**non-integral */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "pow" : "powf";
		if (_LIB_VERSION == _SVID_)
		    exc.retval = zero;
		else
		    exc.retval = zero/zero;	/* X/Open allow NaN */
		if (_LIB_VERSION == _POSIX_)
		   errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("neg**non-integral: DOMAIN error\n", 32);
		      }
		  errno = EDOM;
		}
		break;
	    case 25:
	    case 125:
		/* sinh(finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "sinh" : "sinhf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = ( (x>zero) ? HUGE : -HUGE);
		else
		  exc.retval = ( (x>zero) ? HUGE_VAL : -HUGE_VAL);
		if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
		else if (!matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 26:
	    case 126:
		/* sqrt(x<0) */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "sqrt" : "sqrtf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = zero;
		else
		  exc.retval = zero/zero;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("sqrt: DOMAIN error\n", 19);
		      }
		  errno = EDOM;
		}
		break;
            case 27:
	    case 127:
                /* fmod(x,0) */
                exc.type = DOMAIN;
                exc.name = type < 100 ? "fmod" : "fmodf";
                if (_LIB_VERSION == _SVID_)
                    exc.retval = x;
		else
		    exc.retval = zero/zero;
                if (_LIB_VERSION == _POSIX_)
                  errno = EDOM;
                else if (!matherr(&exc)) {
                  if (_LIB_VERSION == _SVID_) {
                    (void) WRITE2("fmod:  DOMAIN error\n", 20);
                  }
                  errno = EDOM;
                }
                break;
            case 28:
	    case 128:
                /* remainder(x,0) */
                exc.type = DOMAIN;
                exc.name = type < 100 ? "remainder" : "remainderf";
                exc.retval = zero/zero;
                if (_LIB_VERSION == _POSIX_)
                  errno = EDOM;
                else if (!matherr(&exc)) {
                  if (_LIB_VERSION == _SVID_) {
                    (void) WRITE2("remainder: DOMAIN error\n", 24);
                  }
                  errno = EDOM;
                }
                break;
            case 29:
	    case 129:
                /* acosh(x<1) */
                exc.type = DOMAIN;
                exc.name = type < 100 ? "acosh" : "acoshf";
                exc.retval = zero/zero;
                if (_LIB_VERSION == _POSIX_)
                  errno = EDOM;
                else if (!matherr(&exc)) {
                  if (_LIB_VERSION == _SVID_) {
                    (void) WRITE2("acosh: DOMAIN error\n", 20);
                  }
                  errno = EDOM;
                }
                break;
            case 30:
	    case 130:
                /* atanh(|x|>1) */
                exc.type = DOMAIN;
                exc.name = type < 100 ? "atanh" : "atanhf";
                exc.retval = zero/zero;
                if (_LIB_VERSION == _POSIX_)
                  errno = EDOM;
                else if (!matherr(&exc)) {
                  if (_LIB_VERSION == _SVID_) {
                    (void) WRITE2("atanh: DOMAIN error\n", 20);
                  }
                  errno = EDOM;
                }
                break;
            case 31:
	    case 131:
                /* atanh(|x|=1) */
                exc.type = SING;
                exc.name = type < 100 ? "atanh" : "atanhf";
		exc.retval = x/zero;	/* sign(x)*inf */
                if (_LIB_VERSION == _POSIX_)
                  errno = EDOM;
                else if (!matherr(&exc)) {
                  if (_LIB_VERSION == _SVID_) {
                    (void) WRITE2("atanh: SING error\n", 18);
                  }
                  errno = EDOM;
                }
                break;
	    case 32:
	    case 132:
		/* scalb overflow; SVID also returns +-HUGE_VAL */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "scalb" : "scalbf";
		exc.retval = x > zero ? HUGE_VAL : -HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
		else if (!matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 33:
	    case 133:
		/* scalb underflow */
		exc.type = UNDERFLOW;
		exc.name = type < 100 ? "scalb" : "scalbf";
		exc.retval = copysign(zero,x);
		if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
		else if (!matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 34:
	    case 134:
		/* j0(|x|>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "j0" : "j0f";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 35:
	    case 135:
		/* y0(x>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "y0" : "y0f";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 36:
	    case 136:
		/* j1(|x|>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "j1" : "j1f";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 37:
	    case 137:
		/* y1(x>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "y1" : "y1f";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 38:
	    case 138:
		/* jn(|x|>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "jn" : "jnf";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 39:
	    case 139:
		/* yn(x>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "yn" : "ynf";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 40:
	    case 140:
		/* gamma(finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "gamma" : "gammaf";
                if (_LIB_VERSION == _SVID_)
                  exc.retval = HUGE;
                else
                  exc.retval = HUGE_VAL;
                if (_LIB_VERSION == _POSIX_)
		  errno = ERANGE;
                else if (!matherr(&exc)) {
                  errno = ERANGE;
                }
		break;
	    case 41:
	    case 141:
		/* gamma(-integer) or gamma(0) */
		exc.type = SING;
		exc.name = type < 100 ? "gamma" : "gammaf";
                if (_LIB_VERSION == _SVID_)
                  exc.retval = HUGE;
                else
                  exc.retval = HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			(void) WRITE2("gamma: SING error\n", 18);
		      }
		  errno = EDOM;
		}
		break;
	    case 42:
	    case 142:
		/* pow(NaN,0.0) */
		/* error only if _LIB_VERSION == _SVID_ & _XOPEN_ */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "pow" : "powf";
		exc.retval = x;
		if (_LIB_VERSION == _IEEE_ ||
		    _LIB_VERSION == _POSIX_) exc.retval = 1.0;
		else if (!matherr(&exc)) {
			errno = EDOM;
		}
		break;
	}
	return exc.retval;
}
