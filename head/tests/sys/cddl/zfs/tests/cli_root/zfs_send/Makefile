# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zfs_send
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_send_test

FILES+=	setup.ksh
FILES+=	cleanup.ksh
FILES+=	zfs_send_003_pos.ksh
FILES+=	zfs_send_002_pos.ksh
FILES+=	zfs_send.cfg
FILES+=	zfs_send_001_pos.ksh
FILES+=	zfs_send_004_neg.ksh

.include <bsd.test.mk>
