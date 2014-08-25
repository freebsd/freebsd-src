# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/privilege
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	privilege_test

FILES+=	privilege_001_pos.ksh
FILES+=	setup.ksh
FILES+=	privilege.cfg
FILES+=	cleanup.ksh
FILES+=	privilege_002_pos.ksh

.include <bsd.test.mk>
