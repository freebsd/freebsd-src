# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zfs_promote
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_promote_test

FILES+=	setup.ksh
FILES+=	zfs_promote_006_neg.ksh
FILES+=	zfs_promote_003_pos.ksh
FILES+=	zfs_promote_common.kshlib
FILES+=	cleanup.ksh
FILES+=	zfs_promote_007_neg.ksh
FILES+=	zfs_promote.cfg
FILES+=	zfs_promote_002_pos.ksh
FILES+=	zfs_promote_004_pos.ksh
FILES+=	zfs_promote_008_pos.ksh
FILES+=	zfs_promote_005_pos.ksh
FILES+=	zfs_promote_001_pos.ksh

.include <bsd.test.mk>
