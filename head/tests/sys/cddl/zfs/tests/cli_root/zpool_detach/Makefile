# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_detach
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_detach_test

FILES+=	zpool_detach.cfg
FILES+=	zpool_detach_001_neg.ksh
FILES+=	cleanup.ksh
FILES+=	setup.ksh

.include <bsd.test.mk>
