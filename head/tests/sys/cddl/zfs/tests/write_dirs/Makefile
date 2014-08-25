# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=${TESTSBASE}/sys/cddl/zfs/tests/write_dirs
FILESDIR=${TESTSDIR}

ATF_TESTS_KSH93+=	write_dirs_test

FILES+=	write_dirs.cfg
FILES+=	write_dirs_002_pos.ksh
FILES+=	cleanup.ksh
FILES+=	setup.ksh
FILES+=	write_dirs_001_pos.ksh

.include <bsd.test.mk>
