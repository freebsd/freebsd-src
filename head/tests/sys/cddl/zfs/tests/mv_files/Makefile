# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/mv_files
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	mv_files_test

FILES+=	cleanup.ksh
FILES+=	mv_files_common.kshlib
FILES+=	mv_files_002_pos.ksh
FILES+=	setup.ksh
FILES+=	mv_files_001_pos.ksh
FILES+=	mv_files.cfg

.include <bsd.test.mk>
