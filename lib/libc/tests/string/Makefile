# $FreeBSD$

ATF_TESTS_C+=		stpncpy_test
ATF_TESTS_C+=		strerror2_test
ATF_TESTS_C+=		wcscasecmp_test
ATF_TESTS_C+=		wcsnlen_test

TESTSDIR=	${TESTSBASE}/lib/libc/string

# TODO: popcount, stresep

NETBSD_ATF_TESTS_C+=	memchr
NETBSD_ATF_TESTS_C+=	memcpy
NETBSD_ATF_TESTS_C+=	memmem
NETBSD_ATF_TESTS_C+=	memset
NETBSD_ATF_TESTS_C+=	strcat
NETBSD_ATF_TESTS_C+=	strchr
NETBSD_ATF_TESTS_C+=	strcmp
NETBSD_ATF_TESTS_C+=	strcpy
NETBSD_ATF_TESTS_C+=	strcspn
NETBSD_ATF_TESTS_C+=	strerror
NETBSD_ATF_TESTS_C+=	strlen
NETBSD_ATF_TESTS_C+=	strpbrk
NETBSD_ATF_TESTS_C+=	strrchr
NETBSD_ATF_TESTS_C+=	strspn
NETBSD_ATF_TESTS_C+=	swab

SRCS.strerror2_test=	strerror_test.c

.include "../Makefile.netbsd-tests"

LDADD.memchr+=	-lmd
DPADD.memchr+=	${LIBMD}

LDADD.memcpy+=	-lmd
DPADD.memcpy+=	${LIBMD}

.include <bsd.test.mk>
