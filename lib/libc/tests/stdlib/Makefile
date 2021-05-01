# $FreeBSD$

.include <src.opts.mk>

ATF_TESTS_C+=		dynthr_test
ATF_TESTS_C+=		heapsort_test
ATF_TESTS_C+=		mergesort_test
ATF_TESTS_C+=		qsort_test
ATF_TESTS_C+=		qsort_r_test
ATF_TESTS_C+=		qsort_s_test
ATF_TESTS_C+=		set_constraint_handler_s_test
ATF_TESTS_C+=		strfmon_test
ATF_TESTS_C+=		tsearch_test
ATF_TESTS_CXX+=		cxa_thread_atexit_test
ATF_TESTS_CXX+=		cxa_thread_atexit_nothr_test

# All architectures on FreeBSD have fenv.h
CFLAGS+=	-D__HAVE_FENV

# Define __HAVE_LONG_DOUBLE for architectures whose long double has greater
# precision than their double.
.if ${MACHINE_CPUARCH} == "aarch64" || \
    ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386" || \
    ${MACHINE_CPUARCH} == "riscv"
CFLAGS+=	-D__HAVE_LONG_DOUBLE
.endif

# TODO: t_getenv_thread, t_mi_vector_hash, t_strtoi
NETBSD_ATF_TESTS_C+=	abs_test
NETBSD_ATF_TESTS_C+=	atoi_test
NETBSD_ATF_TESTS_C+=	div_test
NETBSD_ATF_TESTS_C+=	getenv_test
NETBSD_ATF_TESTS_C+=	exit_test
NETBSD_ATF_TESTS_C+=	hsearch_test
NETBSD_ATF_TESTS_C+=	posix_memalign_test
NETBSD_ATF_TESTS_C+=	random_test
NETBSD_ATF_TESTS_C+=	strtod_test
NETBSD_ATF_TESTS_C+=	strtol_test
NETBSD_ATF_TESTS_C+=	system_test

# TODO: need to come up with a correct explanation of what the patch pho does
# with h_atexit
#ATF_TESTS_SH=	atexit_test
NETBSD_ATF_TESTS_SH=	getopt_test

.include "../Makefile.netbsd-tests"

BINDIR=		${TESTSDIR}

# TODO: see comment above
#PROGS+=		h_atexit
PROGS+=		h_getopt h_getopt_long

CFLAGS+=	-I${.CURDIR}

CXXSTD.cxa_thread_atexit_test=	c++11
CXXSTD.cxa_thread_atexit_nothr_test=	c++11
LIBADD.cxa_thread_atexit_test+=		pthread

.for t in h_getopt h_getopt_long
CFLAGS.$t+=	-I${LIBNETBSD_SRCDIR} -I${SRCTOP}/contrib/netbsd-tests
LDFLAGS.$t+=	-L${LIBNETBSD_OBJDIR}

LIBADD.${t}+=	netbsd util
.endfor

LIBADD.strtod_test+=		m

SUBDIR+=	dynthr_mod

.include <bsd.test.mk>
