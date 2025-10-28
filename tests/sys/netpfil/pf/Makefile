PACKAGE=	tests

TESTSDIR=       ${TESTSBASE}/sys/netpfil/pf
TESTS_SUBDIRS+=	ioctl

ATF_TESTS_SH+=	altq \
		anchor \
		counters \
		debug \
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
		if_enc \
		limits \
		loginterface \
		killstate \
		macro \
		match \
		max_pkt_rate \
		max_pkt_size \
		max_states \
		mbuf \
		modulate \
		names \
		nat \
		nat64 \
		once \
		pass_block \
		pflog \
		pflow \
		pfsync	\
		prio \
		proxy \
		rdr \
		ridentifier \
		route_to \
		rtable \
		rules_counter \
		scrub_compat \
		scrub_pass \
		sctp \
		set_skip \
		set_tos \
		snmp \
		src_track \
		status \
		syncookie \
		synproxy \
		table \
		tcp \
		tos

ATF_TESTS_PYTEST+=	frag4.py
ATF_TESTS_PYTEST+=	frag6.py
ATF_TESTS_PYTEST+=	header.py
ATF_TESTS_PYTEST+=	icmp.py
ATF_TESTS_PYTEST+=	igmp.py
ATF_TESTS_PYTEST+=	mld.py
ATF_TESTS_PYTEST+=	nat44.py
ATF_TESTS_PYTEST+=	nat64.py
ATF_TESTS_PYTEST+=	nat66.py
ATF_TESTS_PYTEST+=	return.py
ATF_TESTS_PYTEST+=	sctp.py
ATF_TESTS_PYTEST+=	tcp.py

# Allow tests to run in parallel in their own jails
TEST_METADATA+= execenv="jail"
TEST_METADATA+= execenv_jail_params="vnet allow.raw_sockets allow.read_msgbuf"

${PACKAGE}FILES+=	\
			bsnmpd.conf \
			CVE-2019-5597.py \
			CVE-2019-5598.py \
			daytime_inetd.conf \
			echo_inetd.conf \
			fragcommon.py \
			frag-overindex.py \
			frag-overlimit.py \
			frag-overreplace.py \
			frag-overhole.py \
			frag-adjhole.py \
			pfsync_defer.py \
			pft_ether.py \
			pft_read_ipfix.py \
			rdr-srcport.py \
			tftpd_inetd.conf \
			tftpd_proxy_inetd.conf \
			utils.subr \
			utils.py

${PACKAGE}FILESMODE_bsnmpd.conf=		0555
${PACKAGE}FILESMODE_CVE-2019-5597.py=	0555
${PACKAGE}FILESMODE_CVE-2019-5598.py=	0555
${PACKAGE}FILESMODE_fragcommon.py=	0555
${PACKAGE}FILESMODE_frag-overindex.py=	0555
${PACKAGE}FILESMODE_frag-overlimit.py=	0555
${PACKAGE}FILESMODE_frag-overreplace.py=	0555
${PACKAGE}FILESMODE_frag-overhole.py=	0555
${PACKAGE}FILESMODE_frag-adjhole.py=	0555
${PACKAGE}FILESMODE_pfsync_defer.py=	0555
${PACKAGE}FILESMODE_pft_ether.py=	0555
${PACKAGE}FILESMODE_pft_read_ipfix.py=	0555

.include <bsd.test.mk>
