# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_export
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_export_test

FILES+=	zpool_export.cfg
FILES+=	setup.ksh
FILES+=	zpool_export_001_pos.ksh
FILES+=	cleanup.ksh
FILES+=	zpool_export_004_pos.ksh
FILES+=	zpool_export_003_neg.ksh
FILES+=	zpool_export_002_pos.ksh

.include <bsd.test.mk>
