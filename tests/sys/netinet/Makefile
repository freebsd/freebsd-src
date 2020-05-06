# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netinet
BINDIR=		${TESTSDIR}

ATF_TESTS_C=	ip_reass_test \
		so_reuseport_lb_test \
		socket_afinet

ATF_TESTS_SH=	carp fibs_test redirect divert forward output lpm

PROGS=	udp_dontroute tcp_user_cookie

${PACKAGE}FILES+=		redirect.py

${PACKAGE}FILESMODE_redirect.py=0555

MAN=

WARNS?=	6

.include <bsd.test.mk>
