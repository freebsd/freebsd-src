# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zfs_inherit
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_inherit_test

FILES+=	setup.ksh
FILES+=	zfs_inherit_002_neg.ksh
FILES+=	zfs_inherit_003_pos.ksh
FILES+=	cleanup.ksh
FILES+=	zfs_inherit_001_neg.ksh
FILES+=	zfs_inherit.cfg

.include <bsd.test.mk>
