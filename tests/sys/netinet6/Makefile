# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netinet6
FILESDIR=	${TESTSDIR}

ATF_TESTS_SH=			\
				exthdr \
				mld \
				scapyi386

${PACKAGE}FILES+=		exthdr.py
${PACKAGE}FILES+=		mld.py
${PACKAGE}FILES+=		scapyi386.py

${PACKAGE}FILESMODE_exthdr.py=	0555
${PACKAGE}FILESMODE_mld.py=	0555
${PACKAGE}FILESMODE_scapyi386.py=0555

TESTS_SUBDIRS+=	frag6

.include <bsd.test.mk>
