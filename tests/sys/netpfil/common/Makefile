# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netpfil/common

ATF_TESTS_SH+=	\
	dummynet \
	forward \
	fragments \
	nat \
	pass_block \
	tos

${PACKAGE}FILES+=	\
	pft_icmp_check.py \
	pft_ping.py \
	pft_synflood.py \
	runner.subr \
	sniffer.py \
	utils.subr

${PACKAGE}FILESMODE_pft_icmp_check.py=	0555
${PACKAGE}FILESMODE_pft_ping.py=	0555
${PACKAGE}FILESMODE_pft_synflood.py=	0555

.include <bsd.test.mk>
