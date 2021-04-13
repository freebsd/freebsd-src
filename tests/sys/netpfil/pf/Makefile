# $FreeBSD$

PACKAGE=	tests

TESTSDIR=       ${TESTSBASE}/sys/netpfil/pf
TESTS_SUBDIRS+=	ioctl

ATF_TESTS_SH+=	anchor \
		pass_block \
		checksum \
		forward \
		fragmentation \
		icmp \
		killstate \
		map_e \
		names \
		nat \
		rules_counter \
		set_tos \
		src_track \
		rdr \
		route_to \
		synproxy \
		set_skip \
		pfsync \
		table \
		tos

${PACKAGE}FILES+=	utils.subr \
			echo_inetd.conf \
			CVE-2019-5597.py \
			CVE-2019-5598.py

${PACKAGE}FILESMODE_CVE-2019-5597.py=	0555
${PACKAGE}FILESMODE_CVE-2019-5598.py=	0555

.include <bsd.test.mk>
