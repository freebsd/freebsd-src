# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_history
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_history_test

FILES+=	setup.ksh
FILES+=	cleanup.ksh
FILES+=	zpool_history_001_neg.ksh
FILES+=	zpool_history_002_pos.ksh
FILES+=	zpool_history.cfg

.include <bsd.test.mk>
