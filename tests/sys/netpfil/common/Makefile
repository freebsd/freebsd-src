
PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netpfil/common


ATF_TESTS_SH+=	\
		dummynet \
		pass_block \
		nat \
		rdr \
		tos \
		fragments \
		forward

# Tests reuse jail names and so cannot run in parallel.
TEST_METADATA+=	is_exclusive=true

${PACKAGE}FILES+=	\
		utils.subr \
		runner.subr \
		pft_icmp_check.py \
		pft_ping.py \
		pft_synflood.py \
		sniffer.py

${PACKAGE}FILESMODE_pft_icmp_check.py=	0555
${PACKAGE}FILESMODE_pft_ping.py=	0555
${PACKAGE}FILESMODE_pft_synflood.py=	0555

.include <bsd.test.mk>
