# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_replace
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_replace_test

FILES+=	zpool_replace_001_neg.ksh
FILES+=	setup.ksh
FILES+=	cleanup.ksh
FILES+=	zpool_replace.cfg

.include <bsd.test.mk>
