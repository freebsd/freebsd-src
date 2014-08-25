# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/grow_pool
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	grow_pool_test

FILES+=	grow_pool_001_pos.ksh
FILES+=	grow_pool.cfg
FILES+=	cleanup.ksh
FILES+=	setup.ksh

.include <bsd.test.mk>
