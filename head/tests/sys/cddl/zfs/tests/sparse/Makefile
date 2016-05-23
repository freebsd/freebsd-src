# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/sparse
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	sparse_test

${PACKAGE}FILES+=	sparse_001_pos.ksh
${PACKAGE}FILES+=	setup.ksh
${PACKAGE}FILES+=	sparse.cfg
${PACKAGE}FILES+=	cleanup.ksh

.include <bsd.test.mk>
