# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/zvol/zvol_ENOSPC
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zvol_ENOSPC_test

FILES+=	zvol_ENOSPC_001_pos.ksh
FILES+=	setup.ksh
FILES+=	zvol_ENOSPC.cfg
FILES+=	cleanup.ksh

.include <bsd.test.mk>
