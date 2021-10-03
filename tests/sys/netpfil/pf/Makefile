# $FreeBSD$

PACKAGE=	tests

TESTSDIR=       ${TESTSBASE}/sys/netpfil/pf
TESTS_SUBDIRS+=	ioctl

ATF_TESTS_SH+=	altq \
		anchor \
		checksum \
		dup \
		forward \
		fragmentation \
		get_state \
		icmp \
		killstate \
		macro \
		map_e \
		names \
		nat \
		pass_block \
		pfsync	\
		proxy \
		rdr \
		route_to \
		rules_counter \
		set_skip \
		set_tos \
		src_track \
		syncookie \
		synproxy \
		table \
		tos

${PACKAGE}FILES+=	CVE-2019-5597.py \
			CVE-2019-5598.py \
			echo_inetd.conf \
			fragcommon.py \
			frag-overindex.py \
			frag-overlimit.py \
			frag-overreplace.py \
			utils.subr

${PACKAGE}FILESMODE_CVE-2019-5597.py=	0555
${PACKAGE}FILESMODE_CVE-2019-5598.py=	0555
${PACKAGE}FILESMODE_fragcommon.py=	0555
${PACKAGE}FILESMODE_frag-overindex.py=	0555
${PACKAGE}FILESMODE_frag-overlimit.py=	0555
${PACKAGE}FILESMODE_frag-overreplace.py=	0555

.include <bsd.test.mk>
