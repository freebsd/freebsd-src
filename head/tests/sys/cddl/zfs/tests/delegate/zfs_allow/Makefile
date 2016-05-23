# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/delegate/zfs_allow
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_allow_test

${PACKAGE}FILES+=	zfs_allow_003_pos.ksh
${PACKAGE}FILES+=	zfs_allow_007_pos.ksh
${PACKAGE}FILES+=	zfs_allow_002_pos.ksh
${PACKAGE}FILES+=	zfs_allow_006_pos.ksh
${PACKAGE}FILES+=	zfs_allow_012_neg.ksh
${PACKAGE}FILES+=	zfs_allow_009_neg.ksh
${PACKAGE}FILES+=	zfs_allow_004_pos.ksh
${PACKAGE}FILES+=	zfs_allow_008_pos.ksh
${PACKAGE}FILES+=	zfs_allow_010_pos.ksh
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	zfs_allow_011_neg.ksh
${PACKAGE}FILES+=	zfs_allow_001_pos.ksh
${PACKAGE}FILES+=	zfs_allow_005_pos.ksh

.include <bsd.test.mk>
