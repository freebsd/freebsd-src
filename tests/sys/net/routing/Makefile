# $FreeBSD$

PACKAGE=	tests
WARNS?=		1

TESTSDIR=       ${TESTSBASE}/sys/net/routing

ATF_TESTS_C +=	test_rtsock_l3
ATF_TESTS_C +=	test_rtsock_lladdr

${PACKAGE}FILES+=	generic_cleanup.sh
${PACKAGE}FILESMODE_generic_cleanup.sh=0555

# Most of the tests operates on a common IPv4/IPv6 prefix,
# so running them in parallel will lead to weird results.
TEST_METADATA+=	is_exclusive=true

CFLAGS+=	-I${.CURDIR:H:H:H}

.include <bsd.test.mk>
