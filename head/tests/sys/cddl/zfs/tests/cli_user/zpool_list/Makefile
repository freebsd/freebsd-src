# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/cli_user/zpool_list
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	zpool_list_test

FILES+=	zpool_list_001_pos.ksh
FILES+=	setup.ksh
FILES+=	cleanup.ksh
FILES+=	zpool_list.cfg
FILES+=	zpool_list_002_neg.ksh

.include <bsd.test.mk>
