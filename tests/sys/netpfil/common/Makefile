
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

# Allow tests to run in parallel in their own jails
TEST_METADATA+= execenv="jail"
TEST_METADATA+= execenv_jail_params="vnet allow.raw_sockets"

${PACKAGE}FILES+=	\
		utils.subr \
		runner.subr \
		pft_icmp_check.py \
		pft_ping.py \
		pft_rst.py \
		pft_synflood.py \
		sniffer.py

${PACKAGE}FILESMODE_pft_icmp_check.py=	0555
${PACKAGE}FILESMODE_pft_ping.py=	0555
${PACKAGE}FILESMODE_pft_rst.py=	0555
${PACKAGE}FILESMODE_pft_synflood.py=	0555

.include <bsd.test.mk>
