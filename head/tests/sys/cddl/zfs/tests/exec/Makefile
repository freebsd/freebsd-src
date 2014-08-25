# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/exec/
FILESDIR=${TESTSDIR}
BINDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	exec_test

FILES+=	exec_001_pos.ksh
FILES+=	setup.ksh
FILES+=	cleanup.ksh
FILES+=	exec_002_neg.ksh
PROG=	mmap_exec
MAN=

.include <bsd.test.mk>
