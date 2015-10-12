# $FreeBSD$

.include <bsd.own.mk>

# TODO: t_getenv_thread, t_mi_vector_hash
NETBSD_ATF_TESTS_C=	abs_test
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

.for t in h_getopt h_getopt_long
CFLAGS.$t+=	-I${LIBNETBSD_SRCDIR} -I${SRCTOP}/contrib/netbsd-tests
LDFLAGS.$t+=	-L${LIBNETBSD_OBJDIR}

DPADD.$t+=	${LIBNETBSD} ${LIBUTIL}
LDADD.$t+=	-lnetbsd -lutil
.endfor

DPADD.strtod_test+=		${LIBM}
LDADD.strtod_test+=		-lm

.include <bsd.test.mk>
