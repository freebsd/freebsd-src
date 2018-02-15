# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/remote/rebooting
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	rebooting_test
TEST_METADATA+=		required_user="root"

${PACKAGE}FILES+=	rebooting.cfg
${PACKAGE}FILES+=	r_setup.ksh
${PACKAGE}FILES+=	r_dir_ops.ksh
${PACKAGE}FILES+=	r_cleanup.ksh
${PACKAGE}FILES+=	rebooting_001_pos.ksh
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	r_verify_booting.ksh

.include <bsd.test.mk>
