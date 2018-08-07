# $FreeBSD$

PACKAGE=	tests

TESTSDIR=       ${TESTSBASE}/sys/netpfil/pf
TESTS_SUBDIRS+=	ioctl

ATF_TESTS_SH+=	pass_block \
		forward \
		fragmentation \
		set_tos \
		route_to \
		synproxy

${PACKAGE}FILES+=	utils.subr \
			echo_inetd.conf \
			pft_ping.py

${PACKAGE}FILESMODE_pft_ping.py=	0555

.include <bsd.test.mk>
