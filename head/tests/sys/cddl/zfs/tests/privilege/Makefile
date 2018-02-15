# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/privilege
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	privilege_test
TEST_METADATA+=		required_user="root"

${PACKAGE}FILES+=	privilege_001_pos.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	privilege.cfg
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	privilege_002_pos.ksh

.include <bsd.test.mk>
