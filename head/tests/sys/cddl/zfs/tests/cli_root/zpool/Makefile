# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_test

FILES+=	zpool_001_neg.ksh
FILES+=	cleanup.ksh
FILES+=	zpool.cfg
FILES+=	zpool_003_pos.ksh
FILES+=	zpool_002_pos.ksh
FILES+=	setup.ksh

.include <bsd.test.mk>
