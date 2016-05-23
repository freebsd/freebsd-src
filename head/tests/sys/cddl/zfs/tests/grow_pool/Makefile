# $FreeBSD$

.include <src.opts.mk>

PACKAGE=tests
TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/grow_pool
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	grow_pool_test

${PACKAGE}FILES+=	grow_pool_001_pos.ksh
${PACKAGE}FILES+=	grow_pool.cfg
${PACKAGE}FILES+=	cleanup.ksh
${PACKAGE}FILES+=	setup.ksh

.include <bsd.test.mk>
