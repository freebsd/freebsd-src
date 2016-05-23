# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zdb
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zdb_test

${PACKAGE}FILES+=	zdb_001_neg.ksh
${PACKAGE}FILES+=	zdb.cfg
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	setup.ksh

.include <bsd.test.mk>
