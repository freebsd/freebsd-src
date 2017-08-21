# $FreeBSD$

TAP_TESTS_C=	sysctl_test

LIBADD+=	casper
LIBADD+=	cap_sysctl
LIBADD+=	nv

WARNS?=		3

TEST_METADATA.sysctl_test+=	required_user="root"

.include <bsd.test.mk>
