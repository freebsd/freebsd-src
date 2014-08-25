# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/refquota
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	refquota_test

FILES+=	refquota_001_pos.ksh
FILES+=	refquota_005_pos.ksh
FILES+=	refquota.cfg
FILES+=	cleanup.ksh
FILES+=	setup.ksh
FILES+=	refquota_004_pos.ksh
FILES+=	refquota_002_pos.ksh
FILES+=	refquota_003_pos.ksh
FILES+=	refquota_006_neg.ksh

.include <bsd.test.mk>
