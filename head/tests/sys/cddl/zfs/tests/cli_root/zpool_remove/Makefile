# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_remove
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_remove_test

FILES+=	zpool_remove_001_neg.ksh
FILES+=	setup.ksh
FILES+=	zpool_remove.cfg
FILES+=	zpool_remove_002_pos.ksh
FILES+=	cleanup.ksh
FILES+=	zpool_remove_003_pos.ksh

.include <bsd.test.mk>
