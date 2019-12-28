# $FreeBSD$

PACKAGE=	tests

TESTSDIR=       ${TESTSBASE}/sys/net

ATF_TESTS_C +=	test_rtsock_l3
ATF_TESTS_C +=	test_rtsock_lladdr

# Most of the tests operates on a common IPv4/IPv6 prefix,
# so running them in parallel will lead to weird results.
TEST_METADATA+=	is_exclusive=true

.include <bsd.test.mk>
