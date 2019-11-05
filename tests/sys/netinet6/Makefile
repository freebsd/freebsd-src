# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netinet6
FILESDIR=	${TESTSDIR}

ATF_TESTS_SH=			\
				exthdr

${PACKAGE}FILES+=		exthdr.py

${PACKAGE}FILESMODE_exthdr.py=	0555

TESTS_SUBDIRS+=	frag6

.include <bsd.test.mk>
