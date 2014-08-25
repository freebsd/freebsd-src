# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/online_offline
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	online_offline_test

FILES+=	online_offline_001_pos.ksh
FILES+=	online_offline.cfg
FILES+=	cleanup.ksh
FILES+=	online_offline_002_neg.ksh

.include <bsd.test.mk>
