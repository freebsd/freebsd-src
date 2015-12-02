# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/mqueue

ATF_TESTS_SH=	mqueue_test

BINDIR=		${TESTSDIR}

CFLAGS+=	-I${SRCTOP}/tests

PROGS+=		mqtest1
PROGS+=		mqtest2
#PROGS+=		mqtest3
#PROGS+=		mqtest4
PROGS+=		mqtest5

LDADD+=		-lrt
DPADD+=		${LIBRT}

WARNS?=		6

.include <bsd.test.mk>
