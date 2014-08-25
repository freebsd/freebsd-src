# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_user/zpool_iostat
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_iostat_test

FILES+=	cleanup.ksh
FILES+=	zpool_iostat.cfg
FILES+=	zpool_iostat_003_neg.ksh
FILES+=	zpool_iostat_002_pos.ksh
FILES+=	setup.ksh
FILES+=	zpool_iostat_001_neg.ksh

.include <bsd.test.mk>
