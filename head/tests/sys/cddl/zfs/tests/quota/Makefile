# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/quota
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	quota_test

FILES+=	quota_003_pos.ksh
FILES+=	quota_006_neg.ksh
FILES+=	quota_002_pos.ksh
FILES+=	setup.ksh
FILES+=	quota.kshlib
FILES+=	cleanup.ksh
FILES+=	quota_004_pos.ksh
FILES+=	quota_005_pos.ksh
FILES+=	quota_001_pos.ksh
FILES+=	quota.cfg

.include <bsd.test.mk>
