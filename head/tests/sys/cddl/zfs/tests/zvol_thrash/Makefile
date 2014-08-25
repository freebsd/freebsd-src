# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/zvol_thrash
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zvol_thrash_test

FILES+=	setup.ksh
FILES+=	cleanup.ksh
FILES+=	zvol_thrash.cfg
FILES+=	zvol_thrash_001_pos.ksh

.include <bsd.test.mk>
