# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netpfil/common

${PACKAGE}FILES+=	\
		pft_ping.py \
		pft_synflood.py \
		sniffer.py

${PACKAGE}FILESMODE_pft_ping.py=	0555
${PACKAGE}FILESMODE_pft_synflood.py=	0555

.include <bsd.test.mk>
