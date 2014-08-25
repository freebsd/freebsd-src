# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/zvol/zvol_cli
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zvol_cli_test

FILES+=	setup.ksh
FILES+=	zvol_cli_003_neg.ksh
FILES+=	zvol_cli_002_pos.ksh
FILES+=	zvol_cli.cfg
FILES+=	cleanup.ksh
FILES+=	zvol_cli_001_pos.ksh

.include <bsd.test.mk>
