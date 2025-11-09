PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netinet
BINDIR=		${TESTSDIR}

TESTS_SUBDIRS+=	libalias

ATF_TESTS_C=	broadcast \
		fibs_multibind_test \
		ip_reass_test \
		ip6_v4mapped_test \
		so_reuseport_lb_test \
		socket_afinet \
		tcp_connect_port_test \
		tcp_implied_connect \
		tcp_md5_getsockopt \
		udp_bindings \
		udp_io

ATF_TESTS_SH=	arp \
		carp \
		divert \
		fibs \
		fibs_test \
		forward \
		lpm \
		multicast \
		output \
		redirect

ATF_TESTS_PYTEST+=	carp.py
ATF_TESTS_PYTEST+=	igmp.py
ATF_TESTS_PYTEST+=	tcp_hpts_test.py

LIBADD.so_reuseport_lb_test=	pthread
LIBADD.udp_bindings=		pthread

# Some of the arp tests look for log messages in the dmesg buffer, so run them
# serially to avoid problems with interleaved output.
TEST_METADATA.arp+=	is_exclusive="true"
TEST_METADATA.divert+=	required_programs="python"	\
			execenv="jail"			\
			execenv_jail_params="vnet allow.raw_sockets"
TEST_METADATA.fibs_test+= execenv="jail"		\
			execenv_jail_params="vnet allow.raw_sockets"
TEST_METADATA.forward+=	required_programs="python"	\
			execenv="jail"			\
			execenv_jail_params="vnet allow.raw_sockets"
TEST_METADATA.multicast+= execenv="jail"		\
			execenv_jail_params="vnet"
TEST_METADATA.output+=	required_programs="python"
TEST_METADATA.redirect+= required_programs="python"

PROGS=	udp_dontroute tcp_user_cookie multicast-send multicast-receive

${PACKAGE}FILES+=		redirect.py

${PACKAGE}FILESMODE_redirect.py=0555

MAN=

.include <bsd.test.mk>
