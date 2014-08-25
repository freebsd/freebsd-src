# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_status
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_status_test

FILES+=	zpool_status.cfg
FILES+=	zpool_status_001_pos.ksh
FILES+=	setup.ksh
FILES+=	cleanup.ksh
FILES+=	zpool_status_002_pos.ksh

.include <bsd.test.mk>
