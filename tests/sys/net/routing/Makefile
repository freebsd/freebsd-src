# $FreeBSD$

PACKAGE=	tests
WARNS?=		1

TESTSDIR=       ${TESTSBASE}/sys/net/routing

ATF_TESTS_C +=	test_rtsock_l3
ATF_TESTS_C +=	test_rtsock_lladdr
ATF_TESTS_PYTEST +=	test_routing_l3.py
ATF_TESTS_PYTEST +=	test_rtsock_multipath.py

${PACKAGE}FILES+=	generic_cleanup.sh
${PACKAGE}FILESMODE_generic_cleanup.sh=0555

# Most of the tests operates on a common IPv4/IPv6 prefix,
# so running them in parallel will lead to weird results.
TEST_METADATA+=	is_exclusive=true

CFLAGS+=	-I${.CURDIR:H:H:H}

.include <bsd.test.mk>
