.include <bsd.own.mk>

TESTSRC=	${SRCTOP}/contrib/netbsd-tests/lib/libm

# All architectures on FreeBSD have fenv.h
CFLAGS+=	-DHAVE_FENV_H

# For isqemu.h
CFLAGS+=	-I${TESTSRC:H}/libc/gen

# Define __HAVE_LONG_DOUBLE for architectures whose long double has greater
# precision than their double.
.if ${MACHINE_CPUARCH} == "aarch64" || \
    ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386" || \
    ${MACHINE_CPUARCH} == "riscv"
CFLAGS+=	-D__HAVE_LONG_DOUBLE
.endif

# Avoid builtins, to force the compiler to emit calls to the libm
# functions, and not calculate any results in advance.
CFLAGS+=        -fno-builtin

NETBSD_ATF_TESTS_C=	acos_test
NETBSD_ATF_TESTS_C+=	asin_test
NETBSD_ATF_TESTS_C+=	atan_test
NETBSD_ATF_TESTS_C+=	bit_test
# TODO: NETBSD_ATF_TESTS_CXX+=	cabsl_test
NETBSD_ATF_TESTS_C+=	cbrt_test
NETBSD_ATF_TESTS_C+=	ceil_test
NETBSD_ATF_TESTS_C+=	casinh_test
NETBSD_ATF_TESTS_C+=	cos_test
NETBSD_ATF_TESTS_C+=	cosh_test
NETBSD_ATF_TESTS_C+=	erf_test
NETBSD_ATF_TESTS_C+=	exp_test
NETBSD_ATF_TESTS_C+=	fmod_test
NETBSD_ATF_TESTS_C+=	fe_round_test
NETBSD_ATF_TESTS_C+=	hypot_test
NETBSD_ATF_TESTS_C+=	infinity_test
NETBSD_ATF_TESTS_C+=	ilogb_test
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

ATF_TESTS_C+=	cexp_test
ATF_TESTS_C+=	conj_test
ATF_TESTS_C+=	csqrt_test
ATF_TESTS_C+=	ctrig_test
ATF_TESTS_C+=	exponential_test
ATF_TESTS_C+=	fenv_test
ATF_TESTS_C+=	fma_test
ATF_TESTS_C+=	fmaxmin_test
ATF_TESTS_C+=	ilogb2_test
ATF_TESTS_C+=	invtrig_test
ATF_TESTS_C+=	invctrig_test
ATF_TESTS_C+=	logarithm_test
ATF_TESTS_C+=	lrint_test
ATF_TESTS_C+=	lround_test
ATF_TESTS_C+=	nan_test
ATF_TESTS_C+=	nearbyint_test
ATF_TESTS_C+=	next_test
ATF_TESTS_C+=	rem_test
ATF_TESTS_C+=	trig_test

.if !empty(PROG) && !empty(TAP_TESTS_C:M${PROG})
CFLAGS+=	-O0
.endif

#COPTS+=	-Wfloat-equal

IGNORE_PRAGMA=

SRCS.ilogb2_test=	ilogb_test.c

LIBADD.fenv_test+=	util
LIBADD+=	m

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

CFLAGS.fe_round_test+=	-D__HAVE_FENV

.include <bsd.compiler.mk>
.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 120000
# Silence '#pragma FENV_ACCESS' is not supported on this target - ignored
CWARNFLAGS+=	-Wno-error=ignored-pragmas
.endif

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
