# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/ctime
FILESDIR=${TESTSDIR}
BINDIR=${TESTSDIR}

FILES+=	setup.ksh
FILES+=	cleanup.ksh
FILES+=	ctime.cfg

PROG=	ctime_001_pos
MAN=
ATF_TESTS_KSH93=	ctime_test

.include <bsd.test.mk>
