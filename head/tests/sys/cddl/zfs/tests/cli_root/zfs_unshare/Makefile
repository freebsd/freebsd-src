# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zfs_unshare
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_unshare_test

FILES+=	zfs_unshare_003_pos.ksh
FILES+=	setup.ksh
FILES+=	zfs_unshare_002_pos.ksh
FILES+=	zfs_unshare.cfg
FILES+=	cleanup.ksh
FILES+=	zfs_unshare_005_neg.ksh
FILES+=	zfs_unshare_001_pos.ksh
FILES+=	zfs_unshare_004_neg.ksh

.include <bsd.test.mk>
