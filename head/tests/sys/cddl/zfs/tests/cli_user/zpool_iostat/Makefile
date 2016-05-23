# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_user/zpool_iostat
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_iostat_test

${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	zpool_iostat.cfg
${PACKAGE}FILES+=	zpool_iostat_003_neg.ksh
${PACKAGE}FILES+=	zpool_iostat_002_pos.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	zpool_iostat_001_neg.ksh

.include <bsd.test.mk>
