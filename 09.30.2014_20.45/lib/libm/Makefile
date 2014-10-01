# $NetBSD: Makefile,v 1.26 2014/08/10 11:30:51 martin Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libm

.if ${MACHINE_CPU} == "aarch64" || ${MACHINE_CPU} == "arm" \
    || ${MACHINE_ARCH} == "sparc" || ${MACHINE_ARCH} == "sparc64" \
    || ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
CPPFLAGS+=	-DHAVE_FENV_H
.endif

.if ${MACHINE} == "alpha"
COPTS+=	-mfloat-ieee -mieee-with-inexact -mfp-trap-mode=sui -mtrap-precision=i
.endif

TESTS_C+=	t_acos
TESTS_C+=	t_asin
TESTS_C+=	t_atan
TESTS_C+=	t_cbrt
TESTS_C+=	t_ceil
TESTS_C+=	t_cos
TESTS_C+=	t_cosh
TESTS_C+=	t_erf
TESTS_C+=	t_exp
TESTS_C+=	t_fmod
TESTS_C+=	t_infinity
TESTS_C+=	t_ldexp
TESTS_C+=	t_log
TESTS_C+=	t_modf
TESTS_C+=	t_pow
TESTS_C+=	t_precision
TESTS_C+=	t_round
TESTS_C+=	t_scalbn
TESTS_C+=	t_sin
TESTS_C+=	t_sinh
TESTS_C+=	t_sqrt
TESTS_C+=	t_tan
TESTS_C+=	t_tanh

LDADD+=		-lm
#COPTS+=	-Wfloat-equal

.include <bsd.test.mk>
