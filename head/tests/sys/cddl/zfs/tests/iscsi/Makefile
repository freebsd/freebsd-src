# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/iscsi
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	iscsi_test

FILES+=	iscsi_004_pos.ksh
FILES+=	cleanup.ksh
FILES+=	iscsi_005_pos.ksh
FILES+=	iscsi_001_pos.ksh
FILES+=	iscsi_common.kshlib
FILES+=	iscsi_002_neg.ksh
FILES+=	iscsi_006_neg.ksh
FILES+=	iscsi_003_neg.ksh
FILES+=	setup.ksh
FILES+=	iscsi.cfg

.include <bsd.test.mk>
