# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zfs_rollback
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_rollback_test

FILES+=	zfs_rollback_003_neg.ksh
FILES+=	zfs_rollback_002_pos.ksh
FILES+=	zfs_rollback_common.kshlib
FILES+=	cleanup.ksh
FILES+=	zfs_rollback_004_neg.ksh
FILES+=	zfs_rollback_001_pos.ksh
FILES+=	setup.ksh
FILES+=	zfs_rollback.cfg

.include <bsd.test.mk>
