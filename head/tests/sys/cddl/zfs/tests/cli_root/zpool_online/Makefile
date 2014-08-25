# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_online
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_online_test

FILES+=	cleanup.ksh
FILES+=	zpool_online.cfg
FILES+=	zpool_online_002_neg.ksh
FILES+=	zpool_online_001_pos.ksh
FILES+=	setup.ksh

.include <bsd.test.mk>
