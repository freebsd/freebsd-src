# $FreeBSD$

TESTSRC=	${SRCTOP}/contrib/netbsd-tests/lib/libm

# All architectures on FreeBSD have fenv.h
CFLAGS+=	-DHAVE_FENV_H

# Not sure why this isn't defined for all architectures, since most
# have long double.
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
CFLAGS+=	-D__HAVE_LONG_DOUBLE
.endif

NETBSD_ATF_TESTS_C=	acos_test
NETBSD_ATF_TESTS_C+=	asin_test
NETBSD_ATF_TESTS_C+=	atan_test
NETBSD_ATF_TESTS_C+=	cbrt_test
NETBSD_ATF_TESTS_C+=	ceil_test
NETBSD_ATF_TESTS_C+=	cos_test
NETBSD_ATF_TESTS_C+=	cosh_test
NETBSD_ATF_TESTS_C+=	erf_test
NETBSD_ATF_TESTS_C+=	exp_test
NETBSD_ATF_TESTS_C+=	fmod_test
NETBSD_ATF_TESTS_C+=	infinity_test
NETBSD_ATF_TESTS_C+=	ldexp_test
NETBSD_ATF_TESTS_C+=	log_test
NETBSD_ATF_TESTS_C+=	pow_test
NETBSD_ATF_TESTS_C+=	precision_test
NETBSD_ATF_TESTS_C+=	round_test
NETBSD_ATF_TESTS_C+=	scalbn_test
NETBSD_ATF_TESTS_C+=	sin_test
NETBSD_ATF_TESTS_C+=	sinh_test
NETBSD_ATF_TESTS_C+=	sqrt_test
NETBSD_ATF_TESTS_C+=	tan_test
NETBSD_ATF_TESTS_C+=	tanh_test

CSTD=		c99

LIBADD+=	m
#COPTS+=	-Wfloat-equal

# Copied from lib/msun/Makefile
.if ${MACHINE_CPUARCH} == "i386"
ARCH_SUBDIR= i387
.else
ARCH_SUBDIR= ${MACHINE_CPUARCH}
.endif

.include "../${ARCH_SUBDIR}/Makefile.inc"

# XXX: for some odd reason float.h doesn't tell the full story about what the
# precision is.
CFLAGS+=	-DLDBL_PREC=${LDBL_PREC}

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
