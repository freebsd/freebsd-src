# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/no_space
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	no_space_test
TEST_METADATA+=		required_user="root"

${PACKAGE}FILES+=	enospc.cfg
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	enospc_001_pos.ksh

.include <bsd.test.mk>
