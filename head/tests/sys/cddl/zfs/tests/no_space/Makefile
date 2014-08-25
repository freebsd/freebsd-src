# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/no_space
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	no_space_test

FILES+=	enospc.cfg
FILES+=	cleanup.ksh
FILES+=	setup.ksh
FILES+=	enospc_001_pos.ksh

.include <bsd.test.mk>
