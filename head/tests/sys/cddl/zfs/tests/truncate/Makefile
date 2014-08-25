# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/truncate
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	truncate_test

FILES+=	truncate_001_pos.ksh
FILES+=	setup.ksh
FILES+=	cleanup.ksh
FILES+=	truncate_002_pos.ksh
FILES+=	truncate.cfg

.include <bsd.test.mk>
