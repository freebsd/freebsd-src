#  @(#)Makefile 5.1beta 93/09/24 
#  $Id: Makefile,v 1.6 1994/05/31 23:53:57 ljo Exp $
# 
#  ====================================================
#  Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
# 
#  Developed at SunPro, a Sun Microsystems, Inc. business.
#  Permission to use, copy, modify, and distribute this
#  software is freely granted, provided that this notice 
#  is preserved.
#  ====================================================
# 
# 

#
# There are two options in making libm at fdlibm compile time:
# 	_IEEE_LIBM 	--- IEEE libm; smaller, and somewhat faster
#	_MULTI_LIBM	--- Support multi-standard at runtime by 
#			    imposing wrapper functions defined in 
#			    fdlibm.h:
#				_IEEE_MODE 	-- IEEE
#				_XOPEN_MODE 	-- X/OPEN
#				_POSIX_MODE 	-- POSIX/ANSI
#				_SVID3_MODE 	-- SVID
#
# Here is how to set up CFLAGS to create the desired libm at 
# compile time:
#
# 	CFLAGS = -D_IEEE_LIBM		... IEEE libm (recommended)
#	CFLAGS = -D_SVID3_MODE	... Multi-standard supported
#					    libm with SVID as the 
#					    default standard
#	CFLAGS = -D_XOPEN_MODE	... Multi-standard supported
#					    libm with XOPEN as the 
#					    default standard
#	CFLAGS = -D_POSIX_MODE	... Multi-standard supported
#					    libm with POSIX as the 
#					    default standard
#	CFLAGS = 			... Multi-standard supported
#					    libm with IEEE as the 
#					    default standard
# 

#Define HAVE_FPU if you have a i387 (or i486 or Pentium)

.if defined(HAVE_FPU)
.PATH:	${.CURDIR}/i387

ARCH_SRCS = e_acos.S e_asin.S e_atan2.S e_exp.S e_fmod.S e_log.S e_log10.S \
	    e_remainder.S e_scalb.S e_sqrt.S s_atan.S s_ceil.S s_copysign.S \
	    s_cos.S s_finite.S s_floor.S s_ilogb.S s_log1p.S s_logb.S \
	    s_rint.S s_scalbn.S s_significand.S s_sin.S s_tan.S

.endif

.PATH:	${.CURDIR}/src


CFLAGS+= -D_IEEE_LIBM 

LIB=	m
first:
SRCS = k_standard.c k_rem_pio2.c k_cos.c k_sin.c k_tan.c \
	e_acos.c e_acosh.c e_asin.c e_atan2.c e_atanh.c e_cosh.c e_exp.c \
	e_fmod.c e_gamma.c e_gamma_r.c e_hypot.c e_j0.c e_j1.c e_jn.c \
	e_lgamma.c e_lgamma_r.c e_log.c e_log10.c e_pow.c e_rem_pio2.c \
	e_remainder.c e_scalb.c e_sinh.c e_sqrt.c \
	w_acos.c w_acosh.c w_asin.c w_atan2.c \
	w_atanh.c w_cosh.c w_exp.c w_fmod.c \
	w_gamma.c w_gamma_r.c w_hypot.c w_j0.c \
	w_j1.c w_jn.c w_lgamma.c w_lgamma_r.c \
	w_log.c w_log10.c w_pow.c w_remainder.c \
	w_scalb.c w_sinh.c w_sqrt.c \
	w_cabs.c w_drem.c \
	s_asinh.c s_atan.c s_cbrt.c s_ceil.c s_copysign.c \
	s_cos.c s_erf.c s_expm1.c s_finite.c s_floor.c \
	s_ilogb.c s_lib_version.c \
	s_log1p.c s_logb.c s_matherr.c s_nextafter.c \
	s_rint.c s_scalbn.c s_signgam.c s_significand.c s_sin.c \
	s_tanh.c s_tan.c

SRCS+=${ARCH_SRCS}


# Substitute common sources with any arch specific sources 
MANSRC= ${.CURDIR}/man

MAN3+=	acos.3 acosh.3 asin.3 asinh.3 atan.3 atan2.3 atanh.3 ceil.3 \
	cos.3 cosh.3 erf.3 exp.3 fabs.3 floor.3 fmod.3 hypot.3 ieee.3 \
	ieee_test.3 j0.3 lgamma.3 math.3 rint.3 sin.3 sinh.3 sqrt.3 \
	tan.3 tanh.3

MLINKS+=erf.3 erfc.3
MLINKS+=exp.3 expm1.3 exp.3 log.3 exp.3 log10.3 exp.3 log1p.3 exp.3 pow.3
MLINKS+=hypot.3 cabs.3
MLINKS+=ieee.3 copysign.3 ieee.3 finite.3 ieee.3 ilogb.3 \
	ieee.3 nextafter.3 ieee.3 remainder.3 ieee.3 scalbn.3
MLINKS+=ieee_test.3 logb.3
MLINKS+=ieee_test.3 scalb.3
MLINKS+=ieee_test.3 significand.3
MLINKS+=j0.3 j1.3 j0.3 jn.3 j0.3 y0.3 j0.3 y1.3 j0.3 yn.3
MLINKS+=lgamma.3 gamma.3
MLINKS+=sqrt.3 cbrt.3

beforeinstall:
	@echo Installing new math.h
	@(cd ${.CURDIR}/src; cmp -s math.h ${DESTDIR}/usr/include/math.h || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 math.h \
		${DESTDIR}/usr/include/math.h;)

.include <bsd.lib.mk>
