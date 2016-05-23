# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/delegate/zfs_unallow
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_unallow_test

${PACKAGE}FILES+=	zfs_unallow_003_pos.ksh
${PACKAGE}FILES+=	zfs_unallow_007_neg.ksh
${PACKAGE}FILES+=	zfs_unallow_006_pos.ksh
${PACKAGE}FILES+=	zfs_unallow_002_pos.ksh
${PACKAGE}FILES+=	zfs_unallow_004_pos.ksh
${PACKAGE}FILES+=	zfs_unallow_008_neg.ksh
${PACKAGE}FILES+=	zfs_unallow_005_pos.ksh
${PACKAGE}FILES+=	zfs_unallow_001_pos.ksh

.include <bsd.test.mk>
