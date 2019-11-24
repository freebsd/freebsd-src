# $FreeBSD$

.include <bsd.compiler.mk>

TESTSDIR=	${TESTSBASE}/sys/sys

ATF_TESTS_C=	arb_test bitstring_test qmath_test rb_test splay_test

WARNS?=	5

.if ${COMPILER_TYPE} == "gcc"
CFLAGS.bitstring_test=	-fno-strict-overflow
.endif

.include <bsd.test.mk>
