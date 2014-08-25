# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zfs_destroy
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_destroy_test

FILES+=	zfs_destroy_001_pos.ksh
FILES+=	zfs_destroy.cfg
FILES+=	cleanup.ksh
FILES+=	zfs_destroy_004_pos.ksh
FILES+=	zfs_destroy_005_neg.ksh
FILES+=	zfs_destroy_common.kshlib
FILES+=	zfs_destroy_002_pos.ksh
FILES+=	zfs_destroy_007_neg.ksh
FILES+=	setup.ksh
FILES+=	zfs_destroy_003_pos.ksh
FILES+=	zfs_destroy_006_neg.ksh

.include <bsd.test.mk>
