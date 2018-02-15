# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/truncate
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	truncate_test
TEST_METADATA+=		required_user="root"

${PACKAGE}FILES+=	truncate_001_pos.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	truncate_002_pos.ksh
${PACKAGE}FILES+=	truncate.cfg

.include <bsd.test.mk>
