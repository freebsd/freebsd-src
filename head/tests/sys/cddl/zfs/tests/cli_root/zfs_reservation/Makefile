# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_root/zfs_reservation
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zfs_reservation_test

FILES+=	zfs_reservation.cfg
FILES+=	zfs_reservation_001_pos.ksh
FILES+=	zfs_reservation_002_pos.ksh
FILES+=	setup.ksh
FILES+=	cleanup.ksh

.include <bsd.test.mk>
