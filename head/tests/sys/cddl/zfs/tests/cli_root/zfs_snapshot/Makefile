# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zfs_snapshot
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_snapshot_test

FILES+=	cleanup.ksh
FILES+=	zfs_snapshot_005_neg.ksh
FILES+=	zfs_snapshot_001_neg.ksh
FILES+=	zfs_snapshot_004_neg.ksh
FILES+=	setup.ksh
FILES+=	zfs_snapshot_002_neg.ksh
FILES+=	zfs_snapshot_006_pos.ksh
FILES+=	zfs_snapshot_007_neg.ksh
FILES+=	zfs_snapshot_003_neg.ksh
FILES+=	zfs_snapshot.cfg

.include <bsd.test.mk>
