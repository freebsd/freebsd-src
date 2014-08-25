# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/threadsappend
FILESDIR=${TESTSDIR}
BINDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	threadsappend_test

FILES+=	cleanup.ksh
FILES+=	threadsappend_001_pos.ksh
FILES+=	threadsappend.cfg
FILES+=	setup.ksh
PROG+=	threadsappend
LDADD+=	-lpthread
MAN=

.include <bsd.test.mk>
