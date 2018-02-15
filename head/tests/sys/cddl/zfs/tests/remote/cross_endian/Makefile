# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/remote/cross_endian
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	cross_endian_test
TEST_METADATA+=		required_user="root"

${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	cross_endian.cfg
${PACKAGE}FILES+=	cross_endian_002_pos.ksh
${PACKAGE}FILES+=	r_verify_recv.ksh
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	cross_endian_001_pos.ksh
${PACKAGE}FILES+=	r_verify_import.ksh
${PACKAGE}FILES+=	r_cleanup.ksh
${PACKAGE}FILES+=	cross_endian_common.kshlib

.include <bsd.test.mk>
