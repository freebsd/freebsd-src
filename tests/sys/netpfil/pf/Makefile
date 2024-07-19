
PACKAGE=	tests

TESTSDIR=       ${TESTSBASE}/sys/netpfil/pf
BINDIR=		${TESTSDIR}
TESTS_SUBDIRS+=	ioctl

ATF_TESTS_SH+=	altq \
		anchor \
		divert-to \
		dup \
		ether \
		forward \
		fragmentation_compat \
		fragmentation_pass \
		fragmentation_no_reassembly \
		get_state \
		icmp \
		icmp6 \
		killstate \
		macro \
		map_e \
		match \
		modulate \
		names \
		nat \
		pass_block \
		pfsync	\
		prio \
		proxy \
		ridentifier \
		route_to \
		rtable \
		rules_counter \
		scrub_compat \
		scrub_pass \
		sctp \
		set_skip \
		set_tos \
		src_track \
		syncookie \
		synproxy \
		table \
		tcp \
		tos

ATF_TESTS_PYTEST+=	frag6.py
ATF_TESTS_PYTEST+=	sctp.py

# Allow tests to run in parallel in their own jails
TEST_METADATA+= execenv="jail"
TEST_METADATA+= execenv_jail_params="vnet allow.raw_sockets"

PROGS=	divapp

${PACKAGE}FILES+=	CVE-2019-5597.py \
			CVE-2019-5598.py \
			daytime_inetd.conf \
			echo_inetd.conf \
			fragcommon.py \
			frag-overindex.py \
			frag-overlimit.py \
			frag-overreplace.py \
			pfsync_defer.py \
			pft_ether.py \
			utils.subr

${PACKAGE}FILESMODE_CVE-2019-5597.py=	0555
${PACKAGE}FILESMODE_CVE-2019-5598.py=	0555
${PACKAGE}FILESMODE_fragcommon.py=	0555
${PACKAGE}FILESMODE_frag-overindex.py=	0555
${PACKAGE}FILESMODE_frag-overlimit.py=	0555
${PACKAGE}FILESMODE_frag-overreplace.py=	0555
${PACKAGE}FILESMODE_pfsync_defer.py=	0555
${PACKAGE}FILESMODE_pft_ether.py=	0555

.include <bsd.test.mk>
