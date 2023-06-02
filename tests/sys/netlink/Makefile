# $FreeBSD$

PACKAGE=	tests
WARNS?=		1

TESTSDIR=       ${TESTSBASE}/sys/netlink

ATF_TESTS_C +=	test_snl test_snl_generic
ATF_TESTS_PYTEST +=	test_nl_core.py
ATF_TESTS_PYTEST +=	test_rtnl_iface.py
ATF_TESTS_PYTEST +=	test_rtnl_ifaddr.py
ATF_TESTS_PYTEST +=	test_rtnl_neigh.py
ATF_TESTS_PYTEST +=	test_rtnl_route.py
ATF_TESTS_PYTEST +=	test_netlink_message_writer.py

CFLAGS+=	-I${.CURDIR:H:H:H}

.include <bsd.test.mk>
