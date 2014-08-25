# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zfs_upgrade
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_upgrade_test

FILES+=	zfs_upgrade.cfg
FILES+=	cleanup.ksh
FILES+=	zfs_upgrade_004_pos.ksh
FILES+=	zfs_upgrade_001_pos.ksh
FILES+=	zfs_upgrade_005_pos.ksh
FILES+=	setup.ksh
FILES+=	zfs_upgrade.kshlib
FILES+=	zfs_upgrade_006_neg.ksh
FILES+=	zfs_upgrade_003_pos.ksh
FILES+=	zfs_upgrade_007_neg.ksh
FILES+=	zfs_upgrade_002_pos.ksh

.include <bsd.test.mk>
