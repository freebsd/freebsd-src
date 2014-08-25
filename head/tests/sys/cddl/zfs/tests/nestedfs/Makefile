# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/nestedfs
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	nestedfs_test

FILES+=	nestedfs.cfg
FILES+=	setup.ksh
FILES+=	nestedfs_001_pos.ksh
FILES+=	cleanup.ksh

.include <bsd.test.mk>
