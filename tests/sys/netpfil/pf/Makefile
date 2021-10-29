# $FreeBSD$

PACKAGE=	tests

TESTSDIR=       ${TESTSBASE}/sys/netpfil/pf
TESTS_SUBDIRS+=	ioctl

ATF_TESTS_SH+=	altq \
		anchor \
		pass_block \
		checksum \
		forward \
		fragmentation \
		get_state \
		icmp \
		killstate \
		macro \
		map_e \
		names \
		nat \
		rules_counter \
		set_tos \
		src_track \
		rdr \
		ridentifier \
		route_to \
		syncookie \
		synproxy \
		set_skip \
		pfsync \
		proxy \
		table \
		tos

${PACKAGE}FILES+=	utils.subr \
			echo_inetd.conf \
			CVE-2019-5597.py \
			CVE-2019-5598.py \
			fragcommon.py \
			frag-overindex.py \
			frag-overlimit.py \
			frag-overreplace.py

${PACKAGE}FILESMODE_CVE-2019-5597.py=	0555
${PACKAGE}FILESMODE_CVE-2019-5598.py=	0555
${PACKAGE}FILESMODE_fragcommon.py=	0555
${PACKAGE}FILESMODE_frag-overindex.py=	0555
${PACKAGE}FILESMODE_frag-overlimit.py=	0555
${PACKAGE}FILESMODE_frag-overreplace.py=	0555

.include <bsd.test.mk>
