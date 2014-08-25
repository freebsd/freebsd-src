# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zfs
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_test

FILES+=	zfs_malformed.cfg
FILES+=	zfs_001_neg.ksh
FILES+=	cleanup.ksh
FILES+=	setup.ksh
FILES+=	zfs_003_neg.ksh
FILES+=	zfs_002_pos.ksh

.include <bsd.test.mk>
