# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/grow_replicas
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	grow_replicas_test

FILES+=	setup.ksh
FILES+=	cleanup.ksh
FILES+=	grow_replicas.cfg
FILES+=	grow_replicas_001_pos.ksh

.include <bsd.test.mk>
