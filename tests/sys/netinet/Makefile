# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/netinet
BINDIR=		${TESTSDIR}

TESTS_SUBDIRS+=	libalias

ATF_TESTS_C=	ip_reass_test \
		so_reuseport_lb_test

ATF_TESTS_SH=	carp fibs_test arp

PROGS=	udp_dontroute tcp_user_cookie

MAN=

WARNS?=	6

.include <bsd.test.mk>
