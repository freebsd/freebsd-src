#  @(#)Makefile 5.1beta 93/09/24
#  $Id: Makefile,v 1.5 1995/05/30 05:47:47 rgrimes Exp $
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

# Enable if you have a i387 (or i486 or Pentium)
.if defined(HAVE_FPU)
.PATH:	${.CURDIR}/i387
ARCH_SRCS = e_acos.S e_asin.S e_atan2.S e_exp.S e_fmod.S e_log.S e_log10.S \
	    e_remainder.S e_scalb.S e_sqrt.S s_atan.S s_ceil.S s_copysign.S \
	    s_cos.S s_finite.S s_floor.S s_ilogb.S s_logb.S \
	    s_rint.S s_scalbn.S s_significand.S s_sin.S s_tan.S

# Broken
# ARCH_SRCS+=	s_log1p.S

.PATH:	${.CURDIR}/man
.PATH:	${.CURDIR}/src

CFLAGS+= -D_IEEE_LIBM

LIB=	m
COMMON_SRCS =	e_acos.c e_acosf.c e_acosh.c e_acoshf.c e_asin.c e_asinf.c \
	e_atan2.c e_atan2f.c e_atanh.c e_atanhf.c e_cosh.c e_coshf.c e_exp.c \
	e_expf.c e_fmod.c e_fmodf.c e_gamma.c e_gamma_r.c e_gammaf.c \
	e_gammaf_r.c e_hypot.c e_hypotf.c e_j0.c e_j0f.c e_j1.c e_j1f.c \
	e_jn.c e_jnf.c e_lgamma.c e_lgamma_r.c e_lgammaf.c e_lgammaf_r.c \
	e_log.c e_log10.c e_log10f.c e_logf.c e_pow.c e_powf.c e_rem_pio2.c \
	e_rem_pio2f.c e_remainder.c e_remainderf.c e_scalb.c e_scalbf.c \
	e_sinh.c e_sinhf.c e_sqrt.c e_sqrtf.c \
	k_cos.c k_cosf.c k_rem_pio2.c k_rem_pio2f.c k_sin.c k_sinf.c \
	k_standard.c k_tan.c k_tanf.c \
	s_asinh.c s_asinhf.c s_atan.c s_atanf.c s_cbrt.c s_cbrtf.c s_ceil.c \
	s_ceilf.c s_copysign.c s_copysignf.c s_cos.c s_cosf.c s_erf.c s_erff.c \
	s_expm1.c s_expm1f.c s_fabsf.c s_finite.c s_finitef.c \
	s_floor.c s_floorf.c s_frexpf.c s_ilogb.c s_ilogbf.c \
	s_isnanf.c s_ldexpf.c s_lib_version.c s_log1p.c \
	s_log1pf.c s_logb.c s_logbf.c s_matherr.c s_modff.c \
	s_nextafter.c s_nextafterf.c s_rint.c s_rintf.c s_scalbn.c s_scalbnf.c \
	s_signgam.c s_significand.c s_significandf.c s_sin.c s_sinf.c s_tan.c \
	s_tanf.c s_tanh.c s_tanhf.c \
	w_acos.c w_acosf.c w_acosh.c w_acoshf.c w_asin.c w_asinf.c w_atan2.c \
	w_atan2f.c w_atanh.c w_atanhf.c w_cabs.c w_cabsf.c w_cosh.c w_coshf.c \
	w_drem.c w_dremf.c w_exp.c w_expf.c w_fmod.c w_fmodf.c w_gamma.c \
	w_gamma_r.c w_gammaf.c w_gammaf_r.c w_hypot.c w_hypotf.c w_j0.c \
	w_j0f.c w_j1.c w_j1f.c w_jn.c w_jnf.c w_lgamma.c w_lgamma_r.c \
	w_lgammaf.c w_lgammaf_r.c w_log.c w_log10.c w_log10f.c w_logf.c \
	w_pow.c w_powf.c w_remainder.c w_remainderf.c w_scalb.c w_scalbf.c \
	w_sinh.c w_sinhf.c w_sqrt.c w_sqrtf.c

# FreeBSD's C library supplies these functions:
#COMMON_SRCS+=	s_fabs.c s_frexp.c s_isnan.c s_ldexp.c s_modf.c


SRCS=${COMMON_SRCS}

# Substitute common sources with any arch specific sources
.for i in ${ARCH_SRCS}
    SRCS:=${SRCS:S/${i:S/.S/.c/}/$i/}
.endfor

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

# XXX we should have only one math.h, and a rule for installing .h's...
beforeinstall:
	@${ECHO} Installing new math.h
	@(cd ${.CURDIR}/src; cmp -s math.h ${DESTDIR}/usr/include/math.h || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 math.h \
		${DESTDIR}/usr/include/math.h;)

.include <bsd.lib.mk>
