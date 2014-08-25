# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zpool_attach
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_attach_test

FILES+=	setup.ksh
FILES+=	zpool_attach_001_neg.ksh
FILES+=	cleanup.ksh
FILES+=	zpool_attach.cfg

.include <bsd.test.mk>
