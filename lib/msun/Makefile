#  @(#)Makefile 5.1beta 93/09/24
# $FreeBSD$
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
#			    math.h:
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

.if ${MACHINE_ARCH} == "alpha"
ARCH=	alpha
ARCH_SRCS = s_copysign.S s_copysignf.S
# XXX Comment from NetBSD/Alpha:
# XXX LINT SIGFPEs in e_exp.c's strtod().  FP underflow/denorm software
# handling is broken (doesn't exist!) on the Alpha port.
# Stock gcc 2.7.2.1 doesn't understand these options.
#CFLAGS += -mtrap-precision=i -mfp-trap-mode=su
.elif ${MACHINE_ARCH} == "i386"
ARCH=	i387
ARCH_SRCS = e_acos.S e_asin.S e_atan2.S e_exp.S e_fmod.S e_log.S e_log10.S \
	    e_remainder.S e_scalb.S e_sqrt.S s_atan.S s_ceil.S s_copysign.S \
	    s_cos.S s_finite.S s_floor.S s_ilogb.S s_logb.S \
	    s_rint.S s_scalbn.S s_significand.S s_sin.S s_tan.S
# Broken:
# ARCH_SRCS+=	s_log1p.S
.endif

.PATH:	${.CURDIR}/bsdsrc
.PATH:	${.CURDIR}/man
.PATH:	${.CURDIR}/src

CFLAGS+= -D_IEEE_LIBM

LIB=	m
COMMON_SRCS= b_exp.c b_log.c b_tgamma.c \
	e_acos.c e_acosf.c e_acosh.c e_acoshf.c e_asin.c e_asinf.c \
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
	s_floor.c s_floorf.c s_frexp.c s_frexpf.c s_ilogb.c s_ilogbf.c \
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
	w_sinh.c w_sinhf.c w_sqrt.c w_sqrtf.c \
	w_y0.c w_y0f.c w_y1.c w_y1f.c w_yn.c w_ynf.c

# FreeBSD's C library supplies these functions:
#COMMON_SRCS+=	s_fabs.c s_frexp.c s_isnan.c s_ldexp.c s_modf.c

# Exclude the generic versions of what we provide in the MD area.
.if defined(ARCH_SRCS)
.PATH:	${.CURDIR}/${ARCH}
.for i in ${ARCH_SRCS}
COMMON_SRCS:=  ${COMMON_SRCS:N${i:R}.c}
.endfor
.endif

SRCS=	${COMMON_SRCS} ${ARCH_SRCS}

INCS=	math.h

MAN=	acos.3 acosh.3 asin.3 asinh.3 atan.3 atan2.3 atanh.3 ceil.3 \
	cos.3 cosh.3 erf.3 exp.3 fabs.3 floor.3 fmod.3 hypot.3 ieee.3 \
	ieee_test.3 j0.3 lgamma.3 math.3 rint.3 sin.3 sinh.3 sqrt.3 \
	tan.3 tanh.3

MLINKS+=acos.3 acosf.3
MLINKS+=acosh.3 acoshf.3
MLINKS+=asin.3 asinf.3
MLINKS+=asinh.3 asinhf.3
MLINKS+=atan.3 atanf.3
MLINKS+=atanh.3 atanhf.3
MLINKS+=atan2.3 atan2f.3
MLINKS+=ceil.3 ceilf.3
MLINKS+=cos.3 cosf.3
MLINKS+=cosh.3 coshf.3
MLINKS+=erf.3 erfc.3 erf.3 erff.3 erf.3 erfcf.3
MLINKS+=exp.3 expm1.3 exp.3 log.3 exp.3 log10.3 exp.3 log1p.3 exp.3 pow.3 \
	exp.3 expf.3 exp.3 exp10.3 exp.3 exp10f.3 \
	exp.3 expm1f.3 exp.3 logf.3 exp.3 powf.3 \
	exp.3 log10f.3 exp.3 log1pf.3
MLINKS+=fabs.3 fabsf.3
MLINKS+=floor.3 floorf.3
MLINKS+=fmod.3 fmodf.3
MLINKS+=hypot.3 cabs.3 hypot.3 cabsf.3 hypot.3 hypotf.3
MLINKS+=ieee.3 copysign.3 ieee.3 copysignf.3 ieee.3 finite.3 ieee.3 finitef.3 \
	ieee.3 ilogb.3 ieee.3 ilogbf.3 ieee.3 nextafter.3 ieee.3 nextafterf.3 \
	ieee.3 remainder.3 ieee.3 remainderf.3 ieee.3 scalbn.3 ieee.3 scalbnf.3
MLINKS+=ieee_test.3 logb.3 ieee_test.3 logbf.3
MLINKS+=ieee_test.3 scalb.3 ieee_test.3 scalbf.3
MLINKS+=ieee_test.3 significand.3 ieee_test.3 significandf.3
MLINKS+=j0.3 j1.3 j0.3 jn.3 j0.3 y0.3 j0.3 y1.3 j0.3 y1f.3 j0.3 yn.3
MLINKS+=j0.3 j0f.3 j0.3 j1f.3 j0.3 jnf.3 j0.3 y0f.3 j0.3 ynf.3
MLINKS+=lgamma.3 gamma.3 lgamma.3 gammaf.3 lgamma.3 lgammaf.3 lgamma.3 tgamma.3
MLINKS+=rint.3 rintf.3
MLINKS+=sin.3 sinf.3
MLINKS+=sinh.3 sinhf.3
MLINKS+=sqrt.3 cbrt.3 sqrt.3 cbrtf.3 sqrt.3 sqrtf.3
MLINKS+=tan.3 tanf.3
MLINKS+=tanh.3 tanhf.3

.include <bsd.lib.mk>
