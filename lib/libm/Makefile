# $NetBSD: Makefile,v 1.42 2016/12/20 06:13:19 maya Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libm

.if ${MACHINE} == "alpha"
COPTS+=	-mfloat-ieee -mieee-with-inexact -mfp-trap-mode=sui -mtrap-precision=i
.endif

CPPFLAGS.t_fenv.c+=	-D__TEST_FENV
CPPFLAGS.t_fe_round.c+=	-D__TEST_FENV
CPPFLAGS.t_ilogb.c+=	-D__TEST_FENV
CPPFLAGS.t_fmod.c+=	-I${.CURDIR}/../libc/gen

TESTS_C+=	t_acos
TESTS_C+=	t_asin
TESTS_C+=	t_atan
TESTS_C+=	t_casinh
TESTS_C+=	t_cbrt
TESTS_C+=	t_ceil
TESTS_C+=	t_cos
TESTS_C+=	t_cosh
TESTS_C+=	t_erf
TESTS_C+=	t_exp
TESTS_C+=	t_fenv
TESTS_C+=	t_fe_round
TESTS_C+=	t_fmod
TESTS_C+=	t_hypot
TESTS_C+=	t_ilogb
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
