# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_offline
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_offline_test

FILES+=	cleanup.ksh
FILES+=	zpool_offline_002_neg.ksh
FILES+=	setup.ksh
FILES+=	zpool_offline_001_pos.ksh
FILES+=	zpool_offline.cfg

.include <bsd.test.mk>
