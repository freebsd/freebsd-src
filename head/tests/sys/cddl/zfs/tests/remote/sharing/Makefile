# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/remote/sharing
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	sharing_test
TEST_METADATA+=		required_user="root"

${PACKAGE}FILES+=	r_setup.ksh
${PACKAGE}FILES+=	r_cleanup.ksh
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	sharing_001_pos.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	sharing.cfg

.include <bsd.test.mk>
