# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/zvol/zvol_misc
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zvol_misc_test

FILES+=	setup.ksh
FILES+=	zvol_misc.cfg
FILES+=	zvol_misc_006_pos.ksh
FILES+=	zvol_misc_002_pos.ksh
FILES+=	zvol_misc_003_neg.ksh
FILES+=	zvol_misc_007_pos.ksh
FILES+=	zvol_misc_009_pos.ksh
FILES+=	zvol_misc_004_pos.ksh
FILES+=	zvol_misc_008_pos.ksh
FILES+=	zvol_misc_001_neg.ksh
FILES+=	cleanup.ksh
FILES+=	zvol_misc_005_neg.ksh

.include <bsd.test.mk>
