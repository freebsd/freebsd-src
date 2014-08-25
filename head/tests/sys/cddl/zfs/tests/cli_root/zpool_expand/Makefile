# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_expand
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_expand_test

FILES+=	zpool_expand_003_neg.ksh
FILES+=	setup.ksh
FILES+=	zpool_expand_002_pos.ksh
FILES+=	cleanup.ksh
FILES+=	zpool_expand.cfg
FILES+=	zpool_expand_001_pos.ksh

.include <bsd.test.mk>
