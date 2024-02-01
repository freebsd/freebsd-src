
PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netinet
BINDIR=		${TESTSDIR}

TESTS_SUBDIRS+=	libalias

ATF_TESTS_C=	ip_reass_test \
		ip6_v4mapped_test \
		so_reuseport_lb_test \
		socket_afinet \
		tcp_connect_port_test \
		tcp_implied_connect \
		tcp_md5_getsockopt

ATF_TESTS_SH=	arp \
		carp \
		divert \
		fibs \
		fibs_test \
		forward \
		lpm \
		output \
		redirect

ATF_TESTS_PYTEST+=	carp.py
ATF_TESTS_PYTEST+=	igmp.py

TEST_METADATA.divert+=	required_programs="python"
TEST_METADATA.forward+=	required_programs="python"
TEST_METADATA.output+=	required_programs="python"
TEST_METADATA.redirect+= required_programs="python"
TEST_METADATA.tcp6_v4mapped_bind_test+=	is_exclusive="true"

PROGS=	udp_dontroute tcp_user_cookie

${PACKAGE}FILES+=		redirect.py

${PACKAGE}FILESMODE_redirect.py=0555

MAN=

.include <bsd.test.mk>
