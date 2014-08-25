# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_upgrade
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_upgrade_test

FILES+=	setup.ksh
FILES+=	zpool_upgrade_003_pos.ksh
FILES+=	zpool_upgrade_007_pos.ksh
FILES+=	zpool_upgrade_006_neg.ksh
FILES+=	zpool_upgrade_002_pos.ksh
FILES+=	zpool_upgrade.cfg
FILES+=	zpool_upgrade_004_pos.ksh
FILES+=	zpool_upgrade_008_pos.ksh
FILES+=	zpool_upgrade_005_neg.ksh
FILES+=	zpool_upgrade_009_neg.ksh
FILES+=	zpool_upgrade.kshlib
FILES+=	cleanup.ksh
FILES+=	zpool_upgrade_001_pos.ksh

SUBDIR+=	blockfiles

.include <bsd.test.mk>
