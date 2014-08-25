# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/redundancy
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	redundancy_test

FILES+=	redundancy.cfg
FILES+=	redundancy_003_pos.ksh
FILES+=	redundancy_002_pos.ksh
FILES+=	setup.ksh
FILES+=	redundancy.kshlib
FILES+=	cleanup.ksh
FILES+=	redundancy_001_pos.ksh
FILES+=	redundancy_004_neg.ksh

.include <bsd.test.mk>
