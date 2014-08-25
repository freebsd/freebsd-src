# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/zinject
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zinject_test

FILES+=	zinject.cfg
FILES+=	cleanup.ksh
FILES+=	zinject_004_pos.ksh
FILES+=	setup.ksh
FILES+=	zinject_001_pos.ksh
FILES+=	zinject.kshlib
FILES+=	zinject_003_pos.ksh
FILES+=	zinject_002_pos.ksh

.include <bsd.test.mk>
