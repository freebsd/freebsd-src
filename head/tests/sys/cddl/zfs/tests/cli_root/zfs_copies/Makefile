# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zfs_copies
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_copies_test

FILES+=	zfs_copies.cfg
FILES+=	zfs_copies_001_pos.ksh
FILES+=	zfs_copies_004_neg.ksh
FILES+=	zfs_copies_005_neg.ksh
FILES+=	zfs_copies.kshlib
FILES+=	cleanup.ksh
FILES+=	setup.ksh
FILES+=	zfs_copies_006_pos.ksh
FILES+=	zfs_copies_002_pos.ksh
FILES+=	zfs_copies_003_pos.ksh

.include <bsd.test.mk>
