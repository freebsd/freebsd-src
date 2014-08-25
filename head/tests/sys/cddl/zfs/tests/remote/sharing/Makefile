# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/remote/sharing
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	sharing_test

FILES+=	r_setup.ksh
FILES+=	r_cleanup.ksh
FILES+=	cleanup.ksh
FILES+=	sharing_001_pos.ksh
FILES+=	setup.ksh
FILES+=	sharing.cfg

.include <bsd.test.mk>
