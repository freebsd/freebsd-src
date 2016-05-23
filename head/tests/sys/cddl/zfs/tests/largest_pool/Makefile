# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/largest_pool
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	largest_pool_test

${PACKAGE}FILES+=	largest_pool.cfg
${PACKAGE}FILES+=	largest_pool_001_pos.ksh

.include <bsd.test.mk>
