# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_get
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_get_test

FILES+=	setup.ksh
FILES+=	zpool_get_002_pos.ksh
FILES+=	zpool_get_003_pos.ksh
FILES+=	zpool_get.cfg
FILES+=	zpool_get_001_pos.ksh
FILES+=	zpool_get_004_neg.ksh
FILES+=	cleanup.ksh

.include <bsd.test.mk>
