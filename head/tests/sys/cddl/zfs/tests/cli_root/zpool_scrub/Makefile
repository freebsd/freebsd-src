# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_scrub
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_scrub_test

FILES+=	zpool_scrub_001_neg.ksh
FILES+=	zpool_scrub_004_pos.ksh
FILES+=	cleanup.ksh
FILES+=	zpool_scrub_005_pos.ksh
FILES+=	zpool_scrub.cfg
FILES+=	zpool_scrub_003_pos.ksh
FILES+=	setup.ksh
FILES+=	zpool_scrub_002_pos.ksh

.include <bsd.test.mk>
