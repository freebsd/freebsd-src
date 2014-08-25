# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/atime
FILESDIR=${TESTSDIR}

FILES+=	atime.cfg
FILES+=	atime_001_pos.ksh
FILES+=	atime_002_neg.ksh
FILES+=	atime_common.kshlib
FILES+=	cleanup.ksh
FILES+=	setup.ksh

ATF_TESTS_KSH93+=	atime_test

.include <bsd.test.mk>
