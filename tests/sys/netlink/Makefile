# $FreeBSD$

PACKAGE=	tests
WARNS?=		1

TESTSDIR=       ${TESTSBASE}/sys/netlink

ATF_TESTS_C +=	test_snl
ATF_TESTS_PYTEST +=	test_rtnl_iface.py

CFLAGS+=	-I${.CURDIR:H:H:H}

.include <bsd.test.mk>
