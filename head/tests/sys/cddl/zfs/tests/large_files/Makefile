# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/large_files
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	large_files_test

FILES+=	cleanup.ksh
FILES+=	setup.ksh
FILES+=	large_files_001_pos.ksh
FILES+=	large_files.cfg

.include <bsd.test.mk>
