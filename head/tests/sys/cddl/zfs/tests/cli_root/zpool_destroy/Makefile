# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_destroy
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_destroy_test

FILES+=	zpool_destroy_001_pos.ksh
FILES+=	cleanup.ksh
FILES+=	zpool_destroy_003_neg.ksh
FILES+=	zpool_destroy_002_pos.ksh
FILES+=	zpool_destroy.cfg

.include <bsd.test.mk>
