# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netinet
BINDIR=		${TESTSDIR}

TESTS_SUBDIRS+=	libalias

ATF_TESTS_C=	ip_reass_test \
		so_reuseport_lb_test \
		socket_afinet \
		tcp_connect_port_test

ATF_TESTS_SH=	arp \
		carp \
		divert \
		fibs \
		fibs_test \
		forward \
		lpm \
		output \
		redirect

TEST_METADATA.output+=	required_programs="python"

PROGS=	udp_dontroute tcp_user_cookie

${PACKAGE}FILES+=		redirect.py

${PACKAGE}FILESMODE_redirect.py=0555

MAN=

.include <bsd.test.mk>
