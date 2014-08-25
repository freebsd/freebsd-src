# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_clear
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_clear_test

FILES+=	cleanup.ksh
FILES+=	zpool_clear_003_neg.ksh
FILES+=	zpool_clear_002_neg.ksh
FILES+=	zpool_clear.cfg
FILES+=	zpool_clear_001_pos.ksh
FILES+=	setup.ksh
FILES+=	zpool_clear_004_pos.ksh

.include <bsd.test.mk>
